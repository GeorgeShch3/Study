#pragma once
#include <cmath>

struct Vec3 {
    double x = 0, y = 0, z = 0;

    Vec3() = default;
    Vec3(double x, double y, double z) : x(x), y(y), z(z) {}

    Vec3  operator+ (const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vec3  operator- (const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    Vec3  operator* (double s)      const { return {x*s,   y*s,   z*s  }; }
    Vec3  operator/ (double s)      const { return {x/s,   y/s,   z/s  }; }
    Vec3& operator+=(const Vec3& o)       { x+=o.x; y+=o.y; z+=o.z; return *this; }

    double dot  (const Vec3& o) const { return x*o.x + y*o.y + z*o.z; }
    Vec3   cross(const Vec3& o) const {
        return {y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x};
    }

    double lengthSq()  const { return x*x + y*y + z*z; }
    double length()    const { return std::sqrt(lengthSq()); }
    Vec3   normalized() const {
        double l = length();
        return l > 1e-12 ? (*this) / l : *this;
    }
};

inline Vec3 operator*(double s, const Vec3& v) { return v * s; }

class CoordFrame {
public:
    Vec3 right, up, forward;

    explicit CoordFrame(const Vec3& fwd, const Vec3& hint_up = {0, 0, 1}) {
        forward = fwd.normalized();
        Vec3 r  = forward.cross(hint_up);
        if (r.lengthSq() < 1e-12)
            r = forward.cross(Vec3{1, 0, 0});
        right = r.normalized();
        up    = right.cross(forward).normalized();
    }

    Vec3 localToWorld(const Vec3& l) const {
        return right * l.x + up * l.y + forward * l.z;
    }
};
