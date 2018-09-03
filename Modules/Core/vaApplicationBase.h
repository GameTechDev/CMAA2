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

#pragma once

#include "Core/vaCoreIncludes.h"

#include "Core/vaEvent.h"

namespace VertexAsylum
{
    class vaRenderDevice;
    class vaXMLSerializer;

    // just keeping events separate for clarity - no other reason
    class vaApplicationEvents
    {
    public:
        vaEvent<void(void)>                 Event_Started;
        vaEvent<void(void)>                 Event_BeforeStopped;
        vaEvent<void(void)>                 Event_Stopped;
        vaEvent<void(void)>                 Event_MouseCaptureChanged;

        //vaEvent<void( void )>               Event_BeforeWindowResized;              // width, height, fullscreen
        vaEvent<void( int, int, bool )>     Event_WindowResized;                    // width, height, fullscreen

        vaEvent<void( float )>              Event_Tick;

        vaEvent<void( vaXMLSerializer & )>  Event_SerializeSettings;

    };

    class vaApplicationBase : public vaApplicationEvents
    {
    public:
        struct Settings
        {
            wstring         AppName;
            wstring         CmdLine;

            int             StartScreenPosX;
            int             StartScreenPosY;
            int             StartScreenWidth;
            int             StartScreenHeight;
            bool            StartFullscreen;

            bool            AllowFullscreen;

            bool            UpdateWindowTitleWithBasicFrameInfo;

            bool            Vsync;

            bool            ApplicationInfoWindowOpenByDefault;

            int             FramerateLimit;

            Settings( )
            {
                AppName             = L"Name me plz";
                CmdLine             = L"";

                StartScreenPosX                     = -1;
                StartScreenPosY                     = -1;
                StartScreenWidth                    = 1920;
                StartScreenHeight                   = 1080;
                StartFullscreen                     = false;
                AllowFullscreen                     = true;
                UpdateWindowTitleWithBasicFrameInfo = false;
                Vsync                               = false;
                FramerateLimit                      = 0;
                ApplicationInfoWindowOpenByDefault  = true;
            }
        };

        enum class HelperUIFlags
        {
            None                    = 0,
            ShowStatsGraph          = ( 1 << 1 ),
            ShowResolutionOptions   = ( 1 << 2 ),
            ShowGPUProfiling        = ( 1 << 3 ),
            ShowCPUProfiling        = ( 1 << 4 ),
            ShowAssetPackManager    = ( 1 << 5 ),
            ShowConsoleWindow       = ( 1 << 6 ),
       
        };

        struct HelperUISettings
        {
            HelperUIFlags           Flags;
            bool                    GPUProfilerDefaultOpen;
            bool                    CPUProfilerDefaultOpen;
            bool                    ConsoleLogOpen;
            int                     ConsoleVSrollPos;   // functionality not implemented yet

            HelperUISettings( )
            {
                Flags = HelperUIFlags(
                (int)HelperUIFlags::ShowStatsGraph           | 
                (int)HelperUIFlags::ShowResolutionOptions    | 
                (int)HelperUIFlags::ShowGPUProfiling         |
                (int)HelperUIFlags::ShowCPUProfiling         |
                (int)HelperUIFlags::ShowAssetPackManager           |
                (int)HelperUIFlags::ShowConsoleWindow );
                
                GPUProfilerDefaultOpen  = false;
                CPUProfilerDefaultOpen  = false;
                ConsoleLogOpen          = false;
                ConsoleVSrollPos     = 0;
            }
        };

    protected:
        const Settings                      m_settings;
        HelperUISettings                    m_helperUISettings;

        bool                                m_initialized;

        std::shared_ptr<vaRenderDevice>     m_renderDevice;

        vaSystemTimer                       m_mainTimer;
        //
        vaVector2i                          m_currentWindowClientSize;
        vaVector2i                          m_currentWindowPosition;
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
        bool                                m_toggleFullscreenNextFrame;
        vaVector2i                          m_setWindowSizeNextFrame;

        bool                                m_vsync;
        
        int                                 m_framerateLimit;

    protected:
        vaApplicationBase( Settings & settings, const std::shared_ptr<vaRenderDevice> & renderDevice, const wstring & cmdLine );
        virtual ~vaApplicationBase( );

    public:
        virtual void                        Initialize( )  ;
        //
    protected:
        void                                UpdateFramerateStats( float deltaTime );
        //
    public:
        // run the main loop!
        virtual void                        Run( )                                  = 0;
        // quit the app (will finish current frame)
        void                                Quit( );
        //
    public:
        const vaSystemTimer &               GetMainTimer( ) const                   { return m_mainTimer; }
        //        //
        float                               GetAvgFramerate( ) const                { return m_avgFramerate; }
        float                               GetAvgFrametime( ) const                { return m_avgFrametime; }
        //
        virtual void                        CaptureMouse( )                         = 0;
        virtual void                        ReleaseMouse( )                         = 0;
        virtual bool                        IsMouseCaptured( ) const;
        //
        const Settings &                    GetSettings( ) const                    { return m_settings; }
        HelperUISettings &                  HelperUISettings( )                     { return m_helperUISettings; }
        //
        virtual bool                        IsFullscreen( ) const                   = 0;
        virtual void                        ToggleFullscreen( )                     = 0;

        bool                                HasFocus( ) const                       { return m_hasFocus; }
        
        bool                                IsInputBlocked( ) const                 { return m_blockInput; }
        void                                SetBlockInput( bool blockInput )        { m_blockInput = blockInput; }

        const vector<pair<wstring,wstring>> &      GetCommandLineParameters( ) const       { return m_cmdLineParams; }

        // Use HelperUISettings for controlling the way it looks
        void                                InsertImGuiWindow( );

        const wstring &                     GetBasicFrameInfoText( )                { return m_basicFrameInfo; }

        vaRenderDevice &                    GetRenderDevice( )                      { return *m_renderDevice; }

        bool                                GetVsync( ) const                       { return m_vsync; }
        void                                SetVsync( bool vsync )                  { m_vsync = vsync; }

        int                                 GetFramerateLimit( ) const              { return m_framerateLimit; }
        void                                SetFramerateLimit( int fpsLimit )       { m_framerateLimit = fpsLimit; }

        bool                                GetConsoleOpen( ) const                 { return m_helperUISettings.ConsoleLogOpen; }
        void                                SetConsoleOpen( bool consoleOpen )      { m_helperUISettings.ConsoleLogOpen = consoleOpen; }

    protected:
        virtual vaVector2i                  GetWindowPosition( ) = 0;
        virtual void                        SetWindowPosition( const vaVector2i & position ) = 0;
        virtual vaVector2i                  GetWindowClientAreaSize( ) = 0;
        virtual void                        SetWindowClientAreaSize( const vaVector2i & clientSize ) = 0;

        virtual void                        OnGotFocus( );
        virtual void                        OnLostFocus( );

        virtual void                        Tick( float deltaTime );

        virtual void                        OnResized( int width, int height, bool windowed );

        virtual void                        SerializeSettings( vaXMLSerializer & serializer );


    private:
        //
        bool                                UpdateUserWindowChanges( );

    };

}