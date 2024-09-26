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
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
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
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
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