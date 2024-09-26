#include "scene/model.hpp"
#include "utils/gltf_loader.h"

Model::Model()
{
}

Model::~Model()
{
}

Model::Node::Node() :
m_name(L""),
m_trans(ZeroVector()),
m_scale(Vector4(1.0f, 1.0f, 1.0f, 0.0f)),
m_rot(IdentityQuat()),
m_localMtx(IdentityMat()),
m_worldMtx(IdentityMat()),
m_parent(nullptr)
{
}

Model::Node::~Node()
{
}

Model::Model(const std::wstring& name, std::unique_ptr<Device>& device)
{
    tinygltf::Model srcModel{};
    if (!LoadGLTF(name, srcModel))
    {
        std::wstring err = L"glTFファイルの読み込みに失敗しました: " + name;
        Error(PrintInfoType::RTCAMP10, err);
    }
    if (!LoadModel(srcModel, device))
    {
        std::wstring err = L"モデルのロードに失敗しました: " + name;
        Error(PrintInfoType::RTCAMP10, err);
    }
}

std::shared_ptr<Actor> Model::Create(std::unique_ptr<Device>& device)
{
    // TODO: 実装予定
    std::shared_ptr<Actor> actor(new Actor());
    return std::shared_ptr<Actor>();
}

void Model::Destroy(std::unique_ptr<Device>& device)
{
    m_textures.clear();
    m_meshes.clear();
    m_materials.clear();
    m_nodes.clear();
}

bool Model::LoadModel(const tinygltf::Model& srcModel, std::unique_ptr<Device>& device)
{
    VertexAttributeVisitor visitor;
    const auto& scene = srcModel.scenes[0];
    for (const auto& nodeIndex : scene.nodes) {
        m_rootNodes.push_back(nodeIndex);
    }

    LoadNode(srcModel);
    LoadMesh(srcModel, visitor);
    LoadMaterial(srcModel);

    auto flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    auto heapType = D3D12_HEAP_TYPE_DEFAULT;

    // 頂点バッファの作成
    auto posSize = sizeof(Float3) * visitor.positionBuffer.size();
    if (posSize > 0)
    {
        m_vertexAtrrib.Position = device->InitializeBuffer(posSize, visitor.positionBuffer.data(), flags, heapType, L"PositionBuffer");
    }
    auto normSize = sizeof(Float3) * visitor.normalBuffer.size();
    if (normSize > 0)
    {
        m_vertexAtrrib.Normal = device->InitializeBuffer(normSize, visitor.normalBuffer.data(), flags, heapType, L"NormalBuffer");
    }
    auto texSize = sizeof(Float2) * visitor.texcoordBuffer.size();
    if (texSize > 0)
    {
        m_vertexAtrrib.Texcoord = device->InitializeBuffer(texSize, visitor.texcoordBuffer.data(), flags, heapType, L"TexcoordBuffer");
    }

    // インデックスバッファの作成
    auto idxSize = sizeof(UINT) * visitor.indexBuffer.size();
    if (idxSize > 0)
    {
        m_pIndexBuffer = device->InitializeBuffer(idxSize, visitor.indexBuffer.data(), flags, heapType, L"IndexBuffer");
    }

    // テクスチャ割り当て
    for (auto& texture : srcModel.textures)
    {
        auto image = srcModel.images[texture.source];
        auto fileName = StrToWStr(image.name + ": " + image.uri);
        TextureResource tex;
        if (image.bufferView >= 0)
        {
            auto bufferView = srcModel.bufferViews[image.bufferView];
            auto offsetBytes = bufferView.byteOffset;
            const void* imageData = &srcModel.buffers[bufferView.buffer].data[offsetBytes];
            UINT64 byteLength = bufferView.byteLength;
            tex = LoadTexture(imageData, byteLength, device);
        }
        else
        {
            // FIXME: CreateDecoderFromStream で失敗している
            tex = LoadTexture(image.image.data(), image.image.size(), device);
        }

        tex.resource->SetName(fileName.c_str());
        m_textures.emplace_back(tex);
    }
    m_dummyTexture = LoadTexture(L"dummy.png", device);

    return true;
}

void Model::LoadNode(const tinygltf::Model& srcModel)
{
    for (auto& srcNode : srcModel.nodes)
    {
        m_nodes.emplace_back(new Node());
        auto node = m_nodes.back();
        node->m_name = StrToWStr(srcNode.name);
        if (!srcNode.translation.empty())
        {
            node->m_trans = DoubleToVector3(srcNode.translation.data());
        }
        if (!srcNode.scale.empty())
        {
            node->m_scale = DoubleToVector3(srcNode.scale.data());
        }
        if (!srcNode.rotation.empty())
        {
            node->m_rot = DoubleToVector4(srcNode.rotation.data());
        }
        for (auto& child : srcNode.children)
        {
            node->m_children.push_back(child);
        }
        node->m_meshIndex = srcNode.mesh;
    }
}

