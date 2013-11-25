#pragma once

#include "GameObject.h"
ref class SumoBlock : public GameObject
{
internal:
	SumoBlock();
	SumoBlock(
		DirectX::XMFLOAT3 position,
		SumoBlock^ target
		);

	void SetPosition(DirectX::XMFLOAT3 position);

	void SetTarget(SumoBlock^ target);

protected:
	virtual void UpdatePosition() override;

	 

private:
	DirectX::XMFLOAT4X4 m_rotationMatrix;
	SumoBlock^ m_target;

};


