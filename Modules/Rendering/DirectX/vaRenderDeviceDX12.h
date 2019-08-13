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

#include "Rendering/vaRenderDevice.h"

#include "Rendering/DirectX/vaDirectXIncludes.h"
#include "Rendering/DirectX/vaDirectXTools.h"

#include "Core/System/vaMemoryStream.h"

// #include "Rendering/DirectX/vaDebugCanvas2DDX11.h"
// #include "Rendering/DirectX/vaDebugCanvas3DDX11.h"

namespace VertexAsylum
{
    class vaApplicationWin;
    class vaBufferDX12;

    struct MemoryBufferComparer
    {
        bool operator()( const vaMemoryBuffer & left, const vaMemoryBuffer & right ) const
        {
            assert( left.GetSize() == right.GetSize() );
            // comparison logic goes here
            return memcmp( left.GetData(), right.GetData(), std::min( left.GetSize(), right.GetSize() ) ) < 0;
        }
    };

    class vaRenderDeviceDX12 : public vaRenderDevice
    {
        // dynamic persistent descriptor heap, allows single descriptor allocation/deallocation.
        class PersistentDescriptorHeap
        {
        private:
            vaRenderDeviceDX12* m_device            = nullptr;
            int                 m_capacity          = 0;
            int                 m_allocatedCount    = 0;
            vector<int>         m_freed;

            D3D12_DESCRIPTOR_HEAP_DESC 
                                m_heapDesc          = { };
            ComPtr<ID3D12DescriptorHeap>
                                m_heap;
            int                 m_descriptorSize    = 0;

            shared_ptr<std::mutex>   m_mutex;

        public:
            PersistentDescriptorHeap( ) { m_mutex = std::make_shared<std::mutex>(); }
            ~PersistentDescriptorHeap( );

            void                        Initialize( vaRenderDeviceDX12 & device, const D3D12_DESCRIPTOR_HEAP_DESC & desc );

            bool                        Allocate( int & outIndex, D3D12_CPU_DESCRIPTOR_HANDLE & outCPUHandle, D3D12_GPU_DESCRIPTOR_HANDLE & outGPUHandle );
            void                        Release( int index );

            const ComPtr<ID3D12DescriptorHeap> &
                                        GetHeap( ) const { return m_heap; }
            const D3D12_DESCRIPTOR_HEAP_DESC &
                                        GetDesc( ) const { return m_heapDesc; }
        };

        // GPU-side descriptor heap, allows allocations of temporary GPU-placed descriptors that are valid for the current frame
        // (there is no deallocation - the whole heap just gets reused after the GPU has finished drawing the frame)
        class TransientGPUDescriptorHeap
        {
        private:
            vaRenderDeviceDX12* m_device            = nullptr;
            int                 m_capacity          = 0;
            int                 m_allocatedCount    = 0;

            D3D12_DESCRIPTOR_HEAP_DESC 
                                m_heapDesc          = { };
            ComPtr<ID3D12DescriptorHeap>
                                m_heap;
            int                 m_descriptorSize    = 0;

            shared_ptr<std::mutex>   m_mutex;

        public:
            TransientGPUDescriptorHeap( ) { m_mutex = std::make_shared<std::mutex>(); }
            ~TransientGPUDescriptorHeap( );

            void                        Initialize( vaRenderDeviceDX12 & device, const D3D12_DESCRIPTOR_HEAP_DESC & desc );
            void                        Deinitialize( );

            bool                        Allocate( int numberOfDescriptors, int & outIndex );
            void                        Reset( );

            D3D12_CPU_DESCRIPTOR_HANDLE ComputeCPUHandle( int index )      { assert( m_device != nullptr ); return CD3DX12_CPU_DESCRIPTOR_HANDLE( m_heap->GetCPUDescriptorHandleForHeapStart(), index, m_descriptorSize ); }
            D3D12_GPU_DESCRIPTOR_HANDLE ComputeGPUHandle( int index )      { assert( m_device != nullptr ); return CD3DX12_GPU_DESCRIPTOR_HANDLE( m_heap->GetGPUDescriptorHandleForHeapStart(), index, m_descriptorSize ); }

