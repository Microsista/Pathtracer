#pragma once

#include "Command.h"

class ResetLightPositionCommand : public Command
{
public:
		ResetLightPositionCommand(InputHandler* inputHandler) : Command{ inputHandler } {

		}
private:
		virtual void execute(float movementSpeed, float elapsedTime, bool strafe) override {}
};

