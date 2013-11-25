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
//     MoveLookController - for handling all input to control player movement, aiming,
//         and firing.
//     GameRenderer - for handling all graphics presentation.
//     Camera - for handling view projections.
//     Audio - for handling sound output.
// This class maintains several lists of objects:
//     m_ammo <Sphere> - is the list of the balls used to throw at targets.  SumoDX
//         cycles through the list in a LRU fashion each time a ball is thrown by the player.
//     m_objects <GameObject> - is the list of all objects in the scene that participate in
//         game physics.  This includes m_player <Sphere> to represent the player in the scene.
//         The player object (m_player) is not visible in the scene so it is not rendered.
//     m_renderObjects <GameObject> - is the list of all objects in the scene that may be
//         rendered.  It includes both the m_ammo list, most of the m_objects list excluding m_player
//         object and the objects representing the bounding world.

#include "../GameObjects/GameConstants.h"
#include "../Audio/Audio.h"
#include "../GameObjects/Camera.h"
#include "../GameObjects/GameObject.h"
#include "../Utilities/GameTimer.h"
#include "../Input/MoveLookController.h"
#include "../Utilities/PersistentState.h"
#include "../Rendering/GameRenderer.h"

#include "../GameObjects/SumoBlock.h"

//--------------------------------------------------------------------------------------

enum class GameState
{

	Waiting,
	Active,
	LevelComplete,
	TimeExpired,
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
    void SetCurrentLevelToSavedState();

    void OnSuspending();
    void OnResuming();

    bool IsActivePlay()                         { return m_timer->Active(); }
    
    bool GameActive()                           { return m_gameActive; };
    
    HighScoreEntry HighScore()                  { return m_topScore; };
  
    Camera^ GameCamera()                        { return m_camera; };
    std::vector<GameObject^> RenderObjects()    { return m_renderObjects; };

private:
    void LoadState();
    void SaveState();
    void SaveHighScore();
    void LoadHighScore();
 
    void UpdateDynamics();

    MoveLookController^                         m_controller;
    GameRenderer^                               m_renderer;
    Camera^                                     m_camera;

    Audio^                                      m_audioController;

  

    HighScoreEntry                              m_topScore;
    PersistentState^                            m_savedState;

    GameTimer^                                  m_timer;
    bool                                        m_gameActive;
   
    float                                       m_levelDuration;
   
    
 
    

    SumoBlock^                                  m_player;
    std::vector<GameObject^>                    m_objects;           // List of all objects to be included in intersection calculations.
    std::vector<GameObject^>                    m_renderObjects;     // List of all objects to be rendered.

    DirectX::XMFLOAT3                           m_minBound;
    DirectX::XMFLOAT3                           m_maxBound;
};

