#pragma once

#include "utils/math_util.h"

class Camera
{
public:
    struct CameraParam
    {
        float FovY;
        float Aspect;
        float NearZ;
        float FarZ;
        Float3 Origin;
        Float3 Target;
        Float3 Up;
    };

    Camera() = default;
    Camera(float fovY, float aspect, float nearZ, float farZ, Float3 origin, Float3 target, Float3 up = Float3(0.0f, 1.0f, 0.0f));
    ~Camera();

    void SetPosition(Float3 origin);
    void SetTarget(Float3 target);
    void SetFovY(float fovY);
    void Rotate(float deltaTime);
    // FIXME: 冗長コード
    void MoveAnimInCubic(float currentTime, float startTime, float endTime, Float3 startPos, Float3 endPos);
    void MoveAnimInOutCubic(float currentTime, float startTime, float endTime, Float3 startPos, Float3 endPos);
    void MoveAnimOutCubic(float currentTime, float startTime, float endTime, Float3 startPos, Float3 endPos);
    void ChangeFovYInCubic(float currentTime, float startTime, float endTime, float startFovY, float endFovY);

    Float3 GetPosition() const { return m_param.Origin; }
    Matrix GetViewMatrix() const { return m_viewMtx; }
    Matrix GetProjMatrix() const { return m_projMtx; }

    CameraParam GetParam() const { return m_param; }

private:
    void UpdateLookAt(Float3 origin, Float3 target, Float3 up = Float3(0.0f, 1.0f, 0.0f));
    void UpdateSetting(float fovY, float aspect, float nearZ, float Z);

private:
    CameraParam m_param;
    Matrix m_viewMtx;
    Matrix m_projMtx;
};
