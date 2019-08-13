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

#pragma once

#include "Core/vaCoreIncludes.h"

#include "Rendering/DirectX/vaDirectXIncludes.h"
#include "Rendering/DirectX/vaDirectXTools.h"

#include "Rendering/vaRenderingIncludes.h"


namespace VertexAsylum
{

    // Constant buffers placed on upload heap - could be upgraded to allow for default heap but no pressing need atm
    class vaConstantBufferDX12 : public vaConstantBuffer, public vaShaderResourceDX12//, public vaResourceStateTransitionHelperDX12
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

        // this is for discarded buffers
        struct DetachableUploadBuffer
        {
            vaRenderDeviceDX12 &                Device;
            ComPtr<ID3D12Resource>              Resource;
            D3D12_RESOURCE_STATES               ResourceState;
            vaConstantBufferViewDX12            CBV;
            const uint32                        SizeInBytes;
            const uint32                        ActualSizeInBytes;
            void *                              MappedData;
            bool                                DestroyImmediate = false;

            DetachableUploadBuffer( vaRenderDeviceDX12 & device, ComPtr<ID3D12Resource> resource, uint32 sizeInBytes, uint32 actualSizeInBytes );
            ~DetachableUploadBuffer( );
        };

    private:
        DetachableUploadBuffer *            m_uploadConstantBuffer = nullptr;

        vector< DetachableUploadBuffer * >  m_unusedBuffersPool;

        // // so we can have weak_ptr-s for tracking the lifetime of this object - useful for lambdas and stuff
        // shared_ptr<vaConstantBufferDX12*>   const m_smartThis                       = std::make_shared<vaConstantBufferDX12*>(this);

        // this gets set and re-set on every Create - used to track deferred initial Update fired from a Create; if there's another Create call before its Update gets called, the Update will get orphaned by looking at m_createdThis
        shared_ptr<vaConstantBufferDX12*>   m_createdThis                           = nullptr;

        // mutex                               m_mutex;        // fairly global mutex - proper multithreading not yet supported though

        uint32                              m_actualSizeInBytes     = 0;            // when aligned

        bool                                m_dynamicUpload         = false;

        // these are used if m_dynamicUpload == false
        ComPtr<ID3D12Resource>              m_GPUConstantBuffer     = nullptr;
        vaConstantBufferViewDX12            m_GPUConstantBufferCBV;

    protected:
        friend class vaConstantBuffer;
        explicit                            vaConstantBufferDX12( const vaRenderingModuleParams & params );
        virtual                             ~vaConstantBufferDX12( );

        virtual void                        Update( vaRenderDeviceContext & renderContext, const void * data, uint32 dataSize ) override;
        virtual void                        Create( int bufferSize, const void * initialData, bool dynamicUpload ) override;
        virtual void                        Destroy( ) override;

    public:

        virtual vaResourceBindSupportFlags  GetBindSupportFlags( ) const override                           { return vaResourceBindSupportFlags::ConstantBuffer; }
        virtual const vaConstantBufferViewDX12 *    GetCBV( ) const override;
        virtual const vaUnorderedAccessViewDX12 *   GetUAV( ) const override                                { return nullptr; }
        virtual const vaShaderResourceViewDX12 *    GetSRV( ) const override                                { return nullptr; }

        virtual void TransitionResource( vaRenderDeviceContextDX12 & context, D3D12_RESOURCE_STATES target )  override { context; target; assert( false ); } //static_cast<vaResourceStateTransitionHelperDX12*>(this); }
        virtual void AdoptResourceState( vaRenderDeviceContextDX12 & context, D3D12_RESOURCE_STATES target )  override { context; target; assert( false ); } // if something external does a transition we can update our internal tracking

