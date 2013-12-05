//// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//// PARTICULAR PURPOSE.
////
//// Copyright (c) Microsoft Corporation. All rights reserved

#include "pch.h"
#include "../Rendering/GameRenderer.h"
#include <time.h>
#include "../Utilities/DirectXSample.h"
#include "../GameObjects/Cylinder.h"

using namespace concurrency;
using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Windows::Storage;
using namespace Windows::UI::Core;

//----------------------------------------------------------------------

SumoDX::SumoDX():
    
    m_gameActive(false)
{
    m_topScore.bestRoundTime = 0;
}

//----------------------------------------------------------------------

void SumoDX::Initialize(
    _In_ MoveLookController^ controller,
    _In_ GameRenderer^ renderer
    )
{
    // This method is expected to be called as an asynchronous task.
    // Care should be taken to not call rendering methods on the
    // m_renderer as this would result in the D3D Context being
    // used in multiple threads, which is not allowed.

    m_controller = controller;
    m_renderer = renderer;

    m_renderObjects = std::vector<GameObject^>();
    

    m_savedState = ref new PersistentState();
    m_savedState->Initialize(ApplicationData::Current->LocalSettings->Values, "SumoGame");

    m_timer = ref new GameTimer();
	srand(time(NULL));

    // Create a box primitive to represent the player.
    // The box will be used to handle collisions and constrain the player in the world.
	m_player = ref new SumoBlock();
	m_player->Position(XMFLOAT3(-3.0f, 0.5f, 0.0f));
	// It is added to the list of render objects so that it appears on screen.
	m_renderObjects.push_back(m_player);

	//Create the enemy
	// The box will be used to handle collisions and constrain the player in the world.
	m_enemy = ref new AISumoBlock(XMFLOAT3(3.0f, 0.5f, 0.0f), m_player, static_cast<GameConstants::Behavior>(rand() % 3));
	// It is added to the list of render objects so that it appears on screen.
	m_renderObjects.push_back(m_enemy);

	//tell the player about their new opponent
	m_player->Target(m_enemy);

	//floor model
	Cylinder^ cylinder;
	cylinder = ref new Cylinder(XMFLOAT3(0.0f, -1.0f, 0.0f), 10.0f, XMFLOAT3(0.0f, 1.0f, 0.0f));
	m_renderObjects.push_back(cylinder);

    m_camera = ref new Camera;
    m_camera->SetProjParams(XM_PI / 2, 1.0f, 0.01f, 100.0f);
    m_camera->SetViewParams(
		XMFLOAT3(0,7,10),// Eye point in world coordinates.
		XMFLOAT3(0.0f, -5.0f, 0.0f),// Look at point in world coordinates.
		XMFLOAT3 (0.0f, 1.0f, 0.0f)      // The Up vector for the camera.
        );

    m_controller->Pitch(m_camera->Pitch());
    m_controller->Yaw(m_camera->Yaw());

    // Load the top score from disk if it exists.
    LoadHighScore();

    // Load the currentScore for saved state if it exists.
    LoadState();

    m_controller->Active(false);
}

//----------------------------------------------------------------------

void SumoDX::LoadGame()
{
	//reset player and enemy
	m_player->Position(XMFLOAT3(-3.0f, 0.5f, 0.0f));
	m_enemy->Position(XMFLOAT3(3.0f, 0.5f, 0.0f));
	m_enemy->Behavior(static_cast<GameConstants::Behavior>(rand() % 3));

	//reset camera
	m_camera->SetViewParams(
		XMFLOAT3(0, 7, 10),// Eye point in world coordinates.
		XMFLOAT3(0.0f, -5.0f, 0.0f),// Look at point in world coordinates.
		XMFLOAT3(0.0f, 1.0f, 0.0f)      // The Up vector for the camera.
		);
    m_controller->Pitch(m_camera->Pitch());
    m_controller->Yaw(m_camera->Yaw());
   
	//reset other things
	m_gameActive = false;
    m_timer->Reset();

	//reset the save game
	SaveState();
}


//----------------------------------------------------------------------


void SumoDX::StartLevel()
{
    m_timer->Reset();
    m_timer->Start();
	m_gameActive = true;
    m_controller->Active(true);
}

//----------------------------------------------------------------------

void SumoDX::PauseGame()
{
    m_timer->Stop();
    SaveState();
}

//----------------------------------------------------------------------

void SumoDX::ContinueGame()
{
    m_timer->Start();
    m_controller->Active(true);
}

//----------------------------------------------------------------------

