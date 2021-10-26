module;
#include <DirectXMath.h>
#include "RayTracingHlslCompat.hlsli"
export module CameraComponent;

import DeviceResources;
import DeviceResourcesInterface;
import ConstantBuffer;
import Camera;

using namespace DirectX;

export class CameraComponent {
    DeviceResourcesInterface* deviceResources;
    ConstantBuffer<SceneConstantBuffer>& sceneCB;
    bool& orbitalCamera;
    XMVECTOR& eye;
    XMVECTOR& at;
    XMVECTOR& up;
    float& aspectRatio;
    Camera& camera;

public:
    CameraComponent(
        DeviceResourcesInterface* deviceResources,
        ConstantBuffer<SceneConstantBuffer>& sceneCB,
        bool& orbitalCamera,
        XMVECTOR& eye,
        XMVECTOR& at,
        XMVECTOR& up,
        float& aspectRatio,
        Camera& camera
    ) :
        deviceResources{ deviceResources },
        sceneCB{ sceneCB },
        orbitalCamera{ orbitalCamera },
        eye{ eye },
        at{ at },
        up{ up },
        aspectRatio{ aspectRatio },
        camera{ camera }
    {}

    void UpdateCameraMatrices() {
        auto frameIndex = deviceResources->GetCurrentFrameIndex();

        if (orbitalCamera) {
            sceneCB->cameraPosition = eye;

            float fovAngleY = 45.0f;

            XMMATRIX view = XMMatrixLookAtLH(eye, at, up);
            XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(fovAngleY), aspectRatio, 0.01f, 125.0f);
            XMMATRIX viewProj = view * proj;
            sceneCB->projectionToWorld = XMMatrixInverse(nullptr, viewProj);
        }
        else {
            sceneCB->cameraPosition = camera.GetPosition();
            float fovAngleY = 45.0f;
            XMMATRIX view = camera.GetView();
            XMMATRIX proj = camera.GetProj();
            XMMATRIX viewProj = view * proj;
            sceneCB->projectionToWorld = XMMatrixInverse(nullptr, XMMatrixMultiply(camera.GetView(), camera.GetProj()));
        }
    }
};