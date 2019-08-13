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

#include "Core/vaInput.h"

namespace VertexAsylum
{
    class vaInputKeyboard : public vaSingletonBase < vaInputKeyboard >, public vaInputKeyboardBase
    {
    public:
        static const int        c_InitPriority = 1;
        static const int        c_TickPriority = -1000;

    protected:
        bool                    g_Keys[KK_MaxValue];
        bool                    g_KeyUps[KK_MaxValue];
        bool                    g_KeyDowns[KK_MaxValue];

    private:
        friend class vaWindowsDirectX;
        friend class vaApplicationBase;
        friend class vaApplicationWin;
        vaInputKeyboard( );
        ~vaInputKeyboard( );

    protected:

    public:
        virtual bool            IsKeyDown( vaKeyboardKeys key )               { return g_Keys[key]; }
        virtual bool            IsKeyClicked( vaKeyboardKeys key )            { return g_KeyDowns[key]; }
        virtual bool            IsKeyReleased( vaKeyboardKeys key )           { return g_KeyUps[key]; }
        //

    protected:
        void                    Tick( float );
        void                    ResetAll( );

    };

}