#pragma once

class InputHandler;

// Command design pattern - base class, represents a triggerable game command.
class Command
{
public:
		Command(InputHandler* inputHandler) : m_inputHandler{ inputHandler } {}
		virtual ~Command() {};
		virtual void execute() = 0;

protected:
		InputHandler* m_inputHandler;
};

