#pragma once

#include <DirectXMath.h>
#include <cmath>

using namespace DirectX;
using Float3 = DirectX::XMFLOAT3;
using Float4 = DirectX::XMFLOAT4;
using Matrix = DirectX::XMMATRIX;
using Vector = DirectX::XMVECTOR;

#ifndef ROUND_UP
#define ROUND_UP(size, align) (((size) + (align) - 1) & ~((align) - 1))
#endif