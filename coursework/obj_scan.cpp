#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include "lidar_sim.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <omp.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " model.obj [output.csv]\n";
        return 1;
    }
    const std::string obj_path = argv[1];
    const std::string csv_path = (argc >= 3) ? argv[2] : "points.csv";

    std::cout << "Loading " << obj_path << " ...\n";
    Mesh mesh = MeshLoader::load(obj_path);
    std::cout << "Loaded " << mesh.triangleCount() << " triangles.\n";
    if (mesh.triangleCount() == 0) {
        std::cerr << "Error: mesh is empty.\n";
        return 1;
    }

    Scene scene;
    scene.addMesh(mesh);

    Vec3   center;
    double radius;
    modelBoundingSphere(mesh, center, radius);

    double dist = radius * 3.0;

    std::cout << "Center: (" << center.x << ", " << center.y << ", "
              << center.z << ")  radius=" << radius
              << "  lidar_dist=" << dist << "\n\n";

    std::vector<Vec3> positions = {
        center + Vec3{ dist,    0,    0},
        center + Vec3{-dist,    0,    0},
        center + Vec3{   0,  dist,    0},
        center + Vec3{   0, -dist,    0},
        center + Vec3{   0,    0,  dist},
        center + Vec3{   0,    0, -dist},
    };

    const int    H_BEAMS     = 512;
    const int    V_CHANNELS  = 64;
    const double H_FOV       = 360.0;
    const double V_FOV       = 45.0;
    const double MIN_RANGE   = 0.01;
    const double MAX_RANGE   = dist * 2.5;
    const double RANGE_NOISE = 0.02;
    const double ANGLE_NOISE = 0.001;
    const double DROPOUT     = 0.002;

    PointCloud combined;
    double t0 = omp_get_wtime();

    for (size_t i = 0; i < positions.size(); ++i) {
        std::cout << "Scanning position " << (i+1)
                  << "/" << positions.size() << " ...\n";

        LidarConfig cfg = LidarConfig::custom(
            positions[i], center,
            H_BEAMS, V_CHANNELS, H_FOV, V_FOV,
            MIN_RANGE, MAX_RANGE
        );
        cfg.range_noise_stddev  = RANGE_NOISE;
        cfg.angle_noise_stddev  = ANGLE_NOISE;
        cfg.dropout_probability = DROPOUT;

        PointCloud cloud = scan(scene, cfg, (int)i);
        std::cout << "  -> " << cloud.size() << " points\n";
        combined.append(cloud);
    }

    double t1 = omp_get_wtime();
    std::cout << "\nTotal: " << combined.size() << " points"
              << "  Time: " << (t1 - t0) << " s"
              << "  Threads: " << omp_get_max_threads() << "\n\n";

    combined.saveToCSV(csv_path);
    return 0;
}