GameState SumoDX::RunGame()
{
    // This method is called to execute a single time interval for active game play.
    // It returns the resulting state of game play after the interval has been executed.
    m_timer->Update();

	//update the camera to look where the user has specified.
	m_camera->LookDirection(m_controller->LookDirection());
	m_controller->Pitch(m_camera->Pitch());
	m_controller->Yaw(m_camera->Yaw());

	// run one frame of game play.
	m_player->Velocity(m_controller->Velocity());

	UpdateDynamics();

	//did either leave the mat?
	if (abs(XMVectorGetY(XMVector3Length(m_player->VectorPosition()))) > 10.0f)
	{
		//player lost, place the ringout time into the score variable but don't save.
		m_topScore.bestRoundTime = m_timer->PlayingTime();
		return GameState::PlayerLost;
	}
	if (abs(XMVectorGetY(XMVector3Length(m_enemy->VectorPosition()))) > 10.0f)
	{
		//player won, save his time if it is a new high score.
		m_topScore.bestRoundTime = m_timer->PlayingTime();
		SaveHighScore();
		return GameState::GameComplete;
	}

    return GameState::Active;
}

//----------------------------------------------------------------------

void SumoDX::OnSuspending()
{
   
}

//----------------------------------------------------------------------

void SumoDX::OnResuming()
{
  
}

//----------------------------------------------------------------------

void SumoDX::UpdateDynamics()
{
    float timeTotal = m_timer->PlayingTime();
    float timeFrame = m_timer->DeltaTime();
    
	// If the elapsed time is too long, we slice up the time and handle physics over several
	// smaller time steps to avoid missing collisions.
	float timeLeft = timeFrame;
	float deltaTime;
	while (timeLeft > 0.0f)
	{
		deltaTime = min(timeLeft, GameConstants::Physics::FrameLength);
		timeLeft -= deltaTime;

		// Update the player position.
		m_player->Position(m_player->VectorPosition() + m_player->VectorVelocity() * deltaTime);

		//AI Update
		m_enemy->DetermineAIAction(deltaTime);

		//Check for player/enemy colision
		float xDelta = m_enemy->Position().x - m_player->Position().x;
		float zDelta = m_enemy->Position().z - m_player->Position().z;

		//since each of our sumo's is 1 unit wide if we subtract one from their position deltas
		float overlap = sqrt(xDelta * xDelta + zDelta * zDelta) - 1;
		XMVECTOR playerToEnemy = m_enemy->VectorPosition() - m_player->VectorPosition();

		
		// then if we get their overlap value as a negative number a contact has occured.
		if (overlap < 0)
		{
			m_player->Position(m_player->VectorPosition() + playerToEnemy * overlap * 0.5f);
			m_enemy->Position(m_enemy->VectorPosition() - playerToEnemy * overlap * 0.5f);
		}
	}
}

//----------------------------------------------------------------------

void SumoDX::SaveState()
{
    // Save basic state of the game.
    m_savedState->SaveBool(":GameActive", m_gameActive);
	m_savedState->SaveSingle(":LevelPlayingTime", m_timer->PlayingTime());
    m_savedState->SaveXMFLOAT3(":PlayerPosition", m_player->Position());
    m_savedState->SaveXMFLOAT3(":EnemyPosition", m_enemy->Position());

 }

//----------------------------------------------------------------------

void SumoDX::LoadState()
{
    m_gameActive = m_savedState->LoadBool(":GameActive", m_gameActive);

    if (m_gameActive)
    {
        // Loading from the last known state means the game wasn't finished when it was last played,
        // Reload the current player and enemy position.
		m_player->Position(m_savedState->LoadXMFLOAT3(":PlayerPosition", XMFLOAT3(0.0f, 0.0f, 0.0f)));
	    m_enemy->Position(m_savedState->LoadXMFLOAT3(":EnemyPosition", XMFLOAT3(0.0f, 0.0f, 0.0f)));
		m_timer->PlayingTime(m_savedState->LoadSingle(":LevelPlayingTime", 0.0f));
    }
}

//----------------------------------------------------------------------

void SumoDX::SaveHighScore()
{
	int currentBest = m_savedState->LoadSingle(":HighScore:LevelCompleted", 0);

	if (currentBest==NULL || currentBest > m_topScore.bestRoundTime)
	{
		m_savedState->LoadSingle(":HighScore:LevelCompleted", m_topScore.bestRoundTime);
	}
}

//----------------------------------------------------------------------

void SumoDX::LoadHighScore()
{
	m_topScore.bestRoundTime = m_savedState->LoadSingle(":HighScore:LevelCompleted", 0);
}

//----------------------------------------------------------------------


