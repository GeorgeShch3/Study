#include <lidar_sim.hpp>
#include <iostream>
#include <vector>
#include <algorithm>
#include <omp.h>

struct DepthBuffer {
    int    width = 0, height = 0;
    double max_range = 0;
    std::vector<double> data;

    DepthBuffer() = default;
    DepthBuffer(int w, int h, double mr) : width(w), height(h), max_range(mr), data(w * h, mr) {}

    double get(int px, int py)          const { return data[py * width + px]; }
    void   set(int px, int py, double d)      { data[py * width + px] = d; }
    bool   hasHit(int px, int py)       const { return get(px,py) < max_range - 1e-6; }
};

class EdgeDetector {
public:
    static std::vector<double> compute(const DepthBuffer& depth, double coplanar_tol = 0.02) {
        int W = depth.width, H = depth.height;
        std::vector<double> edges(W * H, 0.0);
        const int DX[] = {-1, 1, 0, 0};
        const int DY[] = { 0, 0,-1, 1};
        double gmax = 0;

        for (int py = 0; py < H; ++py)
        for (int px = 0; px < W; ++px) {
            bool   self_hit = depth.hasHit(px, py);
            double d        = depth.get(px, py);
            double max_diff = 0;

            for (int k = 0; k < 4; ++k) {
                int nx = px + DX[k], ny = py + DY[k];
                if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
                bool nbr_hit = depth.hasHit(nx, ny);

                if (self_hit != nbr_hit) {
                    // Силуэт
                    max_diff = depth.max_range;
                } else if (self_hit) {
                    double nd  = depth.get(nx, ny);
                    double tol = coplanar_tol * std::min(d, nd);
                    max_diff   = std::max(max_diff, std::max(0.0, std::abs(d - nd) - tol));
                }
            }
            edges[py * W + px] = max_diff;
            gmax = std::max(gmax, max_diff);
        }

        if (gmax > 1e-12)
            for (auto& e : edges) e /= gmax;
        return edges;
    }
};

class AdaptiveSampler {
public:
    static std::vector<std::pair<double, double>>
    generate(const std::vector<double>& edges, int coarse_W, int coarse_H, int full_W, int local_H, int start_ch, double bias) {
        auto imp     = buildImportance(edges, coarse_W, coarse_H, bias);
        auto row_cdf = buildRowCDF(imp, coarse_W, coarse_H);

        std::vector<std::pair<double, double>> samples;
        samples.reserve(full_W * local_H);

        for (int ky = 0; ky < local_H; ++ky) {
            double cy_cont = inverseCDF(row_cdf, (ky + 0.5) / local_H);
            double py_cont = start_ch + cy_cont * (double)local_H / coarse_H;

            int    cy0   = std::clamp((int)cy_cont,     0, coarse_H - 1);
            int    cy1   = std::clamp((int)cy_cont + 1, 0, coarse_H - 1);
            double alpha = cy_cont - std::floor(cy_cont);

            std::vector<double> col_w(coarse_W);
            for (int cx = 0; cx < coarse_W; ++cx)
                col_w[cx] = (1 - alpha) * imp[cy0 * coarse_W + cx]
                           +      alpha  * imp[cy1 * coarse_W + cx];

            auto col_cdf = buildCDF(col_w);
            for (int kx = 0; kx < full_W; ++kx) {
                double cx_cont = inverseCDF(col_cdf, (kx + 0.5) / full_W);
                double px_cont = cx_cont * (double)full_W / coarse_W;
                samples.emplace_back(px_cont, py_cont);
            }
        }
        return samples;
    }

private:
    static std::vector<double> buildImportance(
        const std::vector<double>& edges,
        int W, int H, double bias)
    {
        constexpr double FLOOR = 0.05;
        std::vector<double> imp(W * H);
        for (int i = 0; i < W * H; ++i) {
            double e = edges[i];
            double w = (bias >= 0.0) ? (1.0 - bias) + bias * e : 1.0 + bias * e;
            imp[i] = FLOOR + (1.0 - FLOOR) * std::max(0.0, w);
        }
        return imp;
    }

