#include "stdafx.h"
#include "InputHandler.h"

#include "MoveCommand.h"
#include "MoveLightCommand.h"
#include "ResetLightPositionCommand.h"
#include "RotateLightCommand.h"
#include "Core.h"

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
		this->key = key;
		if (isPressed(KEY_W)) keyW->execute();
		else if (isPressed(KEY_S)) keyS->execute();
		else if (isPressed(KEY_A)) keyA->execute();
		else if (isPressed(KEY_D)) keyD->execute();
}
