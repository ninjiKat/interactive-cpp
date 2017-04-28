//--------------------------------------------------------------------------------------
// Interactivity.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SampleGUI.h"
#include "ATGColors.h"
#include "Interactivity.h"

using namespace DirectX;

namespace
{
    const wchar_t *ProgressStateText[] = {
        L"Unknown",
        L"Achieved",
        L"Not Started",
        L"In Progress"
    };

    const wchar_t *AchievementTypeText[] = {
        L"Unknown",
        L"All",
        L"Persistent",
        L"Challenge"
    };

    const int c_maxAchievements          = 10;
    const int c_liveHUD                  = 1000;
    const int c_sampleUIPanel            = 2000;
    const int c_getAchievementsBtn       = 2101;
    const int c_getSingleAchievementBtn  = 2102;
    const int c_writeEventBtn            = 2103;

    const string_t c_singleAchievementId = L"1";
}

Sample::Sample()
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
    m_deviceResources->RegisterDeviceNotify(this);

    m_liveResources = std::make_unique<ATG::LiveResources>();

    // Set up UI using default fonts and colors.
    ATG::UIConfig uiconfig;
    m_ui = std::make_unique<ATG::UIManager>(uiconfig);
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation)
{
    m_gamePad = std::make_unique<GamePad>();

    m_keyboard = std::make_unique<Keyboard>();
    m_keyboard->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));

    m_mouse = std::make_unique<Mouse>();
    m_mouse->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));

    m_ui->LoadLayout(L".\\Assets\\Layout.csv", L".\\Assets");
    m_liveResources->Initialize(m_ui, m_ui->FindPanel<ATG::Overlay>(c_sampleUIPanel));

    m_deviceResources->SetWindow(window, width, height, rotation);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    string_t oauthUri = L"http://localhost:3000/api/v1/oauth/authorize"; // TODO: this is what we hit to get an oauth token
    
    string_t mockOAuthToken = L"test";
    string_t mockInteractiveExperienceId = L"AnExperience";
    string_t mockInteractiveVersion = L"0.0.0";
    string_t mockProtocolVersion = L"2.0";

    m_beamManager.initialize(mockOAuthToken, mockOAuthToken, 0, mockInteractiveExperienceId, mockInteractiveVersion, mockProtocolVersion);

    SetupUI();
}

#pragma region UI Methods
void Sample::SetupUI()
{
    using namespace ATG;

    /*
    // Get all Achievements
    m_ui->FindControl<Button>(c_sampleUIPanel, c_getAchievementsBtn)->SetCallback([this](IPanel*, IControl*)
    {
        m_console->Clear();
        GetAllAchievements();
    });

    // Get single Achievement
    m_ui->FindControl<Button>(c_sampleUIPanel, c_getSingleAchievementBtn)->SetCallback([this](IPanel*, IControl*)
    {
        m_console->Clear();
        GetAchievement();
    });

    // Write an event
    m_ui->FindControl<Button>(c_sampleUIPanel, c_writeEventBtn)->SetCallback([this](IPanel*, IControl*)
    {
        m_console->Clear();
        // Write the event for the achievements
        this->WriteEvent();
    });
    */
}
#pragma endregion

#pragma region Interactivity Methods
/*
void Sample::GetAllAchievements()
{
    auto& achievementService = m_liveResources->GetLiveContext()->achievement_service();
    achievementService.get_achievements_for_title_id(
        m_liveResources->GetUser()->xbox_user_id(),                         // Xbox LIVE user Id
        m_liveResources->GetTitleId(),                                      // Title Id to get achievement data for
        xbox::services::achievements::achievement_type::all,                // achievement_type filter: All mean to get Persistent and Challenge achievements
        false,                                                              // All possible achievements including accurate unlocked data
        xbox::services::achievements::achievement_order_by::title_id,       // achievement_order_by filter: Default means no particular order
        0,                                                                  // The number of achievement items to skip
        c_maxAchievements                                                   // The maximum number of achievement items to return in the response
        )
    .then([this](xbox::services::xbox_live_result<xbox::services::achievements::achievements_result> result)
    {
        this->ProcessAchievments(result);
    });
}
*/
#pragma endregion

#pragma region Frame Update
// Executes basic render loop.
void Sample::Tick()
{
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();
}

// Updates the world.
void Sample::Update(DX::StepTimer const& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    float elapsedTime = float(timer.GetElapsedSeconds());

    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);

        if (!m_ui->Update(elapsedTime, pad))
        {
            // UI is not consuming input, so implement sample gamepad controls
        }

        if (pad.IsViewPressed())
        {
            Windows::ApplicationModel::Core::CoreApplication::Exit();
        }
    }
    else
    {
        m_gamePadButtons.Reset();
    }

    auto mouse = m_mouse->GetState();
    mouse;

    auto kb = m_keyboard->GetState();
    m_keyboardButtons.Update(kb);

    if (!m_ui->Update(elapsedTime, *m_mouse, *m_keyboard))
    {
        // UI is not consuming input, so implement sample mouse & keyboard controls
    }

    if (kb.Escape)
    {
        Windows::ApplicationModel::Core::CoreApplication::Exit();
    }

    if (m_keyboardButtons.IsKeyPressed(Keyboard::A))
    {
        //m_liveResources->SignIn();
    }

    if (m_keyboardButtons.IsKeyPressed(Keyboard::Y))
    {
        //m_liveResources->SwitchAccount();
    }

    PIXEndEvent();
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Sample::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    Clear();

    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Render");

    // Allow UI to render last
    m_ui->Render();
    m_console->Render();

    PIXEndEvent(context);

    // Show the new frame.
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    PIXEndEvent();
}

// Helper method to clear the back buffers.
void Sample::Clear()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.
    auto renderTarget = m_deviceResources->GetBackBufferRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    context->ClearRenderTargetView(renderTarget, ATG::Colors::Background);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

    // Set the viewport.
    auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    PIXEndEvent(context);
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Sample::OnActivated()
{
}

void Sample::OnDeactivated()
{
}

void Sample::OnSuspending()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    context->ClearState();

    m_deviceResources->Trim();
}

void Sample::OnResuming()
{
    m_timer.ResetElapsedTime();
    m_gamePadButtons.Reset();
    m_keyboardButtons.Reset();

    // Reset UI on return from suspend.
    m_ui->Reset();
}

void Sample::OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation)
{
    if (!m_deviceResources->WindowSizeChanged(width, height, rotation))
        return;

    CreateWindowSizeDependentResources();
}

void Sample::ValidateDevice()
{
    m_deviceResources->ValidateDevice();
}

// Properties
void Sample::GetDefaultSize(int& width, int& height) const
{
    width = 1280;
    height = 720;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Sample::CreateDeviceDependentResources()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    m_console = std::make_unique<DX::TextConsole>(context, L"Courier_16.spritefont");

    // Let UI create Direct3D resources.
    m_ui->RestoreDevice(context);
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    // Let UI know the size of our rendering viewport.
    RECT fullscreen = m_deviceResources->GetOutputSize();
    m_ui->SetWindow(fullscreen);

    static const RECT consoleDisplay = { 660, 150, 1150, 705 };
    m_console->SetWindow(consoleDisplay);
}

void Sample::OnDeviceLost()
{
    m_ui->ReleaseDevice();
}

void Sample::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
