#include "sphere.hpp"

Sphere::Sphere(std::unique_ptr<Device>& device, float radius, int slices, int stacks)
{
    m_vertices.clear();
    m_indices.clear();
    // 頂点データ
    for (int stack = 0; stack <= stacks; ++stack)
    {
        for (int slice = 0; slice <= slices; ++slice)
        {
            Float3 pos;
            pos.y = 2.0f * stack / float(stacks) - 1.0f;
            float r = std::sqrtf(1 - pos.y * pos.y);
            float theta = 2.0f * XM_PI * slice / float(slices);
            pos.x = r * std::sinf(theta);
            pos.z = r * std::cosf(theta);

            Vector vec = XMLoadFloat3(&pos) * radius;
            Vector norm = XMVector3Normalize(vec);

            Vertex vtx{};
            vtx.pos = vec;
            vtx.norm = norm;
            m_vertices.push_back(vtx);
        }
    }
    // インデックスデータ
    for (int stack = 0; stack < stacks; ++stack)
    {
        const int sliceMax = slices + 1;
        for (int slice = 0; slice < slices; ++slice)
        {
            int idx = stack * sliceMax;
            int i0 = idx + (slice + 0) % sliceMax;
            int i1 = idx + (slice + 1) % sliceMax;
            int i2 = i0 + sliceMax;
            int i3 = i1 + sliceMax;
            m_indices.push_back(i0);
            m_indices.push_back(i1);
            m_indices.push_back(i2);
            m_indices.push_back(i2);
            m_indices.push_back(i1);
            m_indices.push_back(i3);
        }
    }
    UINT vertexStride = sizeof(Vertex);
    UINT indexStride = sizeof(UINT);
    UINT vertexSize = m_vertices.size() * vertexSize;
    UINT indexSize = m_indices.size() * indexStride;

    // バッファの確保
    auto flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    auto heapType = D3D12_HEAP_TYPE_DEFAULT;
    m_vertexBuffer = device->InitializeBuffer(vertexSize, m_vertices.data(), flags, heapType, L"Sphere:VertexBuffer");
    m_indexBuffer = device->InitializeBuffer(indexSize, m_indices.data(), flags, heapType, L"Sphere:IndexBuffer");
}

Sphere::~Sphere()
{
    m_vertices.clear();
    m_indices.clear();
}
