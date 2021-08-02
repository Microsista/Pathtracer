#pragma once

class InputHandler;
class GameActor;

// Command design pattern - base class, represents a triggerable game command.
class Command
{
public:
		Command(InputHandler* inputHandler) : m_inputHandler{ inputHandler } {}
		virtual ~Command() {};
		virtual void execute(GameActor& actor, float movementSpeed, float elapsedTime, int axis) = 0;

protected:
		InputHandler* m_inputHandler;
};

