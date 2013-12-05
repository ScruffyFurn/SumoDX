//// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//// PARTICULAR PURPOSE.
////
//// Copyright (c) Microsoft Corporation. All rights reserved

#include "pch.h"
#include "DirectXApp.h"

using namespace concurrency;
using namespace DirectX;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::UI::ViewManagement;

[Platform::MTAThread]
int main(Platform::Array<Platform::String^>^)
{
	auto directXAppSource = ref new DirectXAppSource();
	CoreApplication::Run(directXAppSource);
	return 0;
}

//--------------------------------------------------------------------------------------

IFrameworkView^ DirectXAppSource::CreateView()
{
	return ref new DirectXApp();
}

//--------------------------------------------------------------------------------------

DirectXApp::DirectXApp() :
    m_windowClosed(false),
    m_haveFocus(false),
    m_gameInfoOverlayCommand(GameInfoOverlayCommand::None),
    m_visible(true),
    m_loadingCount(0),
    m_updateState(UpdateEngineState::WaitingForResources)
{
}

//--------------------------------------------------------------------------------------

void DirectXApp::Initialize(
    _In_ CoreApplicationView^ applicationView
    )
{
    applicationView->Activated +=
        ref new TypedEventHandler<CoreApplicationView^, IActivatedEventArgs^>(this, &DirectXApp::OnActivated);

    CoreApplication::Suspending +=
        ref new EventHandler<SuspendingEventArgs^>(this, &DirectXApp::OnSuspending);

    CoreApplication::Resuming +=
        ref new EventHandler<Platform::Object^>(this, &DirectXApp::OnResuming);

	//The three main classes in our game!
	m_game = ref new SumoDX();
    m_renderer = ref new GameRenderer();
	m_controller = ref new MoveLookController();
}

//--------------------------------------------------------------------------------------

void DirectXApp::OnActivated(
	_In_ CoreApplicationView^ /* applicationView */,
	_In_ IActivatedEventArgs^ /* args */
	)
{
	CoreWindow::GetForCurrentThread()->Activated +=
		ref new TypedEventHandler<CoreWindow^, WindowActivatedEventArgs^>(this, &DirectXApp::OnWindowActivationChanged);
	CoreWindow::GetForCurrentThread()->Activate();
}

//--------------------------------------------------------------------------------------

void DirectXApp::OnWindowActivationChanged(
	_In_ Windows::UI::Core::CoreWindow^ /* sender */,
	_In_ Windows::UI::Core::WindowActivatedEventArgs^ args
	)
{
	if (args->WindowActivationState == CoreWindowActivationState::Deactivated)
	{
		m_haveFocus = false;

		switch (m_updateState)
		{
		case UpdateEngineState::Dynamics:
			// From Dynamic mode, when coming out of Deactivated rather than going directly back into game play
			// go to the paused state waiting for user input to continue
			m_updateStateNext = UpdateEngineState::WaitingForPress;
			m_pressResult = PressResultState::Continue;
			SetGameInfoOverlay(GameInfoOverlayState::Pause);
			ShowGameInfoOverlay();
			m_game->PauseGame();
			m_updateState = UpdateEngineState::Deactivated;
			SetAction(GameInfoOverlayCommand::None);
			m_renderNeeded = true;
			break;

		case UpdateEngineState::WaitingForResources:
		case UpdateEngineState::WaitingForPress:
			m_updateStateNext = m_updateState;
			m_updateState = UpdateEngineState::Deactivated;
			SetAction(GameInfoOverlayCommand::None);
			ShowGameInfoOverlay();
			m_renderNeeded = true;
			break;
		}
		// Don't have focus so shutdown input processing
		m_controller->Active(false);
	}
	else if (args->WindowActivationState == CoreWindowActivationState::CodeActivated
		|| args->WindowActivationState == CoreWindowActivationState::PointerActivated)
	{
		m_haveFocus = true;

		if (m_updateState == UpdateEngineState::Deactivated)
		{
			m_updateState = m_updateStateNext;

			if (m_updateState == UpdateEngineState::WaitingForPress)
			{
				SetAction(GameInfoOverlayCommand::TapToContinue);
				m_controller->WaitForPress(m_renderer->GameInfoOverlayUpperLeft(), m_renderer->GameInfoOverlayLowerRight());
			}
			else if (m_updateStateNext == UpdateEngineState::WaitingForResources)
			{
				SetAction(GameInfoOverlayCommand::PleaseWait);
			}
		}
	}
}

