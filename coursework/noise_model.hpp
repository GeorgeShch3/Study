#pragma once
#include "vec3.hpp"
#include <random>
#include <algorithm>

class NoiseModel {
public:
    explicit NoiseModel(double range_std   = 0,
                        double angle_std   = 0,
                        double dropout_prob = 0,
                        unsigned seed      = 0)
        : rs_(range_std), as_(angle_std), dp_(dropout_prob)
        , gen_(seed == 0 ? std::random_device{}() : seed)
        , uni_(0.0, 1.0)
        , rdist_(0.0, std::max(range_std,   1e-12))
        , adist_(0.0, std::max(angle_std,   1e-12))
    {}

    bool drop() {
        return dp_ > 0 && uni_(gen_) < dp_;
    }

    Vec3 angNoise(const Vec3& d) {
        if (as_ <= 0) return d;
        return (d + Vec3{adist_(gen_), adist_(gen_), adist_(gen_)}).normalized();
    }

    double rngNoise(double t, double mn, double mx) {
        if (rs_ <= 0) return t;
        return std::clamp(t + rdist_(gen_), mn, mx);
    }

    double rngNoiseBiased(double t, double mn, double mx, double bias) {
        double val = t + bias;
        if (rs_ > 0) val += rdist_(gen_);
        return std::clamp(val, mn, mx);
    }

    void setRangeStd(double s) {
        rs_ = s;
        rdist_ = std::normal_distribution<double>(0, std::max(s, 1e-12));
    }
    void setAngleStd(double s) {
        as_ = s;
        adist_ = std::normal_distribution<double>(0, std::max(s, 1e-12));
    }
    void setDropout(double p) { dp_ = p; }

private:
    double rs_, as_, dp_;
    std::mt19937 gen_;
    std::uniform_real_distribution<double>  uni_;
    std::normal_distribution<double>        rdist_, adist_;
};
