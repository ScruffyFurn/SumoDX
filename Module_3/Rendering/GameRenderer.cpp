//// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//// PARTICULAR PURPOSE.
////
//// Copyright (c) Microsoft Corporation. All rights reserved

#include "pch.h"
#include "../Utilities/DirectXSample.h"
#include "../Rendering/GameRenderer.h"
#include "../Rendering/ConstantBuffers.h"
#include "../Utilities/BasicLoader.h"
#include "../Meshes/SumoMesh.h"
#include "../Meshes/CylinderMesh.h"
#include "../GameObjects/Cylinder.h"

using namespace concurrency;
using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Windows::UI::Core;

//----------------------------------------------------------------------

GameRenderer::GameRenderer() :
    m_initialized(false),
    m_gameResourcesLoaded(false),
    m_levelResourcesLoaded(true)
{
}

//----------------------------------------------------------------------

void GameRenderer::Initialize(
    _In_ CoreWindow^ window,
    float dpi
    )
{
    m_gameHud = ref new GameHud(
        "MVA Content",
        "SumoDX DirectX Example"
        );
    m_gameInfoOverlay = ref new GameInfoOverlay();

    DirectXBase::Initialize(window, dpi);
}

//----------------------------------------------------------------------

void GameRenderer::HandleDeviceLost()
{
    // On device lost all the device resources are invalid.
    // Set the state of the renderer to not have a pointer to the
    // SumoDX object.  It will be reset as a part of the
    // game devices resources being recreated.
    m_game = nullptr;

    DirectXBase::HandleDeviceLost();
}

//----------------------------------------------------------------------

void GameRenderer::CreateDeviceIndependentResources()
{
    DirectXBase::CreateDeviceIndependentResources();
    m_gameHud->CreateDeviceIndependentResources(m_dwriteFactory.Get(), m_wicFactory.Get());
    m_gameInfoOverlay->CreateDeviceIndependentResources(m_dwriteFactory.Get());
}

//----------------------------------------------------------------------

void GameRenderer::CreateDeviceResources()
{
    m_gameResourcesLoaded = false;
    m_levelResourcesLoaded = true;

    DirectXBase::CreateDeviceResources();

    m_gameHud->CreateDeviceResources(m_d2dContext.Get());
    m_gameInfoOverlay->CreateDeviceResources(m_d2dContext.Get());
}

//----------------------------------------------------------------------

void GameRenderer::UpdateForWindowSizeChange()
{
    DirectXBase::UpdateForWindowSizeChange();

    m_gameHud->UpdateForWindowSizeChange(m_windowBounds);

    if (m_game != nullptr)
    {
        // Update the Projection Matrix and the associated Constant Buffer.
        m_game->GameCamera()->SetProjParams(
            XM_PI / 2, m_renderTargetSize.Width / m_renderTargetSize.Height,
            0.01f,
            100.0f
            );
        ConstantBufferChangeOnResize changesOnResize;
        XMStoreFloat4x4(
            &changesOnResize.projection,
            XMMatrixMultiply(
                XMMatrixTranspose(m_game->GameCamera()->Projection()),
                XMMatrixTranspose(XMLoadFloat4x4(&m_rotationTransform3D))
                )
            );

        m_d3dContext->UpdateSubresource(
            m_constantBufferChangeOnResize.Get(),
            0,
            nullptr,
            &changesOnResize,
            0,
            0
            );
    }
}

//----------------------------------------------------------------------

void GameRenderer::SetDpi(float dpi)
{
    DirectXBase::SetDpi(dpi);

    m_gameInfoOverlay->CreateDpiDependentResources(dpi);
}

//----------------------------------------------------------------------

