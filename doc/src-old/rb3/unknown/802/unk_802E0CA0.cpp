#include "math/Mtx.h"
#include "math/Trig.h"

#include "common.hpp"
#include "hmx/quat.hpp"
#include "world/vector3.hpp"
#include "vector_ops.hpp"

Vector3 *Vector3::operator+=(const Vector3 &v) {
    x += v.x;
    y += v.y;
    z += v.z;
}

Vector3 *Vector3::operator-=(const Vector3 &v) {
    x -= v.x;
    y -= v.y;
    z -= v.z;
}

// fn_802E2D8C
void Normalize(const Hmx::Matrix3 &src, Hmx::Matrix3 &dst) {
    Normalize(src.row2, dst.row2);
    Cross(dst.row2, src.row3, dst.row1);
    Normalize(dst.row1, dst.row1);
    Cross(dst.row1, dst.row2, dst.row3);
}


Hmx::Quat::Quat(const Vector3 &vec) {
    Set(vec);
}


extern "C" void *memset(void *, int, unsigned int);

// fn_802E1A90
void *MemSetZero(void *v, unsigned int ui) {
    return memset(v, 0, ui);
}