            const ComPtr<ID3D12DescriptorHeap> &
                                        GetHeap( ) const { return m_heap; }
            const D3D12_DESCRIPTOR_HEAP_DESC &
                                        GetDesc( ) const { return m_heapDesc; }
        };

        struct DefaultRootSignatureIndexRanges
        {
            static const int            RootParameterIndexSRVCBVUAV = 0;
            static const int            SRVBase                     = 0;
            static const int            SRVCount                    = 20;
            static const int            CBVBase                     = SRVBase + SRVCount;
            static const int            CBVCount                    = 8;
            static const int            UAVBase                     = CBVBase + CBVCount;
            static const int            UAVCount                    = 8;
            static const int            SRVCBVUAVTotalCount         = SRVCount + CBVCount + UAVCount;

            // static const int            RootParameterIndexSampler   = RootParameterIndexSRVCBVUAV + 1;
            // static const int            SamplerBase                 = 0;
            // static const int            SamplerCount                = 4;
        };

        // this is used for stuff that doesn't change often (with every vaGraphicsItem) like vaSceneDrawContext-related stuff (lighting, globals)
        struct ExtendedRootSignatureIndexRanges
        {
            static const int            RootParameterIndexSRVCBVUAV = DefaultRootSignatureIndexRanges::RootParameterIndexSRVCBVUAV + 1; //RootParameterIndexSampler+1;
            static const int            SRVBase                     = 0;
            static const int            SRVCount                    = 16;
            static const int            CBVBase                     = SRVBase + SRVCount;
            static const int            CBVCount                    = 8;
            static const int            UAVBase                     = CBVBase + CBVCount;
            static const int            UAVCount                    = 4;
            static const int            SRVCBVUAVTotalCount         = SRVCount + CBVCount + UAVCount;

            // static const int            RootParameterIndexSampler   = RootParameterIndexSRVCBVUAV + 1;
            // static const int            SamplerBase                 = 0;
            // static const int            SamplerCount                = 4;
        };

    private:
        string                              m_preferredAdapterNameID = "";

        ComPtr<IDXGIFactory5>               m_DXGIFactory;
        ComPtr<ID3D12Device3>               m_device;

        ComPtr<ID3D12CommandQueue>          m_commandQueue;

        ComPtr<IDXGISwapChain3>             m_swapChain;

        vector<PersistentDescriptorHeap>    m_defaultPersistentDescriptorHeaps;
        TransientGPUDescriptorHeap          m_defaultTransientDescriptorHeaps[vaRenderDevice::c_BackbufferCount];     // SRV/CBV/UAV only - when we hit the per-frame limit, expand to a more dynamic heap allocation - TODO: future
        std::atomic_bool                    m_defaultDescriptorHeapsInitialized = false;

        // this are temporary - should be replaced by vaTexture wrapper once that starts working
        //ComPtr<ID3D12Resource>             m_renderTargets[vaRenderDevice::c_BackbufferCount];
        //vaRenderTargetViewDX12             m_renderTargetViews[vaRenderDevice::c_BackbufferCount];
        shared_ptr<vaTexture>               m_renderTargets[vaRenderDevice::c_BackbufferCount];

        // default root signatures
        ComPtr<ID3D12RootSignature>         m_defaultGraphicsRootSignature;
        ComPtr<ID3D12RootSignature>         m_defaultComputeRootSignature;
        // DefaultRootSignatureIndexRanges     m_defaultRootParamIndexRanges;
        // ExtendedRootSignatureIndexRanges    m_extendedRootParamIndexRanges;

