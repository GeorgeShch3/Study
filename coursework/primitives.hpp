#pragma once
#include "geometry.hpp"
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

inline Mesh makeRoom(double w, double d, double h) {
    Vec3 v[8] = {
        {0, 0, 0}, {w, 0, 0}, {w, d, 0}, {0, d, 0},
        {0, 0, h}, {w, 0, h}, {w, d, h}, {0, d, h},
    };
    Mesh m;
    m.addQuad(v[0], v[3], v[2], v[1]); // пол
    m.addQuad(v[4], v[5], v[6], v[7]); // потолок
    m.addQuad(v[0], v[1], v[5], v[4]); // стена Y=0
    m.addQuad(v[3], v[7], v[6], v[2]); // стена Y=D
    m.addQuad(v[0], v[4], v[7], v[3]); // стена X=0
    m.addQuad(v[1], v[2], v[6], v[5]); // стена X=W
    return m;
}

inline Mesh makeBox(double ox, double oy, double oz, double w,  double d,  double h) {
    Vec3 v[8] = {
        {ox,   oy,   oz  }, {ox+w, oy,   oz  },
        {ox+w, oy+d, oz  }, {ox,   oy+d, oz  },
        {ox,   oy,   oz+h}, {ox+w, oy,   oz+h},
        {ox+w, oy+d, oz+h}, {ox,   oy+d, oz+h},
    };
    Mesh m;
    m.addQuad(v[3], v[2], v[1], v[0]); 
    m.addQuad(v[4], v[5], v[6], v[7]); 
    m.addQuad(v[0], v[1], v[5], v[4]);
    m.addQuad(v[2], v[3], v[7], v[6]);
    m.addQuad(v[3], v[0], v[4], v[7]);
    m.addQuad(v[1], v[2], v[6], v[5]);
    return m;
}

class PrimitiveFactory {
public:
    static Mesh createBox(const Vec3& center, double width, double height, double depth) {
        double hw = width  / 2, hh = height / 2, hd = depth / 2;
        double cx = center.x,   cy = center.y,   cz = center.z;
        Vec3 v[8] = {
            {cx-hw, cy-hh, cz-hd}, {cx+hw, cy-hh, cz-hd},
            {cx+hw, cy+hh, cz-hd}, {cx-hw, cy+hh, cz-hd},
            {cx-hw, cy-hh, cz+hd}, {cx+hw, cy-hh, cz+hd},
            {cx+hw, cy+hh, cz+hd}, {cx-hw, cy+hh, cz+hd},
        };
        Mesh m;
        m.addQuad(v[0], v[1], v[2], v[3]);
        m.addQuad(v[7], v[6], v[5], v[4]);
        m.addQuad(v[0], v[4], v[7], v[3]);
        m.addQuad(v[1], v[2], v[6], v[5]);
        m.addQuad(v[0], v[1], v[5], v[4]);
        m.addQuad(v[3], v[7], v[6], v[2]);
        return m;
    }

    static Mesh createSphere(const Vec3& center, double radius, int rings = 16, int sectors = 32) {
        std::vector<Vec3> verts;
        verts.reserve((rings + 1) * (sectors + 1));
        for (int r = 0; r <= rings; ++r) {
            double phi = M_PI * r / rings;
            for (int s = 0; s <= sectors; ++s) {
                double theta = 2 * M_PI * s / sectors;
                verts.push_back({
                    center.x + radius * std::sin(phi) * std::cos(theta),
                    center.y + radius * std::sin(phi) * std::sin(theta),
                    center.z + radius * std::cos(phi),
                });
            }
        }
        Mesh m;
        int cols = sectors + 1;
        for (int r = 0; r < rings; ++r)
            for (int s = 0; s < sectors; ++s) {
                const Vec3& a = verts[ r      * cols + s    ];
                const Vec3& b = verts[ r      * cols + s + 1];
                const Vec3& c = verts[(r + 1) * cols + s    ];
                const Vec3& d = verts[(r + 1) * cols + s + 1];
                m.addTriangle(a, b, d);
                m.addTriangle(a, d, c);
            }
        return m;
    }
};
