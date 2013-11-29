#include "pch.h"
#include "SumoBlock.h"

using namespace DirectX;

SumoBlock::SumoBlock()
{
	Initialize(XMFLOAT3(0, 0, 0), nullptr);

}

SumoBlock::SumoBlock(
	DirectX::XMFLOAT3 position,
	SumoBlock^ target
	)
{
	Initialize(position, target);
}
void SumoBlock::Initialize(DirectX::XMFLOAT3 position, SumoBlock^ target)
{
	m_target = target;
	if (target != nullptr)
	{
		angle = XMVectorGetY(XMVector3AngleBetweenNormals(XMVector3Normalize(m_target->VectorPosition() - VectorPosition()), XMVector3Normalize(XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f))));
	}
	Position(position);
	XMMATRIX mat1 = XMMatrixIdentity();
	XMStoreFloat4x4(&m_rotationMatrix, mat1);
}


void SumoBlock::SetTarget(SumoBlock^ target)
{
	m_target = target;
}

void SumoBlock::UpdatePosition()
{
	if (m_target != nullptr)
	{
		//TODO: fix the jitter.
		//face the target
		angle += XMVectorGetY(XMVector3AngleBetweenNormals(XMVector3Normalize(m_target->VectorPosition() - VectorPosition()), XMVector3Normalize(XMVector3Transform(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), XMMatrixRotationY(angle)))));
		if (angle < -XM_PI*2.0f)
		{
			angle += XM_PI*2.0f;
		}
		if (angle > XM_PI*2.0f)
		{
			angle -= XM_PI*2.0f;
		}

		XMStoreFloat4x4(&m_modelMatrix, XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixRotationY(angle) * XMMatrixTranslation(m_position.x, m_position.y, m_position.z));
	}
	else
	{
		XMStoreFloat4x4(&m_modelMatrix, XMMatrixScaling(1.0f, 1.0f, 1.0f) *	XMLoadFloat4x4(&m_rotationMatrix) *	XMMatrixTranslation(m_position.x, m_position.y, m_position.z) );
	}


}