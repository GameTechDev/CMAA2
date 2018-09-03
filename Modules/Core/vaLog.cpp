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

#include "vaLog.h"

#include "vaCoreIncludes.h"

#include "Misc/vaPropertyContainer.h"

// if VA_REMOTERY_INTEGRATION_ENABLED, dump log stuff there too
#ifdef VA_REMOTERY_INTEGRATION_ENABLED
#include "IntegratedExternals\vaRemoteryIntegration.h"
#endif

using namespace VertexAsylum;

vaLog::vaLog( )
{
    m_lastAddedTime = 0;

    m_timer.Start();

    if( !m_outStream.Open( vaCore::GetExecutableDirectory( ) + L"log.txt", FileCreationMode::Create, FileAccessMode::Write, FileShareMode::Read ) )
    {
        // where does one log failure to open the log file?
        vaCore::DebugOutput( L"Unable to open log output file" );
    }
    else
    {
        // Using byte order marks: https://msdn.microsoft.com/en-us/library/windows/desktop/dd374101
        uint16 utf16LE = 0xFEFF;
        m_outStream.WriteValue<uint16>(utf16LE);
    }
}

vaLog::~vaLog( )
{
    m_timer.Stop();

    m_outStream.Close();
}

void vaLog::Clear( )
{
    vaRecursiveMutexScopeLock lock( m_mutex );
    m_logEntries.clear();
}

void vaLog::Add( const vaVector4 & color, const std::wstring & text )
{
    vaRecursiveMutexScopeLock lock( m_mutex );

    vaCore::DebugOutput( L"vaLog: " + text + L"\n" );

#ifdef VA_REMOTERY_INTEGRATION_ENABLED
    rmt_LogText( vaStringTools::SimpleNarrow(text).c_str() );
#endif

    {
        time_t locTime = time( NULL );
        double now = m_timer.GetCurrentTimeDouble( );
        assert( now >= m_lastAddedTime );
        if( now > m_lastAddedTime )
            m_lastAddedTime = now;
        m_logEntries.push_back( Entry( color, text, locTime, m_lastAddedTime ) );

        if( m_logEntries.size( ) > c_maxEntries )
        {
            const int countToDelete = c_maxEntries / 10;
            m_logEntries.erase( m_logEntries.begin(), m_logEntries.begin()+countToDelete );
        }

        if( m_outStream.IsOpen() )
        {
            char buff[64];
#pragma warning ( suppress: 4996 )
            strftime( buff, sizeof( buff ), "%H:%M:%S: ", localtime( &locTime ) );

            m_outStream.WriteTXT( vaStringTools::SimpleWiden(buff) + text + L"\r\n" );
        }
    }
}

void vaLog::Add( const char * messageFormat, ... )
{
    va_list args;
    va_start( args, messageFormat );
    std::string txt = vaStringTools::Format( messageFormat, args );
    va_end( args );

    Add( LOG_COLORS_NEUTRAL, vaStringTools::SimpleWiden(txt) );
}

void vaLog::Add( const wchar_t * messageFormat, ... )
{
    va_list args;
    va_start( args, messageFormat );
    std::wstring txt = vaStringTools::Format( messageFormat, args );
    va_end( args );

    Add( LOG_COLORS_NEUTRAL, txt );
}

void vaLog::Add( const vaVector4 & color, const char * messageFormat, ... )
{
    va_list args;
    va_start( args, messageFormat );
    std::string txt = vaStringTools::Format( messageFormat, args );
    va_end( args );

    Add( color, vaStringTools::SimpleWiden( txt ) );
}

void vaLog::Add( const vaVector4 & color, const wchar_t * messageFormat, ... )
{
    va_list args;
    va_start( args, messageFormat );
    std::wstring txt = vaStringTools::Format( messageFormat, args );
    va_end( args );

    Add( color, txt );
}

