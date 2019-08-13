///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019, Intel Corporation
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

#include "Core/vaUI.h"

#include "IntegratedExternals/vaImguiIntegration.h"

using namespace VertexAsylum;

vaApplicationBase::vaApplicationBase( const Settings & settings, const std::shared_ptr<vaRenderDevice> & renderDevice )
    : m_settings( settings ), m_renderDevice( renderDevice ), vaUIPanel( "System & Performance", -10, true, vaUIPanel::DockLocation::DockedRight, "", vaVector2( 500, 550 ) )
{
    m_initialized = false;

    m_currentWindowPosition = vaVector2i( -1, -1 );
    m_currentWindowClientSize = vaVector2i( -1, -1 );
    m_lastNonFullscreenWindowClientSize = vaVector2i( -1, -1 );

    for( int i = 0; i < _countof( m_frametimeHistory ); i++ ) m_frametimeHistory[i] = 0.0f;
    m_frametimeHistoryLast = 0;
    m_avgFramerate = 0.0f;
    m_avgFrametime = 0.0f;
    m_accumulatedDeltaFrameTime = 0.0f;

    m_shouldQuit = false;
    m_running = false;

    m_hasFocus = false;

    m_blockInput    = false;

    m_cmdLineParams = vaStringTools::SplitCmdLineParams( m_settings.CmdLine );

    m_setWindowSizeNextFrame = vaVector2i( 0, 0 );

    m_vsync             = m_settings.Vsync;
    m_framerateLimit    = m_settings.FramerateLimit;
}

