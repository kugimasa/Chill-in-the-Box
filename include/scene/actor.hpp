#pragma once

#include "device.hpp"
#include "scene/model.hpp"
#include "utils/texture_util.h"

class Actor
{
public:
    Actor() = delete;
    ~Actor();

    class ActorNode;
    class ActorMaterial;

    // ノード情報
    class ActorNode
    {
    public:
        ActorNode();
        ~ActorNode();

        void UpdateLocalMatrix();
        void UpdateWorldMatrix(Matrix parentMtx);
        void UpdateMatrix(Matrix parentMtx);

        void SetTranslation(Vector trans) { m_trans = trans; }
        void SetRotation(Vector rot) { m_rot = rot; }
        void SetScale(Vector scale) { m_scale = scale; }

        const std::wstring GetName() const { return m_name; }
        Matrix GetLocalMatrix() { return m_localMtx; }
        Matrix GetWorldMatrix() { return m_worldMtx; }
        auto GetParent() { return m_parent; }
    private:
        std::wstring m_name;
        Vector m_trans;
        Vector m_rot;
        Vector m_scale;
        Matrix m_localMtx;
        Matrix m_worldMtx;
        std::weak_ptr<ActorNode> m_parent;
        std::vector<std::shared_ptr<ActorNode>> m_children;
        std::vector<UINT> m_meshes;
        int m_meshGroupIndex = -1;
        
        friend class Model;
        friend class Actor;
    };

    // マテリアル情報
    class ActorMaterial
    {
    public:
        ActorMaterial(std::unique_ptr<Device>& device, const Model::Material& srcMat);
        ~ActorMaterial();

        void SetHitGroupStr(const std::wstring& hitGroupStr) { m_hitGroupStr = hitGroupStr; }
        void SetTexture(TextureResource& texRes);

        std::wstring GetName() const { return m_name; }
        std::wstring GetHitGroupStr() const { return m_hitGroupStr; }
        DescriptorHeap GetTextureDescriptor() const { return m_texture.srv; }
        DescriptorHeap GetMaterialDescriptor() const { return m_cbv[m_pDevice->GetCurrentFrameIndex()]; }
    private:
        std::wstring m_name;
        std::wstring m_hitGroupStr;
        std::vector<ComPtr<ID3D12Resource>> m_pMatrialCB;
        std::vector<DescriptorHeap> m_cbv;
        TextureResource m_texture;
        struct MaterialParam
        {
            Float4 diffuse;
        };
        MaterialParam m_materialParam;

        std::unique_ptr<Device>& m_pDevice;
        friend class Model;
    };

    // メッシュ情報
    class ActorMesh
    {
    public:
        UINT GetIndexStart() const { return m_indexStart; }
        UINT GetIndexCount() const { return m_indexCount; }
        UINT GetVertexStart() const { return m_vertexStart; }
        UINT GetVertexCount() const { return m_vertexCount; }
        DescriptorHeap GetPosition() const { return m_vbAttrPos; }
        DescriptorHeap GetNormal() const { return m_vbAttrNorm; }
        DescriptorHeap GetTexcoord() const { return m_vbAttrTexCoord; }
        DescriptorHeap GetIndexBuffer() const { return m_indexBuffer; }
        std::shared_ptr<ActorMaterial> GetMaterial() const { return m_material; }
        ComPtr<ID3D12Resource> GetMeshParamCB() const { return m_pMeshParamCB; }
    private:
        UINT m_indexStart;
        UINT m_indexCount;
        UINT m_vertexStart;
        UINT m_vertexCount;
        DescriptorHeap m_vbAttrPos;
        DescriptorHeap m_vbAttrNorm;
        DescriptorHeap m_vbAttrTexCoord;
        DescriptorHeap m_indexBuffer;
        std::shared_ptr<ActorMaterial> m_material;
        struct MeshParam
        {
            Float4 diffuse;
            UINT matrixBuffStride;
            UINT meshGroupIndex;
        };
        ComPtr<ID3D12Resource> m_pMeshParamCB;

        friend class Model;
    };

    // メッシュグループ情報
    class ActorMeshGroup
    {
    public:
        const std::shared_ptr<ActorNode> GetNode() const { return m_node; }
        UINT GetMeshCount() const { return UINT(m_meshes.size()); }
        const ActorMesh& GetMesh(int index) const { return m_meshes[index]; }
    private:
        std::shared_ptr<ActorNode> m_node;
        std::vector<ActorMesh> m_meshes;
        friend class Model;
        friend class Actor;
    };
    void Rotate(float deltaTime, float speed, Float3 up);

    void MoveAnimInCubic(float currentTime, float startTime, float endTime, Float3 startPos, Float3 endPos);

    void SetRotaion(float degree, Float3 up);
    void SetWorldPos(Float3 worldPos);
    void SetWorldMatrix(Matrix worldMtx) { m_worldMtx = worldMtx; }
    void SetMaterialHitGroup(const std::wstring& hitGroupName);
    // 各ノードの行列を更新
    void UpdateMatrices();
    void UpdateBLAS(ComPtr<ID3D12GraphicsCommandList4> cmdList);
    void UpdateTransform();
    uint8_t* WriteHitGroupShaderRecord(uint8_t* dst, UINT hitGroupRecordSize, ComPtr<ID3D12StateObjectProperties> rtStateObjectProps);

    Matrix GetWorldMatrix()const { return m_worldMtx; }
    Float3 GetWorldPos() const { return m_worldPos; }
    const Model* GetModel() { return m_modelRef; }
    UINT GetMeshGroupCount() const; 
    UINT GetMeshCount(int groupIndex) const;
    const ActorMesh& GetMesh(int groupIndex, int meshIndex) const;
    UINT GetTotalMeshCount() const;
    UINT GetMaterialCount() const;
    std::shared_ptr<ActorMaterial> GetMaterial(UINT idx) const;
    ComPtr<ID3D12Resource> GetBLAS() const { return m_pBLAS; }
    ComPtr<ID3D12Resource> GetBLASMatrices() const { return m_pBLASMatrices; }
    DescriptorHeap GetBLASMatrixDescriptor() { return m_blasMatrixDescriptor; }


private:
    Actor(std::unique_ptr<Device>& device, const Model* model);
    void CreateMatrixBufferBLAS(UINT mtxCount);
    void CreateBLAS();
    void CreateRTGeoDesc(std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>& rtGeoDesc);
    
    Float3 m_worldPos;
    Matrix m_worldMtx;
    const Model* m_modelRef;
    std::vector<std::shared_ptr<ActorNode>> m_nodes;
    std::vector<std::shared_ptr<ActorMaterial>> m_materials;
    std::vector<ActorMeshGroup> m_meshGroups;

    ComPtr<ID3D12Resource> m_pBLAS;
    ComPtr<ID3D12Resource> m_pBLASUpdateBuffer;
    ComPtr<ID3D12Resource> m_pBLASMatrices;

    DescriptorHeap m_blasMatrixDescriptor;
    std::unique_ptr<Device>& m_pDevice;
    friend class Model;
};