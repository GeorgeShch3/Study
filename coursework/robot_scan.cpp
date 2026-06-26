#include "lidar_sim.hpp"
#include <small_gicp/registration/registration_helper.hpp>
#include <small_gicp/util/downsampling.hpp>
#include <small_gicp/points/point_cloud.hpp>
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <iostream>
#include <vector>
#include <random>
#include <omp.h>

struct Pose2D { double x, y, theta; };

std::vector<Pose2D> makePath(double x0, double y0, double x1, double y1, double step, double row_gap) {
    std::vector<Pose2D> path;
    double margin = row_gap * 0.5;
    double xs = x0 + margin, xe = x1 - margin;
    double ys = y0 + margin, ye = y1 - margin;
    int row = 0;
    for (double x = xs; x <= xe + 1e-6; x += row_gap, ++row) {
        double yA  = (row % 2 == 0) ? ys : ye;
        double yB  = (row % 2 == 0) ? ye : ys;
        double dir = (row % 2 == 0) ? 1.0 : -1.0;
        for (double y = yA;
             (dir > 0) ? (y <= yB + 1e-6) : (y >= yB - 1e-6);
             y += dir * step)
        {
            path.push_back({std::min(x, xe),
                            std::clamp(y, ys, ye),
                            (dir > 0) ? M_PI / 2 : -M_PI / 2});
        }
    }
    return path;
}

std::vector<Eigen::Vector4d> toSgVec(const PointCloud& pc) {
    std::vector<Eigen::Vector4d> pts;
    pts.reserve(pc.size());
    for (const auto& p : pc.points)
        pts.push_back({p.pos.x, p.pos.y, p.pos.z, 1.0});
    return pts;
}

std::shared_ptr<small_gicp::PointCloud> toSgCloud(const PointCloud& pc) {
    auto cloud = std::make_shared<small_gicp::PointCloud>();
    cloud->resize(pc.size());
    for (size_t i = 0; i < pc.size(); ++i)
        cloud->point(i) = Eigen::Vector4d(
            pc.points[i].pos.x, pc.points[i].pos.y,
            pc.points[i].pos.z, 1.0);
    return cloud;
}

PointCloud applyTransform(const PointCloud& pc, const Eigen::Isometry3d& T, int id) {
    PointCloud result;
    for (const auto& p : pc.points) {
        Eigen::Vector3d tv = T * Eigen::Vector3d(p.pos.x, p.pos.y, p.pos.z);
        result.add({tv.x(), tv.y(), tv.z()}, p.dist, id);
    }
    return result;
}

void voxelFilter(PointCloud& pc, double vsize, int id) {
    auto sg   = toSgCloud(pc);
    auto down = small_gicp::voxelgrid_sampling(*sg, vsize);
    pc.clear();
    for (size_t i = 0; i < down->size(); ++i) {
        const auto& p = down->point(i);
        pc.add(Vec3(p[0], p[1], p[2]), 0, id);
    }
}