task<void> GameRenderer::CreateGameDeviceResourcesAsync(_In_ SumoDX^ game)
{
    // Create the device dependent game resources.
    // Only the m_d3dDevice is used in this method.  It is expected
    // to not run on the same thread as the GameRenderer was created.
    // Create methods on the m_d3dDevice are free-threaded and are safe while any methods
    // in the m_d3dContext should only be used on a single thread and handled
    // in the FinalizeCreateGameDeviceResources method.
    m_game = game;

    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));

    // Create the constant buffers.
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    bd.ByteWidth = (sizeof(ConstantBufferNeverChanges) + 15) / 16 * 16;
    DX::ThrowIfFailed(
        m_d3dDevice->CreateBuffer(&bd, nullptr, &m_constantBufferNeverChanges)
        );

    bd.ByteWidth = (sizeof(ConstantBufferChangeOnResize) + 15) / 16 * 16;
    DX::ThrowIfFailed(
        m_d3dDevice->CreateBuffer(&bd, nullptr, &m_constantBufferChangeOnResize)
        );

    bd.ByteWidth = (sizeof(ConstantBufferChangesEveryFrame) + 15) / 16 * 16;
    DX::ThrowIfFailed(
        m_d3dDevice->CreateBuffer(&bd, nullptr, &m_constantBufferChangesEveryFrame)
        );

    bd.ByteWidth = (sizeof(ConstantBufferChangesEveryPrim) + 15) / 16 * 16;
    DX::ThrowIfFailed(
        m_d3dDevice->CreateBuffer(&bd, nullptr, &m_constantBufferChangesEveryPrim)
        );

    D3D11_SAMPLER_DESC sampDesc;
    ZeroMemory(&sampDesc, sizeof(sampDesc));

    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = FLT_MAX;
    DX::ThrowIfFailed(
        m_d3dDevice->CreateSamplerState(&sampDesc, &m_samplerLinear)
        );

    // Start the async tasks to load the shaders and textures.
    BasicLoader^ loader = ref new BasicLoader(m_d3dDevice.Get());

    std::vector<task<void>> tasks;

    uint32 numElements = ARRAYSIZE(PNTVertexLayout);
    tasks.push_back(loader->LoadShaderAsync("VertexShader.cso", PNTVertexLayout, numElements, &m_vertexShader, &m_vertexLayout));
   // tasks.push_back(loader->LoadShaderAsync("VertexShaderFlat.cso", nullptr, numElements, &m_vertexShaderFlat, nullptr));
    tasks.push_back(loader->LoadShaderAsync("PixelShader.cso", &m_pixelShader));
  //  tasks.push_back(loader->LoadShaderAsync("PixelShaderFlat.cso", &m_pixelShaderFlat));

    // Make sure the previous versions if any of the textures are released.
	m_playerTexture = nullptr;
    m_cylinderTexture = nullptr;
	m_enemyTexture = nullptr;
    m_floorTexture = nullptr;
    m_wallsTexture = nullptr;

    // Load Game specific textures.
    tasks.push_back(loader->LoadTextureAsync("Resources\\SumoBlue.dds", nullptr, &m_playerTexture));
    tasks.push_back(loader->LoadTextureAsync("Resources\\metal_texture.dds", nullptr, &m_cylinderTexture));
    tasks.push_back(loader->LoadTextureAsync("Resources\\SumoRed.dds", nullptr, &m_enemyTexture));
    tasks.push_back(loader->LoadTextureAsync("Resources\\cellfloor.dds", nullptr, &m_floorTexture));
    tasks.push_back(loader->LoadTextureAsync("Resources\\cellwall.dds", nullptr, &m_wallsTexture));
    tasks.push_back(create_task([]()
    {
        // Simulate loading additional resources by introducing a delay.
        wait(GameConstants::InitialLoadingDelay);
    }));

    // Return the task group of all the async tasks for loading the shader and texture assets.
    return when_all(tasks.begin(), tasks.end());
}

//----------------------------------------------------------------------

