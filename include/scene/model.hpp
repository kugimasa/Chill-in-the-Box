#pragma once

#include "device.hpp"
#include "utils/texture_util.h"

namespace tinygltf {
    class Model;
}

class Actor;

class Model
{
public:
    Model();
    Model(const std::wstring& name, std::unique_ptr<Device>& device);
    ~Model();

    std::shared_ptr<Actor> InstantiateActor(std::unique_ptr<Device>& device);
    void Destroy(std::unique_ptr<Device>& device);

    std::wstring GetName() const { return m_name; }

    // ノード情報
    class Node
    {
    public:
        Node();
        ~Node();
    private:
        std::wstring m_name;
        Vector m_trans;
        Vector m_rot;
        Vector m_scale;
        Matrix m_localMtx;
        Matrix m_worldMtx;
        Node* m_parent;
        std::vector<int> m_childIndices;
        int m_meshIndex = -1;
        friend class Model;
    };

    // ポリゴン情報
    class Primitive
    {
    private:
        UINT m_indexStart;
        UINT m_vertexStart;
        UINT m_indexCount;
        UINT m_vertexCount;
        UINT m_materialIndex;
        friend class Model;
    };

    // ポリゴンメッシュのグループ
    class Mesh
    {
    private:
        std::vector<Primitive> m_primitives;
        int m_nodeIndex;
        friend class Model;
    };

    // マテリアル情報
    class Material
    {
    public:
        Material() : m_name(), m_textureIndex(-1), m_diffuseColor(1, 1, 1) {}
        std::wstring GetName() const { return m_name; }
        int GetTextureIndex() const { return m_textureIndex; }
        Float3 GetDiffuseColor() const { return m_diffuseColor; }
    private:
        std::wstring m_name;
        int m_textureIndex;
        // TODO: BRDF用の情報をセット
        Float3 m_diffuseColor;
        friend class Model;
    };

    ComPtr<ID3D12Resource> GetPositionBuffer() const { return m_vertexAtrrib.position; }
    ComPtr<ID3D12Resource> GetNormalBuffer() const { return m_vertexAtrrib.normal; }
    ComPtr<ID3D12Resource> GetIndexBuffer() const { return m_pIndexBuffer; }

private:
    struct VertexAttributeVisitor
    {
        std::vector<UINT> indexBuffer;
        std::vector<Float3> positionBuffer;
        std::vector<Float3> normalBuffer;
        std::vector<Float2> texcoordBuffer;
    };
    bool LoadModel(const tinygltf::Model& srcModel, std::unique_ptr<Device>& device);
    void LoadNode(const tinygltf::Model& srcModel);
    void LoadMesh(const tinygltf::Model& srcModel, VertexAttributeVisitor& visitor);
    void LoadMaterial(const tinygltf::Model& srcModel);

    struct VertexAttrib
    {
        ComPtr<ID3D12Resource> position;
        ComPtr<ID3D12Resource> normal;
        ComPtr<ID3D12Resource> texcoord;
    };
    std::wstring m_name;
    VertexAttrib m_vertexAtrrib;
    ComPtr<ID3D12Resource> m_pIndexBuffer;
    std::vector<TextureResource> m_textures;
    std::vector<Mesh> m_meshes;
    std::vector<Material> m_materials;
    std::vector<std::shared_ptr<Node>> m_nodes;
    std::vector<int> m_rootNodeIndices;
    TextureResource m_dummyTexture;

    friend class Actor;
};