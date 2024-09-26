#pragma once

#include <DirectXMath.h>
#include <cmath>

using namespace DirectX;
using Float2 = DirectX::XMFLOAT2;
using Float3 = DirectX::XMFLOAT3;
using Float4 = DirectX::XMFLOAT4;
using Matrix = DirectX::XMMATRIX;
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

static Matrix IdentityMat()
{
    return XMMatrixIdentity();
}

#ifndef ROUND_UP
#define ROUND_UP(size, align) (((size) + (align) - 1) & ~((align) - 1))
#endif