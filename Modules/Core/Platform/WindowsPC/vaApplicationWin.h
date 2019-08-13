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

#include "Core/vaApplicationBase.h"

#include "vaInputKeyboard.h"
#include "vaInputMouse.h"

namespace VertexAsylum
{

    class vaRenderDevice;

    class vaApplicationWin : public vaApplicationBase
    {
    private:
        static vaApplicationWin *              s_instance;

    public:
        struct Settings : vaApplicationBase::Settings
        {
            HCURSOR         Cursor;
            HICON           Icon;
            HICON           SmallIcon;
            int             CmdShow;

            Settings( const wstring & appName = L"Name me plz", const wstring & cmdLine = L"", int cmdShow = SW_SHOWDEFAULT );
        };

    protected:
        const Settings                      m_localSettings;

        wstring                             m_wndClassName;
        HWND                                m_hWnd;

        HMENU                               m_systemMenu;

        HCURSOR                             m_cursorHand;
        HCURSOR                             m_cursorArrow;
        HCURSOR                             m_cursorNone;

        bool                                m_preventWMSIZEResizeSwapChain;
        bool                                m_inResizeOrMove;

        vaInputKeyboard                     m_keyboard;
        vaInputMouse                        m_mouse;

    public:
        vaApplicationWin( const Settings & settings, const std::shared_ptr<vaRenderDevice> & renderDevice );
        virtual ~vaApplicationWin( );

        //
    public:
        // run the main loop!
        virtual void                        Run( );
        //
    public:
        const vaSystemTimer &               GetMainTimer( ) const                   { return m_mainTimer; }
        //
        vaVector2i                          GetWindowPosition( ) const override ;
        void                                SetWindowPosition( const vaVector2i & position ) override ;
        //
        vaVector2i                          GetWindowClientAreaSize( ) const override ;
        void                                SetWindowClientAreaSize( const vaVector2i & clientSize ) override ;
        //        //
        float                               GetAvgFramerate( ) const                { return m_avgFramerate; }
        float                               GetAvgFrametime( ) const                { return m_avgFrametime; }
        //
        virtual void                        CaptureMouse( );
        virtual void                        ReleaseMouse( );
        //
    protected:
        virtual void                        Initialize( ) override;
        void                                UpdateMouseClientWindowRect( );
        //
    public:
        static vaApplicationWin &           GetInstance( )                          { return *s_instance; }
        //
    protected:

        virtual void                        Tick( float deltaTime );

        bool                                IsWindowFullscreenInternal( ) const;
        void                                SetFullscreenWindowInternal( bool fullscreen );

    public:

        HWND                                GetMainHWND( ) const                    { return m_hWnd; }

    protected:
        virtual void                        PreWndProcOverride( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, bool & overrideDefault );
        virtual LRESULT                     WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );

    private:
        static LRESULT CALLBACK             WndProcStatic( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );

        void                                UpdateDeviceSizeOnWindowResize( );
        //
        bool                                UpdateUserWindowChanges( );

    public:
        // this creates the render device, application and calls initialize and shutdown callbacks - just an example of use, one can do everything manually instead
        static void                         Run( const vaApplicationWin::Settings & settings, std::function< void( vaApplicationBase & application, bool starting ) > startStopCallback );

        static vector<pair<string, string>> EnumerateGraphicsAPIsAndAdapters( );

    };

}