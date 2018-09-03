///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018, Intel Corporation
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

#include "vaEvent.h"
//using namespace VertexAsylum;

namespace VertexAsylum
{

    void vaEventTest( )
    {
        vaEvent<void(int)> testEvent;
        testEvent.Invoke( 1 );
        {
            shared_ptr< int > aNumberInMemory = std::make_shared<int>( int(41) );
            testEvent.AddWithToken( weak_ptr<void>(aNumberInMemory), [aNumberInMemory](int p) { *aNumberInMemory += p; } );
            testEvent.Invoke( 1 );
            assert( *aNumberInMemory == 42 );
            testEvent.Remove( weak_ptr<void>(aNumberInMemory) );
            testEvent.Invoke( 1 );
            assert( *aNumberInMemory == 42 );
        }
        testEvent.Invoke( 1 );
    }

}