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

    Camera::CameraParam GetCameraParam() { return m_camera->GetParam(); }
    std::shared_ptr<Camera> GetCamera() { return m_camera; }
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
    void InitializeActors();
    void InstantiateActor(std::shared_ptr<Actor>& actor, const std::wstring name, const std::wstring hitGroup, Float3 pos);

private:
    SceneParam m_param;
    std::shared_ptr<Camera> m_camera;
    std::shared_ptr<Actor> m_modelActor;
    std::shared_ptr<Actor> m_tableActor;
    std::unique_ptr<Device>& m_pDevice;

    std::vector<ComPtr<ID3D12Resource>> m_pConstantBuffers;
};