//--------------------------------------------------------------------------------------

void DirectXApp::OnSuspending(
	_In_ Platform::Object^ /* sender */,
	_In_ SuspendingEventArgs^ args
	)
{
	// Save application state.
	switch (m_updateState)
	{
	case UpdateEngineState::Dynamics:
		// Game is in the active game play state, Stop Game Timer and Pause play and save state
		SetAction(GameInfoOverlayCommand::None);
		SetGameInfoOverlay(GameInfoOverlayState::Pause);
		m_updateStateNext = UpdateEngineState::WaitingForPress;
		m_pressResult = PressResultState::Continue;
		m_game->PauseGame();
		break;

	case UpdateEngineState::WaitingForResources:
	case UpdateEngineState::WaitingForPress:
		m_updateStateNext = m_updateState;
		break;

	default:
		// any other state don't save as next state as they are trancient states and have already set m_updateStateNext
		break;
	}
	m_updateState = UpdateEngineState::Suspended;

	m_controller->Active(false);
	m_game->OnSuspending();

	// Hint to the driver that the app is entering an idle state and that its memory
	// can be temporarily used for other apps.
	m_renderer->Trim();
}

//--------------------------------------------------------------------------------------

void DirectXApp::OnResuming(
	_In_ Platform::Object^ /* sender */,
	_In_ Platform::Object^ /* args */
	)
{
	if (m_haveFocus)
	{
		m_updateState = m_updateStateNext;
	}
	else
	{
		m_updateState = UpdateEngineState::Deactivated;
	}

	if (m_updateState == UpdateEngineState::WaitingForPress)
	{
		SetAction(GameInfoOverlayCommand::TapToContinue);
		m_controller->WaitForPress(m_renderer->GameInfoOverlayUpperLeft(), m_renderer->GameInfoOverlayLowerRight());
	}
	m_game->OnResuming();
	ShowGameInfoOverlay();
	m_renderNeeded = true;
}

//--------------------------------------------------------------------------------------

void DirectXApp::SetWindow(
    _In_ CoreWindow^ window
    )
{
    window->PointerCursor = ref new CoreCursor(CoreCursorType::Arrow, 0);

    PointerVisualizationSettings^ visualizationSettings = PointerVisualizationSettings::GetForCurrentView();
    visualizationSettings->IsContactFeedbackEnabled = false;
    visualizationSettings->IsBarrelButtonFeedbackEnabled = false;

    window->SizeChanged +=
        ref new TypedEventHandler<CoreWindow^, WindowSizeChangedEventArgs^>(this, &DirectXApp::OnWindowSizeChanged);

    window->Closed +=
        ref new TypedEventHandler<CoreWindow^, CoreWindowEventArgs^>(this, &DirectXApp::OnWindowClosed);

    window->VisibilityChanged +=
        ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>(this, &DirectXApp::OnVisibilityChanged);

    DisplayInformation::GetForCurrentView()->DpiChanged +=
        ref new TypedEventHandler<DisplayInformation^, Platform::Object^>(this, &DirectXApp::OnDpiChanged);

    DisplayInformation::DisplayContentsInvalidated +=
        ref new TypedEventHandler<DisplayInformation^, Platform::Object^>(this, &DirectXApp::OnDisplayContentsInvalidated);

    m_controller->Initialize(window);

    m_controller->SetMoveRect(
        XMFLOAT2(0.0f, window->Bounds.Height - GameConstants::TouchRectangleSize),
        XMFLOAT2(GameConstants::TouchRectangleSize, window->Bounds.Height)
        );

    m_renderer->Initialize(window, DisplayInformation::GetForCurrentView()->LogicalDpi);
    m_renderer->DeviceLost += ref new D3DDeviceEventHandler(this, &DirectXApp::OnDeviceLost);
    m_renderer->DeviceReset += ref new D3DDeviceEventHandler(this, &DirectXApp::OnDeviceReset);

    SetGameInfoOverlay(GameInfoOverlayState::Loading);
    ShowGameInfoOverlay();
}

//--------------------------------------------------------------------------------------

