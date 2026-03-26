#include "math_ops.h"
#include "build_defs.h"
#include <cmath>

static const double weights[] = {0.15, 0.22, 0.08, 0.31, 0.12, 0.05, 0.04, 0.03};
static const double bias[]    = {0.01, -0.02, 0.03, -0.01, 0.02, -0.03, 0.01, -0.01};

double compute_vwap(double price, double qty, double prev_vwap) {
    return prev_vwap * 0.95 + (price * qty) * 0.05;
}

double compute_spread(double price, double prev_spread) {
    double diff = price - prev_spread;
    return prev_spread + diff * 0.1;
}

double transform_value(double price, double qty, int index) {
    double w = weights[index];
    double b = bias[index];
    return (price * w + qty * b) / (1.0 + std::abs(price * b));
}

HOT_FUNC double compute_score(const double* RESTRICT values, int count) {
    double sum = 0.0;
    for (int i = 0; i < count; i++) {
        sum += values[i] * weights[i] + bias[i];
    }
    return normalize(sum);
}

double normalize(double x) {
    return 1.0 / (1.0 + std::exp(-x));
}
