#include "stdafx.h"
#include "MoveCommand.h"
#include "InputHandler.h"
#include "Core.h"
#include "GameActor.h"

void MoveCommand::execute(GameActor& actor, float movementSpeed, float elapsedTime, bool strafe)
{
    strafe ? actor/*m_inputHandler->GetCore()->GetCamera()*/.Strafe(movementSpeed * elapsedTime) :
    actor/* m_inputHandler->GetCore()->GetCamera()*/.Walk(movementSpeed * elapsedTime);
}