        // synchronization objects
        uint32                              m_currentBackBufferIndex = 0;   // 0..vaRenderDevice::c_BackbufferCount-1, a.k.a. m_frameIndex
        HANDLE                              m_fenceEvent;
        ComPtr<ID3D12Fence>                 m_fence;
        uint64                              m_fenceValues[vaRenderDevice::c_BackbufferCount];

        // not doing this now due to complexity caused by flushes 
        // D3D12_QUERY_DATA_PIPELINE_STATISTICS m_lastStats = {0};
        // ComPtr<ID3D12QueryHeap>             m_statsHeap;
        // shared_ptr<vaBufferDX12>            m_statsReadbackBuffers[vaRenderDevice::c_BackbufferCount];

        // this is the profiler node that covers the whole frame
        vaNestedProfilerNode *              m_frameProfilingNode = nullptr;
//        vaNestedProfilerNode *              m_presentProfilingNode = nullptr;

//
        HWND                                m_hwnd                  = 0;

        std::mutex                          m_beginFrameCallbacksMutex;
        std::atomic_bool                    m_beginFrameCallbacksExecuting = false;
        std::atomic_bool                    m_beginFrameCallbacksDisable = false;
        vector< std::function<void( vaRenderDeviceDX12 & device )> >
                                            m_beginFrameCallbacks;

        std::mutex                          m_GPUFrameFinishedCallbacksMutex;
        std::atomic_bool                    m_GPUFrameFinishedCallbacksExecuting = false;
        vector< std::function<void( vaRenderDeviceDX12 & device )> >
                                            m_GPUFrameFinishedCallbacks[vaRenderDevice::c_BackbufferCount];

        int                                 m_imguiSRVDesc          = -1;
        D3D12_CPU_DESCRIPTOR_HANDLE         m_imguiSRVDescCPUHandle;

        vaConstantBufferViewDX12            m_nullCBV;
        vaShaderResourceViewDX12            m_nullSRV;
        vaUnorderedAccessViewDX12           m_nullUAV;
        vaRenderTargetViewDX12              m_nullRTV;
        vaDepthStencilViewDX12              m_nullDSV;
        vaSamplerViewDX12                   m_nullSamplerView;

        // PSO cache
        std::mutex                          m_PSOCacheMutex;

        std::map<vaMemoryBuffer, shared_ptr<vaGraphicsPSODX12>, MemoryBufferComparer>
                                            m_graphicsPSOCache;
        vaMemoryBuffer                      m_graphicsPSOCacheCleanupLastKey;
        std::map<vaMemoryBuffer, shared_ptr<vaComputePSODX12>, MemoryBufferComparer>
                                            m_computePSOCache;
        vaMemoryBuffer                      m_computePSOCacheCleanupLastKey;

    private:

    public:
        vaRenderDeviceDX12( const string & preferredAdapterNameID = "", const vector<wstring> & shaderSearchPaths = { vaCore::GetExecutableDirectory( ), vaCore::GetExecutableDirectory( ) + L"../../Modules/Rendering/Shaders" } );
        virtual ~vaRenderDeviceDX12( void );

    public:
        HWND                                GetHWND( ) { return m_hwnd; }

    private:
        bool                                Initialize( const vector<wstring> & shaderSearchPaths );
        void                                Deinitialize( );

    protected:
        virtual void                        CreateSwapChain( int width, int height, HWND hwnd, vaFullscreenState fullscreenState ) override;
        virtual bool                        ResizeSwapChain( int width, int height, vaFullscreenState fullscreenState ) override;             // returns true if actually resized
        virtual void                        SetWindowed( ) override;

        virtual bool                        IsSwapChainCreated( ) const                                                     { return m_swapChain != nullptr; }

        virtual void                        BeginFrame( float deltaTime );
        virtual void                        EndAndPresentFrame( int vsyncInterval = 0 );

