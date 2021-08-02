#include "stdafx.h"
#include "InputHandler.h"

#include "MoveCommand.h"
#include "MoveLightCommand.h"
#include "ResetLightPositionCommand.h"
#include "RotateLightCommand.h"
#include "Core.h"
#include "RayTracingHlslCompat.h"
#include "DXSampleHelper.h"

InputHandler::InputHandler(Core* core) : m_core{ core }, keyW{ new MoveCommand(this) }, keyS{ new MoveCommand(this) }, keyA{ new MoveCommand(this) },
keyD{ new MoveCommand(this) }, key{}
{
}

InputHandler::~InputHandler() {
		delete keyW;
		delete keyS;
		delete keyA;
		delete keyD;
}

void InputHandler::handleInput(unsigned char key)
{
    // rotation
    float elapsedTime = static_cast<float>(m_core->GetTimer()->GetElapsedSeconds());
    float secondsToRotateAround = 0.1f;
    float angleToRotateBy = -360.0f * (elapsedTime / secondsToRotateAround);
    //const XMVECTOR& prevLightPosition = m_core->GetSceneCB()->lightPosition;
    XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(angleToRotateBy));
    XMMATRIX rotateClockwise = XMMatrixRotationY(XMConvertToRadians(-angleToRotateBy));

    auto speed = 100.0f;
    auto movementSpeed = 100.0f;
    if (GetKeyState(VK_SHIFT))
        movementSpeed *= 5;

    this->key = key;
    if (isPressed(KEY_W)) keyW->execute(movementSpeed, elapsedTime);
    else if (isPressed(KEY_S)) keyS->execute(movementSpeed, elapsedTime);
    else if (isPressed(KEY_A)) keyA->execute(movementSpeed, elapsedTime);
    else if (isPressed(KEY_D)) keyD->execute(movementSpeed, elapsedTime);



   /* switch (key)
    {
    case 'W': m_camera.Walk(movementSpeed * elapsedTime); break;
    case 'S': m_camera.Walk(-movementSpeed * elapsedTime); break;
    case 'A': m_camera.Strafe(-movementSpeed * elapsedTime); break;
    case 'D': m_camera.Strafe(movementSpeed * elapsedTime); break;
    case 'Q': m_sceneCB->lightPosition = XMVector3Transform(prevLightPosition, rotate); break;
    case 'E': m_sceneCB->lightPosition = XMVector3Transform(prevLightPosition, rotateClockwise); break;
    case 'I': m_sceneCB->lightPosition += speed * Directions::FORWARD * elapsedTime; break;
    case 'J': m_sceneCB->lightPosition += speed * Directions::LEFT * elapsedTime; break;
    case 'K': m_sceneCB->lightPosition += speed * Directions::BACKWARD * elapsedTime; break;
    case 'L': m_sceneCB->lightPosition += speed * Directions::RIGHT * elapsedTime; break;
    case 'U': m_sceneCB->lightPosition += speed * Directions::DOWN * elapsedTime; break;
    case 'O': m_sceneCB->lightPosition += speed * Directions::UP * elapsedTime;  break;
    case '1':
        XMFLOAT4 equal;
        XMStoreFloat4(&equal, XMVectorEqual(m_sceneCB->lightPosition, XMVECTOR{ 0.0f, 0.0f, 0.0f }));
        equal.x ? m_sceneCB->lightPosition = XMVECTOR{ 0.0f, 18.0f, -20.0f, 0.0f } : m_sceneCB->lightPosition = XMVECTOR{ 0.0f, 0.0f, 0.0f, 0.0f };
        break;
    case '2': m_orbitalCamera = !m_orbitalCamera; break;
    }*/
    m_core->GetCamera()->UpdateViewMatrix();
    m_core->UpdateCameraMatrices();

		
}