void DirectXApp::Load(
    _In_ Platform::String^ /* entryPoint */
    )
{
    create_task([this]()
    {
        // Asynchronously initialize the game class and load the renderer device resources.
        // By doing all this asynchronously, the game gets to its main loop more quickly
        // and in parallel all the necessary resources are loaded on other threads.
        m_game->Initialize(m_controller, m_renderer);

        return m_renderer->CreateGameDeviceResourcesAsync(m_game);

    }).then([this]()
    {
        // The finalize code needs to run in the same thread context
        // as the m_renderer object was created because the D3D device context
        // can ONLY be accessed on a single thread.
        m_renderer->FinalizeCreateGameDeviceResources();

        InitializeGameState();

        if (m_updateState == UpdateEngineState::WaitingForResources)
        {
            // In the middle of a game so spin up the async task to load the level.
            create_task([this]()
            {
                //Place an asyc level load call here.

            }).then([this]()
            {
                // The m_game object may need to deal with D3D device context work so
                // again the finalize code needs to run in the same thread
                // context as the m_renderer object was created because the D3D
                // device context can ONLY be accessed on a single thread.

               // Place your finilize level loading call here;
                m_updateState = UpdateEngineState::ResourcesLoaded;

            }, task_continuation_context::use_current());
        }
    }, task_continuation_context::use_current());
}

//--------------------------------------------------------------------------------------

void DirectXApp::InitializeGameState()
{
	// Set up the initial state machine for handling Game playing state
	if (m_game->GameActive())
	{
		// The last time the game terminated it was in the middle
		// of a level.
		// We are waiting for the user to continue the game.
		m_updateState = UpdateEngineState::WaitingForResources;
		m_pressResult = PressResultState::Continue;
		SetGameInfoOverlay(GameInfoOverlayState::Pause);
		SetAction(GameInfoOverlayCommand::PleaseWait);
	}
	else if (!m_game->GameActive() && (m_game->HighScore().bestRoundTime > 0))
	{
		// The last time the game terminated the game had been completed.
		// Show the high score.
		// We are waiting for the user to acknowledge the high score and start a new game.
		// The level resources for the first level will be loaded later.
		m_updateState = UpdateEngineState::WaitingForPress;
		m_pressResult = PressResultState::LoadGame;
		SetGameInfoOverlay(GameInfoOverlayState::GameStats);
		m_controller->WaitForPress(m_renderer->GameInfoOverlayUpperLeft(), m_renderer->GameInfoOverlayLowerRight());
		SetAction(GameInfoOverlayCommand::TapToContinue);
	}
	else
	{
		// This is either the first time the game has run or
		// the last time the game terminated the level was completed.
		// We are waiting for the user to begin the next level.
		m_updateState = UpdateEngineState::WaitingForResources;
		m_pressResult = PressResultState::Play;
		SetGameInfoOverlay(GameInfoOverlayState::GameStart);
		SetAction(GameInfoOverlayCommand::PleaseWait);
	}
	ShowGameInfoOverlay();
}

//--------------------------------------------------------------------------------------

void DirectXApp::Run()
{
	//The main game loop.
    while (!m_windowClosed)
    {
        if (m_visible)
        {
            switch (m_updateState)
            {
            case UpdateEngineState::Deactivated:
            case UpdateEngineState::TooSmall:
                if (!m_renderNeeded)
                {
                    // The App is not currently the active window, so just wait for events.
                    CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessOneAndAllPending);
                    break;
                }
                // otherwise fall through and do normal processing to get the rendering handled.
            default:
				//process any new events.
                CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
				//Enter the Update stage of the game and apply all of the state and game logic for the current state of the game.
				Update();
				//Finally, render the new state of the game to the screen.
                m_renderer->Render();
                m_renderNeeded = false;
            }
        }
        else
        {
            CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessOneAndAllPending);
        }
    }
    m_game->OnSuspending();  // exiting due to window close.  Make sure to save state.
}

//--------------------------------------------------------------------------------------

