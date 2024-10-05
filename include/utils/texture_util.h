#pragma once

#include "device.hpp"

#include <filesystem>
#include <fstream>

#include <DirectXTex.h>

struct TextureResource
{
    ComPtr<ID3D12Resource> resource;
    DescriptorHeap srv;
};

inline TextureResource LoadTexture(const void* data, UINT64 size, std::unique_ptr<Device>& device)
{
    using namespace DirectX;
    DirectX::TexMetadata metadata;
    DirectX::ScratchImage image;

    HRESULT hr = E_FAIL;
    TextureResource res{};
    hr = LoadFromDDSMemory(data, size, DDS_FLAGS_NONE, &metadata, image);
    if (FAILED(hr)) {
        hr = LoadFromWICMemory(data, size, WIC_FLAGS_NONE/*WIC_FLAGS_FORCE_RGB*/, &metadata, image);
        if (FAILED(hr)) {
            Error(PrintInfoType::D3D12, "テクスチャのロードに失敗しました: ", hr);
            return res;
        }
    }
    CreateTexture(device->GetDevice().Get(), metadata, &res.resource);

    ComPtr<ID3D12Resource> srcBuffer;
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    PrepareUpload(device->GetDevice().Get(), image.GetImages(), image.GetImageCount(), metadata, subresources);
    const auto totalBytes = GetRequiredIntermediateSize(res.resource.Get(), 0, UINT(subresources.size()));

    auto stagingBuffer = device->CreateBuffer(
        totalBytes,
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_HEAP_TYPE_UPLOAD
    );
    if (stagingBuffer)
    {
        stagingBuffer->SetName(L"Tex-Staging");
    }

    auto cmd = device->CreateCommandList();
    UpdateSubresources(cmd.Get(), res.resource.Get(), stagingBuffer.Get(), 0, 0, UINT(subresources.size()), subresources.data());
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(res.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmd->ResourceBarrier(1, &barrier);
    cmd->Close();

    // 転送
    device->ExecuteCommandList(cmd);

    // シェーダーリソースビューの作成
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = metadata.IsCubemap() ? D3D12_SRV_DIMENSION_TEXTURECUBE : D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0;
    res.srv = device->CreateSRV(res.resource.Get(), &srvDesc);

    // 待機
    device->WaitForGpu();
    return res;
}

inline TextureResource LoadTexture(const std::wstring& fileName, std::unique_ptr<Device>& device)
{
    namespace fs = std::filesystem;
    const fs::path path{ RESOURCE_DIR L"/texture/" + fileName };
    std::ifstream srcFile(path, std::ios::binary);
    if (!srcFile)
    {
        std::wstring err = L"テクスチャのロードに失敗しました: " + path.wstring();
        Error(PrintInfoType::D3D12, err);
        return TextureResource();
    }
    std::vector<char> buf;
    buf.resize(srcFile.seekg(0, std::ios::end).tellg());
    srcFile.seekg(0, std::ios::beg).read(buf.data(), buf.size());
    return LoadTexture(buf.data(), buf.size(), device);
}

inline TextureResource LoadHDRTexture(const std::wstring& fileName, std::unique_ptr<Device>& device)
{
    namespace fs = std::filesystem;
    const fs::path path{ RESOURCE_DIR L"/texture/" + fileName };
    std::ifstream srcFile(path, std::ios::binary);
    if (!srcFile)
    {
        std::wstring err = L"テクスチャのロードに失敗しました: " + path.wstring();
        Error(PrintInfoType::D3D12, err);
        return TextureResource();
    }
    DirectX::ScratchImage image;
    DirectX::TexMetadata metadata;
    TextureResource res{};
    HRESULT hr = LoadFromHDRFile(path.c_str(), &metadata, image);
    if (FAILED(hr))
    {
        std::wstring err = L"HDRテクスチャのロードに失敗しました: " + path.wstring();
        Error(PrintInfoType::D3D12, err);
    }
    // TODO: 下記の処理はLoadTextureと同様なのでまとめたい
    DirectX::CreateTexture(device->GetDevice().Get(), metadata, &res.resource);
    ComPtr<ID3D12Resource> srcBuffer;
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    PrepareUpload(device->GetDevice().Get(), image.GetImages(), image.GetImageCount(), metadata, subresources);
    const auto totalBytes = GetRequiredIntermediateSize(res.resource.Get(), 0, UINT(subresources.size()));

    auto stagingBuffer = device->CreateBuffer(
        totalBytes,
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_HEAP_TYPE_UPLOAD
    );
    if (stagingBuffer)
    {
        stagingBuffer->SetName(L"Tex-Staging");
    }

    auto cmd = device->CreateCommandList();
    UpdateSubresources(cmd.Get(), res.resource.Get(), stagingBuffer.Get(), 0, 0, UINT(subresources.size()), subresources.data());
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(res.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmd->ResourceBarrier(1, &barrier);
    cmd->Close();

    // 転送
    device->ExecuteCommandList(cmd);

    // シェーダーリソースビューの作成
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0;
    res.srv = device->CreateSRV(res.resource.Get(), &srvDesc);

    // 待機
    device->WaitForGpu();
    return res;
}
