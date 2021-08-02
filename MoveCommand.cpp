#include "stdafx.h"
#include "MoveCommand.h"
#include "InputHandler.h"
#include "Core.h"

void MoveCommand::execute(float movementSpeed, float elapsedTime)
{
    m_inputHandler->GetCore()->GetCamera()->Walk(movementSpeed * elapsedTime);
}
