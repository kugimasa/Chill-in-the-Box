#pragma once

#include "device.hpp"
#include "scene/camera.hpp"

class Scene
{
public:
    Scene();
    ~Scene();

    void OnInit(std::unique_ptr<Device>& device, float aspect);
    void OnUpdate(std::unique_ptr<Device>& device, int currentFrame, int maxFrame);

    void UpdateSceneParam();
    Camera::CameraParam GetCameraParam() { return m_camera.GetParam(); }
    ComPtr<ID3D12Resource> GetConstantBuffer(std::unique_ptr<Device>& device);


    struct SceneParam
    {
        Matrix viewMtx;
        Matrix projMtx;
        Matrix invViewMtx;
        Matrix invProjMtx;
    };

private:
    Camera m_camera;
    SceneParam m_param;
    // TODO: モデル用のデータクラス

    std::vector<ComPtr<ID3D12Resource>> m_pConstantBuffers;
};
