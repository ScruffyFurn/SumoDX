#pragma once

#include "SumoBlock.h"
#include "GameConstants.h"

ref class AISumoBlock : public SumoBlock
{
internal:
	AISumoBlock();
	AISumoBlock(DirectX::XMFLOAT3 position,	SumoBlock^ target,	GameConstants::Behavior aiBehavior);

	void Behavior(GameConstants::Behavior newBehavior);
	void DetermineAIAction(float deltaTime);

private:
	
	GameConstants::Behavior m_behavior;
	GameConstants::ManeuverState m_choice;
	float m_delay;
	
};

__forceinline void AISumoBlock::Behavior(GameConstants::Behavior newBehavior)
{
	m_behavior = newBehavior;
}