int vaLog::FindNewest( float maxAgeSeconds )
{
    vaRecursiveMutexScopeLock lock( m_mutex );

    if( m_logEntries.size() == 0 )
        return (int)m_logEntries.size();

    double now = m_timer.GetCurrentTimeDouble( );
    double searchTime = now - (double)maxAgeSeconds;

    assert( now >= m_lastAddedTime );

    // nothing to return?
    if( m_logEntries.back().SystemTime < searchTime )
        return (int)m_logEntries.size();

    // return all?
    if( m_logEntries.front( ).SystemTime >= searchTime )
        return 0;

    int currentIndex    = (int)m_logEntries.size()-1;
    int prevStepIndex   = currentIndex;
    int stepSize        = vaMath::Max( 1, currentIndex / 100 );
    while( true )
    {
        assert( m_logEntries[prevStepIndex].SystemTime >= searchTime );

        currentIndex = vaMath::Max( 0, prevStepIndex - stepSize );

        if( m_logEntries[currentIndex].SystemTime < searchTime )
        {
            if( stepSize == 1 )
                return currentIndex+1;
            currentIndex = prevStepIndex + stepSize;
            stepSize = (stepSize+1) / 2;
        }
        else
        {
            prevStepIndex = currentIndex;
        }
    }

    // shouldn't ever happen!
    assert( false );
    return -1;
}



// Portable helpers
//static int   Stricmp(const char* str1, const char* str2)         { int d; while ((d = toupper(*str2) - toupper(*str1)) == 0 && *str1) { str1++; str2++; } return d; }
static int   Strnicmp(const char* str1, const char* str2, int n) { int d = 0; while (n > 0 && (d = toupper(*str2) - toupper(*str1)) == 0 && *str1) { str1++; str2++; n--; } return d; }
//static char* Strdup(const char *str)                             { size_t len = strlen(str) + 1; void* buff = malloc(len); return (char*)memcpy(buff, (const void*)str, len); }


vaConsole::vaConsole( )
{ 
    m_scrollToBottom = true;
    m_consoleOpen = false;
    m_consoleWasOpen = false;
    memset( m_textInputBuffer, 0, sizeof(m_textInputBuffer) );

    m_commands.push_back( CommandInfo( "HELP" ) );
    m_commands.push_back( CommandInfo( "HISTORY" ) );
    m_commands.push_back( CommandInfo( "CLEAR" ) );
    m_commands.push_back( CommandInfo( "QUIT" ) );
    //m_commands.push_back("CLASSIFY");  // "classify" is here to provide an example of "C"+[tab] completing to "CL" and displaying matches.
}

vaConsole::~vaConsole( )
{
}

