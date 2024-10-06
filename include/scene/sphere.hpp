#pragma once

#include "device.hpp"

class Sphere
{
public:
    Sphere(std::unique_ptr<Device>& device, float raduis, int slices, int stacks);
    ~Sphere();

private:
    struct Vertex
    {
        Float3 pos;
        Float3 norm;
    };
    std::vector<Vertex> m_vertices;
    std::vector<UINT> m_indices;
};

class SphereInstance
{
private:
    ComPtr<ID3D12Resource> m_vertexBuffer;
    ComPtr<ID3D12Resource> m_indexBuffer;
    ComPtr<ID3D12Resource> m_blas;
    std::wstring shaderName;
};