int main() {
    const double W = 6.0, D = 6.0, H = 3.0;
    Scene scene;
    scene.add(makeRoom(W, D, H));
    scene.add(makeBox(1.0, 1.0, 0.0,  0.6, 0.6, 1.0));
    scene.add(makeBox(4.0, 4.0, 0.0,  0.8, 0.5, 1.2));
    scene.add(makeBox(2.5, 3.0, 0.0,  0.4, 0.4, 1.5));

    LidarConfig base_cfg = LidarConfig::robotVacuum({0, 0, 0});

    const double ODOM_SIGMA = 0.02;  
    const double VOXEL_SIZE = 0.05;

    small_gicp::RegistrationSetting icp;
    icp.type                        = small_gicp::RegistrationSetting::GICP;
    icp.voxel_resolution            = 0.10;
    icp.max_correspondence_distance = 0.8;
    icp.rotation_eps                = 1e-4;
    icp.translation_eps             = 1e-4;
    icp.max_iterations              = 50;
    icp.num_threads                 = omp_get_max_threads();

    const double STEP = 0.2, ROW_GAP = 1.0;
    auto path = makePath(0, 0, W, D, STEP, ROW_GAP);
    int  N    = (int)path.size();

    std::cout << "=== Скан роботом-пылесосом ===\n"
              << "Комната " << W << "×" << D << "×" << H << " м + 3 препятствия\n"
              << "Маршрут: " << N << " позиций  шаг=" << STEP << " м\n\n";

    std::mt19937 odom_gen(42);
    std::normal_distribution<double> odom_dist(0, ODOM_SIGMA);
    std::vector<double> odx(N), ody(N);
    {
        double cx = 0, cy = 0;
        for (int i = 0; i < N; ++i) {
            cx += odom_dist(odom_gen); odx[i] = cx;
            cy += odom_dist(odom_gen); ody[i] = cy;
        }
    }
    std::cout << "Накопленная ошибка одометрии к концу: ("
              << odx[N-1] * 100 << " см, " << ody[N-1] * 100 << " см)\n\n";

    std::cout << "--- Прогон 1: без ICP ---\n";
    PointCloud map_no_icp;
    double t0 = omp_get_wtime();

    for (int i = 0; i < N; ++i) {
        auto cfg      = base_cfg;
        cfg.position  = {path[i].x, path[i].y, base_cfg.position.z};
        PointCloud s  = scan(scene, cfg, i);

        for (const auto& pt : s.points)
            map_no_icp.add(pt.pos + Vec3(odx[i], ody[i], 0), pt.dist, i);

        voxelFilter(map_no_icp, VOXEL_SIZE, i);
    }

    std::cout << "Время: " << (omp_get_wtime() - t0) << " с"
              << "  Точек: " << map_no_icp.size() << "\n";
    map_no_icp.saveToCSV("robot_before.csv", "scan_id");

    std::cout << "\n--- Прогон 2: с ICP ---\n";
    PointCloud map_icp;
    t0 = omp_get_wtime();
    int icp_ok = 0, icp_fail = 0;

    for (int i = 0; i < N; ++i) {
        auto cfg     = base_cfg;
        cfg.position = {path[i].x, path[i].y, base_cfg.position.z};
        PointCloud s = scan(scene, cfg, i);

        if (i == 0) {
            map_icp.append(s);
            voxelFilter(map_icp, VOXEL_SIZE, i);
            continue;
        }

        Eigen::Isometry3d init = Eigen::Isometry3d::Identity();
        init.translation() = Eigen::Vector3d(odx[i] - odx[i-1],
                                             ody[i] - ody[i-1], 0);

        auto source = toSgVec(s);
        auto target = toSgVec(map_icp);
        auto result = small_gicp::align(target, source, init, icp);

        PointCloud corrected = applyTransform(s, result.T_target_source, i);
        map_icp.append(corrected);
        voxelFilter(map_icp, VOXEL_SIZE, i);

        if (result.converged) ++icp_ok; else ++icp_fail;

        if ((i + 1) % 20 == 0 || i == N - 1)
            std::cout << "Скан " << (i+1) << "/" << N
                      << "  карта: " << map_icp.size()
                      << "  ICP: " << (result.converged ? "OK" : "--")
                      << "  итер: " << result.iterations << "\n";
    }

    std::cout << "Время: " << (omp_get_wtime() - t0) << " с"
              << "  Точек: " << map_icp.size() << "\n"
              << "ICP: сошёлся " << icp_ok
              << " раз  не сошёлся " << icp_fail << " раз\n";
    map_icp.saveToCSV("robot_after.csv", "scan_id");

    std::cout << "\nФайлы для сравнения:\n"
              << "  robot_before.csv — карта с ошибкой одометрии\n"
              << "  robot_after.csv  — карта после коррекции ICP\n";
    return 0;
}
