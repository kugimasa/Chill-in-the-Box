#include "scene/scene.hpp"

Scene::Scene(std::unique_ptr<Device>& device) :
    m_pDevice(device),
    m_camera(),
    m_param(),
    m_maxPathDepth(10),
    m_maxSPP(50)
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
    m_camera = std::shared_ptr<Camera>(new Camera(fovY, aspect, nearZ, farZ, origin, target));
    if (m_pDevice->CreateConstantBuffer(m_pConstantBuffers, sizeof(SceneParam), L"SceneCB"))
    {
        UpdateSceneParam(0);
    }

    // ���f���̏����ݒ�
    InitializeActors();

    // �w�i�e�N�X�`���̃��[�h
    m_bgTex = LoadHDRTexture(L"studio_small_09_4k.hdr", m_pDevice);

    Print(PrintInfoType::RTCAMP10, L"�V�[���\�z ����");
}

void Scene::OnUpdate(int currentFrame, int maxFrame)
{
    int fps = 60;
    float deltaTime = float(currentFrame % fps) / float(fps);

    // �J�����̍X�V
    UpdateSceneParam(currentFrame);

    // �V�[���o�b�t�@�̏�������
    UINT frameIndex = m_pDevice->GetCurrentFrameIndex();
    auto cb = m_pConstantBuffers[frameIndex];
    m_pDevice->WriteBuffer(cb, &m_param, sizeof(SceneParam));

    // ���f���̉�]
    // m_modelActor->Rotate(deltaTime, 2.0f, Float3(0, 1, 0));
}

void Scene::OnDestroy()
{
    m_lightActor.reset();
    m_tableActor.reset();
    m_modelActor.reset();
    for (auto& sceneCB : m_pConstantBuffers)
    {
        sceneCB.Reset();
    }
}

void Scene::CreateRTInstanceDesc(std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& instanceDescs)
{
    UINT instanceHitGroupOffset = 0;
    // ���C�g
    {
        D3D12_RAYTRACING_INSTANCE_DESC desc{};
        auto mtxTrans = m_lightActor->GetWorldMatrix();
        XMStoreFloat3x4(reinterpret_cast<Mtx3x4*>(&desc.Transform), mtxTrans);
        desc.InstanceID = 1;
        desc.InstanceMask = 0xFF;
        desc.InstanceContributionToHitGroupIndex = instanceHitGroupOffset;
        desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        desc.AccelerationStructure = m_lightActor->GetBLAS()->GetGPUVirtualAddress();
        instanceDescs.push_back(desc);
        instanceHitGroupOffset += m_lightActor->GetTotalMeshCount();

    }
    // TODO: �w�i
    {
    }
    // �e�[�u��
    {
        D3D12_RAYTRACING_INSTANCE_DESC desc{};
        auto mtxTrans = m_tableActor->GetWorldMatrix();
        XMStoreFloat3x4(reinterpret_cast<Mtx3x4*>(&desc.Transform), mtxTrans);
        desc.InstanceID = 0;
        desc.InstanceMask = 0xFF;
        desc.InstanceContributionToHitGroupIndex = instanceHitGroupOffset;
        desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        desc.AccelerationStructure = m_tableActor->GetBLAS()->GetGPUVirtualAddress();
        instanceDescs.push_back(desc);
        instanceHitGroupOffset += m_tableActor->GetTotalMeshCount();
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
void Scene::UpdateSceneParam(UINT currentFrame)
{
    m_param.viewMtx = m_camera->GetViewMatrix();
    m_param.projMtx = m_camera->GetProjMatrix();
    m_param.invViewMtx = XMMatrixInverse(nullptr, m_param.viewMtx);
    m_param.invProjMtx = XMMatrixInverse(nullptr, m_param.projMtx);
    m_param.frameIndex = m_pDevice->GetCurrentFrameIndex();
    m_param.currentFrameNum = currentFrame;
    m_param.maxPathDepth = m_maxPathDepth;
    m_param.maxSPP = m_maxSPP;
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
    dst = m_lightActor->WriteHitGroupShaderRecord(dst, hitGroupRecordSize, rtStateObjectProps);
    dst = m_tableActor->WriteHitGroupShaderRecord(dst, hitGroupRecordSize, rtStateObjectProps);
    dst = m_modelActor->WriteHitGroupShaderRecord(dst, hitGroupRecordSize, rtStateObjectProps);
    return dst;
}

/// <summary>
/// BLAS�̍X�V
/// </summary>
/// <param name="cmdList"></param>
void Scene::UpdateBLAS(ComPtr<ID3D12GraphicsCommandList4> cmdList)
{
    for (auto& model : { m_lightActor,m_tableActor, m_modelActor })
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
/// HitGroup���v�̎擾
/// </summary>
/// <returns></returns>
UINT Scene::GetTotalHitGroupCount()
{
    // TODO: ���������ɃL���b�V�����Ă�������
    UINT hitGroupCount = 0;
    for (const auto& model : { m_lightActor, m_modelActor, m_tableActor })
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
void Scene::InitializeActors()
{
    // ����
    InstantiateActor(m_lightActor, L"teapot.glb", L"Actor", Float3(0, 1, -3));
    // �e�[�u��
    InstantiateActor(m_tableActor, L"round_table.glb", L"Actor", Float3(0, -5, -3));
    // �L�����N�^�[
    InstantiateActor(m_modelActor, L"model.glb", L"Actor", Float3(0, 0, -3));
}

/// <summary>
/// �I�u�W�F�N�g�̃C���X�^���X��
/// </summary>
/// <param name="actor"></param>
/// <param name="fileName"></param>
/// <param name="hitGroup"></param>
/// <param name="pos"></param>
void Scene::InstantiateActor(std::shared_ptr<Actor>& actor, const std::wstring fileName, const std::wstring hitGroup, Float3 pos)
{
    // ���f���̃��[�h
    auto model = new Model(fileName, m_pDevice);
    actor = model->InstantiateActor(m_pDevice);
    actor->SetMaterialHitGroup(hitGroup);
    // �����ʒu�ݒ�
    actor->SetWorldPos(pos);
    actor->UpdateMatrices();
}