void DirectXApp::Update()
{
	static uint32 loadCount = 0;

	//Signal the controller to update and check for any user input.
	m_controller->Update();

	switch (m_updateState)
	{
	//If we are waiting on resources
	case UpdateEngineState::WaitingForResources:
		// Waiting for initial load.  Display an update once per 60 updates.
		loadCount++;
		if ((loadCount % 60) == 0)
		{
			m_loadingCount++;
			SetGameInfoOverlay(m_gameInfoOverlayState);
		}
		break;

    //Requested resources have been loaded, transition to the next state.
	case UpdateEngineState::ResourcesLoaded:
		switch (m_pressResult)
		{
		case PressResultState::LoadGame:
			SetGameInfoOverlay(GameInfoOverlayState::GameStats);
			break;

		case PressResultState::Play:
			SetGameInfoOverlay(GameInfoOverlayState::GameStart);
			break;

		case PressResultState::Continue:
			SetGameInfoOverlay(GameInfoOverlayState::Pause);
			break;
		}
		m_updateState = UpdateEngineState::WaitingForPress;
		SetAction(GameInfoOverlayCommand::TapToContinue);
		m_controller->WaitForPress(m_renderer->GameInfoOverlayUpperLeft(), m_renderer->GameInfoOverlayLowerRight());
		ShowGameInfoOverlay();
		m_renderNeeded = true;
		break;

	//If we are waiting for user input due to a pop-up message.
	case UpdateEngineState::WaitingForPress:
		if (m_controller->IsPressComplete())
		{
			switch (m_pressResult)
			{
			case PressResultState::LoadGame:
				m_updateState = UpdateEngineState::WaitingForResources;
				m_pressResult = PressResultState::Play;
				m_controller->Active(false);
				m_game->LoadGame();
				SetAction(GameInfoOverlayCommand::PleaseWait);
				SetGameInfoOverlay(GameInfoOverlayState::GameStart);
				ShowGameInfoOverlay();

				m_updateState = UpdateEngineState::ResourcesLoaded;
				break;

			case PressResultState::Play:
				m_updateState = UpdateEngineState::Dynamics;
				HideGameInfoOverlay();
				m_controller->Active(true);
				m_game->StartLevel();
				break;

			case PressResultState::Continue:
				m_updateState = UpdateEngineState::Dynamics;
				HideGameInfoOverlay();
				m_controller->Active(true);
				m_game->ContinueGame();
				break;
			}
		}
		break;

    //Or if we are in the main game play state.
	case UpdateEngineState::Dynamics:
		//if the player has pressed a button to pause the game.
		if (m_controller->IsPauseRequested())
		{
			m_game->PauseGame();
			SetGameInfoOverlay(GameInfoOverlayState::Pause);
			SetAction(GameInfoOverlayCommand::TapToContinue);
			m_updateState = UpdateEngineState::WaitingForPress;
			m_pressResult = PressResultState::Continue;
			ShowGameInfoOverlay();
		}
		//otherwise continue on as normal
		else
		{
			//instruct the game to run one update
			GameState runState = m_game->RunGame();

			//Now check the returned runState of the game after that update.
			switch (runState)
			{
			//If the player lost the game.
			case GameState::PlayerLost:
				SetAction(GameInfoOverlayCommand::TapToContinue);
				SetGameInfoOverlay(GameInfoOverlayState::GameOverLost);
				ShowGameInfoOverlay();
				m_updateState = UpdateEngineState::WaitingForPress;
				m_pressResult = PressResultState::LoadGame;
				break;

			case GameState::GameComplete:
				SetAction(GameInfoOverlayCommand::TapToContinue);
				SetGameInfoOverlay(GameInfoOverlayState::GameOverWon);
				ShowGameInfoOverlay();
				m_updateState = UpdateEngineState::WaitingForPress;
				m_pressResult = PressResultState::LoadGame;
				break;
			}
		}

		if (m_updateState == UpdateEngineState::WaitingForPress)
		{
			// Transitioning state, so enable waiting for the press event
			m_controller->WaitForPress(m_renderer->GameInfoOverlayUpperLeft(), m_renderer->GameInfoOverlayLowerRight());
		}
		if (m_updateState == UpdateEngineState::WaitingForResources)
		{
			// Transitioning state, so shut down the input controller until resources are loaded
			m_controller->Active(false);
		}
		break;
	}
}


//--------------------------------------------------------------------------------------

void DirectXApp::Uninitialize()
{
}

//--------------------------------------------------------------------------------------

void DirectXApp::OnWindowSizeChanged(
    _In_ CoreWindow^ window,
    _In_ WindowSizeChangedEventArgs^ /* args */
    )
{
    UpdateLayoutState(window);
    m_renderer->UpdateForWindowSizeChange();

    // The location of the Control regions may have changed with the size change so update the controller.
    m_controller->SetMoveRect(
        XMFLOAT2(0.0f, window->Bounds.Height - GameConstants::TouchRectangleSize),
        XMFLOAT2(GameConstants::TouchRectangleSize, window->Bounds.Height)
        );

    if (m_updateState == UpdateEngineState::WaitingForPress)
    {
        // A side effect of m_renderer->UpdateForWindowSizeChange() is that the GameInfoOverlay rectangle may have
        // changed as a result of layout, so make sure the WaitForPress rectangle is set to the new size.
        m_controller->WaitForPress(m_renderer->GameInfoOverlayUpperLeft(), m_renderer->GameInfoOverlayLowerRight());
    }
}

