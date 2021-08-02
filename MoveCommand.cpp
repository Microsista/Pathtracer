#include "stdafx.h"
#include "MoveCommand.h"
#include "InputHandler.h"
#include "Core.h"
#include "GameActor.h"

void MoveCommand::execute(GameActor& actor, float movementSpeed, float elapsedTime, int axis)
{
    switch (axis) {
    case 0:
        actor.Walk(movementSpeed * elapsedTime);
        break;
    case 1:
        actor.Strafe(movementSpeed * elapsedTime);
        break;
    case 2:
        actor.Fly(movementSpeed * elapsedTime);
        break;
    }
}
