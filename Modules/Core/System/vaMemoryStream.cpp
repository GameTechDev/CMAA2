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

#include "vaMemoryStream.h"
#include "..\vaMath.h"

#include <stdio.h>
#include <memory.h>

//////////////////////////////////////////////////////////////////////////////
// vaMemoryStream
//////////////////////////////////////////////////////////////////////////////
using namespace VertexAsylum;
//
vaMemoryStream::vaMemoryStream( void * buffer, int64 bufferSize )
{ 
   m_buffer = (uint8 *)buffer;
   m_bufferSize = bufferSize;
   m_autoBufferCapacity = 0;
   m_pos = 0;
   m_autoBuffer = false;
}
//
vaMemoryStream::vaMemoryStream( int64 initialSize, int64 reserve  )
{
   reserve = vaMath::Max( reserve, initialSize );
   assert( reserve > 0 );
   assert( reserve >= initialSize );
   m_buffer = new uint8[reserve];
   m_autoBufferCapacity = reserve;
   m_bufferSize = initialSize;
   m_pos = 0;
   m_autoBuffer = true;
}
//
vaMemoryStream::vaMemoryStream( const vaMemoryStream & copyFrom )
{
    if( copyFrom.m_buffer != nullptr )
    {
        m_buffer = new uint8[copyFrom.m_bufferSize];
        memcpy( m_buffer, copyFrom.m_buffer, copyFrom.m_bufferSize );
    }
    else
    {
        m_buffer = nullptr;
    }
    m_bufferSize            = copyFrom.m_bufferSize;
    m_autoBufferCapacity    = copyFrom.m_autoBufferCapacity;
    m_pos                   = copyFrom.m_pos;
    m_autoBuffer            = copyFrom.m_autoBuffer;
}
//
vaMemoryStream::~vaMemoryStream(void) 
{
   if( m_autoBuffer )
   {
      delete[] m_buffer;
   }
}
//
bool vaMemoryStream::Read( void * buffer, int64 count, int64 * outCountRead )
{ 
    int64 countToReallyRead = count;
    if( count + m_pos > m_bufferSize )
        countToReallyRead = m_bufferSize - m_pos;

    memcpy( buffer, m_buffer + m_pos, countToReallyRead ); 

    m_pos += countToReallyRead;

    if( outCountRead != nullptr )
        *outCountRead = countToReallyRead;

    return count == countToReallyRead; 
}
bool vaMemoryStream::Write( const void * buffer, int64 count, int64 * outCountWritten )
{ 
   assert( outCountWritten == NULL ); // not implemented!
   outCountWritten;

   if( (count + m_pos) > m_bufferSize )
   {
      if( m_autoBuffer )
      {
         if( (count + m_pos) > m_autoBufferCapacity )
         {
            Grow( count + m_pos + m_autoBufferCapacity );
         }
         m_bufferSize = count + m_pos;
      }
      else
      {
         // buffer overrun, should handle this differently now that we've got 
         assert( false );
         return false;
      }
   }

   memcpy(m_buffer + m_pos, buffer, count); 
   
   m_pos += count;

   return true; 
}
//
void vaMemoryStream::Resize( int64 newSize )
{
   if( m_autoBuffer )
   {
      if( newSize > m_autoBufferCapacity )
      {
         Grow( newSize );
      }
      m_bufferSize = newSize;
   }
   else
   {
      // buffer overrun
      assert( false );
      return;
   }
}
//
void vaMemoryStream::Grow( int64 nextCapacity )
{
   assert( m_autoBuffer );
   assert( nextCapacity > m_autoBufferCapacity );

   uint8 * newBuffer = new uint8[nextCapacity];
   memcpy(newBuffer, m_buffer, m_bufferSize); 
   m_autoBufferCapacity = nextCapacity;
   delete[] m_buffer;
   m_buffer = newBuffer;
}
//
///////////////////////////////////////////////////////////////////////////////////////////////////