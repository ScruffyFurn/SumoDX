#include "pch.h"
#include "SumoBlock.h"

using namespace DirectX;

SumoBlock::SumoBlock()
{
}

SumoBlock::SumoBlock(
	DirectX::XMFLOAT3 position,
	SumoBlock^ target
	)
{
	m_position = position;
	m_target = target;

	UpdatePosition();
}

void SumoBlock::SetPosition(DirectX::XMFLOAT3 position)
{
	m_position = position;
}

void SumoBlock::SetTarget(SumoBlock^ target)
{
	m_target = target;
}

void SumoBlock::UpdatePosition()
{
	XMStoreFloat4x4(
		&m_modelMatrix,
		XMMatrixScaling(1.0f, 1.0f, 1.0f) *
		XMLoadFloat4x4(&m_rotationMatrix) *
		XMMatrixTranslation(m_position.x, m_position.y, m_position.z)
		);
}