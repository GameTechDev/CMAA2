///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016, Intel Corporation
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
// documentation files (the "Software"), to deal in the Software without restriction, including without limitation 
// the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of 
// the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
// SOFTWARE.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Author(s):  Filip Strugar (filip.strugar@intel.com)
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "vaApplicationBase.h"

#include "Core\System\vaFileTools.h"

#include "Rendering/vaAssetPack.h"

#include "vaInputMouse.h"

#include "Rendering/vaRenderDevice.h"

#include "Core/Misc/vaProfiler.h"

//#include <windows.h>

using namespace VertexAsylum;

// class Test1 : public vaDevGUIWindow
// {
// public:
//     virtual string                                          GUI_GetWindowName( ) const override     { return "Test1"; }
// };
// 
// class Test2 : public vaDevGUIWindow
// {
//     virtual string                                          GUI_GetWindowName( ) const override     { return "Test2"; }
// };

vaApplicationBase::vaApplicationBase( Settings & settings, const std::shared_ptr<vaRenderDevice> &  renderDevice, const wstring & cmdLine )
    : m_settings( settings ), m_renderDevice( renderDevice )
{
    m_initialized = false;

    m_currentWindowPosition = vaVector2i( -1, -1 );
    m_currentWindowClientSize = vaVector2i( -1, -1 );

    for( int i = 0; i < _countof( m_frametimeHistory ); i++ ) m_frametimeHistory[i] = 0.0f;
    m_frametimeHistoryLast = 0;
    m_avgFramerate = 0.0f;
    m_avgFrametime = 0.0f;
    m_accumulatedDeltaFrameTime = 0.0f;

    m_shouldQuit = false;
    m_running = false;

    if( !m_settings.AllowFullscreen && m_settings.StartFullscreen )
    {
        VA_ASSERT_ALWAYS( L"Incompatible vaApplicationBase::Settings" );
    }

    m_hasFocus = false;

    m_blockInput    = false;

    m_cmdLineParams = vaStringTools::SplitCmdLineParams( cmdLine );

    m_toggleFullscreenNextFrame = false;
    m_setWindowSizeNextFrame = vaVector2i( 0, 0 );

    m_vsync             = m_settings.Vsync;
    m_framerateLimit    = m_settings.FramerateLimit;

    // assert( vaDevGUI::GetInstancePtr() == nullptr );
    // new vaDevGUI();

    // new Test1( );
    // new Test2( );
}

vaApplicationBase::~vaApplicationBase( )
{
    // assert( vaDevGUI::GetInstancePtr() != nullptr );
 //   delete vaDevGUI::GetInstancePtr();

    assert( !m_running );
}

void vaApplicationBase::Quit( )
{
    assert( m_running );

    m_shouldQuit = true;
}

void vaApplicationBase::Initialize( )
{
    m_initialized = true;
}

void vaApplicationBase::SerializeSettings( vaXMLSerializer & serializer )
{
    if( serializer.SerializeOpenChildElement( "RootSettings" ) )
    {
        if( serializer.IsReading( ) )
        {
            vaVector2i windowPos( -1, -1 );
            serializer.SerializeValue( "WindowPositionX", windowPos.x );
            serializer.SerializeValue( "WindowPositionY", windowPos.y );
            if( windowPos.x != -1 && windowPos.y != -1 )
            {
                SetWindowPosition( windowPos );
            }
        }
        else
        {
            vaVector2i windowPos = GetWindowPosition();
            serializer.SerializeValue( "WindowPositionX", windowPos.x );
            serializer.SerializeValue( "WindowPositionY", windowPos.y );
        }

        if( serializer.IsReading( ) )
        {
            vaVector2i windowSize;
            serializer.SerializeValue( "WindowClientSizeX", windowSize.x );
            serializer.SerializeValue( "WindowClientSizeY", windowSize.y );
            if( windowSize.x > 0 && windowSize.y > 0 )
                SetWindowClientAreaSize( windowSize );
        }
        else
        {
            vaVector2i windowSize = GetWindowClientAreaSize();
            serializer.SerializeValue( "WindowClientSizeX", windowSize.x );
            serializer.SerializeValue( "WindowClientSizeY", windowSize.y );
        }

        if( serializer.SerializeOpenChildElement( "ApplicationSettings" ) )
        {
            Event_SerializeSettings.Invoke( serializer );

            bool ok = serializer.SerializePopToParentElement( "ApplicationSettings" );
            assert( ok ); ok;
        }

        bool ok = serializer.SerializePopToParentElement( "RootSettings" );
        assert( ok ); ok;
    }

}


