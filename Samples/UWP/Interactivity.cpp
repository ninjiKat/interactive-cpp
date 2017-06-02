// Copyright (c) Microsoft Corporation
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "pch.h"
#include "SampleGUI.h"
#include "ATGColors.h"
#include "Interactivity.h"

using namespace DirectX;
using namespace Platform;
using namespace xbox::services;
using namespace xbox::services::leaderboard;
using namespace concurrency;
using namespace MICROSOFT_MIXER_NAMESPACE;

namespace
{
    const int c_debugLog = 202;

    const int c_maxLeaderboards         = 10;
    const int c_liveHUD                 = 1000;
    const int c_sampleUIPanel           = 2000;
    const int c_goInteractiveBtn        = 2101;
    const int c_placeParticipantBtn     = 2102;
    const int c_disbandGroupsBtn        = 2103;
    const int c_cooldownRedBtn          = 2104;
    const int c_cooldownBlueBtn         = 2105;
    const int c_switchScenesBtn         = 2106;
    const int c_voteYesCountLabel       = 1004;
    const int c_voteNoCountLabel        = 1006;

    const string_t s_interactiveVersion = L"19005";

    const string_t s_defaultGroup       = L"default";
    const string_t s_redGroup           = L"redGroup";
    const string_t s_blueGroup          = L"blueGroup";

    const string_t s_defaultScene       = L"default";
    const string_t s_redMinesScene      = L"red_mines";
    const string_t s_redLasersScene     = L"red_lasers";
    const string_t s_blueMinesScene     = L"blue_mines";
    const string_t s_blueLasersScene    = L"blue_lasers";
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

    SetupUI();

    InitializeInteractiveManager();

    std::weak_ptr<Sample> thisWeakPtr = shared_from_this();
    m_signInContext = m_liveResources->add_signin_handler([thisWeakPtr](
        _In_ std::shared_ptr<xbox::services::system::xbox_live_user> user,
        _In_ xbox::services::system::sign_in_status result
        )
    {
        std::shared_ptr<Sample> pThis(thisWeakPtr.lock());
        if (pThis != nullptr)
        {
            pThis->HandleSignin(user, result);
        }
    });

    m_signOutContext = xbox::services::system::xbox_live_user::add_sign_out_completed_handler(
        [thisWeakPtr](const xbox::services::system::sign_out_completed_event_args& args)
    {
        UNREFERENCED_PARAMETER(args);
        std::shared_ptr<Sample> pThis(thisWeakPtr.lock());
        if (pThis != nullptr)
        {
            pThis->HandleSignout(args.user());
        }
    });

    m_liveResources->SetLogCallback([thisWeakPtr](const std::wstring& log)
    {
        std::shared_ptr<Sample> pThis(thisWeakPtr.lock());
        if (pThis != nullptr)
        {
            pThis->m_console->WriteLine(log.c_str());
        }
    });
}

void Sample::HandleSignin(
    _In_ std::shared_ptr<xbox::services::system::xbox_live_user> user,
    _In_ xbox::services::system::sign_in_status result
)
{
    if (result == xbox::services::system::sign_in_status::success)
    {
        AddUserToInteractiveManager(user);
    }
}

void Sample::HandleSignout(_In_ std::shared_ptr<xbox::services::system::xbox_live_user> user)
{
}

void Sample::SetupDefaultUser(Windows::System::User^ systemUser)
{
    if (!ATG::LiveResources::IsMultiUserApplication() || systemUser != nullptr)
    {
        m_liveResources->SetupCurrentUser(systemUser);
        m_liveResources->TrySignInCurrentUserSilently();
    }
}

#pragma region UI Methods
void Sample::SetupUI()
{
    using namespace ATG;

    // Toggle interactivity
    m_ui->FindControl<Button>(c_sampleUIPanel, c_goInteractiveBtn)->SetCallback([this](IPanel*, IControl*)
    {
        ToggleInteractivity();
    });

    // Add another participant to one of the groups
    m_ui->FindControl<Button>(c_sampleUIPanel, c_placeParticipantBtn)->SetCallback([this](IPanel*, IControl*)
    {
        AddParticipantToGroup();
    });

    // Move all participants back to the default group
    m_ui->FindControl<Button>(c_sampleUIPanel, c_disbandGroupsBtn)->SetCallback([this](IPanel*, IControl*)
    {
        DisbandGroups();
    });

    // Trigger cooldowns on red group
    m_ui->FindControl<Button>(c_sampleUIPanel, c_cooldownRedBtn)->SetCallback([this](IPanel*, IControl*)
    {
        TriggerCooldownOnButtons(s_redGroup);
    });

    // Trigger cooldowns on blue group
    m_ui->FindControl<Button>(c_sampleUIPanel, c_cooldownBlueBtn)->SetCallback([this](IPanel*, IControl*)
    {
        TriggerCooldownOnButtons(s_blueGroup);
    });

    // Rotate through scenes defined for the groups
    m_ui->FindControl<Button>(c_sampleUIPanel, c_switchScenesBtn)->SetCallback([this](IPanel*, IControl*)
    {
        SwitchScenes();
    });
}
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

    for (int i = 0; i < m_gamePad->MAX_PLAYER_COUNT; i++)
    {
        auto pad = m_gamePad->GetState(i);
        if (pad.IsConnected())
        {
            auto systemUserId = m_gamePad->GetCapabilities(i).id;
            m_gamePadButtons[i].Update(pad);

            if (!m_ui->Update(elapsedTime, pad))
            {
                // UI is not consuming input, so implement sample gamepad controls
            }

            if (m_gamePadButtons[i].a == GamePad::ButtonStateTracker::PRESSED)
            {
                //If it's current gamepad, skip user setup
                if (i != m_liveResources->GetCurrentGamepadIndex())
                {
                    m_liveResources->SetCurrentGamepadAndUser(i, systemUserId);
                }

                m_liveResources->TrySignInCurrentUser();
            }

            if (m_gamePadButtons[i].x == GamePad::ButtonStateTracker::PRESSED)
            {
                this->m_console->WriteLine(L"Launching user picker");

                auto userPicker = ref new Windows::System::UserPicker();
                create_task(userPicker->PickSingleUserAsync())
                .then([this, i] (task<Windows::System::User^> task)
                {
                    try
                    {
                        auto user = task.get();
                        if (user != nullptr)
                        {
                            m_liveResources->SetCurrentGamepadAndUser(i, user);
                            m_liveResources->TrySignInCurrentUser();
                        }
                    }
                    catch (Platform::Exception^ ex)
                    {
                        this->m_console->WriteLine(ex->Message->Data());
                    }
                });
            }

            if (pad.IsViewPressed())
            {
                Windows::ApplicationModel::Core::CoreApplication::Exit();
            }
        }
        else
        {
            m_gamePadButtons[i].Reset();
        }
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
        m_liveResources->TrySignInCurrentUser();
    }

    if (m_interactivityManager != nullptr)
    {
        UpdateInteractivityManager();
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
    m_console->Render(true);

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

    const RECT* label = m_ui->FindControl<ATG::Image>(c_sampleUIPanel, c_debugLog)->GetRectangle();

    RECT console = { 0 };
    console.top = label->top;
    console.left = label->left;
    console.bottom = console.top + 600;
    console.right = console.left + 800;

    m_console->SetWindow(console);
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
