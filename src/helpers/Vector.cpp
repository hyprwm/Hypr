#include "Vector.hpp"
#include <cmath>

Vector2D::Vector2D(double xx, double yy) {
    x = xx;
    y = yy;
}

Vector2D::Vector2D() { x = 0; y = 0; }
Vector2D::~Vector2D() {}

double Vector2D::normalize() {
    // get max abs
    const auto max = abs(x) > abs(y) ? abs(x) : abs(y);

    x /= max;
    y /= max;

    return max;
}