void vaApplicationBase::Tick( float deltaTime )
{
    if( m_blockInput && IsMouseCaptured() )
        this->ReleaseMouse();

    if( m_hasFocus )
    {
        vaInputMouse::GetInstance( ).Tick( deltaTime );
        vaInputKeyboard::GetInstance( ).Tick( deltaTime );
    }
    else
    {
        vaInputMouse::GetInstance( ).ResetAll( );
        vaInputKeyboard::GetInstance( ).ResetAll( );
    }

    // must be a better way to do this
    if( m_blockInput )
    {
        vaInputMouse::GetInstance( ).ResetAll();
        vaInputKeyboard::GetInstance( ).ResetAll();
    }

    if( !m_blockInput && HasFocus() && vaInputMouse::GetInstance( ).IsKeyClicked( MK_Right ) )
    {
        if( IsMouseCaptured( ) )
            this->ReleaseMouse( );
        else
            this->CaptureMouse( );
    }

    if( m_hasFocus )
    {
        // recompile shaders if needed - not sure if this should be here...
        if( ( vaInputKeyboardBase::GetCurrent( ) != NULL ) && vaInputKeyboardBase::GetCurrent( )->IsKeyDown( KK_CONTROL ) )
        {
            if( vaInputKeyboardBase::GetCurrent( )->IsKeyClicked( ( vaKeyboardKeys )'R' ) )
                vaShader::ReloadAll( );
        }

        // I guess this is good place as any... it must not happen between ImGuiNewFrame and Draw though!
        if( vaInputKeyboardBase::GetCurrent( )->IsKeyClicked( KK_F1 ) )
            GetRenderDevice().ImGuiSetVisible( !GetRenderDevice().ImGuiIsVisible( ) );
    }

    Event_Tick.Invoke( deltaTime );

    vaInputMouse::GetInstance( ).ResetWheelDelta( );

    if( vaCore::GetAppQuitFlag() )
        Quit();
}

bool vaApplicationBase::IsMouseCaptured( ) const
{
    return vaInputMouse::GetInstance( ).IsCaptured( );
}

void vaApplicationBase::OnResized( int width, int height, bool windowed )
{
    assert( width == m_currentWindowClientSize.x );
    assert( height == m_currentWindowClientSize.y );

    Event_WindowResized.Invoke( width, height, windowed );
}

void vaApplicationBase::UpdateFramerateStats( float deltaTime )
{
    m_frametimeHistoryLast = ( m_frametimeHistoryLast + 1 ) % _countof( m_frametimeHistory );

    // add this frame's time to the accumulated frame time
    m_accumulatedDeltaFrameTime += deltaTime;

    // remove oldest frame time from the accumulated frame time
    m_accumulatedDeltaFrameTime -= m_frametimeHistory[m_frametimeHistoryLast];

    m_frametimeHistory[m_frametimeHistoryLast] = deltaTime;

    m_avgFrametime = ( m_accumulatedDeltaFrameTime / (float)_countof( m_frametimeHistory ) );
    m_avgFramerate = 1.0f / m_avgFrametime;

    float avgFramerate = GetAvgFramerate( );
    float avgFrametimeMs = GetAvgFrametime( ) * 1000.0f;
    m_basicFrameInfo = vaStringTools::Format( L"%.2fms/frame avg (%.2fFPS, %dx%d)", avgFrametimeMs, avgFramerate, m_currentWindowClientSize.x, m_currentWindowClientSize.y );
#ifdef _DEBUG
    m_basicFrameInfo += L" DEBUG";
#endif
}

void vaApplicationBase::OnGotFocus( )
{
    vaInputKeyboard::GetInstance( ).ResetAll( );
    vaInputMouse::GetInstance( ).ResetAll( );

    m_hasFocus = true;
}

void vaApplicationBase::OnLostFocus( )
{
    vaInputKeyboard::GetInstance( ).ResetAll( );
    vaInputMouse::GetInstance( ).ResetAll( );
    
    m_hasFocus = false;
}


