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

#pragma once

#include "Core/vaCoreIncludes.h"

#include "Core/vaEvent.h"

#include "Core/vaUI.h"

#include "Rendering/vaRendering.h"

namespace VertexAsylum
{
    class vaRenderDevice;
    class vaXMLSerializer;
    class vaApplicationBase;

    // just keeping events separate for clarity - no other reason
    class vaApplicationEvents
    {
    public:
        vaEvent<void(void)>                 Event_Started;
        vaEvent<void(void)>                 Event_BeforeStopped;
        vaEvent<void(void)>                 Event_Stopped;
        vaEvent<void(void)>                 Event_MouseCaptureChanged;

        vaEvent<void( float )>              Event_Tick;

        vaEvent<void( vaXMLSerializer & )>  Event_SerializeSettings;

    };

    class vaApplicationBase : public vaApplicationEvents, protected vaUIPanel
    {
    public:
        struct Settings
        {
            wstring             AppName;
            wstring             CmdLine;

            int                 StartScreenPosX;
            int                 StartScreenPosY;
            int                 StartScreenWidth;
            int                 StartScreenHeight;
            vaFullscreenState   StartFullscreenState;

            bool                UpdateWindowTitleWithBasicFrameInfo;

            bool                Vsync;

            int                 FramerateLimit;

            Settings( const wstring & appName = L"Name me plz", const wstring & cmdLine = L"" ) : AppName(appName), CmdLine( cmdLine )
            {
                StartScreenPosX                     = -1;
                StartScreenPosY                     = -1;
                StartScreenWidth                    = 1920;
                StartScreenHeight                   = 1080;
                StartFullscreenState                = vaFullscreenState::Windowed;
                UpdateWindowTitleWithBasicFrameInfo = true;
                Vsync                               = false;
                FramerateLimit                      = 0;
            }
        };

    protected:
        const Settings                      m_settings;
        vector<pair<string, string>>        m_enumeratedAPIsAdapters;

        bool                                m_initialized;

        std::shared_ptr<vaRenderDevice>     m_renderDevice;

        vaSystemTimer                       m_mainTimer;
        //
        vaVector2i                          m_currentWindowClientSize;
        vaVector2i                          m_currentWindowPosition;
        vaVector2i                          m_lastNonFullscreenWindowClientSize;
        //
        static const int                    c_framerateHistoryCount = 96;
        float                               m_frametimeHistory[c_framerateHistoryCount];
        int                                 m_frametimeHistoryLast;
        float                               m_avgFramerate;
        float                               m_avgFrametime;
        float                               m_accumulatedDeltaFrameTime;
        //
        bool                                m_running;
        bool                                m_shouldQuit;

        bool                                m_hasFocus;

        bool                                m_blockInput;

        // switch/value commandline pairs
        std::vector< std::pair<wstring, wstring> > m_cmdLineParams;

        wstring                             m_basicFrameInfo;

        // used for delayed switch to fullscreen or window size change
        vaVector2i                          m_setWindowSizeNextFrame;                                       // only works if not fullscreen 
        vaFullscreenState                   m_setFullscreenStateNextFrame   = vaFullscreenState::Unknown;   // next state

        bool                                m_vsync;
        
        int                                 m_framerateLimit;

        vaFullscreenState                   m_currentFullscreenState        = vaFullscreenState::Unknown;   // this track current/last state and is valid after window is destroyed too (to enable serialization)

        const float                         m_windowTitleInfoUpdateFrequency    = 0.1f;                     // at least every 100ms - should be enough
        float                               m_windowTitleInfoTimeFromLastUpdate = 0;

    protected:
        vaApplicationBase( const Settings & settings, const std::shared_ptr<vaRenderDevice> & renderDevice );
        virtual ~vaApplicationBase( );

    protected:
        void                                UpdateFramerateStats( float deltaTime );
        virtual void                        Initialize( );
        //
        void                                TickUI( );
        //
        virtual void                        UIPanelDraw( ) override;
        //
    public:
        // run the main loop!
        virtual void                        Run( )                                  = 0;
        // quit the app (will finish current frame)
        void                                Quit( );
        //
    public:
        const vaSystemTimer &               GetMainTimer( ) const                   { return m_mainTimer; }
        double                              GetTimeFromStart( ) const               { return m_mainTimer.GetTimeFromStart(); }
        //        //
        float                               GetAvgFramerate( ) const                { return m_avgFramerate; }
        float                               GetAvgFrametime( ) const                { return m_avgFrametime; }
        //
        virtual void                        CaptureMouse( )                         = 0;
        virtual void                        ReleaseMouse( )                         = 0;
        virtual bool                        IsMouseCaptured( ) const;
        //
        const Settings &                    GetSettings( ) const                    { return m_settings; }
        //
        vaFullscreenState                   GetFullscreenState( ) const             { return ( m_setFullscreenStateNextFrame != vaFullscreenState::Unknown ) ? ( m_setFullscreenStateNextFrame ) : ( m_currentFullscreenState ); }
        void                                SetFullscreenState( vaFullscreenState state ) { m_setFullscreenStateNextFrame = state; }
        bool                                IsFullscreen( ) const                   { return GetFullscreenState() != vaFullscreenState::Windowed; }

        bool                                HasFocus( ) const                       { return m_hasFocus; }
        
        bool                                IsInputBlocked( ) const                 { return m_blockInput; }
        void                                SetBlockInput( bool blockInput )        { m_blockInput = blockInput; }

        const vector<pair<wstring,wstring>> &      GetCommandLineParameters( ) const       { return m_cmdLineParams; }

        const wstring &                     GetBasicFrameInfoText( )                { return m_basicFrameInfo; }

        vaRenderDevice &                    GetRenderDevice( )                      { return *m_renderDevice; }

        bool                                GetVsync( ) const                       { return m_vsync; }
        void                                SetVsync( bool vsync )                  { m_vsync = vsync; }

        int                                 GetFramerateLimit( ) const              { return m_framerateLimit; }
        void                                SetFramerateLimit( int fpsLimit )       { m_framerateLimit = fpsLimit; }

        virtual vaVector2i                  GetWindowPosition( ) const = 0;
        virtual void                        SetWindowPosition( const vaVector2i & position ) = 0;
        virtual vaVector2i                  GetWindowClientAreaSize( ) const = 0;
        virtual void                        SetWindowClientAreaSize( const vaVector2i & clientSize ) = 0;

        virtual void                        OnGotFocus( );
        virtual void                        OnLostFocus( );

        virtual void                        Tick( float deltaTime );

        virtual wstring                     GetSettingsFileName()                   { return vaCore::GetExecutableDirectory( ) + L"ApplicationSettings.xml"; }
        virtual void                        NamedSerializeSettings( vaXMLSerializer & serializer );

    public:
        virtual void                        UIPanelStandaloneMenu( ) override;

    private:
        //
        bool                                UpdateUserWindowChanges( );

    public:
        static wstring                      GetDefaultGraphicsAPIAdapterInfoFileName()  { return vaCore::GetExecutableDirectory( ) + L"APIAdapter"; }
        static void                         SaveDefaultGraphicsAPIAdapter( pair<string, string> apiAdapter );
        static pair<string, string>         LoadDefaultGraphicsAPIAdapter( );
    };
}