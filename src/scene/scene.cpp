#include "scene/scene.hpp"
#include "utils/color_util.h"

Scene::Scene(std::unique_ptr<Device>& device) :
    m_pDevice(device),
    m_camera(),
    m_param(),
    m_maxPathDepth(10),
    m_maxSPP(50),
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
    Float3 startPos(-10.0, 0.36, 5.8);
    Float3 target(0.0f, 5.0f, 0.0f);
    m_camera = std::shared_ptr<Camera>(new Camera(fovY, aspect, nearZ, farZ, startPos, target));
    if (m_pDevice->CreateConstantBuffer(m_pConstantBuffers, sizeof(SceneParam), L"SceneCB"))
    {
        UpdateSceneParam(0);
    }

    // モデルの初期設定
    InitializeActors();

    // 背景テクスチャのロード
    m_bgTex = LoadHDRTexture(L"rural_asphalt_road_4k.hdr", m_pDevice);

    Print(PrintInfoType::RTCAMP10, L"シーン構築 完了");
}

void Scene::OnUpdate(int currentFrame, int maxFrame)
{
    // アニメーション処理はこちらで行う
    // 本番提出想定設定
    // MAX_FRAME: 600
    // FPS: 60
    // MAX_SEC: 10
    // TODO: 後ほど削除
    maxFrame = 600;
    int fps = 60;
    float maxSec = 10;
    float deltaTime = float(currentFrame % fps) / float(fps);
    float currentTime = float(currentFrame) * maxSec / float(maxFrame);

    /// カメラ演出
    {
        float startTime = 0.0f;
        float action1Time = 2.0f;
        float action2Time = 2.5f;
        float action3Time = 5.5f;
        float action4Time = 8.0;
        float action5Time = 10.0f;
        Float3 camStartPos(-10.0, 0.36, 5.8);
        Float3 camAction1Pos(3.0, 5.5, 3.1);
        Float3 camAction2Pos(6.6, 9.5, 3.6);
        Float3 camAction3Pos(8.0, 10.0, -6);
        float camStartFovY = XM_PIDIV4;
        float camAction3FovY = XM_PIDIV4 * 0.5;

        ////// ACTION 1 ///////
        // TIME: 0.0 - 2.0 (sec)
        if (startTime <= currentTime && currentTime < action1Time)
        {
            // カメラ移動
            m_camera->MoveAnimInCubic(currentTime, startTime, action1Time, camStartPos, camAction1Pos);
        }
        ////// ACTION 2 //////
        // TIME: 2.0 - 2.5 (sec)
        else if (action1Time <= currentTime && currentTime < action2Time)
        {
            // カメラ移動
            m_camera->MoveAnimOutCubic(currentTime, action1Time, action2Time, camAction1Pos, camAction2Pos);
        }
        ////// ACTION 3 - READY //////
        // TIME: 2.5 - 5.5 (sec)
        else if (action2Time <= currentTime && currentTime < action3Time)
        {
            // カメラ移動
            m_camera->MoveAnimInCubic(currentTime, action2Time, action3Time, camAction2Pos, camAction3Pos);
            // カメラ画角調整
            m_camera->ChangeFovYInCubic(currentTime, action2Time, action3Time, camStartFovY, camAction3FovY);
        }
        ////// ACTION 4 - STAY //////
        // TIME: 5.5 - 8.0 (sec)
        ////// ACTION 6 - FLY //////
        // TIME: 8.0 - 10.0 (sec)
        else if (action4Time <= currentTime && currentTime < action5Time)
        {
            // カメラ画角調整
            m_camera->ChangeFovYInCubic(currentTime, action4Time, (action4Time + 0.5f), camAction3FovY, camStartFovY);
            // カメラ追従
            if (currentTime < (action4Time + 1.2f))
            {
                auto actorPos = m_modelActor->GetWorldPos();
                m_camera->SetTarget(actorPos);
            }
            // モデル移動
            Float3 modelStartPos(0, 5, 0);
            Float3 modelEndPos(0, 100, 0);
            m_modelActor->MoveAnimInCubic(currentTime, action4Time, action5Time, modelStartPos, modelEndPos);
        }
    }

    /// 背景展開
    {
        float startTime = 7.9f;
        float endTime = 8.1f;
        if (startTime <= currentTime && currentTime < endTime)
        {
            // ハッチオープン
            // 天板移動
            Float3 topStartPos(0, 10, 0);
            Float3 topEndPos(0, 10, -10);
            m_planeTop->MoveAnimInCubic(currentTime, startTime, endTime, topStartPos, topEndPos);
            m_planeTop->SetRotation(180, Float3(1, 0, 0));
            // 後壁
            float t = (currentTime - startTime) / (endTime - startTime);
            float startDeg = -90;
            float endDeg = 0;
            float degree = std::lerp(startDeg, endDeg, t);
            m_planeBack->SetRotation(degree, Float3(1, 0, 0));
            m_planeBack->Translate(Float3(0, -5, 0));
            // 左壁
            m_planeLeft->SetRotation(degree, Float3(0, 0, 1));
            m_planeLeft->Translate(Float3(0, -5, 0));
        }
    }

    /// モデル回転挙動
    {
        float rotTime1 = 2.0f; // 予備動作 (右回転)
        float rotTime2 = 5.0f; // 予備動作 (左回転)
        float rotTime3 = 5.5f; // 停止開始
        float rotTime4 = 6.0f; // 停止解除
        float rotTime5 = 8.0f; // フルスロットル
        float modelReadyDeg1 = -60.0f;
        float modelReadyDeg2 = 130.0f;
        float modelSpeedUpDeg = 2000.0f;
        float modelFinalDeg = 0.0f;
        float modelFinalRot = 10.0f;
        // 回転の予備動作1
        // TIME: 2.0 - 5.0
        if (rotTime1 <= currentTime && currentTime < rotTime2)
        {
            // モデルの回転
            float t = (currentTime - rotTime1) / (rotTime2 - rotTime1);
            float s = EaseInCubic(t);
            float degree = std::lerp(0.0, modelReadyDeg1, s);
            m_modelActor->SetRotation(degree, Float3(0, 1, 0));
        }
        // 回転の予備動作2
        // TIME: 5.0 - 5.5
        else if (rotTime2 <= currentTime && currentTime < rotTime3)
        {
            // モデルの回転
            float t = (currentTime - rotTime2) / (rotTime3 - rotTime2);
            float s = EaseOutCubic(t);
            float degree = std::lerp(modelReadyDeg1, modelReadyDeg2, s);
            m_modelActor->SetRotation(degree, Float3(0, 1, 0));
        }
        // 一時停止
        // TIME: 5.5 - 6.0
        // 回転の予備動作2
        // TIME: 6.0 - 8.0
        else if (rotTime4 <= currentTime && currentTime < rotTime5)
        {
            // モデルの回転
            float t = (currentTime - rotTime4) / (rotTime5 - rotTime4);
            float s = EaseInCubic(t);
            float degree = std::lerp(modelReadyDeg2, modelSpeedUpDeg, s);
            // モデルの回転
            m_modelActor->SetRotation(degree, Float3(0, 1, 0));
        }
        // 最終回転動作
        // TIME: 8.0 - 10.0
        else if (rotTime5 <= currentTime)
        {
            // モデルの回転
            m_modelActor->Rotate(modelFinalRot * deltaTime, modelSpeedUpDeg, Float3(0, 1, 0));
        }
    }

    /// ライト演出
    {
        // 上昇挙動
        float startTime = 0;
        float endTime = 0.5f;
        if (startTime <= currentTime && currentTime < endTime)
        {
            Float3 startPos1 = Float3(0, 1, 2);
            Float3 endPos1 = Float3(0, 5, 2);
            m_sphereLight1->MoveAnimInCubic(currentTime, startTime, endTime, startPos1, endPos1);
            Float3 startPos2 = Float3(sqrt(3), 1, -1);
            Float3 endPos2 = Float3(sqrt(3), 5, -1);
            m_sphereLight2->MoveAnimInCubic(currentTime, startTime, endTime, startPos2, endPos2);
            Float3 startPos3 = Float3(-sqrt(3), 1, -1);
            Float3 endPos3 = Float3(-sqrt(3), 5, -1);
            m_sphereLight3->MoveAnimInCubic(currentTime, startTime, endTime, startPos3, endPos3);
        }
        // TODO: 旋回挙動

        // TODO: 強度の変化

        // ライトパラメータの更新
        m_param.light1.center = m_sphereLight1->GetWorldPos();
        m_param.light2.center = m_sphereLight2->GetWorldPos();
        m_param.light3.center = m_sphereLight3->GetWorldPos();
    }

    // シーンバッファの書き込み
    UINT frameIndex = m_pDevice->GetCurrentFrameIndex();
    auto cb = m_pConstantBuffers[frameIndex];
    m_pDevice->WriteBuffer(cb, &m_param, sizeof(SceneParam));

    // シーンパラメータの更新
    UpdateSceneParam(currentFrame);
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
            desc.InstanceMask = 0x08; // ライト用のマスク
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
    // ライト1
    Float3 light1Pos = Float3(0, 1, 2);
    Float3 light1Color = COL_LIGHT_SKY_BLUE;
    SphereLightParam light1Param(light1Pos, 0.2, light1Color, 50.0);
    m_param.light1 = light1Param;
    InstantiateActor(m_sphereLight1, L"sphere.glb", L"Actor", light1Pos);
    m_actors.push_back(m_sphereLight1);
    // ライト2
    Float3 light2Pos = Float3(sqrt(3), 1, -1);
    Float3 light2Color = COL_MEDIUM_ORCHID;
    SphereLightParam light2Param(light2Pos, 0.2, light2Color, 50.0);
    m_param.light2 = light2Param;
    InstantiateActor(m_sphereLight2, L"sphere.glb", L"Actor", light2Pos);
    m_actors.push_back(m_sphereLight2);
    // ライト3
    Float3 light3Pos = Float3(-sqrt(3), 1, -1);
    Float3 light3Color = COL_ROYAL_BLUE;
    SphereLightParam light3Param(light3Pos, 0.2, light3Color, 50.0);
    m_param.light3 = light3Param;
    InstantiateActor(m_sphereLight3, L"sphere.glb", L"Actor", light3Pos);
    m_actors.push_back(m_sphereLight3);

    // 背景
    InstantiateActor(m_planeBottom, L"plane.glb", L"Actor", Float3(0, 0, 0));
    m_actors.push_back(m_planeBottom);
    InstantiateActor(m_planeTop, L"plane.glb", L"Actor", Float3(0, 10, 0));
    m_planeTop->SetRotation(180, Float3(1, 0, 0));
    m_actors.push_back(m_planeTop);
    InstantiateActor(m_planeRight, L"plane.glb", L"Actor", Float3(5, 5, 0));
    m_planeRight->SetRotation(90, Float3(0, 0, 1));
    m_actors.push_back(m_planeRight);
    InstantiateActor(m_planeLeft, L"plane.glb", L"Actor", Float3(-5, 5, 0));
    m_planeLeft->SetRotation(-90, Float3(0, 0, 1));
    m_actors.push_back(m_planeLeft);
    InstantiateActor(m_planeFront, L"plane.glb", L"Actor", Float3(0, 5, -5));
    m_planeFront->SetRotation(90, Float3(1, 0, 0));
    m_actors.push_back(m_planeFront);
    InstantiateActor(m_planeBack, L"plane.glb", L"Actor", Float3(0, 5, 5));
    m_planeBack->SetRotation(-90, Float3(1, 0, 0));
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
