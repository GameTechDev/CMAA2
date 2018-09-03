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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// These classes are a platform-independent CPU/GPU constant/vertex/index buffer layer. They are, however, only
// first iteration and interface is clunky and incomplete - expect major changes in the future.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Core/vaCoreIncludes.h"

#include "vaRendering.h"

#include "Core/Misc/vaResourceFormats.h"

#include "Rendering/vaRenderDeviceContext.h"

namespace VertexAsylum
{

    class vaRenderBuffer : public vaShaderResource
    {
    public:
        virtual ~vaRenderBuffer( )          { }
    };

    class vaConstantBuffer : public vaRenderingModule, public vaRenderBuffer
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    protected:
        uint32                              m_dataSize      = 0;
    
    protected:
        vaConstantBuffer( const vaRenderingModuleParams & params ) : vaRenderingModule( params ) { }
        vaConstantBuffer( vaRenderDevice & device );
    public:
        virtual ~vaConstantBuffer( )        { }

        uint32                              GetDataSize( ) const                                                    { return m_dataSize; }

    public:
        virtual void                        Update( vaRenderDeviceContext & apiContext, const void * data, uint32 dataSize )                                = 0;

        virtual void                        Create( int bufferSize, const void * initialData )                                                              = 0;
        virtual void                        Destroy( )                                                                                                      = 0;

