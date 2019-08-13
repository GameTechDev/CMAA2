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

#include "vaInputKeyboard.h"

#include "Core\Misc\vaProfiler.h"

using namespace VertexAsylum;

vaInputKeyboard::vaInputKeyboard( )
{
    ResetAll( );

    vaInputKeyboardBase::SetCurrent( this );
}

vaInputKeyboard::~vaInputKeyboard( )
{
    assert( vaInputKeyboardBase::GetCurrent( ) == this );
    vaInputKeyboardBase::SetCurrent( NULL );
}


void vaInputKeyboard::Tick( float deltaTime )
{
    VA_SCOPE_CPU_TIMER( vaInputKeyboard_Tick );
    deltaTime;
    // m_timeFromLastUpdate += deltaTime;
    // if( m_timeFromLastUpdate > m_updateMinFrequency )
    //     m_timeFromLastUpdate = vaMath::Clamp( m_timeFromLastUpdate-m_updateMinFrequency, 0.0f, m_updateMinFrequency );
    // else
    //     return;
#if 0 // old, CPU-intensive approach

    for( int i = 0; i < KK_MaxValue; i++ )
    {
       bool isDown = (GetAsyncKeyState(i) & 0x8000) != 0;

       g_KeyUps[i]    = g_Keys[i] && !isDown;
       g_KeyDowns[i]  = !g_Keys[i] && isDown;

       g_Keys[i]      = isDown;
    }
#else
    byte keyStates[256];
    if( !GetKeyboardState( keyStates ) )
    {
        // See GetLastErrorAsString.. in vaPlatformFileStream on how to format the message into text
        DWORD errorMessageID = ::GetLastError( );
        VA_WARN( "GetKeyboardState returns false, error: %x", errorMessageID );
    }

    for( int i = 0; i < KK_MaxValue; i++ )
    {
       bool isDown = (keyStates[i] & 0x80) != 0;

       g_KeyUps[i]    = g_Keys[i] && !isDown;
       g_KeyDowns[i]  = !g_Keys[i] && isDown;

       g_Keys[i]      = isDown;
    }
#endif
}

void vaInputKeyboard::ResetAll( )
{
   for( int i = 0; i < KK_MaxValue; i++ )
   {
      g_Keys[i]         = false;
      g_KeyUps[i]       = false;
      g_KeyDowns[i]	=    false;
   }
}