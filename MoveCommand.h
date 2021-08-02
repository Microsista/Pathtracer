#pragma once

#include "Command.h"

class MoveCommand : public Command
{
public:
		MoveCommand(InputHandler* inputHandler) : Command{ inputHandler } {

		}
private:
		virtual void execute(float movementSpeed, float elapsedTime) override;
};

