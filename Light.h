#pragma once

#include "GameActor.h"

class Light : public GameActor
{
public:
		void Strafe(float d) override;
		void Walk(float d) override;
		void Fly(float d) override;

		DirectX::XMVECTOR GetPosition() const override {
				DirectX::XMFLOAT4 pos = { mPosition.x, mPosition.y, mPosition.z, 1.0f };
				return XMLoadFloat4(&pos);
		}

		bool isCamera() const override {
				return false;
		}

private:
		DirectX::XMFLOAT3 mPosition = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 mRight = { 1.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 mUp = { 0.0f, 1.0f, 0.0f };
		DirectX::XMFLOAT3 mLook = { 0.0f, 0.0f, 1.0f };

		bool mViewDirty = true;
};

