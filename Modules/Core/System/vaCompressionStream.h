///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017, Intel Corporation
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

namespace VertexAsylum
{
    struct vaCompressionStreamWorkingContext;

    class vaCompressionStream : public vaStream
    {
    public:
        enum class Profile
        {
            Default             = 0,
            PassThrough         = 1,
        };

    private:
        
        Profile                 m_compressionProfile;

        shared_ptr<vaStream>    m_compressedStream;
        vaStream *              m_compressedStreamNakedPtr;

        bool                    m_decompressing;

        vaCompressionStreamWorkingContext *
                                m_workingContext;

    public:
        vaCompressionStream( bool decompressing, shared_ptr<vaStream> compressedStream, Profile profile = Profile::Default );
        vaCompressionStream( bool decompressing, vaStream * compressedStreamNakedPtr, Profile profile = Profile::Default );     // same as above except no smart pointer
        virtual ~vaCompressionStream( void );

        virtual bool            CanSeek( ) override                 { return false; }
        virtual void            Seek( int64 position ) override     { assert( false ); position; }
        virtual void            Close( ) override;
        virtual bool            IsOpen( ) const override            { return GetInnerStream() != nullptr; }
        virtual int64           GetLength( ) override               { assert( false ); return -1; }
        virtual int64           GetPosition( ) const override       { assert( false ); return -1; }
        virtual void            Truncate( ) override                { assert( false ); }

        virtual bool            CanRead( ) const override           { return IsOpen() && m_decompressing; }
        virtual bool            CanWrite( )const  override          { return IsOpen() && !m_decompressing; }

        virtual bool            Read( void * buffer, int64 count, int64 * outCountRead = NULL );
        virtual bool            Write( const void * buffer, int64 count, int64 * outCountWritten = NULL );

    private:
        void                    Initialize( bool decompressing );
        vaStream *              GetInnerStream( ) const             { return (m_compressedStream!=nullptr)?(m_compressedStream.get()):(m_compressedStreamNakedPtr); }
    };


}

