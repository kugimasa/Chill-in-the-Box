#pragma once

#include "device.hpp"
#include "scene/camera.hpp"

class Scene
{
public:
    Scene();
    ~Scene();

    void OnInit(Device& device, float aspect);
    void OnUpdate(Device& device, int currentFrame, int maxFrame);

    void UpdateSceneParam();
    Camera::CameraParam GetCameraParam() { return m_camera.GetParam(); }
    ComPtr<ID3D12Resource> GetConstantBuffer(Device& device);


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

    std::vector<ComPtr<ID3D12Resource>> m_pConstantBuffers;
};
