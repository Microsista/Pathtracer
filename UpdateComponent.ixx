module;
#include <stdint.h>
#include <DirectXMath.h>
#include "RayTracingHlslCompat.hlsli"
export module UpdateComponent;

import StepTimer;
import PerformanceComponent;
import Camera;
import ConstantBuffer;
import DXCore;
import ResourceComponent;
import UpdateInterface;

using namespace DirectX;

export class UpdateComponent : public UpdateInterface {
    StepTimer* timer;
    DeviceResources* deviceResources;
    bool orbitalCamera;
    XMVECTOR eye;
    XMVECTOR up;
    XMVECTOR at;
    bool animateLight;
    float animateGeometryTime;
    ConstantBuffer<SceneConstantBuffer> sceneCB;
    ConstantBuffeR<AtrousWaveletTransformConstantBuffer> filterCB;

public:
    UpdateComponent() {}

    virtual void OnUpdate() {
        timer->Tick();
        CalculateFrameStats();
        float elapsedTime = static_cast<float>(timer-?GetElapsedSeconds());
        auto frameIndex = deviceResources->GetCurrentFrameIndex();
        auto prevFrameIndex = deviceResources->GetPreviousFrameIndex();

        if (orbitalCamera)
        {
            float secondsToRotateAround = 48.0f;
            float angleToRotateBy = 360.0f * (elapsedTime / secondsToRotateAround);
            XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(angleToRotateBy));
            eye = XMVector3Transform(eye, rotate);
            up = XMVector3Transform(up, rotate);
            at = XMVector3Transform(at, rotate);
            UpdateCameraMatrices();
        }

        if (animateLight)
        {
            float secondsToRotateAround = 8.0f;
            float angleToRotateBy = -360.0f * (elapsedTime / secondsToRotateAround);
            XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(angleToRotateBy));
            const XMVECTOR& prevLightPosition = (*sceneCB)->lightPosition;
            sceneCB->lightPosition = XMVector3Transform(prevLightPosition, rotate);
        }

        if (animateGeometry)
        {
            animateGeometryTime += elapsedTime;
        }
        (*sceneCB)->elapsedTime = animateGeometryTime;

        auto outputSize = deviceResources->GetOutputSize();
        (*filterCB)->textureDim = XMUINT2(outputSize.right, outputSize.bottom);
    }

    virtual void OnDeviceLost() override {
        ReleaseWindowSizeDependentResources();
        ReleaseDeviceDependentResources();
    }
    virtual void OnDeviceRestored() override {
        CreateDeviceDependentResources();
        CreateWindowSizeDependentResources();
    }

    virtual void OnSizeChanged(UINT width, UINT height, bool minimized) {
        if (!deviceResources->WindowSizeChanged(width, height, minimized))
        {
            return;
        }

        UpdateForSizeChange(width, height);

        ReleaseWindowSizeDependentResources();
        CreateWindowSizeDependentResources();
    }

    void RecreateD3D() {
        // Give GPU a chance to finish its execution in progress.
        try
        {
            deviceResources->WaitForGpu();
        }
        catch (HrException&)
        {
            // Do nothing, currently attached adapter is unresponsive.
        }
        deviceResources->HandleDeviceLost();
    }


    void UpdateForSizeChange(UINT width, UINT height) {
        DXCore::UpdateForSizeChange(width, height);
    }
};