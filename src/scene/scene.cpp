#include "scene/scene.hpp"

Scene::Scene() :
    m_camera(),
    m_param()
{
}

Scene::~Scene()
{
}

void Scene::OnInit(std::unique_ptr<Device>& device, float aspect)
{
    // �J�����̏����ݒ�
    float fovY = XM_PIDIV4;
    float nearZ = 0.1f;
    float farZ = 100.0f;
    Float3 origin(0.0f, 0.0f, 1.0f);
    Float3 target(0.0f, 0.0f, -1.0f);
    m_camera = Camera(fovY, aspect, nearZ, farZ, origin, target);
    if (device->CreateConstantBuffer(m_pConstantBuffers, sizeof(SceneParam), L"SceneCB"))
    {
        UpdateSceneParam();
    }

    Print(PrintInfoType::RTCAMP10, "�V�[���\�z ����");
}

void Scene::OnUpdate(std::unique_ptr<Device>& device, int currentFrame, int maxFrame)
{
    int fps = 60;
    float deltaTime = float(currentFrame % fps) / float(fps);

    // �J�����̍X�V
    m_camera.Rotate(deltaTime);
    UpdateSceneParam();

    // �V�[���o�b�t�@�̏�������
    UINT frameIndex = device->GetCurrentFrameIndex();
    auto cb = m_pConstantBuffers[frameIndex];
    device->WriteBuffer(cb, &m_param, sizeof(SceneParam));
}

/// <summary>
/// �V�[���p�����[�^�̍X�V
/// </summary>
void Scene::UpdateSceneParam()
{
    m_param.viewMtx = m_camera.GetViewMatrix();
    m_param.projMtx = m_camera.GetProjMatrix();
    m_param.invViewMtx = XMMatrixInverse(nullptr, m_param.viewMtx);
    m_param.invProjMtx = XMMatrixInverse(nullptr, m_param.projMtx);
}

ComPtr<ID3D12Resource> Scene::GetConstantBuffer(std::unique_ptr<Device>& device)
{
    UINT frameIndex = device->GetCurrentFrameIndex();
    return m_pConstantBuffers[frameIndex];
}
