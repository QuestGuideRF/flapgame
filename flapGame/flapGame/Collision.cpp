#include <flapGame/Core.h>
#include <flapGame/Collision.h>
namespace flap {
SphCylCollResult::Type sphereCylinderCollisionTest(const Float3& spherePos, float sphereRadius,
                                                   float cylRadius, SphCylCollResult* result) {
    float v = spherePos.z;
    if (v >= sphereRadius) {
        return SphCylCollResult::None;
    }
    Float3 deltaFromAxis = {spherePos.x, spherePos.y, 0};
    float u2 = deltaFromAxis.length2();
    float minU = cylRadius + sphereRadius;
    if (u2 >= minU * minU) {
        return SphCylCollResult::None;
    }
    float u = sqrtf(u2);
    Float3 unitPerp;
    if (u >= 0.01f) {
        unitPerp = deltaFromAxis / u;
    } else {
        unitPerp = {1, 0, 0};
    }
    if (u <= cylRadius + 0.01f) {
        if (v <= 0.01f) {
            result->pos = spherePos;
            if (cylRadius - u < -v) {
                result->norm = unitPerp;
                result->penetrationDepth = sphereRadius + (cylRadius - u);
            } else {
                result->norm = {0, 0, 1};
                result->penetrationDepth = sphereRadius + (-v);
            }
            return SphCylCollResult::Inside;
        } else {
            result->pos = deltaFromAxis;
            result->norm = {0, 0, 1};
            result->penetrationDepth = sphereRadius + (-v);
            return SphCylCollResult::CapPlane;
        }
    } else {
        if (v <= 0.01f) {
            result->pos = unitPerp * cylRadius + Float3{0, 0, v};
            result->norm = unitPerp;
            result->penetrationDepth = sphereRadius + (cylRadius - u);
            return SphCylCollResult::Side;
        } else {
            Float2 posRelEdge = Float2{u - cylRadius, v};
            PLY_ASSERT(posRelEdge.x > 0);
            PLY_ASSERT(posRelEdge.y > 0);
            float L2 = posRelEdge.length2();
            PLY_ASSERT(L2 >= 0.00005f);
            if (L2 >= sphereRadius * sphereRadius) {
                return SphCylCollResult::None;
            }
            float L = sqrtf(L2);
            posRelEdge /= L;
            result->pos = unitPerp * cylRadius;
            result->norm = unitPerp * posRelEdge.x + Float3{0, 0, posRelEdge.y};
            result->penetrationDepth = sphereRadius - L;
            return SphCylCollResult::CapEdge;
        }
    }
}
SphCylCollResult::Type sphereCylinderCollisionTest(const Float3& spherePos, float sphereRadius,
                                                   const Float3x4& cylToWorld, float cylRadius,
                                                   SphCylCollResult* result) {
    Float3x4 worldToCyl = cylToWorld.invertedOrtho();
    SphCylCollResult::Type type =
        sphereCylinderCollisionTest(worldToCyl * spherePos, sphereRadius, cylRadius, result);
    if (type != SphCylCollResult::None) {
        result->pos = cylToWorld * result->pos;
        result->norm = cylToWorld.asFloat3x3() * result->norm;
    }
    return type;
}
}