void GameRenderer::FinalizeCreateGameDeviceResources()
{
    // All asynchronously loaded resources have completed loading.
    // Now associate all the resources with the appropriate game objects.
    // This method is expected to run in the same thread as the GameRenderer
    // was created. All work will happen behind the "Loading ..." screen after the
    // main loop has been entered.

    // Initialize the Constant buffer with the light positions
    // These are handled here to ensure that the d3dContext is only
    // used in one thread.

    ConstantBufferNeverChanges constantBufferNeverChanges;
    constantBufferNeverChanges.lightPosition[0] = XMFLOAT4( 3.5f, 2.5f,  5.5f, 1.0f);
    constantBufferNeverChanges.lightPosition[1] = XMFLOAT4( 3.5f, 2.5f, -5.5f, 1.0f);
    constantBufferNeverChanges.lightPosition[2] = XMFLOAT4(-3.5f, 2.5f, -5.5f, 1.0f);
    constantBufferNeverChanges.lightPosition[3] = XMFLOAT4( 3.5f, 2.5f,  5.5f, 1.0f);
    constantBufferNeverChanges.lightColor = XMFLOAT4(0.25f, 0.25f, 0.25f, 1.0f);
    m_d3dContext->UpdateSubresource(m_constantBufferNeverChanges.Get(), 0, nullptr, &constantBufferNeverChanges, 0, 0);

	Material^ playerMaterial = ref new Material(
		XMFLOAT4(0.8f, 0.8f, 0.8f, .5f),
		XMFLOAT4(1.0f, 1.0f, 1.0f, .5f),
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		15.0f,
		m_playerTexture.Get(),
		m_vertexShader.Get(),
		m_pixelShader.Get()
		);

	Material^ enemyMaterial = ref new Material(
		XMFLOAT4(0.8f, 0.8f, 0.8f, .5f),
		XMFLOAT4(0.65f, 0.65f, 0.6f, .5f),
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		15.0f,
		m_enemyTexture.Get(),
		m_vertexShader.Get(),
		m_pixelShader.Get()
		);

	Material^ cylinderMaterial = ref new Material(
		XMFLOAT4(0.8f, 0.8f, 0.8f, .5f),
		XMFLOAT4(0.8f, 0.8f, 0.8f, .5f),
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		15.0f,
		m_cylinderTexture.Get(),
		m_vertexShader.Get(),
		m_pixelShader.Get()
		);
   
    MeshObject^ sumoMesh = ref new SumoMesh(m_d3dDevice.Get());
   
	MeshObject^ cylinderMesh = ref new CylinderMesh(m_d3dDevice.Get(), 26);

    auto objects = m_game->RenderObjects();

    // Attach the textures to the appropriate game objects.
    for (auto object = objects.begin(); object != objects.end(); object++)
    {
		if (AISumoBlock^ evilSumoBlock = dynamic_cast<AISumoBlock^>(*object))
		{
			evilSumoBlock->Mesh(sumoMesh);
			evilSumoBlock->NormalMaterial(enemyMaterial);
		}
		else if (SumoBlock^ sumoBlock = dynamic_cast<SumoBlock^>(*object))
		{
			sumoBlock->Mesh(sumoMesh);
			sumoBlock->NormalMaterial(playerMaterial);
		}
		else if (Cylinder^ cylinder = dynamic_cast<Cylinder^>(*object))
		{
			cylinder->Mesh(cylinderMesh);
			cylinder->NormalMaterial(cylinderMaterial);
		}
    }

    // Ensure that the camera has been initialized with the right Projection
    // matrix.  The camera is not created at the time the first window resize event
    // occurs.
    m_game->GameCamera()->SetProjParams(
        XM_PI / 2,
        m_renderTargetSize.Width / m_renderTargetSize.Height,
        0.01f,
        100.0f
        );

    // Make sure that Projection matrix has been set in the constant buffer
    // now that all the resources are loaded.
    // DirectXBase is handling screen rotations directly to eliminate an unaligned
    // fullscreen copy.  As a result, it is necessary to post multiply the rotationTransform3D
    // matrix to the camera projection matrix.
    // The matrices are transposed due to the shader code expecting the matrices in the opposite
    // row/column order from the DirectX math library.
    ConstantBufferChangeOnResize changesOnResize;
    XMStoreFloat4x4(
        &changesOnResize.projection,
        XMMatrixMultiply(
            XMMatrixTranspose(m_game->GameCamera()->Projection()),
            XMMatrixTranspose(XMLoadFloat4x4(&m_rotationTransform3D))
            )
        );

    m_d3dContext->UpdateSubresource(
        m_constantBufferChangeOnResize.Get(),
        0,
        nullptr,
        &changesOnResize,
        0,
        0
        );

    m_gameResourcesLoaded = true;
}


//----------------------------------------------------------------------

