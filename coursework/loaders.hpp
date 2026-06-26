#pragma once
#include "geometry.hpp"
#include "vec3.hpp"
#include <string>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <cmath>
#include <cstdint>
#include <iostream>

class IMeshLoader {
public:
    virtual ~IMeshLoader() = default;
    virtual Mesh load(const std::string& path) = 0;
};

class ObjLoader : public IMeshLoader {
public:
    Mesh load(const std::string& path) override {
#ifdef TINYOBJLOADER_IMPLEMENTATION
        tinyobj::attrib_t                attrib;
        std::vector<tinyobj::shape_t>    shapes;
        std::vector<tinyobj::material_t> mats;
        std::string warn;

        bool ok = tinyobj::LoadObj(&attrib, &shapes, &mats, &warn, path.c_str());
        if (!warn.empty()) std::cerr << "OBJ: " << warn << '\n';
        if (!ok)           throw std::runtime_error("OBJ load failed: " + warn);

        Mesh mesh;
        int skipped = 0;
        for (const auto& shape : shapes) {
            size_t off = 0;
            for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
                size_t fv = shape.mesh.num_face_vertices[f];
                std::vector<Vec3> face;
                for (size_t v = 0; v < fv; ++v) {
                    auto idx = shape.mesh.indices[off + v];
                    if (idx.vertex_index < 0) { ++skipped; continue; }
                    face.push_back({
                        attrib.vertices[3 * idx.vertex_index + 0],
                        attrib.vertices[3 * idx.vertex_index + 1],
                        attrib.vertices[3 * idx.vertex_index + 2],
                    });
                }
                for (size_t v = 1; v + 1 < face.size(); ++v)
                    mesh.addTriangle(face[0], face[v], face[v + 1]);
                off += fv;
            }
        }
        if (skipped > 0)
            std::cerr << "Skipped " << skipped << " invalid vertex indices.\n";
        return mesh;
#else
        throw std::runtime_error(
            "OBJ требует tinyobjloader: "
            "https://github.com/tinyobjloader/tinyobjloader\n"
            "Скачай tiny_obj_loader.h и раскомментируй "
            "#define TINYOBJLOADER_IMPLEMENTATION в loaders.hpp");
#endif
    }
};

class StlLoader : public IMeshLoader {
public:
    Mesh load(const std::string& path) override {
        return isBinary(path) ? loadBinary(path) : loadAscii(path);
    }

private:
    static bool isBinary(const std::string& p) {
        std::ifstream f(p, std::ios::binary);
        char hdr[80]; f.read(hdr, 80);
        uint32_t n; f.read(reinterpret_cast<char*>(&n), 4);
        f.seekg(0, std::ios::end);
        return f.tellg() == 80 + 4 + (std::streampos)n * 50;
    }

    static Mesh loadBinary(const std::string& p) {
        std::ifstream f(p, std::ios::binary);
        char hdr[80]; f.read(hdr, 80);
        uint32_t n; f.read(reinterpret_cast<char*>(&n), 4);
        Mesh m;
        for (uint32_t i = 0; i < n; ++i) {
            float nr[3], a[3], b[3], c[3]; uint16_t attr;
            f.read(reinterpret_cast<char*>(nr), 12);
            f.read(reinterpret_cast<char*>(a),  12);
            f.read(reinterpret_cast<char*>(b),  12);
            f.read(reinterpret_cast<char*>(c),  12);
            f.read(reinterpret_cast<char*>(&attr), 2);
            m.addTriangle({a[0],a[1],a[2]},
                          {b[0],b[1],b[2]},
                          {c[0],c[1],c[2]});
        }
        return m;
    }

    static Mesh loadAscii(const std::string& p) {
        std::ifstream f(p);
        std::string tok;
        Vec3 vs[3]; int vi = 0;
        Mesh m;
        while (f >> tok) {
            if (tok == "vertex") {
                double x, y, z; f >> x >> y >> z;
                vs[vi++] = {x, y, z};
                if (vi == 3) { m.addTriangle(vs[0], vs[1], vs[2]); vi = 0; }
            }
        }
        return m;
    }
};

class MeshLoader {
public:
    static Mesh load(const std::string& path) {
        auto pos = path.rfind('.');
        if (pos == std::string::npos)
            throw std::runtime_error("Нет расширения файла: " + path);

        std::string ext = path.substr(pos + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == "obj") return ObjLoader().load(path);
        if (ext == "stl") return StlLoader().load(path);
        throw std::runtime_error("Неподдерживаемый формат: " + ext);
    }
};

inline void modelBounds(const Mesh& mesh, Vec3& mn, Vec3& mx) {
    mesh.bounds(mn, mx);
}

inline void modelBoundingSphere(const Mesh& mesh, Vec3& center, double& radius) {
    Vec3 mn, mx;
    mesh.bounds(mn, mx);
    center = (mn + mx) * 0.5;
    Vec3 half = (mx - mn) * 0.5;
    radius = std::sqrt(half.x*half.x + half.y*half.y + half.z*half.z);
}