void vaApplicationBase::InsertImGuiWindow( )
{
    if( !m_renderDevice->ImGuiIsVisible() )
        return;

//    if( vaDevGUI::GetInstancePtr() != nullptr )
//        vaDevGUI::GetInstance().Draw();

#ifdef VA_IMGUI_INTEGRATION_ENABLED

    HelperUIFlags uiFlags = m_helperUISettings.Flags;

    if( HasFocus( ) && !vaInputMouseBase::GetCurrent( )->IsCaptured( ) )
    {
        vaInputKeyboardBase & keyboard = *vaInputKeyboardBase::GetCurrent( );
        if( keyboard.IsKeyClicked( KK_OEM_3 ) )
        {
            // m_helperUISettings.ConsoleLogOpen = !m_helperUISettings.ConsoleLogOpen;
            vaConsole::GetInstance().SetConsoleOpen( !vaConsole::GetInstance().IsConsoleOpen() );
        }
    }

    bool rightWindow = ( ((int)HelperUIFlags::ShowStatsGraph | (int)HelperUIFlags::ShowResolutionOptions |  (int)HelperUIFlags::ShowGPUProfiling | (int)HelperUIFlags::ShowCPUProfiling | (int)HelperUIFlags::ShowAssetPackManager ) & (int)uiFlags ) != 0;
    // info stuff on the right
    if( rightWindow )
    {
        int sizeX = 500;
        int sizeY = 550;
        float appWindowLeft = (float)( m_currentWindowClientSize.x - sizeX - 6.0f );
        float appWindowTop = 6.0f;
        float appWindowBottom = appWindowTop;

        ImGui::SetNextWindowPos( ImVec2( appWindowLeft, appWindowTop ), ImGuiCond_Always );     // Normally user code doesn't need/want to call it because positions are saved in .ini file anyway. Here we just want to make the demo initial state a bit more friendly!
        ImGui::SetNextWindowSize( ImVec2( (float)sizeX, (float)sizeY ), ImGuiCond_Once );
        ImGui::SetNextWindowCollapsed( !m_settings.ApplicationInfoWindowOpenByDefault, ImGuiCond_FirstUseEver );
        if( ImGui::Begin( "Application", 0, ImVec2(0.f, 0.f), 0.7f, 0 ) )
        {
            ImVec4 fpsInfoColor = ImVec4( 1.0f, 1.0f, 0.0f, 1.0f );

            // Adopting David Bregman's FPS info style
            //ImGui::TextColored( fpsInfoColor, frameInfo.c_str( ) );

            string frameInfo = vaStringTools::SimpleNarrow( GetBasicFrameInfoText() );

            // graph (there is some CPU/drawing cost to this)
            if( (int)HelperUIFlags::ShowStatsGraph & (int)uiFlags )
            {
                float frameTimeMax = 0.0f;
                float frameTimeMin = VA_FLOAT_HIGHEST;
                float frameTimesMS[_countof( m_frametimeHistory )];
                float frameTimeAvg = 0.0f;
                for( int i = 0; i < _countof( m_frametimeHistory ); i++ )
                {
                    frameTimesMS[i] = m_frametimeHistory[(i+m_frametimeHistoryLast+1) % _countof( m_frametimeHistory )] * 1000.0f;
                    frameTimeMax = vaMath::Max( frameTimeMax, frameTimesMS[i] );
                    frameTimeMin = vaMath::Min( frameTimeMin, frameTimesMS[i] );
                    frameTimeAvg += frameTimesMS[i];
                }
                frameTimeAvg /= (float)_countof( m_frametimeHistory );

                static float avgFrametimeGraphMax = 1.0f;
                avgFrametimeGraphMax = vaMath::Lerp( avgFrametimeGraphMax, frameTimeMax * 1.5f, 0.05f );
                avgFrametimeGraphMax = vaMath::Min( 1000.0f, vaMath::Max( avgFrametimeGraphMax, frameTimeMax * 1.1f ) );

                float graphWidth = sizeX - 110.0f;

                ImGui::PushStyleColor( ImGuiCol_Text, fpsInfoColor );
                ImGui::PushStyleColor( ImGuiCol_PlotLines, ImVec4( 1.0f, 1.0f, 1.0f, 1.0f ) );
                ImGui::PlotLines( "", frameTimesMS, _countof(frameTimesMS), 0, frameInfo.c_str(), 0.0f, avgFrametimeGraphMax, ImVec2( graphWidth, 100.0f ) );
                ImGui::PopStyleColor( 2 );

                ImGui::SameLine( );
                
                ImGui::BeginGroup( );
                {
                    ImGui::Text( "\nmin: %.2f", frameTimeMin );
                    ImGui::Text( "\nmax: %.2f", frameTimeMax );
                    ImGui::Text( "\navg: %.2f", frameTimeAvg );
                }
                ImGui::EndGroup( );
            }

            if( (int)HelperUIFlags::ShowResolutionOptions & (int)uiFlags )
            {
                ImGui::Separator( );

                {
                    bool fullscreen = IsFullscreen( );

                    int ws[2] = { m_currentWindowClientSize.x, m_currentWindowClientSize.y };
                    if( ImGui::InputInt2( "Window size", ws, ImGuiInputTextFlags_EnterReturnsTrue | ( ( IsFullscreen( ) ) ? ( ImGuiInputTextFlags_ReadOnly ) : ( 0 ) ) ) )
                    {
                        if( ( ws[0] != m_currentWindowClientSize.x ) || ( ws[1] != m_currentWindowClientSize.y ) )
                        {
                            m_setWindowSizeNextFrame = vaVector2i( ws[0], ws[1] );
                        }
                    }

                    bool wasFullscreen = fullscreen;
                    ImGui::Checkbox( "Fullscreen (borderless windowed)", &fullscreen );
                    if( wasFullscreen != fullscreen )
                        ToggleFullscreen( );
                    ImGui::SameLine();
                    ImGui::Checkbox( "Vsync", &m_vsync );
                    if( ImGui::Button( "Recompile modified shaders (Ctrl+R)" ) )
                    {
                        vaShader::ReloadAll();
                    }


#ifdef VA_REMOTERY_INTEGRATION_ENABLED
                    if( ImGui::Button( "Launch Remotery profiler" ) )
                    {
                        wstring remoteryPath = vaCore::GetExecutableDirectory() + L"../../Modules/IntegratedExternals/remotery/vis/index.html";
                        if( !vaFileTools::FileExists( remoteryPath ) )
                            remoteryPath = vaCore::GetExecutableDirectory() + L"remotery/vis/index.html";
                        if( vaFileTools::FileExists( remoteryPath ) )
                        {
                            std::thread([remoteryPath](){ system( vaStringTools::SimpleNarrow( remoteryPath ).c_str() ); }).detach();
                        }
                        else
                        {
                            VA_LOG_WARNING( "Cannot find Remotery html interface on '%s'", remoteryPath.c_str() );
                        }
                    }
#endif
                }
            }

            if( (int)HelperUIFlags::ShowGPUProfiling & (int)uiFlags )
            {
                ImGui::Separator( );

                if( ImGui::CollapsingHeader( "GPU Profiling", ImGuiTreeNodeFlags_Framed | ((m_helperUISettings.GPUProfilerDefaultOpen)?(ImGuiTreeNodeFlags_DefaultOpen):(0)) ) )
                {
                    ImGui::Text( "Note: numbers below based on D3D timestamp queries" );
                    ImGui::Text( "and might not be entirely correct/realistic on all HW." );
                    vaProfiler::GetInstance( ).InsertImGuiContent( false );
                }
            }
            if( (int)HelperUIFlags::ShowCPUProfiling & (int)uiFlags )
            {
                ImGui::Separator( );

                if( ImGui::CollapsingHeader( "CPU Profiling", ImGuiTreeNodeFlags_Framed | ((m_helperUISettings.CPUProfilerDefaultOpen)?(ImGuiTreeNodeFlags_DefaultOpen):(0)) ) )
                {
                    vaProfiler::GetInstance( ).InsertImGuiContent( true );
                }
            }
        }
        appWindowBottom = appWindowTop + ImGui::GetWindowSize().y;
        ImGui::End( );

        if( (int)HelperUIFlags::ShowAssetPackManager & (int)uiFlags )
        {
            ImGui::SetNextWindowPos( ImVec2( appWindowLeft, appWindowBottom + 6.0f ), ImGuiCond_Always );     // Normally user code doesn't need/want to call it because positions are saved in .ini file anyway. Here we just want to make the demo initial state a bit more friendly!
            ImGui::SetNextWindowSize( ImVec2( (float)sizeX, (float)sizeY ), ImGuiCond_Once );
            ImGui::SetNextWindowCollapsed( true, ImGuiCond_FirstUseEver );
            if( ImGui::Begin( "Assets", 0, ImVec2(0.f, 0.f), 0.7f, 0 ) )
            {
                m_renderDevice->GetAssetPackManager().IHO_Draw();
                //vaImguiHierarchyObject::DrawCollapsable( vaAssetPackManager::GetInstance(), false, false, false );
                ImGui::Separator();
                ((vaImguiHierarchyObject&)m_renderDevice->GetAssetPackManager().GetImporter()).IHO_Draw();
                //vaImguiHierarchyObject::DrawCollapsable( m_assetImporter, false, false );
            }
            ImGui::End( );
        }
    }

    // console & log at the bottom
    if( (int)HelperUIFlags::ShowConsoleWindow & (int)uiFlags )
    {
        vaConsole::GetInstance().Draw( m_currentWindowClientSize.x, m_currentWindowClientSize.y );
    }

#endif
}