    public:

//        ID3D11DeviceContext *               GetImmediateRenderContext( ) const                                            { return m_deviceImmediateContext; }
        const ComPtr<ID3D12Device3> &       GetPlatformDevice( ) const                                                      { return m_device; }
        const ComPtr<ID3D12CommandQueue> &  GetCommandQueue( ) const                                                        { return m_commandQueue; }

        virtual vaShaderManager &           GetShaderManager( ) override                                                    { assert(false); return *((vaShaderManager *)nullptr); }

        static void                         RegisterModules( );

        uint32                              GetCurrentBackBufferIndex( ) const                                              { return m_currentBackBufferIndex; }
        virtual shared_ptr<vaTexture>       GetCurrentBackbuffer( ) const override                                          { return m_renderTargets[m_currentBackBufferIndex]; }

        // useful for any early loading, resource copying, etc - happens just after the main context command list is ready to receive commands but before any other rendering is done
        void                                ExecuteAtBeginFrame( const std::function<void( vaRenderDeviceDX12 & device )> & callback );

        // not sure if this is a good idea but seems so at this point - intended to schedule resource deletion after the current fence
        void                                ExecuteAfterCurrentGPUFrameDone( const std::function<void( vaRenderDeviceDX12 & device )> & callback );

        // safely releases the object only after all active command lists have been executed; it makes sure resourcePtr is the only
        // reference to the object, takes ownership and resets resourcePtr so no one can use it after
        template<typename T>
        void                                SafeReleaseAfterCurrentGPUFrameDone( ComPtr<T> & resourcePtr, bool assertOnNotUnique = true );

        const vaConstantBufferViewDX12 &    GetNullCBV        () const                                                     { return  m_nullCBV;       }
        const vaShaderResourceViewDX12 &    GetNullSRV        () const                                                     { return  m_nullSRV;       }
        const vaUnorderedAccessViewDX12&    GetNullUAV        () const                                                     { return  m_nullUAV;       }
        const vaRenderTargetViewDX12   &    GetNullRTV        () const                                                     { return  m_nullRTV;       }
        const vaDepthStencilViewDX12   &    GetNullDSV        () const                                                     { return  m_nullDSV;       }
        const vaSamplerViewDX12        &    GetNullSamplerView() const                                                     { return m_nullSamplerView;}

        // find in cache or create
        shared_ptr<vaGraphicsPSODX12>       FindOrCreateGraphicsPipelineState( const vaGraphicsPSODescDX12 & psoDesc );
        // will set pso to nullptr and add it to reuse cache or deletion queue; this is the only safe way to release this shared pointer
        void                                ReleasePipelineState( shared_ptr<vaGraphicsPSODX12> & pso );

        // find in cache or create
        shared_ptr<vaComputePSODX12>        FindOrCreateComputePipelineState( const vaComputePSODescDX12 & psoDesc );
        // will set pso to nullptr and add it to reuse cache or deletion queue; this is the only safe way to release this shared pointer
        void                                ReleasePipelineState( shared_ptr<vaComputePSODX12> & pso );

        //
    private:
        friend class vaResourceViewDX12;
        friend class vaRenderDeviceContextDX12;
        bool                                AllocatePersistentResourceView( D3D12_DESCRIPTOR_HEAP_TYPE type, int & outIndex, D3D12_CPU_DESCRIPTOR_HANDLE & outCPUHandle, D3D12_GPU_DESCRIPTOR_HANDLE & outGPUHandle );
        void                                ReleasePersistentResourceView( D3D12_DESCRIPTOR_HEAP_TYPE type, int index )       { GetPersistentDescriptorHeap( type )->Release( index ); }

        bool                                ExecuteBeginFrameCallbacks( );
        bool                                ExecuteEndFrameCallbacks( );

        void                                CreateSwapChainRelatedObjects( );
        void                                ReleaseSwapChainRelatedObjects( );

