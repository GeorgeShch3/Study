#pragma once
#include "vec3.hpp"
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <iostream>

struct LidarPoint {
    Vec3   pos;
    double dist   = 0;
    int    id     = -1;
    int    beam_h = -1;
    int    beam_v = -1;
};

class PointCloud {
public:
    std::vector<LidarPoint> points;

    void add(const Vec3& p, double d, int id = -1, int beam_h = -1, int beam_v = -1) {
        points.push_back({p, d, id, beam_h, beam_v});
    }

    void append(const PointCloud& o) {
        points.insert(points.end(), o.points.begin(), o.points.end());
    }

    void   clear() { points.clear(); }
    size_t size()  const { return points.size(); }

    void saveToCSV(const std::string& filename, const std::string& id_col_name = "id") const {
        std::ofstream f(filename);
        if (!f) throw std::runtime_error("Cannot write: " + filename);

        bool has_id = false;
        for (const auto& p : points)
            if (p.id != -1) { has_id = true; break; }

        f << "x,y,z,distance";
        if (has_id) f << ',' << id_col_name;
        f << '\n';

        for (const auto& p : points) {
            f << p.pos.x << ',' << p.pos.y << ','
              << p.pos.z << ',' << p.dist;
            if (has_id) f << ',' << p.id;
            f << '\n';
        }

        std::cout << "  -> " << filename
                  << " (" << points.size() << " точек)\n";
    }
};
