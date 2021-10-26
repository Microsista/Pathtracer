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
import DeviceResourcesInterface;
import CameraComponent;
import DXSampleHelper;
import DXCoreInterface;

using namespace DirectX;

export class UpdateComponent : public UpdateInterface {
    StepTimer& timer;
    DeviceResourcesInterface* deviceResources;
    bool& orbitalCamera;
    XMVECTOR& eye;
    XMVECTOR& up;
    XMVECTOR& at;
    bool& animateLight;
    bool& animateGeometry;
    float& animateGeometryTime;
    ConstantBuffer<SceneConstantBuffer>& sceneCB;
    ConstantBuffer<AtrousWaveletTransformFilterConstantBuffer>& filterCB;

    PerformanceComponent* performanceComponent;
    CameraComponent* cameraComponent;
    ResourceComponent* resourceComponent;

    DXCoreInterface* dxCore;

public:
    UpdateComponent(
        StepTimer& timer,
        DeviceResourcesInterface* deviceResources,
        bool& orbitalCamera,
        XMVECTOR& eye,
        XMVECTOR& up,
        XMVECTOR& at,
        bool& animateLight,
        bool& animateGeometry,
        float& animateGeometryTime,
        ConstantBuffer<SceneConstantBuffer>& sceneCB,
        ConstantBuffer<AtrousWaveletTransformFilterConstantBuffer>& filterCB,
        PerformanceComponent* performanceComponent,
        CameraComponent* cameraComponent,
        ResourceComponent* resourceComponent,
        DXCoreInterface* dxCore
    ) :
        timer{ timer },
        deviceResources{ deviceResources },
        orbitalCamera{ orbitalCamera },
        eye{ eye },
        up{ up },
        at{ at },
        animateLight{ animateLight },
        animateGeometry{ animateGeometry },
        animateGeometryTime{ animateGeometryTime },
        sceneCB{ sceneCB },
        filterCB{ filterCB },
        performanceComponent{ performanceComponent },
        cameraComponent{ cameraComponent },
        resourceComponent{ resourceComponent },
        dxCore{ dxCore }
    {}

    virtual void OnUpdate() {
        timer.Tick();
        performanceComponent->CalculateFrameStats();
        float elapsedTime = static_cast<float>(timer.GetElapsedSeconds());
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
            cameraComponent->UpdateCameraMatrices();
        }

        if (animateLight)
        {
            float secondsToRotateAround = 8.0f;
            float angleToRotateBy = -360.0f * (elapsedTime / secondsToRotateAround);
            XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(angleToRotateBy));
            const XMVECTOR& prevLightPosition = sceneCB->lightPosition;
            sceneCB->lightPosition = XMVector3Transform(prevLightPosition, rotate);
        }

        if (animateGeometry)
        {
            animateGeometryTime += elapsedTime;
        }
        sceneCB->elapsedTime = animateGeometryTime;

        auto outputSize = deviceResources->GetOutputSize();
        filterCB->textureDim = XMUINT2(outputSize.right, outputSize.bottom);
    }

    virtual void OnDeviceLost() override {
        resourceComponent->ReleaseWindowSizeDependentResources();
        resourceComponent->ReleaseDeviceDependentResources();
    }
    virtual void OnDeviceRestored() override {
        resourceComponent->CreateDeviceDependentResources();
        resourceComponent->CreateWindowSizeDependentResources();
    }

    virtual void OnSizeChanged(UINT width, UINT height, bool minimized) {
        if (!deviceResources->WindowSizeChanged(width, height, minimized))
        {
            return;
        }

        UpdateForSizeChange(width, height);

        resourceComponent->ReleaseWindowSizeDependentResources();
        resourceComponent->CreateWindowSizeDependentResources();
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
        dxCore->UpdateForSizeChange(width, height);
    }
};