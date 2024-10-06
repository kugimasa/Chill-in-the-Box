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
    void UpdateSceneParam(UINT currentFrame);
    uint8_t* WriteHitGroupShaderRecord(uint8_t* dst, UINT hitGroupRecordSize, ComPtr<ID3D12StateObject>& rtStateObject);
    void UpdateBLAS(ComPtr<ID3D12GraphicsCommandList4> cmdList);

    void SetMaxPathDepth(UINT maxPathDepth) { m_maxPathDepth = maxPathDepth;  }
    void SetMaxSPP(UINT maxSPP) { m_maxSPP = maxSPP; }

    UINT GetMaxPathDepth() { return m_maxPathDepth; }
    UINT GetMaxSPP() { return m_maxSPP; }
    Camera::CameraParam GetCameraParam() { return m_camera->GetParam(); }
    std::shared_ptr<Camera> GetCamera() { return m_camera; }
    ComPtr<ID3D12Resource> GetConstantBuffer();
    TextureResource GetBackgroundTex() { return m_bgTex; }
    UINT GetTotalHitGroupCount() { return m_totalHitGroupCount; }

    struct SceneParam
    {
        Matrix viewMtx;
        Matrix projMtx;
        Matrix invViewMtx;
        Matrix invProjMtx;
        UINT frameIndex;
        UINT currentFrameNum;
        UINT maxPathDepth;
        UINT maxSPP;
    };

private:
    void InitializeActors();
    void InstantiateActor(std::shared_ptr<Actor>& actor, const std::wstring name, const std::wstring hitGroup, Float3 pos);
    void SetTotalHitGroupCount();

private:
    SceneParam m_param;
    UINT m_maxPathDepth;
    UINT m_maxSPP;
    UINT m_totalHitGroupCount;

    std::shared_ptr<Camera> m_camera;
    std::shared_ptr<Actor> m_sphereLight1;
    std::shared_ptr<Actor> m_planeBottom;
    std::shared_ptr<Actor> m_modelActor;
    std::shared_ptr<Actor> m_tableActor;
    std::unique_ptr<Device>& m_pDevice;

    std::vector<std::shared_ptr<Actor>> m_actors;

    TextureResource m_bgTex;

    std::vector<ComPtr<ID3D12Resource>> m_pConstantBuffers;
};
