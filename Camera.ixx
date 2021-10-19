module;
#include <DirectXMath.h>
export module Camera;

import Math;

using namespace DirectX;

export class Camera
{
public:
	Camera()
	{
		SetLens(0.25f * XM_PI, 1.0f, 1.0f, 1000.0f);
	}
	~Camera()
	{
	}

	// Get/Set world camera position.
	DirectX::XMVECTOR GetPosition() const
	{
		return XMLoadFloat3(&mPosition);
	}
	DirectX::XMFLOAT3 GetPosition3f() const
	{
		return mPosition;
	}
	void SetPosition(float x, float y, float z)
	{
		mPosition = XMFLOAT3(x, y, z);
		mViewDirty = true;
	}
	void SetPosition(const DirectX::XMFLOAT3& v)
	{
		mPosition = v;
		mViewDirty = true;
	}

	// Get camera basis vectors.
	DirectX::XMVECTOR GetRight() const
	{
		return XMLoadFloat3(&mRight);
	}
	DirectX::XMFLOAT3 GetRight3f() const
	{
		return mRight;
	}
	DirectX::XMVECTOR GetUp() const
	{
		return XMLoadFloat3(&mUp);
	}
	DirectX::XMFLOAT3 GetUp3f() const
	{
		return mUp;
	}
	DirectX::XMVECTOR GetLook() const
	{
		return XMLoadFloat3(&mLook);
	}
	DirectX::XMFLOAT3 GetLook3f() const
	{
		return mLook;
	}

	// Get frustum properties.
	float GetNearZ() const
	{
		return mNearZ;
	}
	float GetFarZ() const
	{
		return mFarZ;
	}
	float GetAspect() const
	{
		return mAspect;
	}
	float GetFovY() const
	{
		return mFovY;
	}
	float GetFovX() const
	{
		float halfWidth = 0.5f * GetNearWindowWidth();
		return 2.0f * atan(halfWidth / mNearZ);
	}

	// Get near and far plane dimensions in view space coordinates.
	float GetNearWindowWidth() const
	{
		return mAspect * mNearWindowHeight;
	}
	float GetNearWindowHeight() const
	{
		return mNearWindowHeight;
	}
	float GetFarWindowWidth() const
	{
		return mAspect * mFarWindowHeight;
	}
	float GetFarWindowHeight() const
	{
		return mFarWindowHeight;
	}

	// Set frustum.
	void SetLens(float fovY, float aspect, float zn, float zf)
	{
		// Cache properties.
		mFovY = fovY;
		mAspect = aspect;
		mNearZ = zn;
		mFarZ = zf;

		mNearWindowHeight = 2.0f * mNearZ * tanf(0.5f * mFovY);
		mFarWindowHeight = 2.0f * mFarZ * tanf(0.5f * mFovY);

		XMMATRIX P = XMMatrixPerspectiveFovLH(mFovY, mAspect, mNearZ, mFarZ);
		XMStoreFloat4x4(&mProj, P);
	}

	// Define camera space via LookAt parameters.
	void LookAt(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR worldUp)
	{
		XMVECTOR L = XMVector3Normalize(XMVectorSubtract(target, pos));
		XMVECTOR R = XMVector3Normalize(XMVector3Cross(worldUp, L));
		XMVECTOR U = XMVector3Cross(L, R);

		XMStoreFloat3(&mPosition, pos);
		XMStoreFloat3(&mLook, L);
		XMStoreFloat3(&mRight, R);
		XMStoreFloat3(&mUp, U);

		//mViewDirty = true;
	}
	void LookAt(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& up)
	{
		XMVECTOR P = XMLoadFloat3(&pos);
		XMVECTOR T = XMLoadFloat3(&target);
		XMVECTOR U = XMLoadFloat3(&up);

		LookAt(P, T, U);

		mViewDirty = true;
	}

	// Get View/Proj matrices.
	DirectX::XMMATRIX GetView() const
	{
		//assert(!mViewDirty);
		return XMLoadFloat4x4(&mView);
	}
	DirectX::XMMATRIX GetProj() const
	{
		return XMLoadFloat4x4(&mProj);
	}