    private:            
        void                                DestroyInternal( /*bool lockMutex*/ );
        void                                AllocateNextUploadBuffer( );
        void                                SafeReleaseUploadBuffer( DetachableUploadBuffer * uploadBuffer );
    };

    class vaVertIndBufferDX12 // : protected vaResourceStateTransitionHelperDX12//, public vaShaderResourceDX12
    {
        // this is for discarded buffers - only applicable to dynamic upload buffers!
        struct DetachableBuffer
        {
            vaRenderDeviceDX12 &                Device;
            ComPtr<ID3D12Resource>              Resource;
            const uint32                        SizeInBytes;
            DetachableBuffer( vaRenderDeviceDX12 & device, ComPtr<ID3D12Resource> resource, uint32 sizeInBytes, uint32 actualSizeInBytes );
            ~DetachableBuffer( );
        };

    private:
        vaRenderDeviceDX12 &                m_device;
        DetachableBuffer *                  m_buffer = nullptr;
        vector< DetachableBuffer * >        m_unusedBuffersPool;

        bool                                m_dynamicUpload;

        void *                              m_mappedData    = nullptr;

        int                                 m_elementCount  = 0;
        int                                 m_elementSize   = 0;
        uint32                              m_dataSize      = 0;

        // so we can have weak_ptr-s for tracking the lifetime of this object - useful for lambdas and stuff
        shared_ptr<vaVertIndBufferDX12*>    const m_smartThis                       = std::make_shared<vaVertIndBufferDX12*>(this);
        
        // this gets set and re-set on every Create - used to track deferred initial Update fired from a Create; if there's another Create call before its Update gets called, the Update will get orphaned by looking at m_createdThis
        shared_ptr<vaVertIndBufferDX12*>    m_createdThis                           = nullptr;


        // mutable mutex                       m_mutex;        // fairly global mutex - proper multithreading not yet supported though

    public:
                                            vaVertIndBufferDX12( vaRenderDeviceDX12 & device );
        virtual                             ~vaVertIndBufferDX12( );

        void                                Create( int elementCount, int elementSize, const void * initialData, bool dynamicUpload );
        void                                Destroy( );
        bool                                IsCreated( ) const              { /*std::unique_lock<mutex> mutexLock(m_mutex);*/ return m_buffer != nullptr; }

        // This is not an efficient way of updating the vertex buffer as it will create a disposable upload resource but it's fine to do at creation time, etc
        // (it could be optimized but why - dynamicUpload true/false should cover most/all cases?)
        void                                Update( vaRenderDeviceContext & renderContext, const void * data, uint32 dataSize );

        bool                                Map( vaRenderDeviceContext & renderContext, vaResourceMapType mapType );
        void                                Unmap( vaRenderDeviceContext & renderContext );
        bool                                IsMapped( ) const               { /*std::unique_lock<mutex> mutexLock(m_mutex);*/ return m_mappedData != nullptr; }
        void *                              GetMappedData( )                { /*std::unique_lock<mutex> mutexLock(m_mutex);*/ assert(m_mappedData != nullptr); return m_mappedData; }

    public:
        ID3D12Resource *                    GetResource( ) const            { /*std::unique_lock<mutex> mutexLock(m_mutex);*/ return (m_buffer != nullptr)?(m_buffer->Resource.Get()):(nullptr); }

    private:            
        void                                DestroyInternal( bool lockMutex );
    };

    class vaIndexBufferDX12 : public vaIndexBuffer//, public vaShaderResourceDX12
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    private:
        vaVertIndBufferDX12                 m_buffer;

    protected:
        friend class vaConstantBuffer;
        explicit                            vaIndexBufferDX12( const vaRenderingModuleParams & params );
        virtual                             ~vaIndexBufferDX12( )  { }

        virtual void                        Create( int indexCount, const void * initialData ) override                                         { m_buffer.Create( indexCount, 4, initialData, false ); if( IsCreated() ) { m_indexCount = indexCount; m_dataSize = 4*indexCount;  } }
        virtual void                        Destroy( ) override                                                                                 { m_indexCount = 0; m_dataSize = 0; return m_buffer.Destroy(); }
        virtual bool                        IsCreated( ) const override                                                                         { return m_buffer.IsCreated(); }

        virtual void                        Update( vaRenderDeviceContext & renderContext, const void * data, uint32 dataSize ) override           { return m_buffer.Update( renderContext, data, dataSize ); }

        // virtual bool                        Map( vaRenderDeviceContext & renderContext, vaResourceMapType mapType )                                { if( m_buffer.Map( renderContext, mapType ) ) { m_mappedData = m_buffer.GetMappedData(); return true; } else return false; }
        // virtual void                        Unmap( vaRenderDeviceContext & renderContext )                                                         { m_mappedData = nullptr; m_buffer.Unmap( renderContext ); }

    public:
        virtual vaResourceBindSupportFlags  GetBindSupportFlags( ) const override                           { return vaResourceBindSupportFlags::IndexBuffer; }
        ID3D12Resource *                    GetResource( ) const                                            { return m_buffer.GetResource(); }
        D3D12_INDEX_BUFFER_VIEW             GetResourceView( ) const                                        { return { (GetResource() != nullptr)?(GetResource()->GetGPUVirtualAddress()):(0), m_dataSize, DXGI_FORMAT_R32_UINT }; }
    };


    class vaVertexBufferDX12 : public vaVertexBuffer//, public vaShaderResourceDX12
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    private:
        vaVertIndBufferDX12                 m_buffer;

    protected:
        friend class vaConstantBuffer;
        explicit                            vaVertexBufferDX12( const vaRenderingModuleParams & params );
        virtual                             ~vaVertexBufferDX12( )  { }

        virtual void                        Create( int vertexCount, int vertexSize, const void * initialData, bool dynamicUpload ) override    { m_buffer.Create( vertexCount, vertexSize, initialData, dynamicUpload ); if( IsCreated() ) { m_vertexSize = vertexSize; m_vertexCount = vertexCount; m_dataSize = vertexSize*vertexCount; m_dynamicUpload = dynamicUpload; } }
        virtual void                        Destroy( ) override                                                                                 { m_vertexSize = 0; m_vertexCount = 0; m_dataSize = 0; return m_buffer.Destroy(); }
        virtual bool                        IsCreated( ) const override                                                                         { return m_buffer.IsCreated(); }

        virtual void                        Update( vaRenderDeviceContext & renderContext, const void * data, uint32 dataSize ) override           { return m_buffer.Update( renderContext, data, dataSize ); }

        virtual bool                        Map( vaRenderDeviceContext & renderContext, vaResourceMapType mapType )                                { if( m_buffer.Map( renderContext, mapType ) ) { m_mappedData = m_buffer.GetMappedData(); return true; } else return false; }
        virtual void                        Unmap( vaRenderDeviceContext & renderContext )                                                         { m_mappedData = nullptr; m_buffer.Unmap( renderContext ); }

    public:
        virtual vaResourceBindSupportFlags  GetBindSupportFlags( ) const override                           { return vaResourceBindSupportFlags::VertexBuffer; }
        ID3D12Resource *                    GetResource( ) const                                            { return m_buffer.GetResource(); }
        D3D12_VERTEX_BUFFER_VIEW            GetResourceView( ) const                                        { return { (GetResource() != nullptr)?(GetResource()->GetGPUVirtualAddress()):(0), m_dataSize, m_vertexSize }; }
    };
    
    /*
    class vaStructuredBufferDX12 : public vaStructuredBuffer, public vaShaderResourceDX12
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    private:
        ID3D11Buffer *                      m_buffer    = nullptr;
        ID3D11UnorderedAccessView *         m_bufferUAV = nullptr;
        ID3D11ShaderResourceView *          m_bufferSRV = nullptr;

    protected:
        friend class vaConstantBuffer;
        explicit                            vaStructuredBufferDX12( const vaRenderingModuleParams & params );
        virtual                             ~vaStructuredBufferDX12( );

        virtual void                        Update( vaRenderDeviceContext & renderContext, const void * data, uint32 dataSize ) override;
        virtual bool                        Create( int elementCount, int structureByteSize, bool hasCounter, const void * initialData ) override;
        virtual void                        Destroy( ) override;

    public:
        virtual vaResourceBindSupportFlags  GetBindSupportFlags( ) const override                           { return vaResourceBindSupportFlags::ShaderResource | vaResourceBindSupportFlags::UnorderedAccess; }
        virtual ID3D11Buffer *              GetBuffer( ) const override { return m_buffer; }
        virtual ID3D11UnorderedAccessView * GetUAV( ) const override    { return m_bufferUAV; }
        virtual ID3D11ShaderResourceView *  GetSRV( ) const override    { return m_bufferSRV; }
    };
    */

    // platform-dependent generic GPU/CPU buffer helper - temporary solution
    // WARNING - usage model is different from the platform-independent ones and this code is not finished
    class vaBufferDX12
    {
        vaRenderDeviceDX12 &                m_device;
        ComPtr<ID3D12Resource>              m_resource;
        uint64 const                        m_dataSize;
        void *                              m_mappedData = nullptr;

    public:
        vaBufferDX12( vaRenderDeviceDX12 & device, uint64 size, vaResourceAccessFlags accessFlags );
        ~vaBufferDX12( );

        bool                                Map( vaRenderDeviceContext & renderContext, vaResourceMapType mapType );
        void                                Unmap( vaRenderDeviceContext & renderContext );
        bool                                IsMapped( ) const       { return m_mappedData != nullptr; }
        void *                              GetMappedData( )        { assert( IsMapped( ) ); return m_mappedData; }

        uint64                              GetSize( ) const        { return m_dataSize; }
        const ComPtr<ID3D12Resource> &      GetResource( ) const    { return m_resource; }
    };

    inline vaConstantBufferDX12 &  AsDX12( vaConstantBuffer & buffer )   { return *buffer.SafeCast<vaConstantBufferDX12*>(); }
    inline vaConstantBufferDX12 *  AsDX12( vaConstantBuffer * buffer )   { return buffer->SafeCast<vaConstantBufferDX12*>(); }

    inline vaIndexBufferDX12 &  AsDX12( vaIndexBuffer & buffer )   { return *buffer.SafeCast<vaIndexBufferDX12*>(); }
    inline vaIndexBufferDX12 *  AsDX12( vaIndexBuffer * buffer )   { return buffer->SafeCast<vaIndexBufferDX12*>(); }

    inline vaVertexBufferDX12 &  AsDX12( vaVertexBuffer & buffer )   { return *buffer.SafeCast<vaVertexBufferDX12*>(); }
    inline vaVertexBufferDX12 *  AsDX12( vaVertexBuffer * buffer )   { return buffer->SafeCast<vaVertexBufferDX12*>(); }


}