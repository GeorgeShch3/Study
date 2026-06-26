#pragma once
#include "vec3.hpp"
#include "geometry.hpp"
#include "point_cloud.hpp"
#include "noise_model.hpp"
#include <cmath>
#include <vector>
#include <omp.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct LidarConfig {
    Vec3   position;
    Vec3   forward_dir  = {1, 0, 0}; 
    Vec3   up_dir       = {0, 0, 1}; 

    int    horizontal_beams   = 512;
    int    vertical_channels  = 32;
    double horizontal_fov_deg = 360.0;
    double vertical_fov_deg   = 30.0;

    double min_range  = 0.1;
    double max_range  = 100.0;

    double range_noise_stddev   = 0.02;
    double angle_noise_stddev   = 0.001;
    double dropout_probability  = 0.002;
    double range_bias           = 0.0; 

    static LidarConfig custom(const Vec3& pos, const Vec3& look_at, int hb, int vc, double hf, double vf, double mn = 0.1, double mx = 100.0) {
        LidarConfig c;
        c.position    = pos;
        c.forward_dir = (look_at - pos).normalized();
        c.horizontal_beams   = hb;  c.vertical_channels = vc;
        c.horizontal_fov_deg = hf;  c.vertical_fov_deg  = vf;
        c.min_range   = mn;         c.max_range          = mx;
        return c;
    }

    static LidarConfig vlp16(const Vec3& pos, const Vec3& look_at) {
        auto c = custom(pos, look_at, 1800, 16, 360.0, 30.0, 0.9, 100.0);
        c.range_noise_stddev = 0.03;
        return c;
    }

    static LidarConfig os1_64(const Vec3& pos, const Vec3& look_at) {
        auto c = custom(pos, look_at, 1024, 64, 360.0, 45.0, 0.3, 120.0);
        c.range_noise_stddev = 0.015;
        return c;
    }

    static LidarConfig robotVacuum(const Vec3& pos,
                                   double lidar_height_m = 0.12) {
        LidarConfig c;
        c.position           = {pos.x, pos.y, pos.z + lidar_height_m};
        c.forward_dir        = {1, 0, 0}; 
        c.horizontal_beams   = 360;
        c.vertical_channels  = 4;
        c.horizontal_fov_deg = 360.0;
        c.vertical_fov_deg   = 10.0;  
        c.min_range          = 0.1;
        c.max_range          = 12.0;
        c.range_noise_stddev = 0.02;
        c.angle_noise_stddev = 0.001;
        c.dropout_probability = 0.001;
        return c;
    }
};

class RayGenerator {
public:
    RayGenerator() : cfg_(), frame_(Vec3{1, 0, 0}) {}   

    explicit RayGenerator(const LidarConfig& cfg)
        : cfg_(cfg), frame_(cfg.forward_dir, cfg.up_dir) {}

    Vec3 direction(int px, int py) const {
        return direction(static_cast<double>(px), static_cast<double>(py));
    }

    Vec3 direction(double px, double py) const {
        double half_h = cfg_.horizontal_fov_deg * M_PI / 360.0;
        double half_v = cfg_.vertical_fov_deg   * M_PI / 360.0;

        double h = (2.0 * (px + 0.5) / cfg_.horizontal_beams  - 1.0) * half_h;
        double v = (1.0 - 2.0 * (py + 0.5) / cfg_.vertical_channels) * half_v;

        Vec3 local{
             std::sin(h) * std::cos(v),  
             std::sin(v),                 
             std::cos(h) * std::cos(v),  
        };
        return frame_.localToWorld(local).normalized();
    }

    const LidarConfig& config() const { return cfg_; }

private:
    LidarConfig cfg_;
    CoordFrame  frame_;
};

inline PointCloud scan(const Scene& scene, const LidarConfig& cfg, int scan_id = -1) {
    RayGenerator ray_gen(cfg);
    const int max_th = omp_get_max_threads();
    std::vector<PointCloud> tc(max_th);

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        NoiseModel noise(
            cfg.range_noise_stddev,
            cfg.angle_noise_stddev,
            cfg.dropout_probability,
            static_cast<unsigned>(std::random_device{}()) ^ (tid * 2654435761u)
        );

        #pragma omp for schedule(dynamic)
        for (int py = 0; py < cfg.vertical_channels; ++py) {
            for (int px = 0; px < cfg.horizontal_beams; ++px) {
                if (noise.drop()) continue;

                Vec3   dir = noise.angNoise(ray_gen.direction(px, py));
                double t   = cfg.max_range;

                if (scene.intersect(cfg.position, dir, t, cfg.max_range)
                    && t >= cfg.min_range)
                {
                    double nt = noise.rngNoiseBiased(
                        t, cfg.min_range, cfg.max_range, cfg.range_bias);
                    tc[tid].add(cfg.position + dir * nt, nt,
                                scan_id, px, py);
                }
            }
        }
    }

    PointCloud result;
    for (auto& c : tc) result.append(c);
    return result;
}