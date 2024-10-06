#include "scene/scene.hpp"

Scene::Scene(std::unique_ptr<Device>& device) :
    m_pDevice(device),
    m_camera(),
    m_param(),
    m_maxPathDepth(10),
    m_maxSPP(1),
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
        auto lights =
        {
            m_sphereLight1,
            m_sphereLight2,
            m_sphereLight3
        };
        UINT lightInstanceID = 1;
        for (auto& light : lights)
        {
            D3D12_RAYTRACING_INSTANCE_DESC desc{};
            auto mtxTrans = light->GetWorldMatrix();
            XMStoreFloat3x4(reinterpret_cast<Mtx3x4*>(&desc.Transform), mtxTrans);
            desc.InstanceID = lightInstanceID;
            desc.InstanceMask = 0xFF;
            desc.InstanceContributionToHitGroupIndex = instanceHitGroupOffset;
            desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
            desc.AccelerationStructure = light->GetBLAS()->GetGPUVirtualAddress();
            instanceDescs.push_back(desc);
            instanceHitGroupOffset += light->GetTotalMeshCount();
            lightInstanceID++;
        }
    }
    // 背景
    {
        auto planes =
        { 
            m_planeBottom, m_planeTop,
            m_planeRight, m_planeLeft,
            m_planeFront, m_planeBack
        };
        for (auto& plane : planes)
        {
            D3D12_RAYTRACING_INSTANCE_DESC desc{};
            auto mtxTrans = plane->GetWorldMatrix();
            XMStoreFloat3x4(reinterpret_cast<Mtx3x4*>(&desc.Transform), mtxTrans);
            desc.InstanceID = 0;
            desc.InstanceMask = 0xFF;
            desc.InstanceContributionToHitGroupIndex = instanceHitGroupOffset;
            desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
            desc.AccelerationStructure = plane->GetBLAS()->GetGPUVirtualAddress();
            instanceDescs.push_back(desc);
            instanceHitGroupOffset += plane->GetTotalMeshCount();
        }
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
    // ライト1 赤
    Float3 light1Pos = Float3(-2, 6, 0);
    Float3 light1Color = Float3(1, 0, 0);
    SphereLightParam light1Param(light1Pos, 0.2, light1Color, 10.0);
    m_param.light1 = light1Param;
    InstantiateActor(m_sphereLight1, L"sphere.glb", L"Actor", light1Pos);
    m_actors.push_back(m_sphereLight1);
    // ライト2 青
    Float3 light2Pos = Float3(2, 6, 0);
    Float3 light2Color = Float3(0, 1, 0);
    SphereLightParam light2Param(light2Pos, 0.2, light2Color, 10.0);
    m_param.light2 = light2Param;
    InstantiateActor(m_sphereLight2, L"sphere.glb", L"Actor", light2Pos);
    m_actors.push_back(m_sphereLight2);
    // ライト3 緑
    Float3 light3Pos = Float3(0, 6, 3);
    Float3 light3Color = Float3(0, 0, 1);
    SphereLightParam light3Param(light3Pos, 0.2, light3Color, 10.0);
    m_param.light3 = light3Param;
    InstantiateActor(m_sphereLight3, L"sphere.glb", L"Actor", light3Pos);
    m_actors.push_back(m_sphereLight3);

    // 背景
    InstantiateActor(m_planeBottom, L"plane.glb", L"Actor", Float3(0, 0, 0));
    m_actors.push_back(m_planeBottom);
    InstantiateActor(m_planeTop, L"plane.glb", L"Actor", Float3(0, 10, 0));
    m_planeTop->SetRotaion(180, Float3(1, 0, 0));
    m_actors.push_back(m_planeTop);
    InstantiateActor(m_planeRight, L"plane.glb", L"Actor", Float3(5, 5, 0));
    m_planeRight->SetRotaion(90, Float3(0, 0, 1));
    m_actors.push_back(m_planeRight);
    InstantiateActor(m_planeLeft, L"plane.glb", L"Actor", Float3(-5, 5, 0));
    m_planeLeft->SetRotaion(-90, Float3(0, 0, 1));
    m_actors.push_back(m_planeLeft);
    InstantiateActor(m_planeFront, L"plane.glb", L"Actor", Float3(0, 5, -5));
    m_planeFront->SetRotaion(90, Float3(1, 0, 0));
    m_actors.push_back(m_planeFront);
    InstantiateActor(m_planeBack, L"plane.glb", L"Actor", Float3(0, 5, 5));
    m_planeBack->SetRotaion(-90, Float3(1, 0, 0));
    m_actors.push_back(m_planeBack);

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
