#pragma once

#include "GameObject.h"
ref class SumoBlock : public GameObject
{
internal:
	SumoBlock();
	SumoBlock( DirectX::XMFLOAT3 position, SumoBlock^ target );
	void Initialize(DirectX::XMFLOAT3 position, SumoBlock^ target);
	void Target(SumoBlock^ target);
	SumoBlock^ Target();
protected:
	void UpdatePosition() override;
	
	 

private:
	DirectX::XMFLOAT4X4 m_rotationMatrix;
	SumoBlock^ m_target;
	float m_angle;

};


