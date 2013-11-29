//// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//// PARTICULAR PURPOSE.
////
//// Copyright (c) Microsoft Corporation. All rights reserved

#include "pch.h"
#include "../Rendering/GameRenderer.h"

#include "../Utilities/DirectXSample.h"

#include "../GameObjects/Animate.h"

#include "../Audio/MediaReader.h"
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
    //m_topScore.totalHits = 0;
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

    m_audioController = ref new Audio;
    m_audioController->CreateDeviceIndependentResources();

   
    m_objects = std::vector<GameObject^>();
    m_renderObjects = std::vector<GameObject^>();
    

    m_savedState = ref new PersistentState();
    m_savedState->Initialize(ApplicationData::Current->LocalSettings->Values, "Game");

    m_timer = ref new GameTimer();

    // Create a box primitive to represent the player.
    // The box will be used to handle collisions and constrain the player in the world.
	m_player = ref new SumoBlock();
	m_player->Position(XMFLOAT3(-3.0f, 0.5f, 0.0f));
	// It is added to the object list so it will be included in intersection calculations.
    m_objects.push_back(m_player);
	// It is added to the list of render objects so that it appears on screen.
	m_renderObjects.push_back(m_player);

	//Create the enemy
	// The box will be used to handle collisions and constrain the player in the world.
	m_enemy = ref new SumoBlock();
	m_enemy->Position(XMFLOAT3(3.0f, 0.5f, 0.0f));
	// It is added to the object list so it will be included in intersection calculations.
	m_objects.push_back(m_enemy);
	// It is added to the list of render objects so that it appears on screen.
	m_renderObjects.push_back(m_enemy);

	//tell the sumo's about their opponent
	m_enemy->SetTarget(m_player);
	m_player->SetTarget(m_enemy);

	//set starting difficulty
	m_difficulty = 2;// rand() % 3;
	m_delay = 2.0f;
	m_choice = 0;

	//floor model
	Cylinder^ cylinder;
	cylinder = ref new Cylinder(XMFLOAT3(0.0f, -1.0f, 0.0f), 10.0f, XMFLOAT3(0.0f, 1.0f, 0.0f));
	//cylinder->Active(true);
	m_objects.push_back(cylinder);
	m_renderObjects.push_back(cylinder);

    m_camera = ref new Camera;
    m_camera->SetProjParams(XM_PI / 2, 1.0f, 0.01f, 100.0f);
    m_camera->SetViewParams(
		XMFLOAT3(0,5,10),// Eye point in world coordinates.
		//m_player->Position(), // Look at point in world coordinates.
		XMFLOAT3(0.0f, 0.0f, 0.0f),
		XMFLOAT3 (0.0f, 1.0f, 0.0f)      // The Up vector for the camera.
        );

    m_controller->Pitch(m_camera->Pitch());
    m_controller->Yaw(m_camera->Yaw());

    // Min and max Bound are defining the world space of the game.
    // All camera motion and dynamics are confined to this space.
    m_minBound = XMFLOAT3(-4.0f, -3.0f, -6.0f);
    m_maxBound = XMFLOAT3(4.0f, 3.0f, 6.0f);

    MediaReader^ mediaReader = ref new MediaReader;
    auto targetHitSound = mediaReader->LoadMedia("/Resources/hit.wav");

    // Instantiate a set of spheres to be used as ammunition for the game
    // and set the material properties of the spheres.
    auto ammoHitSound = mediaReader->LoadMedia("/Resources/bounce.wav");

    // Load the top score from disk if it exists.
    LoadHighScore();

    // Load the currentScore for saved state if it exists.
    LoadState();

    m_controller->Active(false);
}

//----------------------------------------------------------------------

void SumoDX::LoadGame()
{
   // m_player->Position(XMFLOAT3 (0.0f, -1.3f, 4.0f));
	/*
    m_camera->SetViewParams(
        m_player->Position(),            // Eye point in world coordinates.
        XMFLOAT3 (0.0f, 0.7f, 0.0f),     // Look at point in world coordinates.
        XMFLOAT3 (0.0f, 1.0f, 0.0f)      // The Up vector for the camera.
        );
		*/
    m_controller->Pitch(m_camera->Pitch());
    m_controller->Yaw(m_camera->Yaw());
   
	m_levelDuration = 0;

  
    m_gameActive = false;
  
    m_timer->Reset();
}





