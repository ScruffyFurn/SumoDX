//// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//// PARTICULAR PURPOSE.
////
//// Copyright (c) Microsoft Corporation. All rights reserved

#pragma once

// SumoDX:
// This is the main game class.  It controls game play logic and game state.
// Some of the key object classes used by SumoDX are:
//     MoveLookController - for handling all input to control player/camera/cursor movement.
//     GameRenderer - for handling all graphics presentation.
//     Camera - for handling view projections.
//     m_renderObjects <GameObject> - is the list of all objects in the scene that may be rendered.

#include "../GameObjects/GameConstants.h"
#include "../GameObjects/Camera.h"
#include "../GameObjects/GameObject.h"
#include "../Utilities/GameTimer.h"
#include "../Input/MoveLookController.h"
#include "../Utilities/PersistentState.h"
#include "../Rendering/GameRenderer.h"
#include "../GameObjects/AISumoBlock.h"
#include "../GameObjects/SumoBlock.h"

//--------------------------------------------------------------------------------------

enum class GameState
{

	Waiting,
	Active,
	PlayerLost,
	GameComplete,
};

typedef struct
{
    Platform::String^ tag;
    float bestRoundTime;
} HighScoreEntry;

typedef std::vector<HighScoreEntry> HighScoreEntries;

//--------------------------------------------------------------------------------------

ref class GameRenderer;

ref class SumoDX
{
internal:
    SumoDX();

    void Initialize(
        _In_ MoveLookController^ controller,
        _In_ GameRenderer^ renderer
        );

    void LoadGame();
  
  
    void StartLevel();
    void PauseGame();
    void ContinueGame();
    GameState RunGame();

    void OnSuspending();
    void OnResuming();

    bool IsActivePlay()                         { return m_timer->Active(); }
	int RoundTime()								{ return m_timer->PlayingTime(); }
    
    bool GameActive()                           { return m_gameActive; }
    
    HighScoreEntry HighScore()                  { return m_topScore; }
  
    Camera^ GameCamera()                        { return m_camera; }
	std::vector<GameObject^> RenderObjects()    { return m_renderObjects; }


private:
    void LoadState();
    void SaveState();
    void SaveHighScore();
    void LoadHighScore();
 
    void UpdateDynamics();

    MoveLookController^                         m_controller;
    GameRenderer^                               m_renderer;
    Camera^                                     m_camera;

    HighScoreEntry                              m_topScore;
    PersistentState^                            m_savedState;

    GameTimer^                                  m_timer;
    bool                                        m_gameActive;

    SumoBlock^                                  m_player;
	AISumoBlock^								m_enemy;
    std::vector<GameObject^>                    m_renderObjects;     // List of all objects to be rendered.
};

