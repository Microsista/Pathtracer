module;
#include <DirectXMath.h>
#include "RayTracingHlslCompat.hlsli"
export module CameraComponent;

import DeviceResources;
import ConstantBuffer;
import Camera;

using namespace DirectX;

export class CameraComponent {
    DeviceResources* deviceResources;
    ConstantBuffer<SceneConstantBuffer>* sceneCB;
    bool orbitalCamera;
    XMVECTOR* eye;
    XMVECTOR* at;
    XMVECTOR* up;
    float aspectRatio;
    XMMATRIX* projectionToWorld;
    XMFLOAT3* cameraPosition;
    Camera* camera;

public:
    CameraComponent(
        DeviceResources* deviceResources,
        ConstantBuffer<SceneConstantBuffer>* sceneCB,
        bool orbitalCamera,
        XMVECTOR* eye,
        XMVECTOR* at,
        XMVECTOR* up,
        float aspectRatio,
        XMMATRIX* projectionToWorld,
        XMFLOAT3* cameraPosition,
        Camera* camera
    )
        :
        deviceResources{ deviceResources },
        sceneCB{ sceneCB },
        orbitalCamera{ orbitalCamera },
        eye{ eye },
        at{ at },
        up{ up },
        aspectRatio{ aspectRatio },
        projectionToWorld{ projectionToWorld },
        cameraPosition{ cameraPosition },
        camera{ camera }
    {}

    void UpdateCameraMatrices() {
        auto frameIndex = deviceResources->GetCurrentFrameIndex();

        if (orbitalCamera) {
            (*sceneCB)->cameraPosition = *eye;

            float fovAngleY = 45.0f;

            XMMATRIX view = XMMatrixLookAtLH(*eye, *at, *up);
            XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(fovAngleY), aspectRatio, 0.01f, 125.0f);
            XMMATRIX viewProj = view * proj;
            (*sceneCB)->projectionToWorld = XMMatrixInverse(nullptr, viewProj);
        }
        else {
            (*sceneCB)->cameraPosition = camera->GetPosition();
            float fovAngleY = 45.0f;
            XMMATRIX view = camera->GetView();
            XMMATRIX proj = camera->GetProj();
            XMMATRIX viewProj = view * proj;
            (*sceneCB)->projectionToWorld = XMMatrixInverse(nullptr, XMMatrixMultiply(camera->GetView(), camera->GetProj()));
        }
    }
};