vaApplicationBase::~vaApplicationBase( )
{
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

void vaApplicationBase::NamedSerializeSettings( vaXMLSerializer & serializer )
{
    if( serializer.IsReading( ) )
    {
        vaVector2i windowPos( -1, -1 );
        serializer.Serialize<int>( "WindowPositionX", windowPos.x );
        serializer.Serialize<int>( "WindowPositionY", windowPos.y );
        if( windowPos.x != -1 && windowPos.y != -1 )
        {
            SetWindowPosition( windowPos );
        }
    }
    else
    {
        vaVector2i windowPos = GetWindowPosition();
        serializer.Serialize<int>( "WindowPositionX", windowPos.x );
        serializer.Serialize<int>( "WindowPositionY", windowPos.y );
    }

    if( serializer.IsReading( ) )
    {
        assert( !IsFullscreen() ); // expecting it to not be fullscreen here
        vaVector2i windowSize;
        serializer.Serialize<int>( "WindowClientSizeX", windowSize.x );
        serializer.Serialize<int>( "WindowClientSizeY", windowSize.y );
        if( windowSize.x > 0 && windowSize.y > 0 )
            m_setWindowSizeNextFrame = windowSize;
            //SetWindowClientAreaSize( windowSize );
        if( !IsFullscreen() )
            m_lastNonFullscreenWindowClientSize = windowSize;
    }
    else
    {
        vaVector2i windowSize = GetWindowClientAreaSize();
        if( IsFullscreen() )
            windowSize = m_lastNonFullscreenWindowClientSize;
        serializer.Serialize<int>( "WindowClientSizeX", windowSize.x );
        serializer.Serialize<int>( "WindowClientSizeY", windowSize.y );
    }

    vaFullscreenState fullscreenState = m_currentFullscreenState;

    serializer.Serialize<int>( "FullscreenState", reinterpret_cast<int&>(fullscreenState), reinterpret_cast<int const &>(m_settings.StartFullscreenState) );

    if( serializer.IsReading( ) )
    {
        if( GetFullscreenState() != fullscreenState )
            SetFullscreenState( fullscreenState );
    }

    if( serializer.SerializeOpenChildElement( "ApplicationSettings" ) )
    {
        Event_SerializeSettings.Invoke( serializer );

        bool ok = serializer.SerializePopToParentElement( "ApplicationSettings" );
        assert( ok ); ok;
    }

    if( serializer.SerializeOpenChildElement( "UISettings" ) )
    {
        vaUIManager::GetInstance().SerializeSettings( serializer );
        bool ok = serializer.SerializePopToParentElement( "UISettings" );
        assert( ok ); ok;
    }
}


void vaApplicationBase::Tick( float deltaTime )
{
    VA_SCOPE_CPU_TIMER( vaApplicationBase_Tick );

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

    TickUI( );

    {
        VA_SCOPE_CPU_TIMER( Event_Tick );
        Event_Tick.Invoke( deltaTime );
    }

    vaInputMouse::GetInstance( ).ResetWheelDelta( );

    if( vaCore::GetAppQuitFlag() )
        Quit();
}

bool vaApplicationBase::IsMouseCaptured( ) const
{
    return vaInputMouse::GetInstance( ).IsCaptured( );
}

void vaApplicationBase::UpdateFramerateStats( float deltaTime )
{
    VA_SCOPE_CPU_TIMER( vaApplicationBase_UpdateFramerateStats );

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

void vaApplicationBase::TickUI( )
{
    VA_SCOPE_CPU_TIMER( vaApplicationBase_TickUI );
    vaUIManager::GetInstance().UpdateUI( m_hasFocus );
    vaUIManager::GetInstance().DrawUI( );
}

void vaApplicationBase::UIPanelStandaloneMenu( )
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    if( ImGui::BeginMenu("System") )
    {
        if( ImGui::MenuItem("Recompile shaders", "CTRL+R") )
        {
            vaShader::ReloadAll();
        }
        bool fpsLimited = m_framerateLimit != 0;
        if( ImGui::MenuItem("Enable 10FPS limiter", "", &fpsLimited ) )
        {
            m_framerateLimit = (fpsLimited)?(10):(0);
        }
        ImGui::EndMenu();
    }
#endif
}


void vaApplicationBase::UIPanelDraw( )
{
    assert( vaUIManager::GetInstance().IsVisible() );

#ifdef VA_IMGUI_INTEGRATION_ENABLED

    // if( rightWindow )
    {
        ImVec4 infoColor = ImVec4( 1.0f, 1.0f, 0.0f, 1.0f );

        string frameInfo = vaStringTools::SimpleNarrow( GetBasicFrameInfoText() );

        // graph (there is some CPU/drawing cost to this)
        //if( (int)HelperUIFlags::ShowStatsGraph & (int)uiFlags )
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

            float graphWidth = ImGui::GetContentRegionAvailWidth() - 75.0f;

            ImGui::PushStyleColor( ImGuiCol_Text, infoColor );
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

        //if( (int)HelperUIFlags::ShowResolutionOptions & (int)uiFlags )
        {
            ImGui::Separator( );

            {
                bool fullscreen = IsFullscreen( );

                ImGui::PushItemWidth( ImGui::GetFontSize() * 8.0f );

                int ws[2] = { m_currentWindowClientSize.x, m_currentWindowClientSize.y };
                if( ImGui::InputInt2( "Resolution", ws, ImGuiInputTextFlags_EnterReturnsTrue | ( ( IsFullscreen( ) ) ? ( ImGuiInputTextFlags_ReadOnly ) : ( 0 ) ) ) )
                {
                    if( ( ws[0] != m_currentWindowClientSize.x ) || ( ws[1] != m_currentWindowClientSize.y ) )
                    {
                        m_setWindowSizeNextFrame = vaVector2i( ws[0], ws[1] );
                    }
                }
                if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Edit and press enter to change resolution. Works only in Windowed, Fullscreen modes currently can only use desktop resolution." );
                ImGui::PopItemWidth( );
                ImGui::SameLine();
                ImGui::VerticalSeparator();
                ImGui::SameLine();

                bool wasFullscreen = fullscreen;

                if( GetFullscreenState( ) == vaFullscreenState::FullscreenBorderless )
                    ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.0f, 1.0f, 0.0f, 1.0f ) );
                ImGui::Checkbox( "Fullscreen", &fullscreen );
                if( GetFullscreenState( ) == vaFullscreenState::FullscreenBorderless )
                    ImGui::PopStyleColor( );

                if( ImGui::IsItemHovered( ) ) 
                {
                    if( GetFullscreenState( ) == vaFullscreenState::FullscreenBorderless )
                        ImGui::SetTooltip( "Currently in Fullscreen Borderless. Click to switch to Windowed." );
                    else if( GetFullscreenState( ) == vaFullscreenState::Fullscreen )
                        ImGui::SetTooltip( "Currently in Fullscreen. Click to switch to Windowed." );
                    else if( GetFullscreenState( ) == vaFullscreenState::Windowed )
                        ImGui::SetTooltip( "Currently in Windowed. Click to switch to Fullscreen or hold Shift and click to switch to Fullscreen Borderless." );
                }


                if( wasFullscreen != fullscreen )
                {
                    if( vaInputKeyboard::GetInstance().IsKeyDown( KK_SHIFT ) )
                        SetFullscreenState( ( fullscreen ) ? ( vaFullscreenState::FullscreenBorderless ) : ( vaFullscreenState::Windowed ) );
                    else
                        SetFullscreenState( ( fullscreen ) ? ( vaFullscreenState::Fullscreen ) : ( vaFullscreenState::Windowed ) );
                }

                ImGui::SameLine();
                ImGui::VerticalSeparator();
                ImGui::SameLine();
                ImGui::Checkbox( "Vsync", &m_vsync );
                if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Enable/disable vsync. Even with vsync off there can be a sync in some API/driver/mode combinations." );
                ImGui::Separator();
                {
                    char buff1[256]; strcpy_s( buff1, _countof(buff1), m_renderDevice->GetAPIName().c_str() );
                    char buff2[256]; strcpy_s( buff2, _countof(buff2), m_renderDevice->GetAdapterNameID().c_str() );

                    string buttonInfo = vaStringTools::Format( "%s, %s (click to change)", m_renderDevice->GetAPIName().c_str(), m_renderDevice->GetAdapterNameID().c_str() );
                    static int aaSelection = -1;
                    if( ImGui::Button( buttonInfo.c_str() ) )
                    {
                        ImGui::OpenPopup( "Select API and Adapter" );
                    }
                    if( ImGui::BeginPopupModal( "Select API and Adapter", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse ) )
                    {
                        vector<string> arr;
                        for( int i = 0; i < m_enumeratedAPIsAdapters.size(); i++ )
                        {
                            auto & aa = m_enumeratedAPIsAdapters[i];
                            arr.push_back( aa.first + " : " + aa.second );
                            if( aaSelection == -1 && aa.first == m_renderDevice->GetAPIName() && aa.second == m_renderDevice->GetAdapterNameID() )
                                aaSelection = i;
                        }
                        ImGui::PushItemWidth( -1 );
                        ImGuiEx_ListBox( "###APIAdapter", aaSelection, arr );
                        ImGui::PopItemWidth( );

                        ImGui::InvisibleButton( "spacer", ImVec2(ImGui::GetFontSize() * 12.0f, 0.1f) );
                        ImGui::SameLine();
                        if( ImGui::Button( "Select and restart", ImVec2(ImGui::GetFontSize() * 12.0f, 0) ) )
                        {
                            SaveDefaultGraphicsAPIAdapter( m_enumeratedAPIsAdapters[aaSelection] );
                            vaCore::SetAppQuitFlag( true, true );
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::SameLine();
                        ImGui::SetItemDefaultFocus();
                        if( ImGui::Button( "Cancel", ImVec2(ImGui::GetFontSize() * 12.0f, 0) ) )
                        {
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::EndPopup();
                    }
                    else
                        aaSelection = -1;
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

        //if( (int)HelperUIFlags::ShowGPUProfiling & (int)uiFlags )
        {
            ImGui::Separator( );

            if( ImGui::CollapsingHeader( "GPU Profiling", ImGuiTreeNodeFlags_Framed | ((/*m_helperUISettings.GPUProfilerDefaultOpen*/true)?(ImGuiTreeNodeFlags_DefaultOpen):(0)) ) )
            {
                ImGui::Text( "Note: numbers below based on GPU timestamp queries" );
                vaProfiler::GetInstance( ).InsertImGuiContent( false );
            }
        }
        //if( (int)HelperUIFlags::ShowCPUProfiling & (int)uiFlags )
        //
        //{
        //    ImGui::Separator( );
        //
        //    if( ImGui::CollapsingHeader( "CPU Profiling", ImGuiTreeNodeFlags_Framed | ((/*m_helperUISettings.CPUProfilerDefaultOpen*/false)?(ImGuiTreeNodeFlags_DefaultOpen):(0)) ) )
        //    {
        //        vaProfiler::GetInstance( ).InsertImGuiContent( true );
        //    }
        //}
    }

#endif

}

void vaApplicationBase::SaveDefaultGraphicsAPIAdapter( pair<string, string> apiAdapter )
{
    const wstring settingsFileName = GetDefaultGraphicsAPIAdapterInfoFileName( );

    vaFileStream settingsFile;
    if( settingsFile.Open( settingsFileName, FileCreationMode::Create ) )
    {
        settingsFile.WriteValue<int64>(42);
        settingsFile.WriteString( apiAdapter.first );
        settingsFile.WriteString( " - " );
        settingsFile.WriteString( apiAdapter.second );
    }
    else
    {
        VA_WARN( "Unable to open '%s'", settingsFileName.c_str() );
    }
    settingsFile.Close();

    assert( apiAdapter == LoadDefaultGraphicsAPIAdapter() );
}

pair<string, string> vaApplicationBase::LoadDefaultGraphicsAPIAdapter( )
{
    const wstring settingsFileName = GetDefaultGraphicsAPIAdapterInfoFileName( );

    vaFileStream settingsFile;
    if( settingsFile.Open( settingsFileName, FileCreationMode::Open ) )
    {
        int64 header; string a, b, c;

        if( settingsFile.ReadValue<int64>( header ) && header == 42 &&
            settingsFile.ReadString( a ) &&
            settingsFile.ReadString( b ) &&
            settingsFile.ReadString( c ) )
            return make_pair(a, c);
        else
            VA_WARN( "Unable to read '%s'", settingsFileName.c_str() );
    }
    else
        VA_WARN( "Unable to open '%s'", settingsFileName.c_str() );
    return make_pair(string(""), string(""));
}
