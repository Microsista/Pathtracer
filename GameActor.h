#pragma once
class GameActor
{
public:
		virtual void Strafe(float speed) = 0;
		virtual void Walk(float speed) = 0;
		virtual void Fly(float speed) {};

		virtual void RotateY(float dx) {};
		virtual void Pitch(float dy) {};
		virtual void SetPosition(float x, float y, float z) {};
		virtual void SetLens(float fovY, float aspect, float zn, float zf) {}

		virtual void UpdateViewMatrix() {}
		virtual DirectX::XMVECTOR GetPosition() const { return DirectX::XMVECTOR(); }
		virtual DirectX::XMMATRIX GetView() const { return DirectX::XMMATRIX(); }
		virtual DirectX::XMMATRIX GetProj() const { return DirectX::XMMATRIX(); }
		
};

