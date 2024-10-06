#include "scene/scene.hpp"

Scene::Scene(std::unique_ptr<Device>& device) :
    m_pDevice(device),
    m_camera(),
    m_param(),
    m_maxPathDepth(10),
    m_maxSPP(500),
    m_totalHitGroupCount(0)
{
}

Scene::~Scene()
{
}

void Scene::OnInit(float aspect)
{
    // カメラの初期設定
    float fovY = XM_PIDIV4;
    float nearZ = 0.1f;
    float farZ = 100.0f;
    Float3 origin(0.0f, 5.0f, 10.0f);
    Float3 target(0.0f, 5.0f, 0.0f);
    m_camera = std::shared_ptr<Camera>(new Camera(fovY, aspect, nearZ, farZ, origin, target));
    if (m_pDevice->CreateConstantBuffer(m_pConstantBuffers, sizeof(SceneParam), L"SceneCB"))
    {
        UpdateSceneParam(0);
    }

    // モデルの初期設定
    InitializeActors();

    // 背景テクスチャのロード
    m_bgTex = LoadHDRTexture(L"studio_small_09_4k.hdr", m_pDevice);

    Print(PrintInfoType::RTCAMP10, L"シーン構築 完了");
}

void Scene::OnUpdate(int currentFrame, int maxFrame)
{
    int fps = 60;
    float deltaTime = float(currentFrame % fps) / float(fps);

    // カメラの更新
    UpdateSceneParam(currentFrame);

    // シーンバッファの書き込み
    UINT frameIndex = m_pDevice->GetCurrentFrameIndex();
    auto cb = m_pConstantBuffers[frameIndex];
    m_pDevice->WriteBuffer(cb, &m_param, sizeof(SceneParam));

    // モデルの回転
    m_modelActor->Rotate(deltaTime, 2.0f, Float3(0, 1, 0));
}

void Scene::OnDestroy()
{
    m_camera.reset();
    for (auto& actor : m_actors)
    {
        actor.reset();
    }
    for (auto& sceneCB : m_pConstantBuffers)
    {
        sceneCB.Reset();
    }
}

void Scene::CreateRTInstanceDesc(std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& instanceDescs)
{
    UINT instanceHitGroupOffset = 0;
    // ライト
    {
        D3D12_RAYTRACING_INSTANCE_DESC desc{};
        auto mtxTrans = m_sphereLight1->GetWorldMatrix();
        XMStoreFloat3x4(reinterpret_cast<Mtx3x4*>(&desc.Transform), mtxTrans);
        desc.InstanceID = 1;
        desc.InstanceMask = 0xFF;
        desc.InstanceContributionToHitGroupIndex = instanceHitGroupOffset;
        desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        desc.AccelerationStructure = m_sphereLight1->GetBLAS()->GetGPUVirtualAddress();
        instanceDescs.push_back(desc);
        instanceHitGroupOffset += m_sphereLight1->GetTotalMeshCount();
    }
    // 背景
    {
        D3D12_RAYTRACING_INSTANCE_DESC desc{};
        auto mtxTrans = m_planeBottom->GetWorldMatrix();
        XMStoreFloat3x4(reinterpret_cast<Mtx3x4*>(&desc.Transform), mtxTrans);
        desc.InstanceID = 0;
        desc.InstanceMask = 0xFF;
        desc.InstanceContributionToHitGroupIndex = instanceHitGroupOffset;
        desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        desc.AccelerationStructure = m_planeBottom->GetBLAS()->GetGPUVirtualAddress();
        instanceDescs.push_back(desc);
        instanceHitGroupOffset += m_planeBottom->GetTotalMeshCount();
    }
    // テーブル
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
    // オブジェクト
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
/// シーンパラメータの更新
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
/// HitGroup用のシェーダーレコードの書き込み
/// </summary>
/// <param name="dst"></param>
/// <param name="hitGroupRecordSize"></param>
/// <param name="rtState"></param>
/// <returns></returns>
uint8_t* Scene::WriteHitGroupShaderRecord(uint8_t* dst, UINT hitGroupRecordSize, ComPtr<ID3D12StateObject>& rtStateObject)
{
    ComPtr<ID3D12StateObjectProperties> rtStateObjectProps;
    rtStateObject.As(&rtStateObjectProps);
    for (const auto& actor : m_actors)
    {
        dst = actor->WriteHitGroupShaderRecord(dst, hitGroupRecordSize, rtStateObjectProps);
    }
    return dst;
}

/// <summary>
/// BLASの更新
/// </summary>
/// <param name="cmdList"></param>
void Scene::UpdateBLAS(ComPtr<ID3D12GraphicsCommandList4> cmdList)
{
    for (auto& actor : m_actors )
    {
        actor->UpdateMatrices();
        actor->UpdateTransform();
        actor->UpdateBLAS(cmdList);
    }
}

/// <summary>
/// シーン用定数バッファの取得
/// </summary>
/// <returns></returns>
ComPtr<ID3D12Resource> Scene::GetConstantBuffer()
{
    UINT frameIndex = m_pDevice->GetCurrentFrameIndex();
    return m_pConstantBuffers[frameIndex];
}

/// <summary>
/// オブジェクトのセットアップ
/// </summary>
void Scene::InitializeActors()
{
    // 光源
    InstantiateActor(m_sphereLight1, L"sphere.glb", L"Actor", Float3(-1, 6, 0));
    m_actors.push_back(m_sphereLight1);
    // 背景
    InstantiateActor(m_planeBottom, L"plane.glb", L"Actor", Float3(0, 0, 0));
    m_actors.push_back(m_planeBottom);
    // テーブル
    InstantiateActor(m_tableActor, L"round_table.glb", L"Actor", Float3(0, 0, 0));
    m_actors.push_back(m_tableActor);
    // キャラクター
    InstantiateActor(m_modelActor, L"model.glb", L"Actor", Float3(0, 5, 0));
    m_actors.push_back(m_modelActor);

    // HitGroup合計の設定
    SetTotalHitGroupCount();
}

/// <summary>
/// オブジェクトのインスタンス化
/// </summary>
/// <param name="actor"></param>
/// <param name="fileName"></param>
/// <param name="hitGroup"></param>
/// <param name="pos"></param>
void Scene::InstantiateActor(std::shared_ptr<Actor>& actor, const std::wstring fileName, const std::wstring hitGroup, Float3 pos)
{
    // モデルのロード
    auto model = new Model(fileName, m_pDevice);
    actor = model->InstantiateActor(m_pDevice);
    actor->SetMaterialHitGroup(hitGroup);
    // 初期位置設定
    actor->SetWorldPos(pos);
    actor->UpdateMatrices();
}

void Scene::SetTotalHitGroupCount()
{
    UINT hitGroupCount = 0;
    for (const auto& actor : m_actors )
    {
        for (UINT groupIdx = 0; groupIdx < actor->GetMeshGroupCount(); ++groupIdx)
        {
            hitGroupCount += actor->GetMeshCount(groupIdx);
        }
    }
    m_totalHitGroupCount = hitGroupCount;
}