        virtual vaResourceBindSupportFlags  GetBindSupportFlags( ) const override                                   { return vaResourceBindSupportFlags::ConstantBuffer; }
    };

    class vaIndexBuffer : public vaRenderingModule, public vaRenderBuffer
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    protected:
        uint32                              m_dataSize      = 0;
        uint32                              m_indexCount    = 0;
    
    protected:
        vaIndexBuffer( const vaRenderingModuleParams & params ) : vaRenderingModule( params ) { }
        vaIndexBuffer( vaRenderDevice & device ) : vaRenderingModule( vaRenderingModuleParams(device) )    { }
    public:
        virtual ~vaIndexBuffer( )           { }


    public:
        virtual void                        Update( vaRenderDeviceContext & apiContext, const void * data, uint32 dataSize )                                = 0;

        uint32                              GetDataSize( ) const                                                    { return m_dataSize; }
        uint32                              GetIndexCount( ) const                                                  { return m_indexCount; }
    
        virtual void                        Create( int indexCount, const void * initialData = nullptr )                                                    = 0;
        virtual void                        Destroy( )                                                                                                      = 0;

        virtual vaResourceBindSupportFlags  GetBindSupportFlags( ) const override                                   { return vaResourceBindSupportFlags::IndexBuffer; }
    };

    class vaVertexBuffer : public vaRenderingModule, public vaRenderBuffer
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    protected:
        void *                              m_mappedData    = nullptr;

        uint32                              m_dataSize      = 0;
        uint32                              m_vertexSize    = 0;
        uint32                              m_vertexCount   = 0;

        bool                                m_dynamic       = false;
    
    protected:
        vaVertexBuffer( const vaRenderingModuleParams & params ) : vaRenderingModule( params ) { }
        vaVertexBuffer( vaRenderDevice & device ) : vaRenderingModule( vaRenderingModuleParams(device) )    { }
    
    public:
        virtual ~vaVertexBuffer( )          { }


    public:
        virtual void                        Update( vaRenderDeviceContext & apiContext, const void * data, uint32 dataSize )                                = 0;

        bool                                IsMapped( ) const                                                       { return m_mappedData != nullptr; }
        void *                              GetMappedData( )                                                        { assert(IsMapped()); return m_mappedData; }
        virtual bool                        TryMap( vaRenderDeviceContext & apiContext, vaResourceMapType mapType, bool doNotWait = false )                 = 0;
        virtual void                        Unmap( vaRenderDeviceContext & apiContext )                                                                     = 0;


        uint32                              GetVertexCount( ) const                                                 { return m_vertexCount; }
        uint                                GetByteStride( ) const                                                  { return m_vertexSize; }

        virtual void                        Create( int vertexCount, int vertexSize, const void * initialData, bool dynamic )                               = 0;
        virtual void                        Destroy( )                                                                                                      = 0;

        virtual vaResourceBindSupportFlags  GetBindSupportFlags( ) const override                                   { return vaResourceBindSupportFlags::VertexBuffer; }
    };

    // DX11-like structured buffer - has an SRV and an UAV by default; not finished
    class vaStructuredBuffer : public vaRenderingModule, public vaRenderBuffer
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    protected:
        uint32                              m_dataSize                  = 0;
        uint32                              m_structureByteSize         = 0;
        uint32                              m_elementCount              = 0;
    
    protected:
        vaStructuredBuffer( const vaRenderingModuleParams & params ) : vaRenderingModule( params ) { }
        vaStructuredBuffer( vaRenderDevice & device ) : vaRenderingModule( vaRenderingModuleParams(device) )    { }
    public:
        virtual ~vaStructuredBuffer( )      { }

    public:

        uint32                              GetDataSize( ) const                                                    { return m_dataSize;            }
        uint32                              GetStructureByteSize( ) const                                           { return m_structureByteSize;   }
        uint32                              GetElementCount( ) const                                                { return m_elementCount;        }

        virtual void                        Update( vaRenderDeviceContext & apiContext, const void * data, uint32 dataSize )                                = 0;

        virtual bool                        Create( int elementCount, int structureByteSize, bool hasCounter, const void * initialData )                    = 0;
        virtual void                        Destroy( )                                                                                                      = 0;

        virtual vaResourceBindSupportFlags  GetBindSupportFlags( ) const override                                   { return vaResourceBindSupportFlags::ShaderResource; }
    };


    // Constant buffer wrapper will always initialize underlying CPU/GPU data on construction because it knows the size
    template< typename T >
    class vaTypedConstantBufferWrapper
    {
        vaAutoRMI< vaConstantBuffer >       m_cbuffer;

    public:
        explicit vaTypedConstantBufferWrapper( const vaRenderingModuleParams & params, const T* initialData = nullptr ) : m_cbuffer( params.RenderDevice )
        {
            m_cbuffer->Create( sizeof( T ), initialData );
        }
        explicit vaTypedConstantBufferWrapper( vaRenderDevice & device, const T* initialData = nullptr ) : m_cbuffer( device )
        {
            m_cbuffer->Create( sizeof( T ), initialData );
        }
        ~vaTypedConstantBufferWrapper( )    { }

        inline uint32                       GetDataSize( ) const { return m_cbuffer->GetDataSize( ); }
        inline void                         Update( vaRenderDeviceContext & apiContext, const T & data ) { m_cbuffer->Update( apiContext, (void*)&data, sizeof( T ) ); }

        inline const shared_ptr<vaConstantBuffer> & 
                                            GetBuffer( )            { return m_cbuffer.get(); }
        inline operator const shared_ptr<vaConstantBuffer> & ( )    { return m_cbuffer.get(); }
    };

    template< class VertexType >
    class vaTypedVertexBufferWrapper
    {
        vaAutoRMI< vaVertexBuffer >         m_vertexBuffer;

    public:
        explicit vaTypedVertexBufferWrapper( const vaRenderingModuleParams & params, int vertexCount = 0, const VertexType * initialData = nullptr, bool dynamic = false ) : m_vertexBuffer( params.RenderDevice )
        {
            Create( vertexCount, initialData, dynamic );
        }
        explicit vaTypedVertexBufferWrapper( vaRenderDevice & device, int vertexCount = 0, const VertexType * initialData = nullptr, bool dynamic = false ) : m_vertexBuffer( device )
        {
            Create( vertexCount, initialData, dynamic );
        }
        ~vaTypedVertexBufferWrapper( )      { }

        inline uint32                       GetVertexCount( ) const { return m_vertexBuffer->GetVertexCount( ); }

        inline void                         Update( vaRenderDeviceContext & apiContext, const VertexType * vertices, uint32 vertexCount )                       { m_vertexBuffer->Update( apiContext, (void*)vertices, sizeof( VertexType ) * vertexCount ); }

        bool                                IsMapped( ) const                                                                                                   { return IsMapped(); }
        VertexType *                        GetMappedData( )                                                                                                    { return (VertexType*)m_vertexBuffer->GetMappedData(); }
        virtual bool                        TryMap( vaRenderDeviceContext & apiContext, vaResourceMapType mapType, bool doNotWait = false )                     { return m_vertexBuffer->TryMap( apiContext, mapType, doNotWait ); }
        virtual void                        Unmap( vaRenderDeviceContext & apiContext )                                                                         { m_vertexBuffer->Unmap( apiContext ); }

        inline void                         SetToRenderTask( vaRenderItem & task )                                                                              { task.VertexShader = m_vertexBuffer; task.VertexBufferByteStride = sizeof( T ); task.VertexBufferByteOffset = 0; }

        void                                Create( int vertexCount, const void * initialData = nullptr, bool dynamic = false )                                 { m_vertexBuffer->Create( vertexCount, sizeof( VertexType ), initialData, dynamic ); }
        void                                Destroy( )                                                                                                          { m_vertexBuffer->Destroy(); }

        inline const shared_ptr<vaVertexBuffer> & 
                                            GetBuffer()         { return m_vertexBuffer.get(); }
        inline operator const shared_ptr<vaVertexBuffer> & ( )  { return m_vertexBuffer.get(); }
    };


    template< class ElementType >
    class vaTypedStructuredBufferWrapper
    {
        vaAutoRMI< vaStructuredBuffer >     m_buffer;

    public:
        vaTypedStructuredBufferWrapper( const vaRenderingModuleParams & params, int elementCount = 0, bool hasCounter = false, const void * initialData = nullptr ) : m_buffer( params.RenderDevice )
        {
            Create( elementCount, hasCounter, initialData );
        }
        vaTypedStructuredBufferWrapper( vaRenderDevice & device, int elementCount = 0, bool hasCounter = false, const void * initialData = nullptr ) : m_buffer( device )
        {
            Create( elementCount, hasCounter, initialData );
        }
        ~vaTypedStructuredBufferWrapper( )  { }

        inline void                         Update( vaRenderDeviceContext & apiContext, const ElementType * elementArray, uint32 elementCount )                 { m_buffer->Update( apiContext, (void*)elementArray, sizeof(ElementType) * elementCount ); }

        void                                Create( int elementCount, bool hasCounter = false, const void * initialData = nullptr )                             { m_buffer->Create( elementCount, sizeof( ElementType ), hasCounter, initialData ); }
        void                                Destroy( )                                                                                                          { m_buffer->Destroy(); }

        inline const shared_ptr<vaStructuredBuffer> & 
                                            GetBuffer()             { return m_buffer.get(); }
        inline operator const shared_ptr<vaStructuredBuffer> & ( )  { return m_buffer.get(); }
        inline operator const shared_ptr<vaShaderResource> & ( )    { return m_buffer.get(); }
    };
}