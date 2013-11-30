#include "pch.h"
#include "SumoBlock.h"

using namespace DirectX;

SumoBlock::SumoBlock()
{
	Initialize(XMFLOAT3(0, 0, 0), nullptr);

}

SumoBlock::SumoBlock(DirectX::XMFLOAT3 position, SumoBlock^ target)
{
	Initialize(position, target);
}

void SumoBlock::Initialize(DirectX::XMFLOAT3 position, SumoBlock^ target)
{
	m_target = target;
	if (target != nullptr)
	{
		m_angle = XMVectorGetY(XMVector3AngleBetweenNormals(XMVector3Normalize(m_target->VectorPosition() - VectorPosition()), XMVector3Normalize(XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f))));
	}
	Position(position);
	XMMATRIX mat1 = XMMatrixIdentity();
	XMStoreFloat4x4(&m_rotationMatrix, mat1);
}


void SumoBlock::Target(SumoBlock^ target)
{
	m_target = target;
}

SumoBlock^ SumoBlock::Target()
{
	return m_target;
}

void SumoBlock::UpdatePosition()
{
	if (m_target != nullptr)
	{
		//Face the target.
		XMVECTOR direction = XMVector3Normalize(m_target->VectorPosition() - VectorPosition());
		float ans = XMVectorGetY(XMVector2AngleBetweenNormals(direction, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f)));
		m_angle = ans;

		//if you are in the upper arc
		if (XMVectorGetZ(direction) > 0)
		{
			m_angle *= -1;
		}

		XMStoreFloat4x4(&m_modelMatrix, XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixRotationY(m_angle) * XMMatrixTranslation(m_position.x, m_position.y, m_position.z));
	}
	else
	{
		XMStoreFloat4x4(&m_modelMatrix, XMMatrixScaling(1.0f, 1.0f, 1.0f) *	XMLoadFloat4x4(&m_rotationMatrix) *	XMMatrixTranslation(m_position.x, m_position.y, m_position.z) );
	}


}