void vaConsole::Draw( int windowWidth, int windowHeight )
{
    // Don't allow log access to anyone else while this is drawing
    vaRecursiveMutexScopeLock lock( vaLog::GetInstance( ).Mutex() );

    const float timerSeparatorX = 80.0f;
    const int linesToShowMax = 15;

    const float secondsToShow = 8.0f;
    int showFrom = vaLog::GetInstance( ).FindNewest( secondsToShow );
    int showCount = vaMath::Min( linesToShowMax, (int)vaLog::GetInstance( ).Entries( ).size( ) - showFrom );
    if( m_consoleOpen )
    {
        showFrom = (int)vaLog::GetInstance( ).Entries( ).size( ) - linesToShowMax - 1; // vaMath::Max( 0, (int)vaLog::GetInstance( ).Entries( ).size( ) - linesToShowMax - 1 );
        showCount = linesToShowMax;
    }
    showFrom = (int)vaLog::GetInstance( ).Entries( ).size( ) - showCount;

    int sizeX               = windowWidth - 10;
    int sizeY               = (int)(ImGui::GetTextLineHeightWithSpacing() * (float)showCount + 10);
    ImGui::SetNextWindowPos( ImVec2( (float)( windowWidth/2 - sizeX/2), (float)( windowHeight - sizeY) - 6 ), ImGuiCond_Always );     // Normally user code doesn't need/want to call it because positions are saved in .ini file anyway. Here we just want to make the demo initial state a bit more friendly!
    ImGui::SetNextWindowSize( ImVec2( (float)sizeX, (float)sizeY ), ImGuiCond_Always );
    ImGui::SetNextWindowCollapsed( false, ImGuiCond_Always );
    bool opened = true;
    ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | /*ImGuiWindowFlags_NoInputs |*/ ImGuiWindowFlags_NoSavedSettings;
    //winFlags |= ImGuiWindowFlags_NoScrollbar; //?

    const vector<vaLog::Entry> & logEntries = vaLog::GetInstance().Entries();

    float winAlpha = (m_consoleOpen)?(0.92f):(0.55f);
    if( showCount == 0 )
        winAlpha = 0.0f;
    ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( 0.0f, 0.0f, 0.0f, 1.0f ) );

    bool showConsoleWindow = (!m_consoleOpen && showCount > 0) || m_consoleOpen;

    if( showConsoleWindow )
    {
        if( ImGui::Begin( "Console", &opened, ImVec2(0.f, 0.f), winAlpha, winFlags ) )
        {
            // if console not open, show just the log
            if( !m_consoleOpen )
            {
                if( showCount > 0 )
                {
                    // for( int i = 0; i < linesToShowMax - showCount; i++ )
                    //     ImGui::Text( "" );

                    // ImGui::Columns( 2 );
                    // float scrollbarSize = 24.0f;
                    // ImGui::SetColumnOffset( 1, (float)sizeX - scrollbarSize );

                    for( int i = showFrom; i < (int)logEntries.size(); i++ )
                    {
                        if( i < 0  )
                        {
                            ImGui::Text("");
                            continue;
                        }
                        const vaLog::Entry & entry = logEntries[i];
                        float lineCursorPosY = ImGui::GetCursorPosY( );

                        char buff[64];
                        #pragma warning ( suppress: 4996 )
                        strftime( buff, sizeof(buff), "%H:%M:%S: ", localtime( &entry.LocalTime ) );
                        ImGui::TextColored( ImVec4( 0.3f, 0.3f, 0.2f, 1.0f ), buff );

                        ImGui::SetCursorPosX( timerSeparatorX );
                        ImGui::SetCursorPosY( lineCursorPosY );
                        ImGui::TextColored( ImFromVA( entry.Color ), vaStringTools::SimpleNarrow( entry.Text ).c_str( ) );
                    }

                    // ImGui::NextColumn();
                    // 
                    // int smax = vaMath::Max( 1, (int)vaLog::GetInstance().Entries().size() - showFrom );
                    // int smin = 0;
                    // int currentPos = smax - smax; // inverted? todo, this is weird
                    // ImGui::VSliderInt( "", ImVec2( scrollbarSize-16, (float)sizeY - 16 ), &currentPos, smin, smax, " " );
                }
            }
            else
            {

                //ImGui::TextWrapped("This example implements a console with basic coloring, completion and history. A more elaborate implementation may want to store entries along with extra data such as timestamp, emitter, etc.");
                ImGui::TextWrapped("Enter 'HELP' for help, press TAB to use text completion.");

                // TODO: display items starting from the bottom

                // if (ImGui::SmallButton("Add Dummy Text")) { vaLog::GetInstance().Add("%d some text", (int)vaLog::GetInstance().Entries().size()); vaLog::GetInstance().Add("some more text"); vaLog::GetInstance().Add("display very important message here!"); } ImGui::SameLine();
                // if (ImGui::SmallButton("Add Dummy Error")) vaLog::GetInstance().Add("[error] something went wrong"); ImGui::SameLine();
                // if (ImGui::SmallButton("Clear")) vaLog::GetInstance().Clear(); ImGui::SameLine();
                // bool copy_to_clipboard = ImGui::SmallButton("Copy"); ImGui::SameLine();
                // if (ImGui::SmallButton("Scroll to bottom")) m_scrollToBottom = true;
                bool copy_to_clipboard = false;

                ImGui::Separator();

                // ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
                // static ImGuiTextFilter filter;
                // filter.Draw("Filter (\"incl,-excl\") (\"error\")", 180);
                // ImGui::PopStyleVar();
                // ImGui::Separator();

                ImGui::BeginChild("ScrollingRegion", ImVec2(0,-ImGui::GetFrameHeightWithSpacing()), false, ImGuiWindowFlags_HorizontalScrollbar);
                if (ImGui::BeginPopupContextWindow())
                {
                    if (ImGui::Selectable("Clear")) vaLog::GetInstance().Clear();
                    ImGui::EndPopup();
                }

                // if( m_scrollByAddedLineCount )
                // {
                //     int lineDiff = vaMath::Max( 0, (int)logEntries.size() - m_lastDrawnLogLineCount );
                //     if( lineDiff > 0 )
                //     {
                //         ImGuiWindow * window = ImGui::GetCurrentWindow();
                //         if( window != nullptr )
                //         {
                //             float newScrollY = window->Scroll.y + ImGui::GetTextLineHeightWithSpacing() * lineDiff;
                // 
                //             window->DC.CursorMaxPos.y += window->Scroll.y; // SizeContents is generally computed based on CursorMaxPos which is affected by scroll position, so we need to apply our change to it.
                //             window->Scroll.y = newScrollY;
                //             window->DC.CursorMaxPos.y -= window->Scroll.y;
                //         }
                //     }
                //     m_scrollByAddedLineCount = false;
                // }


                // Display every line as a separate entry so we can change their color or add custom widgets. If you only want raw text you can use ImGui::TextUnformatted(log.begin(), log.end());
                // NB- if you have thousands of entries this approach may be too inefficient and may require user-side clipping to only process visible items.
                // You can seek and display only the lines that are visible using the ImGuiListClipper helper, if your elements are evenly spaced and you have cheap random access to the elements.
                // To use the clipper we could replace the 'for (int i = 0; i < Items.Size; i++)' loop with:
                //     ImGuiListClipper clipper(Items.Size);
                //     while (clipper.Step())
                //         for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                // However take note that you can not use this code as is if a filter is active because it breaks the 'cheap random-access' property. We would need random-access on the post-filtered list.
                // A typical application wanting coarse clipping and filtering may want to pre-compute an array of indices that passed the filtering test, recomputing this array when user changes the filter,
                // and appending newly elements as they are inserted. This is left as a task to the user until we can manage to improve this example code!
                // If your items are of variable size you may want to implement code similar to what ImGuiListClipper does. Or split your data into fixed height items to allow random-seeking into your list.
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4,1)); // Tighten spacing
                // for (int i = 0; i < Items.Size; i++)
                // {
                //     const char* item = Items[i];
                //     if (!filter.PassFilter(item))
                //         continue;
                //     ImVec4 col = ImVec4(1.0f,1.0f,1.0f,1.0f); // A better implementation may store a type per-item. For the sample let's just parse the text.
                //     if (strstr(item, "[error]")) col = ImColor(1.0f,0.4f,0.4f,1.0f);
                //     else if (strncmp(item, "# ", 2) == 0) col = ImColor(1.0f,0.78f,0.58f,1.0f);
                //     ImGui::PushStyleColor(ImGuiCol_Text, col);
                //     ImGui::TextUnformatted(item);
                //     ImGui::PopStyleColor();
                // }
                float scrollY = ImGui::GetScrollY();
                float windowStartY = ImGui::GetCursorPosY();
                windowStartY;

                float availableDrawArea = (linesToShowMax+1) * ImGui::GetTextLineHeightWithSpacing( );
                int drawFromLine = vaMath::Clamp( (int)(scrollY / ImGui::GetTextLineHeightWithSpacing( )), 0, vaMath::Max( 0, (int)logEntries.size()-1) );
                int drawToLine = vaMath::Clamp( (int)((scrollY+availableDrawArea)/ ImGui::GetTextLineHeightWithSpacing( )), 0, (int)logEntries.size() );

                if( copy_to_clipboard )
                {
                    // below was never tested but it might work
                    assert( false );
                    drawFromLine = 0;
                    drawToLine = (int)logEntries.size();
                    ImGui::LogToClipboard();
                }

                // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                // these (and same below) are the only place that remains to be optimized for huge LOG buffers; it's not that complicated probably 
                // but requires going into NewLine and figuring out how to correctly make a MultiNewLine instead
                for( int i = 0; i < drawFromLine; i++ )
                    ImGui::NewLine();
                // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

                for( int i = drawFromLine; i < drawToLine; i++ )
                {
                    const vaLog::Entry & entry = logEntries[i];
                    //float lineCursorPosY = ImGui::GetCursorPosY( );

                    char buff[64];
                    #pragma warning ( suppress: 4996 )
                    strftime( buff, sizeof(buff), "%H:%M:%S: ", localtime( &entry.LocalTime ) );
                    ImGui::TextColored( ImVec4( 0.3f, 0.3f, 0.2f, 1.0f ), buff );

                    // ImGui::SetCursorPosX( timerSeparatorX );
                    // ImGui::SetCursorPosY( lineCursorPosY );
                    ImGui::SameLine( timerSeparatorX );
                    ImGui::TextColored( ImFromVA( entry.Color ), vaStringTools::SimpleNarrow( entry.Text ).c_str( ) );
                }

                // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                // see comments above
                for( int i = drawToLine; i < (int)logEntries.size( ); i++ )
                    ImGui::NewLine();
                // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

                if( copy_to_clipboard )
                    ImGui::LogFinish();

                int addedLineCount = (int)logEntries.size() - m_lastDrawnLogLineCount;
                m_lastDrawnLogLineCount = (int)logEntries.size();

                // keep scrolling to bottom if we're at bottom and new lines were added (also reverse scroll if lines removed)
                if( addedLineCount != 0 && (drawToLine+vaMath::Abs(addedLineCount)) >= (int)logEntries.size( ) )
                    m_scrollToBottom = true;
           

                if( m_scrollToBottom )
                    ImGui::SetScrollHere();
                m_scrollToBottom = false;

                ImGui::PopStyleVar();
                ImGui::EndChild();
                ImGui::Separator();

                if( !m_consoleWasOpen )
                    ImGui::SetKeyboardFocusHere( 0 );
                // Keep auto focus on the input box
                if( ImGui::IsItemHovered( ) || ( ImGui::IsRootWindowOrAnyChildFocused( ) && !ImGui::IsAnyItemActive( ) && !ImGui::IsMouseClicked( 0 ) ) )
                    ImGui::SetKeyboardFocusHere( 0 ); // Auto focus previous widget

                // Command-line
                if (ImGui::InputText("Input", m_textInputBuffer, _countof(m_textInputBuffer), ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_CallbackCompletion|ImGuiInputTextFlags_CallbackHistory, (ImGuiTextEditCallback)&TextEditCallbackStub, (void*)this))
                {
                    char* input_end = m_textInputBuffer+strlen(m_textInputBuffer);
                    while (input_end > m_textInputBuffer && input_end[-1] == ' ') input_end--; *input_end = 0;
                    if (m_textInputBuffer[0])
                    {
                        ExecuteCommand( m_textInputBuffer );
                    }
                    strcpy_s(m_textInputBuffer, "");
                }
            }
        }
        ImGui::End( );
    }
    ImGui::PopStyleColor( 1 );

    m_consoleWasOpen = m_consoleOpen;
}

