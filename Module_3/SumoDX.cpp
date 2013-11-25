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

    // Create a sphere primitive to represent the player.
    // The sphere will be used to handle collisions and constrain the player in the world.
    // It is not rendered so it is not added to the list of render objects.
    // It is added to the object list so it will be included in intersection calculations.
   // m_player = ref new Sphere(XMFLOAT3(0.0f, -1.3f, 4.0f), 0.2f);
	m_player = ref new SumoBlock();
	m_player->SetPosition(XMFLOAT3(10, 10, 100));
    m_objects.push_back(m_player);
	m_renderObjects.push_back(m_player);
	
   // m_player->Active(true);

    m_camera = ref new Camera;
    m_camera->SetProjParams(XM_PI / 2, 1.0f, 0.01f, 100.0f);
    m_camera->SetViewParams(
       // m_player->Position(),  // Eye point in world coordinates.
		XMFLOAT3(0,-0,-10),
		m_player->Position(),// XMFLOAT3 (0.0f, 0.0f, 0.0f),     // Look at point in world coordinates.
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

    m_camera->SetViewParams(
        m_player->Position(),            // Eye point in world coordinates.
        XMFLOAT3 (0.0f, 0.7f, 0.0f),     // Look at point in world coordinates.
        XMFLOAT3 (0.0f, 1.0f, 0.0f)      // The Up vector for the camera.
        );

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

	m_camera->Eye(XMFLOAT3(0, -10, 0));
	m_camera->LookDirection(m_controller->LookDirection());


	m_controller->Pitch(m_camera->Pitch());
	m_controller->Yaw(m_camera->Yaw());

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
    }
    else
    {
       
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

void SumoDX::UpdateDynamics()
{
    float timeTotal = m_timer->PlayingTime();
    float timeFrame = m_timer->DeltaTime();
    
	/*


#pragma region Animate Objects
    // Walk the list of objects looking for any objects that have an animation associated with it.
    // Update the position of the object based on evaluating the animation object with the current time.
    // Once the current time (timeTotal) is past the end of the animation time remove
    // the animation object since it is no longer needed.
    for (uint32 i = 0; i < m_objects.size(); i++)
    {
        if (m_objects[i]->AnimatePosition())
        {
            m_objects[i]->Position(m_objects[i]->AnimatePosition()->Evaluate(timeTotal));
            if (m_objects[i]->AnimatePosition()->IsFinished(timeTotal))
            {
                m_objects[i]->AnimatePosition(nullptr);
            }
        }
    }
#pragma endregion

    // If the elapsed time is too long, we slice up the time and handle physics over several
    // smaller time steps to avoid missing collisions.
    float timeLeft = timeFrame;
    float elapsedFrameTime;
    while (timeLeft > 0.0f)
    {
        elapsedFrameTime = min(timeLeft, GameConstants::Physics::FrameLength);
        timeLeft -= elapsedFrameTime;

        // Update the player position.
        m_player->Position(m_player->VectorPosition() + m_player->VectorVelocity() * elapsedFrameTime);

        // Do m_player / object intersections.
        for (uint32 a = 0; a < m_objects.size(); a++)
        {
            if (m_objects[a]->Active() && m_objects[a] != m_player)
            {
                XMFLOAT3 contact;
                XMFLOAT3 normal;

                if (m_objects[a]->IsTouching(m_player->Position(), m_player->Radius(), &contact, &normal))
                {
                    // Player is in contact with m_objects[a].
                    XMVECTOR oneToTwo;
                    oneToTwo = -XMLoadFloat3(&normal);

                    float impact = XMVectorGetX(
                        XMVector3Dot (oneToTwo, m_player->VectorVelocity())
                        );
                    // Make sure that the player is actually headed towards the object. At grazing angles there
                    // could appear to be an impact when the player is actually already hit and moving away.
                    if (impact > 0.0f)
                    {
                        // Compute the normal and tangential components of the player's velocity.
                        XMVECTOR velocityOneNormal = XMVector3Dot(oneToTwo, m_player->VectorVelocity()) * oneToTwo;
                        XMVECTOR velocityOneTangent = m_player->VectorVelocity() - velocityOneNormal;

                        // Compute post-collision velocity.
                        m_player->Velocity(velocityOneTangent - velocityOneNormal);

                        // Fix the positions so that the player is just touching the object.
                        float distanceToMove = m_player->Radius();
                        m_player->Position(XMLoadFloat3(&contact) - (oneToTwo * distanceToMove));
                    }
                }
            }
        }
        {
            // Do collision detection of the player with the bounding world.
            XMFLOAT3 position = m_player->Position();
            XMFLOAT3 velocity = m_player->Velocity();
            float radius = m_player->Radius();

            // Check for player collisions with the walls floor or ceiling and adjust the position.
            float limit = m_minBound.x + radius;
            if (position.x < limit)
            {
                position.x = limit;
                velocity.x = -velocity.x * GameConstants::Physics::GroundRestitution;
            }
            limit = m_maxBound.x - radius;
            if (position.x > limit)
            {
                position.x = limit;
                velocity.x = -velocity.x + GameConstants::Physics::GroundRestitution;
            }
            limit = m_minBound.y + radius;
            if (position.y < limit)
            {
                position.y = limit;
                velocity.y = -velocity.y * GameConstants::Physics::GroundRestitution;
            }
            limit = m_maxBound.y - radius;
            if (position.y > limit)
            {
                position.y = limit;
                velocity.y = -velocity.y * GameConstants::Physics::GroundRestitution;
            }
            limit = m_minBound.z + radius;
            if (position.z < limit)
            {
                position.z = limit;
                velocity.z = -velocity.z * GameConstants::Physics::GroundRestitution;
            }
            limit = m_maxBound.z - radius;
            if (position.z > limit)
            {
                position.z = limit;
                velocity.z = -velocity.z * GameConstants::Physics::GroundRestitution;
            }
            m_player->Position(position);
            m_player->Velocity(velocity);
        }

       
		 Apply Gravity and world intersection
            // Apply gravity and check for collision against enclosing volume.
            for (uint32 i = 0; i < m_ammoCount; i++)
            {
                // Update the position and velocity of the ammo instance.
                m_ammo[i]->Position(m_ammo[i]->VectorPosition() + m_ammo[i]->VectorVelocity() * elapsedFrameTime);

                XMFLOAT3 velocity = m_ammo[i]->Velocity();
                XMFLOAT3 position = m_ammo[i]->Position();

                velocity.x -= velocity.x * 0.1f * elapsedFrameTime;
                velocity.z -= velocity.z * 0.1f * elapsedFrameTime;
                if (!m_ammo[i]->OnGround())
                {
                    // Apply gravity if the ammo instance is not already resting on the ground.
                    velocity.y -= GameConstants::Physics::Gravity * elapsedFrameTime;
                }

                if (!m_ammo[i]->OnGround())
                {
                    float limit = m_minBound.y + GameConstants::AmmoRadius;
                    if (position.y < limit)
                    {
                        // The ammo instance hit the ground.
                        // Align the ammo instance to the ground, invert the Y component of the velocity and
                        // play the impact sound. The X and Z velocity components will be reduced
                        // because of friction.
                        position.y = limit;
                        m_ammo[i]->PlaySound(-velocity.y, m_player->Position());

                        velocity.y = -velocity.y * GameConstants::Physics::GroundRestitution;
                        velocity.x *= GameConstants::Physics::Friction;
                        velocity.z *= GameConstants::Physics::Friction;
                    }
                }
                else
                {
                    // The ammo instance is resting or rolling on ground.
                    // X and Z velocity components are reduced because of friction.
                    velocity.x *= GameConstants::Physics::Friction;
                    velocity.z *= GameConstants::Physics::Friction;
                }

                float limit = m_maxBound.y - GameConstants::AmmoRadius;
                if (position.y > limit)
                {
                    // The ammo instance hit the ceiling.
                    // Align the ammo instance to the ceiling, invert the Y component of the velocity and
                    // play the impact sound. The X and Z velocity components will be reduced
                    // because of friction.
                    position.y = limit;
                    m_ammo[i]->PlaySound(-velocity.y, m_player->Position());

                    velocity.y = -velocity.y * GameConstants::Physics::GroundRestitution;
                    velocity.x *= GameConstants::Physics::Friction;
                    velocity.z *= GameConstants::Physics::Friction;
                }

                limit = m_minBound.y + GameConstants::AmmoRadius;
                if ((GameConstants::Physics::Gravity * (position.y - limit) + 0.5f * velocity.y * velocity.y) < GameConstants::Physics::RestThreshold)
                {
                    // The Y velocity component is below the resting threshold so flag the instance as
                    // laying on the ground and set the Y velocity component to zero.
                    position.y = limit;
                    velocity.y = 0.0f;
                    m_ammo[i]->OnGround(true);
                }

                limit = m_minBound.z + GameConstants::AmmoRadius;
                if (position.z < limit)
                {
                    // The ammo instance hit the a wall in the min Z direction.
                    // Align the ammo instance to the wall, invert the Z component of the velocity and
                    // play the impact sound.
                    position.z = limit;
                    m_ammo[i]->PlaySound(-velocity.z, m_player->Position());
                    velocity.z = -velocity.z * GameConstants::Physics::GroundRestitution;
                }

                limit = m_maxBound.z - GameConstants::AmmoRadius;
                if (position.z > limit)
                {
                    // The ammo instance hit the a wall in the max Z direction.
                    // Align the ammo instance to the wall, invert the Z component of the velocity and
                    // play the impact sound.
                    position.z = limit;
                    m_ammo[i]->PlaySound(-velocity.z, m_player->Position());
                    velocity.z = -velocity.z * GameConstants::Physics::GroundRestitution;
                }

                limit = m_minBound.x + GameConstants::AmmoRadius;
                if (position.x < limit)
                {
                    // The ammo instance hit the a wall in the min X direction.
                    // Align the ammo instance to the wall, invert the X component of the velocity and
                    // play the impact sound.
                    position.x = limit;
                    m_ammo[i]->PlaySound(-velocity.x, m_player->Position());
                    velocity.x = -velocity.x * GameConstants::Physics::GroundRestitution;
                }

                limit = m_maxBound.x - GameConstants::AmmoRadius;
                if (position.x > limit)
                {
                    // The ammo instance hit the a wall in the max X direction.
                    // Align the ammo instance to the wall, invert the X component of the velocity and
                    // play the impact sound.
                    position.x = limit;
                    m_ammo[i]->PlaySound(-velocity.x, m_player->Position());
                    velocity.x = -velocity.x * GameConstants::Physics::GroundRestitution;
                }

                // Save the updated velocity and position for the ammo instance.
                m_ammo[i]->Velocity(velocity);
                m_ammo[i]->Position(position);
            }
        }
    }
#pragma endregion
	*/
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
        m_player->Position(
            m_savedState->LoadXMFLOAT3(":PlayerPosition", XMFLOAT3(0.0f, 0.0f, 0.0f))
            );
        m_camera->Eye(m_player->Position());
        m_camera->LookDirection(
            m_savedState->LoadXMFLOAT3(":PlayerLookDirection", XMFLOAT3(0.0f, 0.0f, 1.0f))
            );
        m_controller->Pitch(m_camera->Pitch());
        m_controller->Yaw(m_camera->Yaw());
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


