#pragma once
#include "vec3.hpp"
#include <vector>
#include <algorithm>
#include <limits>

struct Triangle {
    Vec3 v0, v1, v2;

    Triangle(const Vec3& a, const Vec3& b, const Vec3& c) : v0(a), v1(b), v2(c) {}

    bool intersect(const Vec3& orig, const Vec3& dir, double& t) const {
        constexpr double EPS = 1e-9;
        Vec3   e1 = v1 - v0, e2 = v2 - v0;
        Vec3   h  = dir.cross(e2);
        double a  = e1.dot(h);
        if (std::abs(a) < EPS) return false;
        double f = 1.0 / a;
        Vec3   s = orig - v0;
        double u = f * s.dot(h);
        if (u < 0.0 || u > 1.0) return false;
        Vec3   q = s.cross(e1);
        double v = f * dir.dot(q);
        if (v < 0.0 || u + v > 1.0) return false;
        t = f * e2.dot(q);
        return t > EPS;
    }
};

class Mesh {
public:
    std::vector<Triangle> triangles;

    void addTriangle(const Vec3& a, const Vec3& b, const Vec3& c) {
        triangles.emplace_back(a, b, c);
    }

    void addQuad(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3) {
        addTriangle(p0, p1, p2);
        addTriangle(p0, p2, p3);
    }

    bool intersect(const Vec3& orig, const Vec3& dir,
                   double& out_t, double max_t) const {
        bool hit = false;
        for (const auto& tri : triangles) {
            double t;
            if (tri.intersect(orig, dir, t) && t < max_t) {
                max_t = t; out_t = t; hit = true;
            }
        }
        return hit;
    }

    void bounds(Vec3& mn, Vec3& mx) const {
        constexpr double INF = 1e18;
        mn = { INF,  INF,  INF};
        mx = {-INF, -INF, -INF};
        for (const auto& tri : triangles) {
            for (const auto& v : {tri.v0, tri.v1, tri.v2}) {
                mn.x = std::min(mn.x, v.x); mx.x = std::max(mx.x, v.x);
                mn.y = std::min(mn.y, v.y); mx.y = std::max(mx.y, v.y);
                mn.z = std::min(mn.z, v.z); mx.z = std::max(mx.z, v.z);
            }
        }
    }

    size_t triangleCount() const { return triangles.size(); }
};

class Scene {
public:
    void addMesh(Mesh m) { meshes_.push_back(std::move(m)); }

    void add(const Mesh& m) { meshes_.push_back(m); }

    bool intersect(const Vec3& orig, const Vec3& dir, double& out_t, double max_t) const {
        bool hit = false;
        for (const auto& mesh : meshes_) {
            double t;
            if (mesh.intersect(orig, dir, t, max_t)) {
                max_t = t; out_t = t; hit = true;
            }
        }
        return hit;
    }

    size_t triangleCount() const {
        size_t n = 0;
        for (const auto& m : meshes_) n += m.triangleCount();
        return n;
    }

    bool empty() const { return meshes_.empty(); }

private:
    std::vector<Mesh> meshes_;
};
