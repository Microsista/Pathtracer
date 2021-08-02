#pragma once

class Command;
class Core;

class InputHandler
{
		enum {
				KEY_W = 87,
				KEY_S = 83,
				KEY_A = 65,
				KEY_D = 68
		};

public:
		InputHandler(Core* core);

		~InputHandler();

		CommandPack* handleInput(unsigned char key);

		bool isPressed(unsigned char key) {
				return key == this->key ? 1 : 0;
		}

		Core* GetCore() {
				return m_core;
		}

private:
		unsigned char key;

		Command* keyW;
		Command* keyS;
		Command* keyA;
		Command* keyD;

		Core* m_core;
};