//--------------------------------------------------------------------------------------

void DirectXApp::OnWindowClosed(
    _In_ CoreWindow^ /* sender */,
    _In_ CoreWindowEventArgs^ /* args */
    )
{
    m_windowClosed = true;
}

//--------------------------------------------------------------------------------------

void DirectXApp::OnDpiChanged(_In_ DisplayInformation^ sender, _In_ Platform::Object^ args)
{
    m_renderer->SetDpi(sender->LogicalDpi);

    // The GameInfoOverlay may have been recreated as a result of DPI changes so
    // regenerate the data.
    SetGameInfoOverlay(m_gameInfoOverlayState);
    SetAction(m_gameInfoOverlayCommand);
}

//--------------------------------------------------------------------------------------

void DirectXApp::OnDisplayContentsInvalidated(_In_ DisplayInformation^ sender, _In_ Platform::Object^ args)
{
    m_renderNeeded = true;
}



//--------------------------------------------------------------------------------------

void DirectXApp::OnVisibilityChanged(
    _In_ CoreWindow^ /* sender */,
    _In_ VisibilityChangedEventArgs^ args
    )
{
    m_visible = args->Visible;
}

//--------------------------------------------------------------------------------------

void DirectXApp::OnDeviceLost()
{
    if (m_updateState == UpdateEngineState::Dynamics)
    {
        // Game is in the active game play state, Stop Game Timer and Pause play and save state
        m_game->PauseGame();
    }
    m_renderNeeded = false;
}
//--------------------------------------------------------------------------------------

void DirectXApp::OnDeviceReset()
{

    SetAction(GameInfoOverlayCommand::PleaseWait);
    SetGameInfoOverlay(GameInfoOverlayState::Loading);
    m_updateState = UpdateEngineState::WaitingForResources;
    m_renderNeeded = true;

    create_task([this]()
    {
        return m_renderer->CreateGameDeviceResourcesAsync(m_game);

    }).then([this]()
    {
        // The finalize code needs to run in the same thread context
        // as the m_renderer object was created because the D3D device context
        // can ONLY be accessed on a single thread.
        m_renderer->FinalizeCreateGameDeviceResources();

        // Reset the main state machine based on the current game state now
        // that the device resources have been restored.
        InitializeGameState();

        if (m_updateState == UpdateEngineState::WaitingForResources)
        {
           
        }
        else
        {
            // The game is not in the middle of a level so there aren't any level
            // resources to load.  Creating a no-op task so that in both cases
            // the same continuation logic is used.
            return create_task([]()
            {
            });
        }

    }, task_continuation_context::use_current()).then([this]()
    {
        // Since Device lost is an unexpected event, the app visual state
        // may be snapped or not have focus.  Put the state machine
        // into the correct state to reflect these cases.
        if (CoreWindow::GetForCurrentThread()->Bounds.Width < 768.0f)
        {
            m_updateStateNext = m_updateState;
            m_updateState = UpdateEngineState::TooSmall;
            m_controller->Active(false);
            HideGameInfoOverlay();
            SetTooSmall();
            m_renderNeeded = true;
        }
        else if (!m_haveFocus)
        {
            m_updateStateNext = m_updateState;
            m_updateState = UpdateEngineState::Deactivated;
            m_controller->Active(false);
            SetAction(GameInfoOverlayCommand::None);
            m_renderNeeded = true;
        }
    }, task_continuation_context::use_current());
}

//--------------------------------------------------------------------------------------

