#pragma once

#include "Command.h"

class MoveLightCommand : public Command
{
public:
		MoveLightCommand(InputHandler* inputHandler) : Command( inputHandler ) {

		}
private:
		virtual void execute() override {}
};