int vaConsole::TextEditCallbackStub( ImGuiTextEditCallbackData * data )
{
    vaConsole* console = (vaConsole*)data->UserData;
    return console->TextEditCallback(data);
}

int vaConsole::TextEditCallback( ImGuiTextEditCallbackData * data )
{
    //AddLog("cursor: %d, selection: %d-%d", data->CursorPos, data->SelectionStart, data->SelectionEnd);
    switch (data->EventFlag)
    {
    case ImGuiInputTextFlags_CallbackCompletion:
        {
            // Example of TEXT COMPLETION

            // Locate beginning of current word
            const char* word_end = data->Buf + data->CursorPos;
            const char* word_start = word_end;
            while (word_start > data->Buf)
            {
                const char c = word_start[-1];
                if (c == ' ' || c == '\t' || c == ',' || c == ';')
                    break;
                word_start--;
            }

            // Build a list of candidates
            ImVector<const char*> candidates;
            for (int i = 0; i < (int)m_commands.size(); i++)
                if (Strnicmp( m_commands[i].Name.c_str(), word_start, (int)(word_end-word_start)) == 0)
                    candidates.push_back( m_commands[i].Name.c_str() );

            if (candidates.Size == 0)
            {
                // No match
                vaLog::GetInstance().Add("No match for \"%.*s\"!\n", (int)(word_end-word_start), word_start);
            }
            else if (candidates.Size == 1)
            {
                // Single match. Delete the beginning of the word and replace it entirely so we've got nice casing
                data->DeleteChars((int)(word_start-data->Buf), (int)(word_end-word_start));
                data->InsertChars(data->CursorPos, candidates[0]);
                data->InsertChars(data->CursorPos, " ");
            }
            else
            {
                // Multiple matches. Complete as much as we can, so inputing "C" will complete to "CL" and display "CLEAR" and "CLASSIFY"
                int match_len = (int)(word_end - word_start);
                for (;;)
                {
                    int c = 0;
                    bool all_candidates_matches = true;
                    for (int i = 0; i < candidates.Size && all_candidates_matches; i++)
                        if (i == 0)
                            c = toupper(candidates[i][match_len]);
                        else if (c == 0 || c != toupper(candidates[i][match_len]))
                            all_candidates_matches = false;
                    if (!all_candidates_matches)
                        break;
                    match_len++;
                }

                if (match_len > 0)
                {
                    data->DeleteChars((int)(word_start - data->Buf), (int)(word_end-word_start));
                    data->InsertChars(data->CursorPos, candidates[0], candidates[0] + match_len);
                }

                // List matches
                vaLog::GetInstance().Add("Possible matches:\n");
                for (int i = 0; i < candidates.Size; i++)
                    vaLog::GetInstance().Add("- %s\n", candidates[i]);
            }

            break;
        }
    case ImGuiInputTextFlags_CallbackHistory:
        {
            // Example of HISTORY
            const int prev_history_pos = m_commandHistoryPos;
            if (data->EventKey == ImGuiKey_UpArrow)
            {
                if (m_commandHistoryPos == -1)
                    m_commandHistoryPos = (int)m_commandHistory.size() - 1;
                else if (m_commandHistoryPos > 0)
                    m_commandHistoryPos--;
            }
            else if (data->EventKey == ImGuiKey_DownArrow)
            {
                if (m_commandHistoryPos != -1)
                    if (++m_commandHistoryPos >= m_commandHistory.size())
                        m_commandHistoryPos = -1;
            }

            // A better implementation would preserve the data on the current input line along with cursor position.
            if (prev_history_pos != m_commandHistoryPos)
            {
                data->CursorPos = data->SelectionStart = data->SelectionEnd = data->BufTextLen = (int)snprintf(data->Buf, (size_t)data->BufSize, "%s", (m_commandHistoryPos >= 0) ? m_commandHistory[m_commandHistoryPos].c_str() : "");
                data->BufDirty = true;
            }
        }
    }
    return 0;
}
void vaConsole::ExecuteCommand( const string & commandLine )
{
    vaLog::GetInstance().Add( "# %s\n", commandLine.c_str() );

    // Insert into history. First find match and delete it so it can be pushed to the back. This isn't trying to be smart or optimal.
    m_commandHistoryPos = -1;
    for( int i = (int)m_commandHistory.size() - 1; i >= 0; i-- )
        if( vaStringTools::CompareNoCase( m_commandHistory[i], commandLine ) == 0 )
        {
            //free( History[i] );
            //History.erase( History.begin( ) + i );
            m_commandHistory.erase( m_commandHistory.begin() + i );
            break;
        }
    m_commandHistory.push_back( commandLine );

    // Process command
    if( vaStringTools::CompareNoCase( commandLine, "CLEAR" ) == 0 )
    {
        vaLog::GetInstance().Clear();
    }
    else if( vaStringTools::CompareNoCase( commandLine, "HELP" ) == 0 )
    {
        vaLog::GetInstance().Add( "Commands:" );
        for( int i = 0; i < (int)m_commands.size(); i++ )
            vaLog::GetInstance().Add( "- %s", m_commands[i].Name.c_str() );
    }
    else if( vaStringTools::CompareNoCase( commandLine, "HISTORY" ) == 0 )
    {
        for( int i = (int)m_commandHistory.size() >= 10 ? (int)m_commandHistory.size() - 10 : 0; i < (int)m_commandHistory.size(); i++ )
            vaLog::GetInstance().Add( "%3d: %s\n", i, m_commandHistory[i].c_str() );
    }
    else if( vaStringTools::CompareNoCase( commandLine, "QUIT" ) == 0 )
    {
        vaCore::SetAppQuitFlag( true );
    }
    else
    {
        vaLog::GetInstance().Add( LOG_COLORS_ERROR, "Unknown command: '%s'\n", commandLine.c_str() );
    }
    m_scrollToBottom = true;
}
