#include "scene/scene.hpp"

Scene::Scene(std::unique_ptr<Device>& device) :
    m_pDevice(device),
    m_camera(),
    m_param()
{
}

Scene::~Scene()
{
}

void Scene::OnInit(float aspect)
{
    // �J�����̏����ݒ�
    float fovY = XM_PIDIV4;
    float nearZ = 0.1f;
    float farZ = 100.0f;
    Float3 origin(0.0f, 0.0f, 1.0f);
    Float3 target(0.0f, 0.0f, -1.0f);
    m_camera = Camera(fovY, aspect, nearZ, farZ, origin, target);
    if (m_pDevice->CreateConstantBuffer(m_pConstantBuffers, sizeof(SceneParam), L"SceneCB"))
    {
        UpdateSceneParam();
    }

    // ���f���̏����ݒ�
    InitializeActor();


    Print(PrintInfoType::RTCAMP10, "�V�[���\�z ����");
}

void Scene::OnUpdate(int currentFrame, int maxFrame)
{
    int fps = 60;
    float deltaTime = float(currentFrame % fps) / float(fps);

    // �J�����̍X�V
    UpdateSceneParam();

    // �V�[���o�b�t�@�̏�������
    UINT frameIndex = m_pDevice->GetCurrentFrameIndex();
    auto cb = m_pConstantBuffers[frameIndex];
    m_pDevice->WriteBuffer(cb, &m_param, sizeof(SceneParam));

    // ���f���̍X�V
    auto transMtx = XMMatrixTranslation(0, 0, -3);
    m_modelActor->SetWorldMatrix(transMtx);
    m_modelActor->UpdateTransform();
}

void Scene::OnDestroy()
{
    m_modelActor.reset();
    for (auto& sceneCB : m_pConstantBuffers)
    {
        sceneCB.Reset();
    }
}

void Scene::CreateRTInstanceDesc(std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& instanceDescs)
{
    UINT instanceHitGroupOffset = 0;
    // TODO: �w�i
    {
    }
    // TODO: �X�e�[�W
    {
    }
    // TODO: ���C�g
    {
    }
    // �I�u�W�F�N�g
    {
        D3D12_RAYTRACING_INSTANCE_DESC desc{};
        auto mtxTrans = m_modelActor->GetWorldMatrix();
        XMStoreFloat3x4(reinterpret_cast<Mtx3x4*>(&desc.Transform), mtxTrans);
        desc.InstanceID = 0;
        desc.InstanceMask = 0xFF;
        desc.InstanceContributionToHitGroupIndex = instanceHitGroupOffset;
        desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        desc.AccelerationStructure = m_modelActor->GetBLAS()->GetGPUVirtualAddress();
        instanceDescs.push_back(desc);
        instanceHitGroupOffset += m_modelActor->GetTotalMeshCount();
    }
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
    m_param.frameIndex = m_pDevice->GetCurrentFrameIndex();
}

/// <summary>
/// HitGroup�p�̃V�F�[�_�[���R�[�h�̏�������
/// </summary>
/// <param name="dst"></param>
/// <param name="hitGroupRecordSize"></param>
/// <param name="rtState"></param>
/// <returns></returns>
uint8_t* Scene::WriteHitGroupShaderRecord(uint8_t* dst, UINT hitGroupRecordSize, ComPtr<ID3D12StateObject>& rtStateObject)
{
    ComPtr<ID3D12StateObjectProperties> rtStateObjectProps;
    rtStateObject.As(&rtStateObjectProps);
    dst = m_modelActor->WriteHitGroupShaderRecord(dst, hitGroupRecordSize, rtStateObjectProps);
    return dst;
}

/// <summary>
/// BLAS�̍X�V
/// </summary>
/// <param name="cmdList"></param>
void Scene::UpdateBLAS(ComPtr<ID3D12GraphicsCommandList4> cmdList)
{
    for (auto& model : { m_modelActor })
    {
        model->UpdateMatrices();
        model->UpdateTransform();
        model->UpdateBLAS(cmdList);
    }
}

/// <summary>
/// �V�[���p�萔�o�b�t�@�̎擾
/// </summary>
/// <returns></returns>
ComPtr<ID3D12Resource> Scene::GetConstantBuffer()
{
    UINT frameIndex = m_pDevice->GetCurrentFrameIndex();
    return m_pConstantBuffers[frameIndex];
}

/// <summary>
/// 
/// </summary>
/// <returns></returns>
UINT Scene::GetTotalHitGroupCount()
{
    UINT hitGroupCount = 0;
    for (const auto& model : { m_modelActor })
    {
        for (UINT groupIdx = 0; groupIdx < model->GetMeshGroupCount(); ++groupIdx)
        {
            hitGroupCount += model->GetMeshCount(groupIdx);
        }
    }
    return hitGroupCount;
}

/// <summary>
/// �I�u�W�F�N�g�̃Z�b�g�A�b�v
/// </summary>
void Scene::InitializeActor()
{
    // ���f���f�[�^�̃��[�h
    auto model = new Model(L"model.glb", m_pDevice);
    m_modelActor = model->InstantiateActor(m_pDevice);
    // HitGroup�ݒ�
    m_modelActor->SetMaterialHitGroup(L"Model");
}