void DirectXApp::UpdateLayoutState(_In_ CoreWindow^ window)
{
    m_renderNeeded = true;

    if (window->Bounds.Width < 768.0F)
    {
        switch (m_updateState)
        {
        case UpdateEngineState::Dynamics:
            // From Dynamic mode, when coming out of TooSmall layout rather than going directly back into game play
            // go to the paused state waiting for user input to continue
            m_updateStateNext = UpdateEngineState::WaitingForPress;
            m_pressResult = PressResultState::Continue;
            SetGameInfoOverlay(GameInfoOverlayState::Pause);
            SetAction(GameInfoOverlayCommand::TapToContinue);
            m_game->PauseGame();
            break;

        case UpdateEngineState::WaitingForResources:
        case UpdateEngineState::WaitingForPress:
            // Avoid corrupting the m_updateStateNext on a transition from Snapped -> Snapped.
            // Otherwise just cache the current state and return to it when leaving SNAPPED layout

            m_updateStateNext = m_updateState;
            break;

        default:
            break;
        }

        m_updateState = UpdateEngineState::TooSmall;
        m_controller->Active(false);
        HideGameInfoOverlay();
        SetTooSmall();
    }
    else
    {
        if (m_updateState == UpdateEngineState::TooSmall)
        {
            HideTooSmall();
            ShowGameInfoOverlay();
            m_renderNeeded = true;

            if (m_haveFocus)
            {
                if (m_updateStateNext == UpdateEngineState::WaitingForPress)
                {
                    SetAction(GameInfoOverlayCommand::TapToContinue);
                }
                else if (m_updateStateNext == UpdateEngineState::WaitingForResources)
                {
                    SetAction(GameInfoOverlayCommand::PleaseWait);
                }

                m_updateState = m_updateStateNext;
            }
            else
            {
                m_updateState = UpdateEngineState::Deactivated;
                SetAction(GameInfoOverlayCommand::None);
            }
        }
        else if (m_updateState == UpdateEngineState::Dynamics)
        {
            // In Dynamic mode, when a resize event happens, go to the paused state waiting for user input to continue.
            m_pressResult = PressResultState::Continue;
            SetGameInfoOverlay(GameInfoOverlayState::Pause);
            m_game->PauseGame();
            if (m_haveFocus)
            {
                m_updateState = UpdateEngineState::WaitingForPress;
                SetAction(GameInfoOverlayCommand::TapToContinue);
            }
            else
            {
                m_updateState = UpdateEngineState::Deactivated;
                SetAction(GameInfoOverlayCommand::None);
            }
            ShowGameInfoOverlay();
            m_renderNeeded = true;
        }
    }
}

//--------------------------------------------------------------------------------------

void DirectXApp::SetGameInfoOverlay(GameInfoOverlayState state)
{
    m_gameInfoOverlayState = state;
    switch (state)
    {
    case GameInfoOverlayState::Loading:
        m_renderer->InfoOverlay()->SetGameLoading(m_loadingCount);
        break;

    case GameInfoOverlayState::GameStats:
        m_renderer->InfoOverlay()->SetGameStats(
			(int)(m_game->HighScore().bestRoundTime)
            );
        break;

    case GameInfoOverlayState::GameStart:
        m_renderer->InfoOverlay()->SetLevelStart( );
        break;

    case GameInfoOverlayState::GameOverWon:
        m_renderer->InfoOverlay()->SetGameOver(
            true,
			(int)(m_game->HighScore().bestRoundTime)
            );
        break;

    case GameInfoOverlayState::GameOverLost:
        m_renderer->InfoOverlay()->SetGameOver(
            false,
			(int)(m_game->HighScore().bestRoundTime)
            );
        break;

    case GameInfoOverlayState::Pause:
        m_renderer->InfoOverlay()->SetPause();
        break;
    }
}

//--------------------------------------------------------------------------------------

void DirectXApp::SetAction (GameInfoOverlayCommand command)
{
    m_gameInfoOverlayCommand = command;
    m_renderer->InfoOverlay()->SetAction(command);
}

//--------------------------------------------------------------------------------------

void DirectXApp::ShowGameInfoOverlay()
{
    m_renderer->InfoOverlay()->ShowGameInfoOverlay();
}

//--------------------------------------------------------------------------------------

void DirectXApp::HideGameInfoOverlay()
{
    m_renderer->InfoOverlay()->HideGameInfoOverlay();
}

//--------------------------------------------------------------------------------------

void DirectXApp::SetTooSmall()
{
    m_renderer->InfoOverlay()->SetPause();
    m_renderer->InfoOverlay()->ShowGameInfoOverlay();
}

//--------------------------------------------------------------------------------------

void DirectXApp::HideTooSmall()
{
    m_renderer->InfoOverlay()->HideGameInfoOverlay();
    SetGameInfoOverlay(m_gameInfoOverlayState);
}

//--------------------------------------------------------------------------------------



