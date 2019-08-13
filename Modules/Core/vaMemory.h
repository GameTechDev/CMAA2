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


namespace VertexAsylum
{
    ////////////////////////////////////////////////////////////////////////////////////////////////
    // vaMemory
    class vaMemory
    {
    private:
       friend class vaCore;

       static void						Initialize( );
       static void						Deinitialize( );
    };

    // Just a simple generic self-contained memory buffer helper class, for passing data as argument, etc.
    class vaMemoryBuffer
    {
        uint8 *                 m_buffer;
        int64                   m_bufferSize;
        bool                    m_hasOwnership;     // if it has ownership, will delete[] and expect it to be of uint8[] type - could expand with custom deallocator if needed

    public:
        enum class InitType
        {
            Copy,               // copy buffer (caller is free to release/modify the memory pointer after this call)
            TakeOwnership,      // take buffer pointer and delete[] when object is destroyed (caller should not ever use the memory after this call)
            View,               // take buffer pointer but don't delete[] when object is destroyed (caller should not ever use the memory after this call)
        };

    public:
        vaMemoryBuffer( ) : m_buffer( nullptr ), m_bufferSize( 0 ), m_hasOwnership( false ) { }
        vaMemoryBuffer( int64 bufferSize ) : m_buffer( new uint8[bufferSize] ), m_bufferSize( bufferSize ), m_hasOwnership( true ) { }
        vaMemoryBuffer( uint8 * buffer, int64 bufferSize, InitType initType = InitType::Copy ) : m_buffer( (initType != InitType::Copy)?(buffer):(new uint8[bufferSize]) ), m_bufferSize( bufferSize ), m_hasOwnership( initType != InitType::View )
        { 
            if( initType == InitType::Copy )
                memcpy( m_buffer, buffer, bufferSize );
            else
            {
                // if taking ownership, must have been allocated with new uint8[]
                assert( typeid(m_buffer) == typeid(buffer) );
            }
        }
        vaMemoryBuffer( const vaMemoryBuffer & copySrc ) : m_buffer( new uint8[copySrc.GetSize()] ), m_bufferSize( copySrc.GetSize() ), m_hasOwnership( true )
        {
            memcpy( m_buffer, copySrc.GetData(), m_bufferSize );
        }
        vaMemoryBuffer( vaMemoryBuffer && moveSrc ) : m_buffer( moveSrc.GetData() ), m_bufferSize( moveSrc.GetSize() ), m_hasOwnership( moveSrc.m_hasOwnership )
        {
            assert( moveSrc.m_hasOwnership );   // it's really not that safe and supported (or at least tricky to make safe) using 'move' for buffers with no ownership
            moveSrc.m_buffer        = nullptr;
            moveSrc.m_bufferSize    = 0;
            moveSrc.m_hasOwnership  = false;
        }

        ~vaMemoryBuffer( )
        {
            Clear( );
        }

        vaMemoryBuffer & operator = ( const vaMemoryBuffer & copySrc )
        {
            Clear( );

            m_buffer = new uint8[copySrc.GetSize()];
            m_bufferSize = copySrc.GetSize();
            m_hasOwnership = true;
            memcpy( m_buffer, copySrc.GetData(), m_bufferSize );

            return *this;
        }

        void Clear( )
        {
            if( m_buffer != nullptr && m_hasOwnership )
                delete[] m_buffer;
            m_buffer = nullptr;
            m_bufferSize = 0;
            m_hasOwnership = false;
        }

    public:
        uint8 *     GetData( ) const        { return m_buffer; }
        int64       GetSize( ) const        { return m_bufferSize; }
    };

}