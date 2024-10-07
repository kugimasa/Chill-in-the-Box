#pragma once

#include <DirectXMath.h>
#include <cmath>

using namespace DirectX;
using Float2 = DirectX::XMFLOAT2;
using Float3 = DirectX::XMFLOAT3;
using Float4 = DirectX::XMFLOAT4;
using Matrix = DirectX::XMMATRIX;
using Mtx3x4 = DirectX::XMFLOAT3X4;
using Mtx4x3 = DirectX::XMFLOAT4X3;
using Mtx4x4 = DirectX::XMFLOAT4X4;
using Vector = DirectX::XMVECTOR;

static Vector ZeroVector()
{
    return XMVectorZero();
}

static Vector IdentityQuat()
{
    return XMQuaternionIdentity();
}

static Vector Vector4(float x, float y, float z, float w)
{
    return XMVectorSet(x, y, z, w);
}

static Vector DoubleToVector3(const double* v)
{
    Float3 vec3;
    vec3.x = static_cast<float>(v[0]);
    vec3.y = static_cast<float>(v[1]);
    vec3.z = static_cast<float>(v[2]);
    return XMLoadFloat3(&vec3);
}

static Vector DoubleToVector4(const double* v)
{
    Float4 vec4;
    vec4.x = static_cast<float>(v[0]);
    vec4.y = static_cast<float>(v[1]);
    vec4.z = static_cast<float>(v[2]);
    vec4.w = static_cast<float>(v[3]);
    return XMLoadFloat4(&vec4);
}

static Matrix IdentityMtx()
{
    return XMMatrixIdentity();
}

#ifndef ROUND_UP
#define ROUND_UP(size, align) (((size) + (align) - 1) & ~((align) - 1))
#endif

// https://easings.net/#easeInCubic
inline float EaseInOutCubic(float t)
{
    return t < 0.5 ? 4.0 * t * t * t : 1.0 - pow(-2.0 * t + 2.0, 3.0) / 2.0;
}

// https://easings.net/#easeInOutCubic
inline float EaseInCubic(float t)
{
    return t * t * t;
}

//https://easings.net/#easeOutCubic
inline float EaseOutCubic(float t)
{
    return 1.0 - pow(1.0 - t, 3.0);
}

inline Float3 Lerp(Float3 a, Float3 b, float t)
{
    XMVECTOR v = XMVectorLerp(XMLoadFloat3(&a), XMLoadFloat3(&b), t);
    Float3 ret;
    XMStoreFloat3(&ret, v);
    return ret;
}

