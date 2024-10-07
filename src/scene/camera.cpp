#include "scene/camera.hpp"

Camera::Camera(float fovY, float aspect, float nearZ, float farZ, Float3 origin, Float3 target, Float3 up)
{
    UpdateSetting(fovY, aspect, nearZ, farZ);
    UpdateLookAt(origin, target, up);
}

Camera::~Camera()
{
}

/// <summary>
/// 旋回処理
/// </summary>
/// <param name="deltaTime"></param>
void Camera::Rotate(float deltaTime)
{
    float radius = 5.0f;
    float speed = 1.0f;
    float theta = deltaTime * speed * XM_2PI;
    float posX = radius * std::cosf(theta);
    float posZ = radius * std::sinf(theta);
    Float3 origin(posX, 0.0f, posZ);
    UpdateLookAt(origin, m_param.Target, m_param.Up);
}

/// <summary>
/// 移動アニメーション処理 (InCubic)
/// </summary>
void Camera::MoveAnimInCubic(float currentTime, float startTime, float endTime, Float3 startPos, Float3 endPos)
{
    float t = (currentTime - startTime) / (endTime - startTime);
    float s = EaseInCubic(t);
    Float3 pos = Lerp(startPos, endPos, t);
    SetPosition(pos);
}

/// <summary>
/// 移動アニメーション処理 (InOutCubic)
/// </summary>
void Camera::MoveAnimInOutCubic(float currentTime, float startTime, float endTime, Float3 startPos, Float3 endPos)
{
    float t = (currentTime - startTime) / (endTime - startTime);
    float s = EaseInOutCubic(t);
    Float3 pos = Lerp(startPos, endPos, t);
    SetPosition(pos);
}

/// <summary>
/// 移動アニメーション処理 (OutCubic)
/// </summary>
void Camera::MoveAnimOutCubic(float currentTime, float startTime, float endTime, Float3 startPos, Float3 endPos)
{
    float t = (currentTime - startTime) / (endTime - startTime);
    float s = EaseOutCubic(t);
    Float3 pos = Lerp(startPos, endPos, t);
    SetPosition(pos);
}

void Camera::ChangeFovYInCubic(float currentTime, float startTime, float endTime, float startFovY, float endFovY)
{
    float t = (currentTime - startTime) / (endTime - startTime);
    float s = EaseInCubic(t);
    float fovY = std::lerp(startFovY, endFovY, t);
    SetFovY(fovY);
}

void Camera::SetPosition(Float3 origin)
{
    m_param.Origin = origin;
    m_viewMtx = XMMatrixLookAtRH(
        XMLoadFloat3(&origin),
        XMLoadFloat3(&m_param.Target),
        XMLoadFloat3(&m_param.Up)
    );
}

void Camera::SetTarget(Float3 target)
{
    m_param.Target = target;
    m_viewMtx = XMMatrixLookAtRH(
        XMLoadFloat3(&m_param.Origin),
        XMLoadFloat3(&target),
        XMLoadFloat3(&m_param.Up)
    );
}

void Camera::SetFovY(float fovY)
{
    UpdateSetting(fovY, m_param.Aspect, m_param.NearZ, m_param.FarZ);
}

/// <summary>
/// カメラ方向の更新
/// </summary>
/// <param name="origin"></param>
/// <param name="target"></param>
/// <param name="up"></param>
void Camera::UpdateLookAt(Float3 origin, Float3 target, Float3 up)
{
    m_param.Origin = origin;
    m_param.Target = target;
    m_param.Up = up;
    m_viewMtx = XMMatrixLookAtRH(
        XMLoadFloat3(&origin),
        XMLoadFloat3(&target),
        XMLoadFloat3(&up)
    );
}

/// <summary>
/// カメラ設定の更新
/// </summary>
/// <param name="fovY"></param>
/// <param name="aspect"></param>
/// <param name="nearZ"></param>
/// <param name="farZ"></param>
void Camera::UpdateSetting(float fovY, float aspect, float nearZ, float farZ)
{
    m_param.FovY = fovY;
    m_param.Aspect = aspect;
    m_param.NearZ = nearZ;
    m_param.FarZ = farZ;
    m_projMtx = XMMatrixPerspectiveFovRH(fovY, aspect, nearZ, farZ);
}