	DirectX::XMFLOAT4X4 GetView4x4f() const
	{
		//assert(!mViewDirty);
		return mView;
	}
	DirectX::XMFLOAT4X4 GetProj4x4f() const
	{
		return mProj;
	}

	// Strafe/Walk the camera a distance d.
	void Strafe(float d)
	{
		// mPosition += d*mRight
		XMVECTOR s = XMVectorReplicate(d);
		XMVECTOR r = XMLoadFloat3(&mRight);
		XMVECTOR p = XMLoadFloat3(&mPosition);
		XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, r, p));

		mViewDirty = true;
	}
	void Walk(float d)
	{
		// mPosition += d*mLook
		XMVECTOR s = XMVectorReplicate(d);
		XMVECTOR l = XMLoadFloat3(&mLook);
		XMVECTOR p = XMLoadFloat3(&mPosition);
		XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, l, p));

		mViewDirty = true;
	}

	// Rotate the camera.
	void Pitch(float angle)
	{
		// Rotate up and look vector about the right vector.

		XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&mRight), angle);

		XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
		XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

		mViewDirty = true;
	}
	void RotateY(float angle)
	{
		// Rotate the basis vectors about the world y-axis.

		XMMATRIX R = XMMatrixRotationY(angle);

		XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), R));
		XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
		XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

		mViewDirty = true;
	}
	void Roll(float angle)
	{
		// Rotate up and look vector about the right vector.

		XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&mLook), angle);

		XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
		XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), R));

		mViewDirty = true;
	}

	// After modifying camera position/orientation, call to rebuild the view matrix.
	void UpdateViewMatrix()
	{
		if (mViewDirty)
		{
			XMVECTOR R = XMLoadFloat3(&mRight);
			XMVECTOR U = XMLoadFloat3(&mUp);
			XMVECTOR L = XMLoadFloat3(&mLook);
			XMVECTOR P = XMLoadFloat3(&mPosition);

			// Keep camera's axes orthogonal to each other and of unit length.
			L = XMVector3Normalize(L);
			U = XMVector3Normalize(XMVector3Cross(L, R));

			// U, L already ortho-normal, so no need to normalize cross product.
			R = XMVector3Cross(U, L);

			// Fill in the view matrix entries.
			float x = -XMVectorGetX(XMVector3Dot(P, R));
			float y = -XMVectorGetX(XMVector3Dot(P, U));
			float z = -XMVectorGetX(XMVector3Dot(P, L));

			XMStoreFloat3(&mRight, R);
			XMStoreFloat3(&mUp, U);
			XMStoreFloat3(&mLook, L);

			mView(0, 0) = mRight.x;
			mView(1, 0) = mRight.y;
			mView(2, 0) = mRight.z;
			mView(3, 0) = x;

			mView(0, 1) = mUp.x;
			mView(1, 1) = mUp.y;
			mView(2, 1) = mUp.z;
			mView(3, 1) = y;

			mView(0, 2) = mLook.x;
			mView(1, 2) = mLook.y;
			mView(2, 2) = mLook.z;
			mView(3, 2) = z;

			mView(0, 3) = 0.0f;
			mView(1, 3) = 0.0f;
			mView(2, 3) = 0.0f;
			mView(3, 3) = 1.0f;

			mViewDirty = false;
		}
	}

private:
	// Camera coordinate system with coordinates relative to world space.
	DirectX::XMFLOAT3 mPosition = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 mRight = { 1.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 mUp = { 0.0f, 1.0f, 0.0f };
	DirectX::XMFLOAT3 mLook = { 0.0f, 0.0f, 1.0f };

	// Cache frustum properties.
	float mNearZ = 0.0f;
	float mFarZ = 0.0f;
	float mAspect = 0.0f;
	float mFovY = 0.0f;
	float mNearWindowHeight = 0.0f;
	float mFarWindowHeight = 0.0f;

	bool mViewDirty = true;

	// Cache View/Proj matrices.
	DirectX::XMFLOAT4X4 mView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mProj = MathHelper::Identity4x4();
};