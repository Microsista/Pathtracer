#pragma once

#include "Command.h"

class RotateLightCommand : public Command
{
public:
		RotateLightCommand(InputHandler* inputHandler) : Command{ inputHandler } {

		}
private:
		virtual void execute() override {}
};

