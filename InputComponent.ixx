module;
#include <DirectXMath.h>
#include <Windows.h>
#include "RayTracingHlslCompat.hlsli"
export module InputComponent;

import Camera;
import Directions;
import StepTimer;
import ConstantBuffer;
import InitComponent;

using namespace DirectX;

export class InputComponent {
    Camera& camera;
    const StepTimer& timer;
    ConstantBuffer<SceneConstantBuffer>& sceneCB;
    bool& orbitalCamera;
    POINT& lastMousePosition;
    const InitComponent initcomponent;

public:
    InputComponent(
        Camera& camera,
        const StepTimer& timer,
        ConstantBuffer<SceneConstantBuffer>& sceneCB,
        bool& orbitalCamera,
        POINT& lastMousePosition,
        const InitComponent initcomponent
    ) :
        camera{ camera },
        timer{ timer },
        sceneCB{ sceneCB },
        orbitalCamera{ orbitalCamera },
        lastMousePosition{ lastMousePosition },
        initComponent{ initComponent }
    {}

    virtual void OnKeyDown(UINT8 key)
    {
        // rotation
        float elapsedTime = static_cast<float>(timer.GetElapsedSeconds());
        float secondsToRotateAround = 0.1f;
        float angleToRotateBy = -360.0f * (elapsedTime / secondsToRotateAround);
        const XMVECTOR& prevLightPosition = sceneCB->lightPosition;
        XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(angleToRotateBy));
        XMMATRIX rotateClockwise = XMMatrixRotationY(XMConvertToRadians(-angleToRotateBy));

        auto speed = 100.0f;
        auto movementSpeed = 100.0f;
        if (GetKeyState(VK_SHIFT))
            movementSpeed *= 5;
        switch (key)
        {
        case 'W': camera.Walk(movementSpeed * elapsedTime); break;
        case 'S': camera.Walk(-movementSpeed * elapsedTime); break;
        case 'A': camera.Strafe(-movementSpeed * elapsedTime); break;
        case 'D': camera.Strafe(movementSpeed * elapsedTime); break;
        case 'Q': sceneCB->lightPosition = XMVector3Transform(prevLightPosition, rotate); break;
        case 'E': sceneCB->lightPosition = XMVector3Transform(prevLightPosition, rotateClockwise); break;
        case 'I': sceneCB->lightPosition += speed * Directions::FORWARD * elapsedTime; break;
        case 'J': sceneCB->lightPosition += speed * Directions::LEFT * elapsedTime; break;
        case 'K': sceneCB->lightPosition += speed * Directions::BACKWARD * elapsedTime; break;
        case 'L': sceneCB->lightPosition += speed * Directions::RIGHT * elapsedTime; break;
        case 'U': sceneCB->lightPosition += speed * Directions::DOWN * elapsedTime; break;
        case 'O': sceneCB->lightPosition += speed * Directions::UP * elapsedTime;  break;
        case '1':
            XMFLOAT4 equal;
            XMStoreFloat4(&equal, XMVectorEqual(sceneCB->lightPosition, XMVECTOR{ 0.0f, 0.0f, 0.0f }));
            equal.x ? sceneCB->lightPosition = XMVECTOR{ 0.0f, 18.0f, -20.0f, 0.0f } : sceneCB->lightPosition = XMVECTOR{ 0.0f, 0.0f, 0.0f, 0.0f };
            break;
        case '2': orbitalCamera = !orbitalCamera; break;
        }
        camera.UpdateViewMatrix();
        initComponent->UpdateCameraMatrices();
    }

    virtual void OnMouseMove(int x, int y) 
    {
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - lastMousePosition.x));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(y - lastMousePosition.y));

        camera.RotateY(dx);
        camera.Pitch(dy);


        camera.UpdateViewMatrix();
        initComponent->UpdateCameraMatrices();

        lastMousePosition.x = x;
        lastMousePosition.y = y;
    }

    void OnLeftButtonDown(UINT x, UINT y) {
        lastMousePosition = { static_cast<int>(x), static_cast<int>(y) };
    }
};