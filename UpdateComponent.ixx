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
    const StepTimer& timer;
    const shared_ptr<DeviceResourcesInterface>& deviceResources;
    const bool& orbitalCamera;
    const XMVECTOR& eye;
    const XMVECTOR& up;
    const XMVECTOR& at;
    const bool& animateLight;
    const bool& animateGeometry;
    const float& animateGeometryTime;
    const ConstantBuffer<SceneConstantBuffer>& sceneCB;
    const ConstantBuffer<AtrousWaveletTransformFilterConstantBuffer>& filterCB;

    const PerformanceComponent*& performanceComponent;
    const CameraComponent*& cameraComponent;
    const ResourceComponent*& resourceComponent;

    const DXCoreInterface*& dxCore;

public:
    UpdateComponent(
        const StepTimer& timer,
        const shared_ptr<DeviceResourcesInterface>& deviceResources,
        const bool& orbitalCamera,
        const XMVECTOR& eye,
        const XMVECTOR& up,
        const XMVECTOR& at,
        const bool& animateLight,
        const bool& animateGeometry,
        const float& animateGeometryTime,
        const ConstantBuffer<SceneConstantBuffer>& sceneCB,
        const ConstantBuffer<AtrousWaveletTransformFilterConstantBuffer>& filterCB,
        const PerformanceComponent*& performanceComponent,
        const CameraComponent*& cameraComponent,
        const ResourceComponent*& resourceComponent,
        const DXCoreInterface*& dxCore
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
        CalculateFrameStats();
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

    void CalculateFrameStats() {
        static int frameCnt = 0;
        static double prevTime = 0.0f;
        double totalTime = timer.GetTotalSeconds();

        frameCnt++;

        // Compute averages over one second period.
        if ((totalTime - prevTime) >= 1.0f)
        {
            float diff = static_cast<float>(totalTime - prevTime);
            float fps = static_cast<float>(frameCnt) / diff; // Normalize to an exact second.

            frameCnt = 0;
            prevTime = totalTime;
            float raytracingTime = static_cast<float>(gpuTimers[GpuTimers::Raytracing].GetElapsedMS());
            float MRaysPerSecond = NumMRaysPerSecond(width, height, raytracingTime);

            wstringstream windowText;
            windowText << setprecision(2) << fixed
                << L"    FPS: " << fps
                << L"    Raytracing time: " << raytracingTime << " ms"
                << L"    Ray throughput: " << MRaysPerSecond << " MRPS"
                << L"    GPU[" << deviceResources->GetAdapterID() << L"]: " << deviceResources->GetAdapterDescription();
            /*SetCustomWindowText(windowText.str().c_str(), hwnd);*/
        }
    }
};