void SumoDX::StartLevel()
{
    m_timer->Reset();
    m_timer->Start();
   
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

	//TODO: Remove debug camera
	m_camera->LookDirection(m_controller->LookDirection());
	m_controller->Pitch(m_camera->Pitch());
	m_controller->Yaw(m_camera->Yaw());

	// run one frame of game play.
	m_player->Velocity(m_controller->Velocity());
	//m_camera->LookDirection(m_controller->LookDirection());

	UpdateDynamics();

	//did either leave the mat?
	if (abs(XMVectorGetY(XMVector3Length(m_player->VectorPosition()))) > 10.0f)
	{
		return GameState::TimeExpired;
	}
	if (abs(XMVectorGetY(XMVector3Length(m_enemy->VectorPosition()))) > 10.0f)
	{
		return GameState::GameComplete;
	}

	/*
    
        if (m_totalHits > m_topScore.totalHits)
        {
            // There is a new high score so save it.
            m_topScore.totalHits = m_totalHits;
            m_topScore.totalShots = m_totalShots;
            m_topScore.levelCompleted = m_currentLevel;

            SaveHighScore();
        }
        return GameState::TimeExpired;
       
	*/
    return GameState::Active;
}

//----------------------------------------------------------------------

void SumoDX::OnSuspending()
{
    m_audioController->SuspendAudio();
}

//----------------------------------------------------------------------

void SumoDX::OnResuming()
{
    m_audioController->ResumeAudio();
}

//----------------------------------------------------------------------
void SumoDX::DetermineAIActions(float deltaTime)
{
	m_delay -= deltaTime;

	if (m_delay <= 0)
	{
		m_choice = rand() % 3;

		//delay until next action
		m_delay = (rand() % 3) + 1.0f;
	}
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);
	XMVECTOR direction;
	switch (m_choice)
	{
	case 1:
		//push harder
		direction = ((m_player->VectorPosition() - m_enemy->VectorPosition()));
		XMVectorSetIntY(direction, 0);
		m_enemy->Position(m_enemy->VectorPosition() + XMVector3Normalize(direction) * deltaTime * m_difficulty);
		break;
	case 2:
		//dodge
		direction = XMVector3Cross(m_player->VectorPosition() - m_enemy->VectorPosition(), up);
		XMVectorSetIntY(direction, 0);
		m_enemy->Position(m_enemy->VectorPosition() + XMVector3Normalize(direction) * deltaTime * (m_difficulty-1));
		

		break;
	default:
		//move forward normally
		direction = ((m_player->VectorPosition() - m_enemy->VectorPosition()));
		XMVectorSetIntY(direction, 0);
		m_enemy->Position(m_enemy->VectorPosition() + XMVector3Normalize(direction) * deltaTime);

		break;
	}
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
		DetermineAIActions(deltaTime);

		//Check for player/enemy colision
		float xDelta = m_enemy->Position().x - m_player->Position().x;
		float zDelta = m_enemy->Position().z - m_player->Position().z;
		float overlap = sqrt(xDelta * xDelta + zDelta * zDelta) - 1;
		XMVECTOR playerToEnemy = m_enemy->VectorPosition() - m_player->VectorPosition();

		//since each of our sumo's is 1 unit wide if we subtract one from their position deltas
		// then we get their overlap value as a negative number and a possitive number means no contact.
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
   
    m_savedState->SaveXMFLOAT3(":PlayerPosition", m_player->Position());
    m_savedState->SaveXMFLOAT3(":PlayerLookDirection", m_controller->LookDirection());

 }

//----------------------------------------------------------------------

void SumoDX::LoadState()
{
    m_gameActive = m_savedState->LoadBool(":GameActive", m_gameActive);
   

    if (m_gameActive)
    {
        // Loading from the last known state means the game wasn't finished when it was last played,
        // so set the current level.

       

        // Reload the current player position and set both the camera and the controller
        // with the current Look Direction.
       // m_player->Position(m_savedState->LoadXMFLOAT3(":PlayerPosition", XMFLOAT3(0.0f, 0.0f, 0.0f)));
       // m_camera->Eye(m_player->Position());
       // m_camera->LookDirection(m_savedState->LoadXMFLOAT3(":PlayerLookDirection", XMFLOAT3(0.0f, 0.0f, 1.0f)) );
       // m_controller->Pitch(m_camera->Pitch());
        //m_controller->Yaw(m_camera->Yaw());
    }
    else
    {
        
    }
}

//----------------------------------------------------------------------

void SumoDX::SetCurrentLevelToSavedState()
{
    if (m_gameActive)
    {
       
    }
}

//----------------------------------------------------------------------

void SumoDX::SaveHighScore()
{
    m_savedState->SaveInt32(":HighScore:LevelCompleted", m_topScore.bestRoundTime);

}

//----------------------------------------------------------------------

void SumoDX::LoadHighScore()
{
    m_topScore.bestRoundTime = m_savedState->LoadInt32(":HighScore:LevelCompleted", 0);
}

//----------------------------------------------------------------------