        ID3D12RootSignature *               GetDefaultGraphicsRootSignature( ) const                                        { return m_defaultGraphicsRootSignature.Get(); }
        ID3D12RootSignature *               GetDefaultComputeRootSignature( ) const                                         { return m_defaultComputeRootSignature.Get();  }
        //const DefaultRootSignatureIndexRanges & GetDefaultRootParamIndexRanges( ) const                                     { return m_defaultRootParamIndexRanges; }

        void                                BindDefaultDescriptorHeaps( ID3D12GraphicsCommandList * commandList );

        void                                PSOCachesClearUnusedTick( );

        // will block until all GPU work on m_currentBackBufferIndex finishes (that is, all GPU work) and optionally
        // execute all callbacks pooled by 'ExecuteAfterCurrentGPUFrameDone'
        void                                WaitForGPU( bool executeAfterFrameDoneCallbacks );

        friend class vaResourceViewDX12;
        friend class vaRenderDeviceContextDX12;
        friend class vaTextureDX12;
        PersistentDescriptorHeap *          GetPersistentDescriptorHeap( D3D12_DESCRIPTOR_HEAP_TYPE type );
        TransientGPUDescriptorHeap *        GetTransientDescriptorHeap( D3D12_DESCRIPTOR_HEAP_TYPE type );

    protected:
        virtual void                        ImGuiCreate( ) override;
        virtual void                        ImGuiDestroy( ) override;
        virtual void                        ImGuiNewFrame( ) override;
        // ImGui gets drawn into the main device context ( GetMainContext() ) - this is fixed for now but could be a parameter
        virtual void                        ImGuiEndFrameAndRender( vaRenderDeviceContext & renderContext ) override;

    public:
        virtual string                      GetAPIName( ) const override                                            { return StaticGetAPIName(); }
        static string                       StaticGetAPIName( )                                                     { return "DirectX12"; }
        static void                         StaticEnumerateAdapters( vector<pair<string, string>> & outAdapters );

    };

    //////////////////////////////////////////////////////////////////////////
    // Inline
    //////////////////////////////////////////////////////////////////////////


    inline bool vaRenderDeviceDX12::AllocatePersistentResourceView( D3D12_DESCRIPTOR_HEAP_TYPE type, int & outIndex, D3D12_CPU_DESCRIPTOR_HANDLE & outCPUHandle, D3D12_GPU_DESCRIPTOR_HANDLE & outGPUHandle )
    {
        PersistentDescriptorHeap * allocator = GetPersistentDescriptorHeap( type );

        if( allocator == nullptr )
            { outIndex = -1; assert( false ); }

        return allocator->Allocate( outIndex, outCPUHandle, outGPUHandle );
    }

    template<typename T>
    inline void vaRenderDeviceDX12::SafeReleaseAfterCurrentGPUFrameDone( ComPtr<T> & resourceComPtr, bool assertOnNotUnique )
    {
        if( resourceComPtr == nullptr )
            return;

        // Get native ptr
        IUnknown * resourcePtr = nullptr; 
        HRESULT hr; V( resourceComPtr.CopyTo( &resourcePtr ) );
        
        // Reset ComPtr and make sure we have the only reference (in resourcePtr)
        UINT refCount = resourceComPtr.Reset();
        if( assertOnNotUnique )
            { assert( refCount == 1 ); refCount; }

        // Let the resource be removed when we can guarantee GPU has finished using it
        ExecuteAfterCurrentGPUFrameDone( 
        [resourcePtr, assertOnNotUnique]( vaRenderDeviceDX12 & device )
            { 
                device;
                ULONG refCount = resourcePtr->Release();
                if( assertOnNotUnique ) 
                { assert( refCount == 0 ); refCount; }
            } );
    }

    inline vaRenderDeviceDX12 & AsDX12( vaRenderDevice & device )   { return *device.SafeCast<vaRenderDeviceDX12*>(); }
    inline vaRenderDeviceDX12 * AsDX12( vaRenderDevice * device )   { return device->SafeCast<vaRenderDeviceDX12*>(); }
}