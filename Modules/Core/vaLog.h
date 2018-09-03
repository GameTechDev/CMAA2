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

#include "vaCore.h"
#include "vaSingleton.h"
#include "vaGeometry.h"
#include <time.h>
#include <chrono>

#include "Core/System/vaSystemTimer.h"
#include "Core/System/vaFileStream.h"
#include "Core/vaStringTools.h"

struct ImGuiTextEditCallbackData;

namespace VertexAsylum
{

    // system-wide logging colors, can't think of a better place to put them
#define LOG_COLORS_NEUTRAL  (vaVector4( 0.8f, 0.8f, 0.8f, 1.0f ) )
#define LOG_COLORS_SUCCESS  (vaVector4( 0.0f, 0.8f, 0.2f, 1.0f ) )
#define LOG_COLORS_WARNING  (vaVector4( 0.8f, 0.8f, 0.1f, 1.0f ) )
#define LOG_COLORS_ERROR    (vaVector4( 1.0f, 0.1f, 0.1f, 1.0f ) )

    class vaLog : public vaSingletonBase< vaLog >
    {
    public:
        struct Entry
        {
            vaVector4           Color;
            wstring             Text;
            time_t              LocalTime;
            double              SystemTime;

            Entry( const vaVector4 & color, const std::wstring & text, time_t localTime, double appTime ) : Color( color ), Text( text ), LocalTime( localTime ), SystemTime( appTime ) { }
        };

        vector<Entry>           m_logEntries;
        double                  m_lastAddedTime;

        vaSystemTimer           m_timer;

        vaRecursiveMutex        m_mutex;

        vaFileStream            m_outStream;

        static const int        c_maxEntries = 100000;
        

    private:
        friend class vaCore;
        vaLog( );
        ~vaLog( );

    public:
        // must lock mutex when using this
        const vector<Entry> &   Entries( )  { return m_logEntries; }
        void                    Clear( );

        // must lock mutex if using this to have any meaning
        int                     FindNewest( float maxAgeSeconds );

        // these will lock mutex
        void                    Add( const char * messageFormat, ... );
        void                    Add( const wchar_t * messageFormat, ... );

        void                    Add( const vaVector4 & color, const char * messageFormat, ... );
        void                    Add( const vaVector4 & color, const wchar_t * messageFormat, ... );

#pragma warning ( suppress: 4996 )
        void                    Add( const vaVector4 & color, const std::wstring & text );

        vaRecursiveMutex &      Mutex( ) { return m_mutex; }

    private:
    };
    ////////////////////////////////////////////////////////////////////////////////////////////////

    // For measuring & logging occasional long taking tasks like loading of a level or similar. 
    // Not precise for short tasks and not useful for something that happens every frame (would 
    // spam the log)!
    class vaSimpleScopeTimerLog
    {
    private:
         clock_t        m_begin;
         vaVector4      m_color;
         wstring        m_info;
    public:
        vaSimpleScopeTimerLog( const wstring & info, const vaVector4 & color = LOG_COLORS_NEUTRAL ) : m_info(info), m_color(color)
        {
            m_begin = clock();
            vaLog::GetInstance().Add( m_color, L"%s : starting...", m_info.c_str() );
        }
        vaSimpleScopeTimerLog( const string & info, const vaVector4 & color = LOG_COLORS_NEUTRAL ) : m_info( vaStringTools::SimpleWiden( info ) ), m_color(color)
        {
            m_begin = clock( );
            vaLog::GetInstance( ).Add( m_color, L"%s : starting...", m_info.c_str( ) );
        }
        ~vaSimpleScopeTimerLog( )
        {
            clock_t end = clock( );
            float elapsed_secs = (float)(double( end - m_begin ) / (double)CLOCKS_PER_SEC);
            vaLog::GetInstance().Add( m_color, L"%s : done, time taken %.2f seconds.", m_info.c_str(), elapsed_secs );
        }
    };

    // This is a half-baked helper class for displaying log in a form of a console, with additional support for 
    // commands and etc. Initially based on ImGui console examples.
    // Requires additional work to be functional.
    class vaConsole : public vaSingletonBase< vaConsole >
    {
        struct CommandInfo
        {
            string      Name;
            //some-kindo-of-callback

            CommandInfo( ) { }
            explicit CommandInfo( const string & name ) : Name(name) { }
        };

    private:
        char                    m_textInputBuffer[256];
        bool                    m_scrollToBottom;
        bool                    m_scrollByAddedLineCount = false;      // if already at bottom, scroll next frame to stay at bottom
        int                     m_lastDrawnLogLineCount = 0;
        
        vector<CommandInfo>     m_commands;

        vector<string>          m_commandHistory;
        int                     m_commandHistoryPos;    // -1: new line, 0..History.Size-1 browsing history.

        // show console with input text (otherwise just show log)
        bool                    m_consoleOpen;
        bool                    m_consoleWasOpen;

    private:
        friend class vaCore;
        vaConsole( ); 
        ~vaConsole( );

    public:
        bool                    IsConsoleOpen( ) const              { return m_consoleOpen; }
        void                    SetConsoleOpen( bool consoleOpen )  { m_consoleOpen = consoleOpen; }

        // Todo:
        //void                    AddCommand( const string & name ... )
        //void                    RemoveCommand( const string & name )

        void                    Draw( int windowWidth, int windowHeight );

    private:
        void                    ExecuteCommand( const string & commandLine );

    private:
        static int              TextEditCallbackStub( ImGuiTextEditCallbackData * data );
        int                     TextEditCallback( ImGuiTextEditCallbackData * data );
    };


#define VA_LOG( format, ... )                   { vaLog::GetInstance().Add( LOG_COLORS_NEUTRAL, format, __VA_ARGS__ ); } 
#define VA_LOG_SUCCESS( format, ... )           { vaLog::GetInstance().Add( LOG_COLORS_SUCCESS, format, __VA_ARGS__ ); } 
#define VA_LOG_WARNING( format, ... )           { vaLog::GetInstance().Add( LOG_COLORS_WARNING, format, __VA_ARGS__ ); } 
#define VA_LOG_ERROR( format, ... )             { vaLog::GetInstance().Add( LOG_COLORS_ERROR, format, __VA_ARGS__ ); } 

#define VA_LOG_STACKINFO( format, ... )         { vaLog::GetInstance().Add( L"%s:%d : " format , VA_T(__FILE__), __LINE__, __VA_ARGS__); } 
#define VA_LOG_WARNING_STACKINFO( format, ... ) { vaLog::GetInstance().Add( LOG_COLORS_WARNING, L"%s:%d : " format , VA_T(__FILE__), __LINE__, __VA_ARGS__); } 
#define VA_LOG_ERROR_STACKINFO( format, ... )   { vaLog::GetInstance().Add( LOG_COLORS_ERROR, L"%s:%d : " format , VA_T(__FILE__), __LINE__, __VA_ARGS__); } 
}