#pragma once

class Command;
class Core;

class InputHandler
{
		enum {
				KEY_W = 87,
				KEY_S = 83,
				KEY_A = 65,
				KEY_D = 68,

				KEY_I = 73,
				KEY_K = 75,
				KEY_J = 74,
				KEY_L = 76,

				KEY_U = 85,
				KEY_O = 79,
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

		Command* keyI;
		Command* keyK;
		Command* keyJ;
		Command* keyL;

		Command* keyU;
		Command* keyO;

		Core* m_core;
};