void Model::LoadMesh(const tinygltf::Model& srcModel, VertexAttributeVisitor& visitor)
{
    auto& indexBuffer = visitor.indexBuffer;
    auto& positionBuffer = visitor.positionBuffer;
    auto& normalBuffer = visitor.normalBuffer;
    auto& texcoordBuffer = visitor.texcoordBuffer;

    for (auto& srcMesh : srcModel.meshes)
    {
        m_meshes.emplace_back(Mesh());
        auto& mesh = m_meshes.back();

        for (auto& srcPrimitive : srcMesh.primitives)
        {
            auto indexStart = static_cast<UINT>(indexBuffer.size());
            auto vertexStart = static_cast<UINT>(positionBuffer.size());
            UINT indexCount = 0;
            UINT vertexCount = 0;

            // 頂点
            const auto& empty = srcPrimitive.attributes.end();
            if (auto attr = srcPrimitive.attributes.find("POSITION"); attr != empty) {
                auto& acc = srcModel.accessors[attr->second];
                auto& view = srcModel.bufferViews[acc.bufferView];
                auto offsetBytes = acc.byteOffset + view.byteOffset;
                const auto* src = reinterpret_cast<const Float3*>(&(srcModel.buffers[view.buffer].data[offsetBytes]));
                vertexCount = UINT(acc.count);
                for (UINT i = 0; i < vertexCount; ++i) {
                    positionBuffer.push_back(src[i]);
                }
            }

            // 法線
            if (auto attr = srcPrimitive.attributes.find("NORMAL"); attr != empty) {
                auto& acc = srcModel.accessors[attr->second];
                auto& view = srcModel.bufferViews[acc.bufferView];
                auto offsetBytes = acc.byteOffset + view.byteOffset;
                const auto* src = reinterpret_cast<const Float3*>(&(srcModel.buffers[view.buffer].data[offsetBytes]));
                vertexCount = UINT(acc.count);
                for (UINT i = 0; i < vertexCount; ++i) {
                    normalBuffer.push_back(src[i]);
                }
            }

            // UV0
            if (auto attr = srcPrimitive.attributes.find("NORMAL"); attr != empty) {
                auto& acc = srcModel.accessors[attr->second];
                auto& view = srcModel.bufferViews[acc.bufferView];
                auto offsetBytes = acc.byteOffset + view.byteOffset;
                const auto* src = reinterpret_cast<const Float2*>(&(srcModel.buffers[view.buffer].data[offsetBytes]));
                for (UINT i = 0; i < vertexCount; ++i) {
                    texcoordBuffer.push_back(src[i]);
                }
            }
            else
            {
                // UVがない場合のゼロ埋め
                for (UINT i = 0; i < vertexCount; ++i) {
                    texcoordBuffer.push_back(Float2(0.0f, 0.0f));
                }
            }

            mesh.m_primitives.emplace_back(Primitive());
            auto& primitive = mesh.m_primitives.back();
            primitive.m_indexStart = indexStart;
            primitive.m_vertexStart = vertexStart;
            primitive.m_indexCount = indexCount;
            primitive.m_vertexCount = vertexCount;
            primitive.m_materialIndex = srcPrimitive.material;
        }
    }

    for (UINT nodeIndex = 0; nodeIndex < UINT(srcModel.nodes.size()); ++nodeIndex)
    {
        auto meshIndex = srcModel.nodes[nodeIndex].mesh;
        if (meshIndex < 0) {
            continue;
        }
        m_meshes[meshIndex].m_nodeIndex = nodeIndex;
    }
}

void Model::LoadMaterial(const tinygltf::Model& srcModel)
{
    for (const auto& srcMat: srcModel.materials)
    {
        m_materials.emplace_back(Material());
        auto& mat = m_materials.back();
        mat.m_name = StrToWStr(srcMat.name);
        // TODO: BRDF用のセットアップ
        for (auto& value : srcMat.values)
        {
            auto valueName = value.first;
            if (valueName == "baseColorTexture") {
                auto textureIndex = value.second.TextureIndex();
                mat.m_textureIndex = textureIndex;
            }
            if (valueName == "normalTexture") {
                auto textureIndex = value.second.TextureIndex();
            }
            if (valueName == "baseColorFactor") {
                auto color = value.second.ColorFactor();
                mat.m_diffuseColor = Float3(
                    float(color[0]), float(color[1]), float(color[2])
                );
            }
        }
    }
}
