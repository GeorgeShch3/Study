#include "lidar_sim.hpp"
#include <iostream>
#include <vector>
#include <array>
#include <random>
#include <cmath>
#include <omp.h>

struct Mat3 {
    double m[3][3] = {};
    double& operator()(int i, int j)       { return m[i][j]; }
    double  operator()(int i, int j) const { return m[i][j]; }
};

static int jacobi3(Mat3& A, Mat3& V) {
    V = Mat3{};
    V(0,0) = V(1,1) = V(2,2) = 1;
    for (int iter = 0; iter < 100; ++iter) {
        int p = 0, q = 1;
        double mx = std::abs(A(0,1));
        if (std::abs(A(0,2)) > mx) { mx = std::abs(A(0,2)); p = 0; q = 2; }
        if (std::abs(A(1,2)) > mx) {                         p = 1; q = 2; }
        if (mx < 1e-14) break;

        double diff = A(q,q) - A(p,p);
        double t;
        if      (std::abs(A(p,q)) < 1e-14) t = 0;
        else if (std::abs(diff)   < 1e-14) t = (A(p,q) > 0) ? 1.0 : -1.0;
        else {
            double tau = diff / (2.0 * A(p,q));
            t = std::copysign(1.0 / (std::abs(tau) + std::sqrt(1 + tau*tau)),
                              A(p,q));
        }
        double c = 1.0 / std::sqrt(1 + t*t), s = t * c;
        double App = A(p,p), Aqq = A(q,q), Apq = A(p,q);
        A(p,p) = App - t*Apq; A(q,q) = Aqq + t*Apq; A(p,q) = A(q,p) = 0;

        for (int r = 0; r < 3; ++r) {
            if (r == p || r == q) continue;
            double Arp = A(r,p), Arq = A(r,q);
            A(r,p) = A(p,r) = c*Arp - s*Arq;
            A(r,q) = A(q,r) = s*Arp + c*Arq;
        }
        for (int r = 0; r < 3; ++r) {
            double Vrp = V(r,p), Vrq = V(r,q);
            V(r,p) = c*Vrp - s*Vrq;
            V(r,q) = s*Vrp + c*Vrq;
        }
    }
    int mn = 0;
    if (A(1,1) < A(mn,mn)) mn = 1;
    if (A(2,2) < A(mn,mn)) mn = 2;
    return mn;
}

struct FittedPlane {
    Vec3   normal;
    double d;
    double distTo(const Vec3& p) const { return std::abs(normal.dot(p) - d); }
};

static FittedPlane fitPlane(const std::vector<size_t>& idx, const std::vector<LidarPoint>& pts) {
    Vec3 c{};
    for (size_t i : idx) c += pts[i].pos;
    c = c / (double)idx.size();

    Mat3 cov;
    for (size_t i : idx) {
        Vec3 d = pts[i].pos - c;
        cov(0,0)+=d.x*d.x; 
        cov(0,1)+=d.x*d.y; 
        cov(0,2)+=d.x*d.z;
        cov(1,1)+=d.y*d.y; 
        cov(1,2)+=d.y*d.z;
        cov(2,2)+=d.z*d.z;
    }
    cov(1,0)=cov(0,1); 
    cov(2,0)=cov(0,2); 
    cov(2,1)=cov(1,2);

    Mat3 V;
    int mn = jacobi3(cov, V);
    Vec3 normal = Vec3{V(0,mn), V(1,mn), V(2,mn)}.normalized();
    double dd = normal.dot(c);
    if (dd < 0) { normal = normal * -1; dd = -dd; }
    return {normal, dd};
}

