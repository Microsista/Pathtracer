#pragma once

class Command;
class Core;

class InputHandler
{
		enum {
				KEY_W,
				KEY_S,
				KEY_A,
				KEY_D
		};

public:
		InputHandler(Core* core);

		~InputHandler();

		void handleInput(unsigned char key);

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

