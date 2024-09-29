#pragma once

#include "device.hpp"

inline ASBuffers CreateASBuffers(std::unique_ptr<Device>& device, const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& asDesc, const std::wstring& name)
{
    ASBuffers asBuffers{};
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO asPrebuild{};
    device->GetDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&asDesc.Inputs, &asPrebuild);

    // スクラッチバッファの確保
    asBuffers.scratchBuffer = device->CreateBuffer(
        asPrebuild.ScratchDataSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_HEAP_TYPE_DEFAULT
    );

    // ASバッファの確保
    asBuffers.asBuffer = device->CreateBuffer(
        asPrebuild.ResultDataMaxSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        D3D12_HEAP_TYPE_DEFAULT
    );

    if (asBuffers.scratchBuffer == nullptr || asBuffers.asBuffer == nullptr)
    {
        Error(PrintInfoType::D3D12, "ASの構築に失敗しました (Scratch/AS)");
    }
    std::wstring scratchName = name + L":scratch";
    asBuffers.scratchBuffer->SetName(scratchName.c_str());
    asBuffers.scratchBuffer->SetName(name.c_str());

    // 更新用のバッファが必要であれば確保
    if (asDesc.Inputs.Flags & D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE)
    {
        asBuffers.updateBuffer = device->CreateBuffer(
            asPrebuild.ScratchDataSizeInBytes,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_HEAP_TYPE_DEFAULT
        );
        if (asBuffers.updateBuffer == nullptr)
        {
            Error(PrintInfoType::D3D12, "ASの構築に失敗しました (Update)");
        }
        std::wstring uploadName = name + L":upload";
        asBuffers.updateBuffer->SetName(uploadName.c_str());
    }

    return asBuffers;
}

inline UINT WriteShaderId(void* dst, const void* shaderId)
{
    memcpy(dst, shaderId, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    return D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
}

inline UINT WriteGPUDescriptorHeap(void* dst, const DescriptorHeap& descHeap)
{
    auto gpuHandle = descHeap.gpuHandle;
    memcpy(dst, &gpuHandle, sizeof(gpuHandle));
    return UINT(sizeof(gpuHandle));
}

inline UINT WriteGPUResourceAddress(void* dst, const ComPtr<ID3D12Resource>& resource)
{
    D3D12_GPU_VIRTUAL_ADDRESS address = resource->GetGPUVirtualAddress();
    memcpy(dst, &address, sizeof(address));
    return UINT(sizeof(address));
}

inline D3D12_ROOT_PARAMETER& CreateRootParam(D3D12_DESCRIPTOR_RANGE_TYPE rangeType, UINT shaderRegister, UINT registerSpace, UINT descCount = 1)
{
    auto descRange = new D3D12_DESCRIPTOR_RANGE{};
    descRange->RangeType = rangeType;
    descRange->BaseShaderRegister = shaderRegister;
    descRange->RegisterSpace = registerSpace;
    descRange->NumDescriptors = descCount;
    descRange->OffsetInDescriptorsFromTableStart = 0;
    auto rootParam = new D3D12_ROOT_PARAMETER{};
    rootParam->ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParam->DescriptorTable.NumDescriptorRanges = 1;
    rootParam->DescriptorTable.pDescriptorRanges = descRange;
    return *rootParam;
}

inline D3D12_ROOT_PARAMETER& CreateRootParam(D3D12_ROOT_PARAMETER_TYPE paramType, UINT shaderRegister, UINT registerSpace)
{
    auto rootParam = new D3D12_ROOT_PARAMETER{};
    rootParam->ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParam->ParameterType = paramType;
    rootParam->Descriptor.ShaderRegister = shaderRegister;
    rootParam->Descriptor.RegisterSpace = registerSpace;
    return *rootParam;
}