    static std::vector<double> buildRowCDF(const std::vector<double>& imp, int W, int H) {
        std::vector<double> row_w(H, 0);
        for (int cy = 0; cy < H; ++cy)
            for (int cx = 0; cx < W; ++cx)
                row_w[cy] += imp[cy * W + cx];
        return buildCDF(row_w);
    }

    static std::vector<double> buildCDF(const std::vector<double>& w) {
        int N = (int)w.size();
        std::vector<double> cdf(N + 1, 0);
        for (int i = 0; i < N; ++i) cdf[i + 1] = cdf[i] + w[i];
        double total = cdf[N];
        if (total > 1e-12)
            for (auto& v : cdf) v /= total;
        else
            for (int i = 1; i <= N; ++i) cdf[i] = (double)i / N;
        return cdf;
    }

    static double inverseCDF(const std::vector<double>& cdf, double t) {
        t = std::clamp(t, 0.0, 1.0);
        int N = (int)cdf.size() - 1;
        int lo = 0, hi = N - 1;
        while (lo < hi) {
            int mid = (lo + hi) / 2;
            if (cdf[mid + 1] <= t) lo = mid + 1; else hi = mid;
        }
        double lo_v = cdf[lo], hi_v = cdf[lo + 1];
        double frac = (hi_v > lo_v) ? (t - lo_v) / (hi_v - lo_v) : 0.5;
        return lo + std::clamp(frac, 0.0, 1.0);
    }
};

class AdaptiveLidar {
public:
    explicit AdaptiveLidar(const LidarConfig& cfg) : ray_gen_(cfg) {}

    void setDensityBias   (double b) { bias_          = std::clamp(b, -1.0, 1.0); }
    void setCoarseFactor  (int f)    { coarse_factor_ = std::max(1, f); }
    void setCoplanarTol   (double t) { coplanar_tol_  = std::max(0.0, t); }

    PointCloud scan(const Scene& scene) const {
        return (std::abs(bias_) < 1e-9) ? uniformScan(scene) : adaptiveScan(scene);
    }

private:
    PointCloud uniformScan(const Scene& scene) const {
        const auto& cfg = ray_gen_.config();
        int max_th = omp_get_max_threads();
        std::vector<PointCloud> tc(max_th);

        #pragma omp parallel
        {
            int tid = omp_get_thread_num();
            NoiseModel noise = makeNoise(tid);
            #pragma omp for schedule(dynamic)
            for (int py = 0; py < cfg.vertical_channels; ++py)
            for (int px = 0; px < cfg.horizontal_beams;  ++px) {
                if (noise.drop()) continue;
                Vec3   dir = noise.angNoise(ray_gen_.direction(px, py));
                double t   = cfg.max_range;
                if (scene.intersect(cfg.position, dir, t, cfg.max_range)
                    && t >= cfg.min_range)
                    tc[tid].add(cfg.position + dir * noise.rngNoise(t, cfg.min_range, cfg.max_range),
                                t, -1, px, py);
            }
        }
        PointCloud result;
        for (auto& c : tc) result.append(c);
        return result;
    }

    PointCloud adaptiveScan(const Scene& scene) const {
        const auto& cfg = ray_gen_.config();
        int cW = std::max(4, cfg.horizontal_beams  / coarse_factor_);
        int cH = std::max(4, cfg.vertical_channels / coarse_factor_);

        DepthBuffer coarse = buildCoarse(scene, cW, cH);
        auto edges    = EdgeDetector::compute(coarse, coplanar_tol_);
        auto positions = AdaptiveSampler::generate(
            edges, cW, cH,
            cfg.horizontal_beams, cfg.vertical_channels,
            0, bias_);

        return castRays(scene, positions);
    }