class PlaneStitcher {
public:
    static PointCloud stitch(const PointCloud& cloud, const Vec3& room_center, double threshold) {
        size_t N = cloud.size();
        std::cout << "\n=== Сшивка по плоскостям ===\n"
                  << "  Точек до: " << N
                  << "  Порог: " << threshold * 100 << " см\n\n";

        const char* names[6] = {"X+","X-","Y+","Y-","Z+ (потолок)","Z- (пол)"};
        std::array<std::vector<size_t>, 6> buckets;
        for (size_t i = 0; i < N; ++i)
            buckets[nearestFace(cloud.points[i].pos, room_center)].push_back(i);

        PointCloud result;
        size_t total_after = 0;
        for (int f = 0; f < 6; ++f) {
            size_t n = buckets[f].size();
            if (n < 10) {
                std::cout << "  " << names[f] << ": " << n << " точек, пропуск\n";
                continue;
            }
            FittedPlane plane = fitPlane(buckets[f], cloud.points);
            size_t inliers = 0, outliers = 0;
            for (size_t i : buckets[f]) {
                if (plane.distTo(cloud.points[i].pos) <= threshold) {
                    result.points.push_back(cloud.points[i]);
                    ++inliers;
                } else { ++outliers; }
            }
            total_after += inliers;
            std::cout << "  " << names[f] << ": " << n
                      << "  инлайеры: " << inliers
                      << " (" << 100.0*inliers/n << "%)"
                      << "  выброшено: " << outliers << "\n";
        }
        std::cout << "\n  Итого: " << total_after
                  << "  убрано: " << (N - total_after)
                  << " (" << 100.0*(N-total_after)/N << "%)\n";
        return result;
    }

private:
    static int nearestFace(const Vec3& p, const Vec3& c) {
        double W = c.x * 2, D = c.y * 2, H = c.z * 2;
        double dist[6] = {
            std::abs(p.x - W),  
            std::abs(p.x),      
            std::abs(p.y - D),  
            std::abs(p.y),      
            std::abs(p.z - H),  
            std::abs(p.z),      
        };
        return (int)(std::min_element(dist, dist + 6) - dist);
    }
};

int main() {
    const double W = 6.0, D = 6.0, H = 3.0;
    Scene scene;
    scene.add(makeRoom(W, D, H));

    const double LH  = H / 2.0;     
    const double MGN = 0.3;          

    Vec3 center{W/2, D/2, LH};
    Vec3 room_center{W/2, D/2, H/2};

    const double CAL_SIGMA = 0.10;
    std::mt19937 cal_gen(1234);
    std::normal_distribution<double> cal_dist(0.0, CAL_SIGMA);

    struct Scanner { Vec3 pos; Vec3 look_at; };
    std::vector<Scanner> scanners = {
        {{MGN,   MGN,   LH}, center},
        {{W-MGN, MGN,   LH}, center},
        {{MGN,   D-MGN, LH}, center},
        {{W-MGN, D-MGN, LH}, center},
    };

    std::cout << "=== Статическое сканирование комнаты ===\n"
              << "Комната " << W << "×" << D << "×" << H << " м\n"
              << "Лидаров: " << scanners.size()
              << "  H_FOV=180°  V_FOV=±60°  высота=" << LH << " м\n\n"
              << "Ошибки калибровки (σ=" << CAL_SIGMA*100 << " см):\n";

    PointCloud combined;
    for (int i = 0; i < (int)scanners.size(); ++i) {
        double bias = cal_dist(cal_gen);
        std::cout << "  Лидар " << (i+1)
                  << " pos=(" << scanners[i].pos.x << ","
                  << scanners[i].pos.y << "," << scanners[i].pos.z << ")"
                  << "  смещение: " << bias * 100 << " см\n";

        LidarConfig cfg = LidarConfig::custom(
            scanners[i].pos, scanners[i].look_at,
            360, 32,           
            180.0, 120.0,      
            0.1, 20.0          
        );
        cfg.range_noise_stddev = 0.02;
        cfg.angle_noise_stddev = 0.001;
        cfg.dropout_probability = 0.001;
        cfg.range_bias = bias;  

        PointCloud s = scan(scene, cfg, i);
        std::cout << "    -> " << s.size() << " точек\n";
        combined.append(s);
    }

    std::cout << "\nВсего точек до сшивки: " << combined.size() << "\n";
    combined.saveToCSV("corners_before.csv", "scanner_id");

    const double STITCH_THRESHOLD = 0.03;
    PointCloud stitched = PlaneStitcher::stitch(
        combined, room_center, STITCH_THRESHOLD);
    stitched.saveToCSV("corners_after.csv", "scanner_id");

    return 0;
}
