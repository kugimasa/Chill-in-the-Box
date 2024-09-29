#pragma once

#include "device.hpp"
#include "scene/camera.hpp"
#include "scene/actor.hpp"

class Scene
{
public:
    Scene(std::unique_ptr<Device>& device);
    ~Scene();

    void OnInit(float aspect);
    void OnUpdate(int currentFrame, int maxFrame);
    void OnDestroy();

    void CreateRTInstanceDesc(std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& instanceDescs);
    void UpdateSceneParam();
    uint8_t* WriteHitGroupShaderRecord(uint8_t* dst, UINT hitGroupRecordSize, ComPtr<ID3D12StateObject>& rtStateObject);
    void UpdateBLAS(ComPtr<ID3D12GraphicsCommandList4> cmdList);

    Camera::CameraParam GetCameraParam() { return m_camera.GetParam(); }
    ComPtr<ID3D12Resource> GetConstantBuffer();
    UINT GetTotalHitGroupCount();


    struct SceneParam
    {
        Matrix viewMtx;
        Matrix projMtx;
        Matrix invViewMtx;
        Matrix invProjMtx;
        UINT frameIndex;
    };

private:
    void InitializeActor();

private:
    Camera m_camera;
    SceneParam m_param;
    std::shared_ptr<Actor> m_modelActor;
    std::unique_ptr<Device>& m_pDevice;

    std::vector<ComPtr<ID3D12Resource>> m_pConstantBuffers;
};