void GameRenderer::Render()
{
    //setup the rendering pass to be completed.
    m_d3dContext->OMSetRenderTargets(1, m_d3dRenderTargetView.GetAddressOf(), m_d3dDepthStencilView.Get());
    m_d3dContext->ClearDepthStencilView(m_d3dDepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
    m_d2dContext->SetTarget(m_d2dTargetBitmap.Get());

	//start by clearing the screen
	const float ClearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
	m_d3dContext->ClearRenderTargetView(m_d3dRenderTargetView.Get(), ClearColor);

	//Next check if we are running the game and should be rendering any 3D elements that have been loaded.
    if (m_game != nullptr && m_gameResourcesLoaded && m_levelResourcesLoaded)
    {
        // This section is only used after the game state has been initialized and all device
        // resources needed for the game have been created and associated with the game objects.

        // Update variables that change once per frame.
        ConstantBufferChangesEveryFrame constantBufferChangesEveryFrame;
        XMStoreFloat4x4(
            &constantBufferChangesEveryFrame.view,
            XMMatrixTranspose(m_game->GameCamera()->View())
            );
        m_d3dContext->UpdateSubresource(
            m_constantBufferChangesEveryFrame.Get(),
            0,
            nullptr,
            &constantBufferChangesEveryFrame,
            0,
            0
            );

        // Setup the graphics pipeline. This sample uses the same InputLayout and set of
        // constant buffers for all shaders, so they only need to be set once per frame.

        m_d3dContext->IASetInputLayout(m_vertexLayout.Get());
        m_d3dContext->VSSetConstantBuffers(0, 1, m_constantBufferNeverChanges.GetAddressOf());
        m_d3dContext->VSSetConstantBuffers(1, 1, m_constantBufferChangeOnResize.GetAddressOf());
        m_d3dContext->VSSetConstantBuffers(2, 1, m_constantBufferChangesEveryFrame.GetAddressOf());
        m_d3dContext->VSSetConstantBuffers(3, 1, m_constantBufferChangesEveryPrim.GetAddressOf());

        m_d3dContext->PSSetConstantBuffers(2, 1, m_constantBufferChangesEveryFrame.GetAddressOf());
        m_d3dContext->PSSetConstantBuffers(3, 1, m_constantBufferChangesEveryPrim.GetAddressOf());
        m_d3dContext->PSSetSamplers(0, 1, m_samplerLinear.GetAddressOf());

		//now walk through the render objects list and draw each object to screen
        auto objects = m_game->RenderObjects();
        for (auto object = objects.begin(); object != objects.end(); object++)
        {
            (*object)->Render(m_d3dContext.Get(), m_constantBufferChangesEveryPrim.Get());
        }
    }

	//Now begin the process of drawing any HUD or 2D overlay elements on top of everything else.
    m_d2dContext->BeginDraw();

    // To handle the swapchain being pre-rotated, set the D2D transformation to include it.
    m_d2dContext->SetTransform(m_rotationTransform2D);

	//Again, only attempt to render in-game HUD elements once we have everything loaded and the game is running
    if (m_game != nullptr && m_gameResourcesLoaded)
    {
        // This is only used after the game state has been initialized.
        m_gameHud->Render(m_game, m_d2dContext.Get(), m_windowBounds);
    }

	//Finally, draw our menus/pop-ups on top of everything else.
    if (m_gameInfoOverlay->Visible())
    {
        m_d2dContext->DrawBitmap(
            m_gameInfoOverlay->Bitmap(),
            D2D1::RectF(
                (m_windowBounds.Width - GameInfoOverlayConstant::Width)/2.0f,
                (m_windowBounds.Height - GameInfoOverlayConstant::Height)/2.0f,
                (m_windowBounds.Width - GameInfoOverlayConstant::Width)/2.0f + GameInfoOverlayConstant::Width,
                (m_windowBounds.Height - GameInfoOverlayConstant::Height)/2.0f + GameInfoOverlayConstant::Height
                )
            );
    }

    // We ignore D2DERR_RECREATE_TARGET here. This error indicates that the device
    // is lost. It will be handled during the next call to Present.
    HRESULT hr = m_d2dContext->EndDraw();
    if (hr != D2DERR_RECREATE_TARGET)
    {
        DX::ThrowIfFailed(hr);
    }
    
    Present();
}

//----------------------------------------------------------------------

#if defined(_DEBUG)
void GameRenderer::ReportLiveDeviceObjects()
{
    // If the debug layer isn't present on the system then this QI will fail.  In that case
    // the routine just exits.   This is a debugging aid to see the active D3D objects and
    // is not critical to functional operation of the sample.

    ComPtr<ID3D11Debug> debugLayer;
    HRESULT hr = m_d3dDevice.As(&debugLayer);
    if (FAILED(hr))
    {
        return;
    }
    DX::ThrowIfFailed(
        debugLayer->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL)
        );
}

//----------------------------------------------------------------------

#endif

