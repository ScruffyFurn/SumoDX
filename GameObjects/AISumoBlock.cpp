#include "pch.h"
#include "AISumoBlock.h"

using namespace DirectX;

AISumoBlock::AISumoBlock()
{
	Initialize(XMFLOAT3(0, 0, 0), nullptr);
	Behavior(GameConstants::Easy);
	m_delay = 2;
	m_choice = GameConstants::Walk;
}

AISumoBlock::AISumoBlock(DirectX::XMFLOAT3 position,SumoBlock^ target,GameConstants::Behavior aiBehavior)
{
	Initialize(position, target);
	Behavior(aiBehavior);
	m_delay = 2;
	m_choice = GameConstants::Walk;
}

void AISumoBlock::DetermineAIAction(float deltaTime)
{
	m_delay -= deltaTime;

	if (m_delay <= 0)
	{
		m_choice = static_cast<GameConstants::ManeuverState>(rand() % 3);

		//delay until next action
		m_delay = (rand() % 3) + 1.0f;
	}
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);
	XMVECTOR direction;
	switch (m_choice)
	{
	case GameConstants::Dodge:
		//dodge
		direction = XMVector3Cross(Target()->VectorPosition() - VectorPosition(), up);
		XMVectorSetIntY(direction, 0);
		Position(VectorPosition() + XMVector3Normalize(direction) * deltaTime * (m_behavior - 1));
		if (m_behavior != GameConstants::Angry)
		{
			break;
		}

	case GameConstants::Push:
		//push harder
		direction = ((Target()->VectorPosition() - VectorPosition()));
		XMVectorSetIntY(direction, 0);
		Position(VectorPosition() + XMVector3Normalize(direction) * deltaTime * m_behavior);
		break;

	default:
		//move forward normally
		direction = ((Target()->VectorPosition() - VectorPosition()));
		XMVectorSetIntY(direction, 0);
		Position(VectorPosition() + XMVector3Normalize(direction) * deltaTime);
		break;
	}

}