    DepthBuffer buildCoarse(const Scene& scene, int cW, int cH) const {
        const auto& cfg = ray_gen_.config();
        DepthBuffer depth(cW, cH, cfg.max_range);
        double sx = (double)cfg.horizontal_beams  / cW;
        double sy = (double)cfg.vertical_channels / cH;

        #pragma omp parallel for schedule(dynamic)
        for (int cy = 0; cy < cH; ++cy)
        for (int cx = 0; cx < cW; ++cx) {
            Vec3   dir = ray_gen_.direction((cx + 0.5) * sx - 0.5, (cy + 0.5) * sy - 0.5);
            double t   = cfg.max_range;
            if (scene.intersect(cfg.position, dir, t, cfg.max_range)
                && t >= cfg.min_range)
                depth.set(cx, cy, t);
        }
        return depth;
    }

    PointCloud castRays(const Scene& scene, const std::vector<std::pair<double,double>>& pos) const {
        const auto& cfg = ray_gen_.config();
        int N = (int)pos.size(), max_th = omp_get_max_threads();
        std::vector<PointCloud> tc(max_th);

        #pragma omp parallel
        {
            int tid = omp_get_thread_num();
            NoiseModel noise = makeNoise(tid);
            #pragma omp for schedule(dynamic)
            for (int k = 0; k < N; ++k) {
                if (noise.drop()) continue;
                auto [px_f, py_f] = pos[k];
                Vec3   dir = noise.angNoise(ray_gen_.direction(px_f, py_f));
                double t   = cfg.max_range;
                if (scene.intersect(cfg.position, dir, t, cfg.max_range)
                    && t >= cfg.min_range) {
                    double nt = noise.rngNoise(t, cfg.min_range, cfg.max_range);
                    tc[tid].add(cfg.position + dir * nt, nt,
                                -1, (int)px_f, (int)py_f);
                }
            }
        }
        PointCloud result;
        for (auto& c : tc) result.append(c);
        return result;
    }

    NoiseModel makeNoise(int tid) const {
        const auto& cfg = ray_gen_.config();
        return NoiseModel(
            cfg.range_noise_stddev, cfg.angle_noise_stddev,
            cfg.dropout_probability,
            static_cast<unsigned>(std::random_device{}()) ^ (tid * 2654435761u));
    }

    RayGenerator ray_gen_;
    double bias_         = 0.0;
    int    coarse_factor_ = 4;
    double coplanar_tol_ = 0.02;
};

int main(int argc, char** argv) {
    Scene scene;

    if (argc >= 2) {
        std::cout << "Загружаем " << argv[1] << " ...\n";
        scene.addMesh(MeshLoader::load(argv[1]));
        std::cout << "Треугольников: " << scene.triangleCount() << "\n\n";
    } else {
        scene.addMesh(PrimitiveFactory::createBox({0,0,0}, 2.0, 2.0, 2.0));
    }

    Vec3 look_at{0, 0, 0};

    std::vector<Vec3> positions = {
        { 5, 0, 0}, {-5, 0, 0},
        { 0, 5, 0}, { 0,-5, 0},
        { 0, 0, 5}, { 0, 0,-5},
    };

    std::cout << "Потоков: " << omp_get_max_threads()
              << "  Треугольников: " << scene.triangleCount() << "\n\n";

    auto run = [&](double bias, const std::string& filename) {
        PointCloud combined;
        double t0 = omp_get_wtime();

        for (const auto& pos : positions) {
            LidarConfig cfg = LidarConfig::custom(
                pos, look_at, 1024, 1024, 90.0, 60.0, 0.1, 50.0);

            AdaptiveLidar lidar(cfg);
            lidar.setDensityBias(bias);

            combined.append(lidar.scan(scene));
        }

        std::cout << filename
                  << "  bias=" << bias
                  << "  points=" << combined.size()
                  << "  time=" << (omp_get_wtime() - t0) << " s\n";
        combined.saveToCSV(filename);
    };

    run( 0.0, "uniform.csv");   
    run(+0.8, "edge_dense.csv");
    run(-0.8, "flat_dense.csv"); 

    return 0;
}