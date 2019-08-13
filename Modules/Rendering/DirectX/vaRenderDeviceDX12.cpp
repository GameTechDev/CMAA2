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

#include "Core/vaCoreIncludes.h"

#include "vaRenderDeviceDX12.h"

#include "Rendering/DirectX/vaTextureDX12.h"
#include "Rendering/DirectX/vaRenderDeviceContextDX12.h"
 
#include "Rendering/DirectX/vaShaderDX12.h"
 
#include "Rendering/DirectX/vaGPUTimerDX12.h"

#include "Rendering/DirectX/vaRenderBuffersDX12.h"

#include "Core/vaUI.h"
#include "IntegratedExternals/vaImguiIntegration.h"

#include "Core/Misc/vaProfiler.h"

#include "Core/vaInput.h"

#include "Objbase.h"

#ifdef VA_IMGUI_INTEGRATION_ENABLED
#include "IntegratedExternals/imgui/examples/imgui_impl_dx12.h"
#include "IntegratedExternals/imgui/examples/imgui_impl_win32.h"
#endif

#pragma warning ( disable : 4238 )  //  warning C4238: nonstandard extension used: class rvalue used as lvalue

using namespace VertexAsylum;

#define ALLOW_DXGI_FULLSCREEN

namespace
{
    const DXGI_FORMAT                           c_DefaultBackbufferFormat       = DXGI_FORMAT_R8G8B8A8_UNORM;
    const DXGI_FORMAT                           c_DefaultBackbufferFormatRTV    = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

#ifdef ALLOW_DXGI_FULLSCREEN
    const uint32                                c_DefaultSwapChainFlags         = 0; //DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
#else
    const uint32                                c_DefaultSwapChainFlags         = 0; //DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
#endif
    const D3D_FEATURE_LEVEL                     c_RequiredFeatureLevel          = D3D_FEATURE_LEVEL_12_0;

    typedef HRESULT( WINAPI * LPCREATEDXGIFACTORY2 )( UINT Flags, REFIID, void ** );

    static HMODULE                              s_hModDXGI = NULL;
    static LPCREATEDXGIFACTORY2                 s_DynamicCreateDXGIFactory2 = nullptr;
    static HMODULE                              s_hModD3D12 = NULL;
    static PFN_D3D12_CREATE_DEVICE              s_DynamicD3D12CreateDevice = nullptr;
}
void RegisterDeviceContextDX12( );
//void RegisterSkyDX11( );
void RegisterSkyboxDX12( );
//void RegisterSimpleParticleSystemDX11( );
void RegisterRenderGlobalsDX12( );
void RegisterRenderMeshDX12( );
void RegisterPrimitiveShapeRendererDX12( );
void RegisterRenderMaterialDX12( );
void RegisterGBufferDX12( );
void RegisterPostProcessDX12( );
void RegisterLightingDX12( );
void RegisterPostProcessTonemapDX12( );
void RegisterPostProcessBlurDX12( );
//void RegisterTextureToolsDX11( );
void RegisterShaderDX12( );
void RegisterBuffersDX12( );

void vaRenderDeviceDX12::RegisterModules( )
{
    RegisterShaderDX12( );
    RegisterBuffersDX12( );

    
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX12, vaTexture, vaTextureDX12 );
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX12, vaGPUTimer, vaGPUTimerDX12 );
    RegisterDeviceContextDX12( );
    //RegisterSkyDX11( );
    RegisterSkyboxDX12( );
    //RegisterSimpleParticleSystemDX11( );
    RegisterRenderGlobalsDX12( );
    RegisterRenderMaterialDX12( );
    RegisterRenderMeshDX12( );
    RegisterPrimitiveShapeRendererDX12( );
    RegisterGBufferDX12( );
    RegisterPostProcessDX12( );
    RegisterLightingDX12( );
    RegisterPostProcessTonemapDX12( );
    RegisterPostProcessBlurDX12( );
    //RegisterTextureToolsDX11( );
}

vaRenderDeviceDX12::vaRenderDeviceDX12( const string & preferredAdapterNameID, const vector<wstring> & shaderSearchPaths ) : vaRenderDevice( ),
    m_nullCBV( *this ),
    m_nullSRV( *this ),
    m_nullUAV( *this ),
    m_nullRTV( *this ),
    m_nullDSV( *this ),
    m_nullSamplerView( *this )
{
    m_preferredAdapterNameID = preferredAdapterNameID;

    assert( IsRenderThread() );
    static bool modulesRegistered = false;
    if( !modulesRegistered )
    {
        modulesRegistered = true;
        RegisterModules( );
    }

    Initialize( shaderSearchPaths );
    InitializeBase( );

    // handle initialization callbacks - the only issue is that frame has not really been started and there's no swap chain 
    // so we might get in trouble potentially with something but so far looks ok
    {
        BeginFrame( 0.0f );
        EndAndPresentFrame( );
    }

}

vaRenderDeviceDX12::~vaRenderDeviceDX12( void )
{
    assert( IsRenderThread() );
    assert( !m_frameStarted );

    // did we miss to call these? some logic could be broken but lambdas shouldn't hold any references anyway so it might be safe!
    if( ExecuteBeginFrameCallbacks( ) )
    {
        VA_WARN( "vaRenderDeviceDX12::Deinitialize() - there were some m_beginFrameCallbacks calls; this likely means that some resources were created just before shutdown which is probably safe but inefficient" );
    }
    { std::unique_lock<std::mutex> mutexLock(m_beginFrameCallbacksMutex); m_beginFrameCallbacksDisable = true; }

    e_DeviceAboutToBeDestroyed.Invoke();

    WaitForGPU( false );

    ImGuiDestroy( );

    DeinitializeBase( );

    // context gets nuked here!
    m_mainDeviceContext = nullptr;

    if( m_fullscreenState != vaFullscreenState::Windowed )
        SetWindowed( );

    // // have to release these first!
    // m_mainDeviceContext->SetRenderTarget( nullptr, nullptr, false );
    ReleaseSwapChainRelatedObjects( );
    m_swapChain = nullptr;

    m_nullCBV.SafeRelease();
    m_nullSRV.SafeRelease();
    m_nullUAV.SafeRelease();
    m_nullRTV.SafeRelease();
    m_nullDSV.SafeRelease();
    m_nullSamplerView.SafeRelease();

    // clear PSO cache
    {
        for( auto & it : m_graphicsPSOCache )
            ReleasePipelineState( it.second );
        m_graphicsPSOCache.clear();
        for( auto & it : m_computePSOCache )
            ReleasePipelineState( it.second );
        m_computePSOCache.clear();
    }

    // one last time but clear all - as there's a queue for each frame/swapchain 
    // and nuke the command queue
    {
        // make sure GPU is not executing anything from us anymore and call & clear all callbacks
        WaitForGPU( false );

        int prevBackBufferIndex = m_currentBackBufferIndex;
        for( int currentBackBufferIndex = 0; currentBackBufferIndex < c_BackbufferCount; currentBackBufferIndex++ )
        {
            m_currentBackBufferIndex = currentBackBufferIndex;
            ExecuteEndFrameCallbacks();
        }
        m_currentBackBufferIndex = prevBackBufferIndex;

        m_fence.Reset();
        CloseHandle(m_fenceEvent);
        m_commandQueue = nullptr;
    }

    m_defaultGraphicsRootSignature.Reset();
    m_defaultComputeRootSignature.Reset();

    for( int i = 0; i < _countof(m_defaultTransientDescriptorHeaps); i++ )
        m_defaultTransientDescriptorHeaps[i].Deinitialize();
    m_defaultPersistentDescriptorHeaps.clear();
    m_defaultDescriptorHeapsInitialized = false;

    // have to go after DeinitializeBase() because that's where vaProfiler gets killed
    delete vaGPUTimerManagerDX12::GetInstancePtr( );

    // we can call them all safely hopefully.
    Deinitialize( );

    // just a sanity check
    {
        std::unique_lock<std::mutex> mutexLock(m_GPUFrameFinishedCallbacksMutex);
#ifdef _DEBUG
        for( int i = 0; i < _countof(m_GPUFrameFinishedCallbacks); i++ )
        {
            auto & callbacks = m_GPUFrameFinishedCallbacks[m_currentBackBufferIndex];
            assert( callbacks.size() == 0 ) ;
        }
#endif
    }

}

string FormatAdapterID( const DXGI_ADAPTER_DESC3 & desc )
{
    string strID = vaStringTools::SimpleNarrow(wstring(desc.Description)) + vaStringTools::Format( " (%#010x)", desc.SubSysId );
    //std::replace( strID.begin( ), strID.end( ), ' ', '_' );
    return strID;
}

void EnsureDirectXAPILoaded( )
{
    if( s_hModDXGI == NULL )
    {
        s_hModDXGI = LoadLibrary( L"dxgi.dll" );
        if( s_hModDXGI == NULL )
            VA_ERROR( L"Unable to load dxgi.dll; Vista SP2, Win7 or above required" );
    }

    if( s_DynamicCreateDXGIFactory2 == nullptr && s_hModDXGI != NULL )
    {
        s_DynamicCreateDXGIFactory2 = (LPCREATEDXGIFACTORY2)GetProcAddress( s_hModDXGI, "CreateDXGIFactory2" );
        if( s_DynamicCreateDXGIFactory2 == nullptr )
            VA_ERROR( L"Unable to create CreateDXGIFactory1 proc; Vista SP2, Win7 or above required" );
    }

    if( s_hModD3D12 == NULL )
    {
        s_hModD3D12 = LoadLibrary( L"d3d12.dll" );
        if( s_hModD3D12 == NULL )
            VA_ERROR( L"Unable to load d3d12.dll; please install the latest DirectX." );
    }

    if( s_DynamicD3D12CreateDevice == NULL && s_hModD3D12 != NULL )
    {
        s_DynamicD3D12CreateDevice = (PFN_D3D12_CREATE_DEVICE)GetProcAddress( s_hModD3D12, "D3D12CreateDevice" );
        if( s_DynamicD3D12CreateDevice == NULL )
            VA_ERROR( L"D3D11CreateDevice proc not found" );
    }
}

/*
// Helper function for acquiring the first available hardware adapter that supports Direct3D 12.
// If no such adapter can be found, *ppAdapter will be set to nullptr.
static void GetHardwareAdapter(IDXGIFactory5* pFactory, IDXGIAdapter4** ppAdapter)
{
    ComPtr<IDXGIAdapter4> adapter;
    *ppAdapter = nullptr;

    IDXGIAdapter1 * adapter1Ptr;
    for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &adapter1Ptr); ++adapterIndex)
    {
        ComPtr<IDXGIAdapter1>(adapter1Ptr).As(&adapter);
        SAFE_RELEASE( adapter1Ptr );

        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            // Don't select the Basic Render Driver adapter.
            // If you want a software adapter, pass in "/warp" on the command line.
            continue;
        }

        // Check to see if the adapter supports Direct3D 12, but don't create the
        // actual device yet.
        if (SUCCEEDED(s_DynamicD3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
        {
            break;
        }
    }

    *ppAdapter = adapter.Detach();
}
*/

bool vaRenderDeviceDX12::Initialize( const vector<wstring> & shaderSearchPaths )
{
    assert( IsRenderThread() );
    assert( !m_frameStarted );

    EnsureDirectXAPILoaded( );

    HRESULT hr = S_OK;
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
        ComPtr<ID3D12Debug3> debugController3;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController3))))
        {
#if 0 // this is really slow so only do it occasionally 
            debugController3->SetEnableGPUBasedValidation( TRUE );
#endif 
        }
    }
#endif
    // Create DXGI factory
    {
        hr = s_DynamicCreateDXGIFactory2( dxgiFactoryFlags, IID_PPV_ARGS(&m_DXGIFactory) );
        if( FAILED( hr ) )
            VA_ERROR( L"Unable to create DXGIFactory; Your Windows 10 probably needs updating" );
    }

    
    // create IDXGIAdapter1 based on m_preferredAdapterNameID
    ComPtr<IDXGIAdapter4> adapter;
    {
        bool useWarpDevice = m_preferredAdapterNameID == "WARP";

        if (useWarpDevice)
        {
            hr = m_DXGIFactory->EnumWarpAdapter(IID_PPV_ARGS(&adapter));
            if( FAILED( hr ) )
            {
                VA_ERROR( L"Unable to create WARP device" );
                adapter = nullptr;
            }
        }
        
        if( adapter == nullptr )
        {
            int i = 0;
            IDXGIAdapter1* adapterTemp;
            ComPtr<IDXGIAdapter4> adapterCandidate;
            while( m_DXGIFactory->EnumAdapters1(i, &adapterTemp) != DXGI_ERROR_NOT_FOUND )
            { 
	            i++; 
                hr = ComPtr<IDXGIAdapter1>(adapterTemp).As(&adapterCandidate);
                SAFE_RELEASE( adapterTemp );

                if( SUCCEEDED( hr ) )
                {
                    DXGI_ADAPTER_DESC3 desc;
                    adapterCandidate->GetDesc3( &desc );

                    // only hardware devices enumerated
                    if( (desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE) != 0 )
                        continue;

                    // check feature level
                    if( FAILED( D3D12CreateDevice(adapterCandidate.Get(), c_RequiredFeatureLevel, _uuidof(ID3D12Device), nullptr) ) )
                        continue;

                    // use first good by default
                    if( adapter == nullptr )
                    {
                        adapter = adapterCandidate;
                    }

                    if( FormatAdapterID(desc) == m_preferredAdapterNameID )
                    {
                        adapter = adapterCandidate;
                        break;
                    }
                }
            } 
        }

        V_RETURN( s_DynamicD3D12CreateDevice(
                adapter.Get(),
                c_RequiredFeatureLevel,
                IID_PPV_ARGS(&m_device)
                ));
    }

    // Describe and create the command queue.
    // command queue is part of vaRenderDevice, command lists are part of vaRenderContext
    {
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        V_RETURN( m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)) );
    }

    {
        // Disable some annoying warnings
        ComPtr<ID3D12InfoQueue> d3dInfoQueue;
        hr = m_device.As(&d3dInfoQueue);
        if( SUCCEEDED( hr ) )
        {
            D3D12_MESSAGE_ID hide [] =
            {
                D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
                D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
                D3D12_MESSAGE_ID_COPY_DESCRIPTORS_INVALID_RANGES,                   // <- false positives; remove once bug in debugging layer gets fixed
                // TODO: Add more message IDs here as needed 
            };
            D3D12_INFO_QUEUE_FILTER filter;
            memset(&filter, 0, sizeof(filter));
            filter.DenyList.NumIDs = _countof(hide);
            filter.DenyList.pIDList = hide;
            d3dInfoQueue->AddStorageFilterEntries(&filter);

             d3dInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_CORRUPTION, true );
             d3dInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_ERROR, true );
             d3dInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_WARNING, true );
        }
    }

    V( m_device->SetName( L"MainDevice" ) );
    V( m_commandQueue->SetName( L"MainDeviceCommandQueue" ) );

    // Default descriptor heaps
    {
        m_defaultPersistentDescriptorHeaps.resize( (int)D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES );

        for( int i = 0; i < m_defaultPersistentDescriptorHeaps.size(); i++ )
        {
            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            heapDesc.Type = (D3D12_DESCRIPTOR_HEAP_TYPE)i;
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            switch( heapDesc.Type )
            {
            case ( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ) : 
                // heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; //no longer using this heap for shader-visible
                heapDesc.NumDescriptors = 16 * 1024;
                break;
            case ( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ) :
                heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; //no longer using this heap for shader-visible
                heapDesc.NumDescriptors = 256;
                break;
            case ( D3D12_DESCRIPTOR_HEAP_TYPE_RTV ) :
                heapDesc.NumDescriptors = 256;
                break;
            case ( D3D12_DESCRIPTOR_HEAP_TYPE_DSV ) :
                heapDesc.NumDescriptors = 256;
                break;
            default:
                assert( false ); // new type added?
                break;
            }
            m_defaultPersistentDescriptorHeaps[i].Initialize( *this, heapDesc );

            switch( heapDesc.Type )
            {
            case ( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ) :   V( m_defaultPersistentDescriptorHeaps[i].GetHeap()->SetName( L"DefaultPersistentHeap_CBV_SRV_UAV" ) ); break;
            case ( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ) :       V( m_defaultPersistentDescriptorHeaps[i].GetHeap()->SetName( L"DefaultPersistentHeap_Sampler" ) ); break;
            case ( D3D12_DESCRIPTOR_HEAP_TYPE_RTV ) :           V( m_defaultPersistentDescriptorHeaps[i].GetHeap()->SetName( L"DefaultPersistentHeap_RTV" ) ); break;
            case ( D3D12_DESCRIPTOR_HEAP_TYPE_DSV ) :           V( m_defaultPersistentDescriptorHeaps[i].GetHeap()->SetName( L"DefaultPersistentHeap_DSV" ) ); break;
            default:    assert( false ); // new type added?
            }
        }

        const int approxMaxDrawItemsPerFrame = 16 * 1024;   // umm... just a guesstimate
        for( int frameIndex = 0; frameIndex < _countof(m_defaultTransientDescriptorHeaps); frameIndex++ )
        {
            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            heapDesc.NumDescriptors = DefaultRootSignatureIndexRanges::SRVCBVUAVTotalCount * approxMaxDrawItemsPerFrame;
            m_defaultTransientDescriptorHeaps[frameIndex].Initialize( *this, heapDesc );
            V( m_defaultTransientDescriptorHeaps[frameIndex].GetHeap()->SetName( L"DefaultTransientHeap_CBV_SRV_UAV" ) );

            /*
            m_defaultTransientDescriptorHeaps[frameIndex].resize( (int)D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES );

            for( int i = 0; i < m_defaultTransientDescriptorHeaps[frameIndex].size(); i++ )
            {
                D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
                heapDesc.Type   = (D3D12_DESCRIPTOR_HEAP_TYPE)i;
                heapDesc.Flags  = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
                switch( heapDesc.Type )
                {
                case ( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ) : 
                    heapDesc.NumDescriptors = DefaultRootSignatureIndexRanges::SRVCBVUAVTotalCount * approxMaxDrawItemsPerFrame;
                    break;
                case ( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ) :
                    heapDesc.NumDescriptors = 2048;// approxMaxDrawItemsPerFrame; limit is 2028....
                    break;
                case ( D3D12_DESCRIPTOR_HEAP_TYPE_RTV ) :
                case ( D3D12_DESCRIPTOR_HEAP_TYPE_DSV ) :
                    heapDesc.NumDescriptors = 0;
                    break;
                default:
                    assert( false ); // new type added?
                    break;
                }
                if( heapDesc.NumDescriptors == 0 )
                    continue;
                m_defaultTransientDescriptorHeaps[frameIndex][i].Initialize( *this, heapDesc );

                switch( heapDesc.Type )
                {
                case ( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ) :   V( m_defaultTransientDescriptorHeaps[frameIndex][i].GetHeap()->SetName( L"DefaultGPUHeap_CBV_SRV_UAV" ) ); break;
                case ( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ) :       V( m_defaultTransientDescriptorHeaps[frameIndex][i].GetHeap()->SetName( L"DefaultGPUHeap_Sampler" ) ); break;
                case ( D3D12_DESCRIPTOR_HEAP_TYPE_RTV ) :           V( m_defaultTransientDescriptorHeaps[frameIndex][i].GetHeap()->SetName( L"DefaultGPUHeap_RTV" ) ); break;
                case ( D3D12_DESCRIPTOR_HEAP_TYPE_DSV ) :           V( m_defaultTransientDescriptorHeaps[frameIndex][i].GetHeap()->SetName( L"DefaultGPUHeap_DSV" ) ); break;
                default:    assert( false ); // new type added?
                }
            }
            */
        }

        m_defaultDescriptorHeapsInitialized = true;
    }


    {
        DXGI_ADAPTER_DESC3 adapterDesc3;
        adapter->GetDesc3( &adapterDesc3 );

        string name = vaStringTools::SimpleNarrow( wstring( adapterDesc3.Description ) );
        // std::replace( name.begin( ), name.end( ), ' ', '_' );

        m_adapterNameShort  = name;
        m_adapterNameID     = FormatAdapterID(adapterDesc3); //vaStringTools::Format( L"%s-%08X_%08X", name.c_str( ), adapterDesc1.DeviceId, adapterDesc1.Revision );
        m_adapterVendorID   = adapterDesc3.VendorId;

        if( (adapterDesc3.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE) != 0 )
            m_adapterNameID = "WARP";
    }

    // synchronization objects
    {
        V( m_device->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence) ) );
        V( m_fence->SetName( L"MainDeviceFence" ) );
        for( int i = 0; i < _countof(m_fenceValues); i++ )
            m_fenceValues[i] = 1;

        // Create an event handle to use for frame synchronization.
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            V( HRESULT_FROM_WIN32(GetLastError()) );
        }
    }

    // // stats
    // {
    //     D3D12_QUERY_HEAP_DESC queryHeapDesc;
    //     queryHeapDesc.Count     = 1;
    //     queryHeapDesc.NodeMask  = 0;
    //     queryHeapDesc.Type      = D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;
    //     m_device->CreateQueryHeap( &queryHeapDesc, IID_PPV_ARGS(&m_statsHeap) );
    //     m_statsHeap->SetName( L"Stats heap" );
    // 
    //     for( int i = 0; i < _countof(m_statsReadbackBuffers); i++ )
    //         m_statsReadbackBuffers[i] = shared_ptr<vaBufferDX12>( new vaBufferDX12( *this, sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS ), vaResourceAccessFlags::CPURead | vaResourceAccessFlags::CPUReadManuallySynced ) );
    // }

    // profiling
    new vaGPUTimerManagerDX12( *this );

    // Shader manager
    {
        m_shaderManager = shared_ptr<vaShaderManager>( new vaDirectX12ShaderManager( *this ) );
        for( auto s : shaderSearchPaths ) m_shaderManager->RegisterShaderSearchPath( s );
    }

    // null descriptors
    {
        m_nullCBV.CreateNull();
        m_nullSRV.CreateNull();
        m_nullUAV.CreateNull();
        m_nullRTV.CreateNull();
        m_nullDSV.CreateNull();
        m_nullSamplerView.CreateNull();
    }

        // Create the root signature.
    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

        // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if( FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))) )
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        /*
        CD3DX12_ROOT_PARAMETER1 rootParameters[ m_defaultRootParamIndexRanges.SRVCount + m_defaultRootParamIndexRanges.CBVCount + m_defaultRootParamIndexRanges.UAVCount + m_defaultRootParamIndexRanges.SamplerCount ];
        CD3DX12_DESCRIPTOR_RANGE1 rootParameterRanges[ m_defaultRootParamIndexRanges.SRVCount + m_defaultRootParamIndexRanges.CBVCount + m_defaultRootParamIndexRanges.UAVCount + m_defaultRootParamIndexRanges.SamplerCount ];

        for( int i = 0; i < m_defaultRootParamIndexRanges.SRVCount; i++ )
        {
            int index = m_defaultRootParamIndexRanges.SRVBase + i;
            rootParameterRanges[index].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 
                i,                                              // <- this refers to shader register, for ex for shader-resource views (SRVs), 3 maps to ": register(t3);" in HLSL.
                0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE, 0 );
            rootParameters[index].InitAsDescriptorTable( 1, &rootParameterRanges[index], D3D12_SHADER_VISIBILITY_ALL );
        }

        for( int i = 0; i < m_defaultRootParamIndexRanges.CBVCount; i++ )
        {
            int index = m_defaultRootParamIndexRanges.CBVBase + i;
            rootParameterRanges[index].Init( D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 
                i,                                              // <- this refers to shader register, for ex for shader-resource views (SRVs), 3 maps to ": register(t3);" in HLSL.
                0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE, 0 );
            rootParameters[index].InitAsDescriptorTable( 1, &rootParameterRanges[index], D3D12_SHADER_VISIBILITY_ALL );
        }

        for( int i = 0; i < m_defaultRootParamIndexRanges.UAVCount; i++ )
        {
            int index = m_defaultRootParamIndexRanges.UAVBase + i;
            rootParameterRanges[index].Init( D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 
                i,                                              // <- this refers to shader register, for ex for shader-resource views (SRVs), 3 maps to ": register(t3);" in HLSL.
                0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE, 0 );
            rootParameters[index].InitAsDescriptorTable( 1, &rootParameterRanges[index], D3D12_SHADER_VISIBILITY_ALL );
        }

        for( int i = 0; i < m_defaultRootParamIndexRanges.SamplerCount; i++ )
        {
            int index = m_defaultRootParamIndexRanges.SamplerBase + i;
            rootParameterRanges[index].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 
                i,                                              // <- this refers to shader register, for ex for shader-resource views (SRVs), 3 maps to ": register(t3);" in HLSL.
                0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE, 0 );
            rootParameters[index].InitAsDescriptorTable( 1, &rootParameterRanges[index], D3D12_SHADER_VISIBILITY_ALL );
        }
        */

        // SRVs, CBVs, UAVs
        CD3DX12_ROOT_PARAMETER1 rootParameterBaseSRVCBVUAV;
        CD3DX12_DESCRIPTOR_RANGE1 rootParameterBaseSRVCBVUAVRanges[ 3 ];
        {
            rootParameterBaseSRVCBVUAVRanges[0].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, DefaultRootSignatureIndexRanges::SRVCount, 
                0,                                              // <- this refers to shader register, for ex for shader-resource views (SRVs), 3 maps to ": register(t3);" in HLSL.
                0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE, DefaultRootSignatureIndexRanges::SRVBase );
    
            // CBVs
            rootParameterBaseSRVCBVUAVRanges[1].Init( D3D12_DESCRIPTOR_RANGE_TYPE_CBV, DefaultRootSignatureIndexRanges::CBVCount, 
                0,                                              // <- this refers to shader register, for ex for shader-resource views (SRVs), 3 maps to ": register(t3);" in HLSL.
                0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE, DefaultRootSignatureIndexRanges::CBVBase );
            
            // UAVs
            rootParameterBaseSRVCBVUAVRanges[2].Init( D3D12_DESCRIPTOR_RANGE_TYPE_UAV, DefaultRootSignatureIndexRanges::UAVCount, 
                0,                                              // <- this refers to shader register, for ex for shader-resource views (SRVs), 3 maps to ": register(t3);" in HLSL.
                0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE, DefaultRootSignatureIndexRanges::UAVBase );
            rootParameterBaseSRVCBVUAV.InitAsDescriptorTable( _countof(rootParameterBaseSRVCBVUAVRanges), &rootParameterBaseSRVCBVUAVRanges[0], D3D12_SHADER_VISIBILITY_ALL );
        }

        // // Samplers
        // CD3DX12_ROOT_PARAMETER1 rootParameterBaseSampler;
        // CD3DX12_DESCRIPTOR_RANGE1 rootParameterBaseSamplerRanges[ 1 ];
        // {
        //     rootParameterBaseSamplerRanges[0].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, DefaultRootSignatureIndexRanges::SamplerCount, 
        //         0,                                              // <- this refers to shader register, for ex for shader-resource views (SRVs), 3 maps to ": register(t3);" in HLSL.
        //         0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE, DefaultRootSignatureIndexRanges::SamplerBase );
        //     rootParameterBaseSampler.InitAsDescriptorTable( _countof(rootParameterBaseSamplerRanges), &rootParameterBaseSamplerRanges[0], D3D12_SHADER_VISIBILITY_ALL );
        // }

        // Extended SRVs, CBVs, UAVs
        CD3DX12_ROOT_PARAMETER1 rootParameterExtendedSRVCBVUAV;
        CD3DX12_DESCRIPTOR_RANGE1 rootParameterExtendedSRVCBVUAVRanges[ 3 ];
        {
            rootParameterExtendedSRVCBVUAVRanges[0].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, ExtendedRootSignatureIndexRanges::SRVCount, 
                EXTENDED_SRV_SLOT_BASE,                 // <- this refers to shader register, for ex for shader-resource views (SRVs), 3 maps to ": register(t3);" in HLSL.
                0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE, ExtendedRootSignatureIndexRanges::SRVBase );
    
            // CBVs
            rootParameterExtendedSRVCBVUAVRanges[1].Init( D3D12_DESCRIPTOR_RANGE_TYPE_CBV, ExtendedRootSignatureIndexRanges::CBVCount, 
                EXTENDED_CBV_SLOT_BASE,                 // <- this refers to shader register, for ex for shader-resource views (SRVs), 3 maps to ": register(t3);" in HLSL.
                0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE, ExtendedRootSignatureIndexRanges::CBVBase );
            
            // UAVs
            rootParameterExtendedSRVCBVUAVRanges[2].Init( D3D12_DESCRIPTOR_RANGE_TYPE_UAV, ExtendedRootSignatureIndexRanges::UAVCount, 
                EXTENDED_UAV_SLOT_BASE,                 // <- this refers to shader register, for ex for shader-resource views (SRVs), 3 maps to ": register(t3);" in HLSL.
                0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE, ExtendedRootSignatureIndexRanges::UAVBase );
            rootParameterExtendedSRVCBVUAV.InitAsDescriptorTable( _countof(rootParameterExtendedSRVCBVUAVRanges), &rootParameterExtendedSRVCBVUAVRanges[0], D3D12_SHADER_VISIBILITY_ALL );
        }

        // // Extended Samplers
        // CD3DX12_ROOT_PARAMETER1 rootParameterExtendedSampler;
        // CD3DX12_DESCRIPTOR_RANGE1 rootParameterExtendedSamplerRanges[ 1 ];
        // {
        //     rootParameterExtendedSamplerRanges[0].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, ExtendedRootSignatureIndexRanges::SamplerCount, 
        //         EXTENDED_SAMPLER_SLOT_BASE,                 // <- this refers to shader register, for ex for shader-resource views (SRVs), 3 maps to ": register(t3);" in HLSL.
        //         0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE, ExtendedRootSignatureIndexRanges::SamplerBase );
        //     rootParameterExtendedSampler.InitAsDescriptorTable( _countof(rootParameterExtendedSamplerRanges), &rootParameterExtendedSamplerRanges[0], D3D12_SHADER_VISIBILITY_ALL );
        // }

        // copy them all to one big root parameter array
        CD3DX12_ROOT_PARAMETER1 rootParameters[ 2 ];
        rootParameters[ DefaultRootSignatureIndexRanges::RootParameterIndexSRVCBVUAV ]  = rootParameterBaseSRVCBVUAV;
        //rootParameters[ DefaultRootSignatureIndexRanges::RootParameterIndexSampler ]    = rootParameterBaseSampler;
        rootParameters[ ExtendedRootSignatureIndexRanges::RootParameterIndexSRVCBVUAV ] = rootParameterExtendedSRVCBVUAV;
        //rootParameters[ ExtendedRootSignatureIndexRanges::RootParameterIndexSampler ]   = rootParameterExtendedSampler;

        D3D12_STATIC_SAMPLER_DESC defaultSamplers[7];
        vaDirectXTools12::FillSamplerStatePointClamp(         defaultSamplers[0] );
        vaDirectXTools12::FillSamplerStatePointWrap(          defaultSamplers[1] );
        vaDirectXTools12::FillSamplerStateLinearClamp(        defaultSamplers[2] );
        vaDirectXTools12::FillSamplerStateLinearWrap(         defaultSamplers[3] );
        vaDirectXTools12::FillSamplerStateAnisotropicClamp(   defaultSamplers[4] );
        vaDirectXTools12::FillSamplerStateAnisotropicWrap(    defaultSamplers[5] );
        vaDirectXTools12::FillSamplerStateShadowCmp(          defaultSamplers[6] );

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, _countof(defaultSamplers), defaultSamplers, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        if( FAILED( D3DX12SerializeVersionedRootSignature( &rootSignatureDesc, featureData.HighestVersion, &signature, &error ) ) )
        {
            wstring errorMsg = vaStringTools::SimpleWiden( (char*)error->GetBufferPointer( ) );
            VA_ERROR( L"Error serializing versioned root signature: \n %s", errorMsg.c_str() );
        }
        V( m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_defaultGraphicsRootSignature)) );
        V( m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_defaultComputeRootSignature)) );
        V( m_defaultGraphicsRootSignature->SetName( L"DefaultGraphicsRootSignature" ) );
        V( m_defaultComputeRootSignature->SetName( L"DefaultGraphicsRootSignature" ) );
    }

    // main context
    {
        m_mainDeviceContext = std::shared_ptr< vaRenderDeviceContext >( vaRenderDeviceContextDX12::Create( *this, 42 ) );
    }

    e_DeviceFullyInitialized.Invoke( *this );

    return true;
}

void vaRenderDeviceDX12::Deinitialize( )
{
    assert( IsRenderThread() );
    assert( !m_frameStarted );

    if( m_device != nullptr )
    {
        // no idea how to do this for D3D12 yet
        // more on http://seanmiddleditch.com/direct3d-11-debug-api-tricks/
#if 0//_DEBUG
        ID3D11Debug* debugDevice = nullptr;
        HRESULT hr = m_device->QueryInterface( __uuidof( ID3D11Debug ), reinterpret_cast<void**>( &debugDevice ) );
        assert( SUCCEEDED( hr ) );
        debugDevice->ReportLiveDeviceObjects( D3D11_RLDO_DETAIL );
        SAFE_RELEASE( debugDevice );
#endif

        m_adapterNameShort  = "";
        m_adapterNameID     = "";
        m_adapterVendorID   = 0;
        m_device.Reset();
        m_DXGIFactory.Reset();
        m_hwnd = 0;
        m_currentBackBufferIndex = 0;
    }
}

void vaRenderDeviceDX12::CreateSwapChain( int width, int height, HWND hwnd, vaFullscreenState fullscreenState )
{
    assert( IsRenderThread() );
    assert( !m_frameStarted );

    m_swapChainTextureSize.x = width;
    m_swapChainTextureSize.y = height;
    m_hwnd = hwnd;

    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount       = vaRenderDevice::c_BackbufferCount;
    swapChainDesc.Width             = width;
    swapChainDesc.Height            = height;
    swapChainDesc.Format            = c_DefaultBackbufferFormat;
    swapChainDesc.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_BACK_BUFFER;
    swapChainDesc.SwapEffect        = DXGI_SWAP_EFFECT_FLIP_DISCARD; // DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL
    swapChainDesc.Flags             = c_DefaultSwapChainFlags;
    swapChainDesc.SampleDesc.Count  = 1;

//#ifdef ALLOW_DXGI_FULLSCREEN
//    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreenDesc = {};
//    fullscreenDesc.RefreshRate      = {0,0};
//    fullscreenDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
//    fullscreenDesc.Scaling          = DXGI_MODE_SCALING_UNSPECIFIED;
//    fullscreenDesc.Windowed         = !fullscreen;
//#endif
//
//    m_fullscreen = fullscreen;

    HRESULT hr;

    ComPtr<IDXGISwapChain1> swapChain;
    hr = m_DXGIFactory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        hwnd,
        &swapChainDesc,
//#ifdef ALLOW_DXGI_FULLSCREEN
//        &fullscreenDesc,
//#else
        nullptr,
//#endif
        nullptr,
        &swapChain
        );
    V( hr );

    // stop automatic alt+enter, we'll handle it manually
    {
        hr = m_DXGIFactory->MakeWindowAssociation( hwnd, DXGI_MWA_NO_ALT_ENTER );
    }

    V( swapChain.As( &m_swapChain ) );

    CreateSwapChainRelatedObjects( );

    ImGuiCreate( );

    // now switch to fullscreen if we are in fullscreen
    assert( fullscreenState != vaFullscreenState::Unknown );
    if( fullscreenState == vaFullscreenState::Fullscreen )
        ResizeSwapChain( m_swapChainTextureSize.x, m_swapChainTextureSize.y, fullscreenState );
    else
        m_fullscreenState = fullscreenState;
}

void vaRenderDeviceDX12::CreateSwapChainRelatedObjects( )
{
    assert( IsRenderThread() );
    assert( !m_frameStarted );

    // Create a RTV and a command allocator for each frame.
    for( int i = 0; i < c_BackbufferCount; i++ )
    {
        ComPtr<ID3D12Resource> renderTarget;
        m_swapChain->GetBuffer( i, IID_PPV_ARGS(&renderTarget) );

        D3D12_RESOURCE_DESC resDesc = renderTarget->GetDesc( );
        assert( resDesc.Width == m_swapChainTextureSize.x );
            
        // D3D12_RENDER_TARGET_VIEW_DESC desc;
        // VA_VERIFY( vaDirectXTools12::FillRenderTargetViewDesc( desc, m_renderTargets[i].Get(), c_DefaultBackbufferFormatSRV ), "Unable to initialize render target view desc" );
        // 
        // m_renderTargetViews[i].Create( m_renderTargets[i].Get(), desc );

        m_renderTargets[i] = std::shared_ptr< vaTexture >( vaTextureDX12::CreateWrap( *this, renderTarget.Get(), vaResourceFormat::Automatic, VAFormatFromDXGI(c_DefaultBackbufferFormatRTV) ) );
    }

    HRESULT hr;

    DXGI_SWAP_CHAIN_DESC scdesc;
    V( m_swapChain->GetDesc( &scdesc ) );

    m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex( );
}

void vaRenderDeviceDX12::ReleaseSwapChainRelatedObjects( )
{
    assert( IsRenderThread() );
    assert( !m_frameStarted );
    
    for( int i = 0; i < _countof( m_renderTargets ); i++ )
    {
        m_renderTargets[i] = nullptr;
    }

    if( m_mainDeviceContext != nullptr )
        m_mainDeviceContext->SetRenderTarget( nullptr, nullptr, false );

    WaitForGPU( true );
}

void vaRenderDeviceDX12::SetWindowed( )
{
    m_fullscreenState = vaFullscreenState::Windowed;

#ifdef ALLOW_DXGI_FULLSCREEN
    if( m_swapChain != nullptr )
    {
        HRESULT hr = m_swapChain->SetFullscreenState( false, NULL );
        if( FAILED( hr ) )
        {
            VA_WARN( L"Error in a call to m_swapChain->SetFullscreenState( false ) [%x]", hr );
        }
    }
#endif
}

bool vaRenderDeviceDX12::ResizeSwapChain( int width, int height, vaFullscreenState fullscreenState )
{
    assert( IsRenderThread() );
    assert( !m_frameStarted );
    assert( fullscreenState != vaFullscreenState::Unknown );

    if( m_swapChain == nullptr ) return false;

    if( (int)m_swapChainTextureSize.x == width && (int)m_swapChainTextureSize.y == height && m_fullscreenState == fullscreenState )
        return false;

    m_swapChainTextureSize.x = width;
    m_swapChainTextureSize.y = height;
    m_fullscreenState = fullscreenState;

    HRESULT hr;

    ReleaseSwapChainRelatedObjects( );

#ifdef ALLOW_DXGI_FULLSCREEN
    if( m_fullscreenState == vaFullscreenState::Fullscreen )
    {
        hr = m_swapChain->ResizeBuffers( c_BackbufferCount, width, height, DXGI_FORMAT_UNKNOWN, c_DefaultSwapChainFlags );
        if( FAILED( hr ) )
        {
            assert( false );
            //VA_ERROR( L"Error trying to m_swapChain->ResizeBuffers" );
            return false;
        }
    }

    hr = m_swapChain->SetFullscreenState( m_fullscreenState == vaFullscreenState::Fullscreen, NULL );
    if( FAILED( hr ) )
    {
        VA_WARN( L"Error in a call to m_swapChain->SetFullscreenState [%x]", hr );
        if( m_fullscreenState != vaFullscreenState::Windowed )
        {
            m_fullscreenState = vaFullscreenState::FullscreenBorderless;
            VA_WARN( L"Falling back to borderless fullscreen", hr );
        }
    }
#endif

    hr = m_swapChain->ResizeBuffers( c_BackbufferCount, width, height, DXGI_FORMAT_UNKNOWN, c_DefaultSwapChainFlags );
    if( FAILED( hr ) )
    {
        assert( false );
        //VA_ERROR( L"Error trying to m_swapChain->ResizeBuffers" );
        return false;
    }

    CreateSwapChainRelatedObjects( );

    return true;
}

void vaRenderDeviceDX12::BeginFrame( float deltaTime )
{
    assert( IsRenderThread() );

    PSOCachesClearUnusedTick( );

    vaRenderDevice::BeginFrame( deltaTime );

    AsDX12( *m_mainDeviceContext ).BeginFrame( );

    // timers/profiling
    if( vaProfiler::GetInstancePtr( ) != nullptr )
    {
        assert( m_frameProfilingNode == nullptr );
        m_frameProfilingNode = vaProfiler::GetInstance( ).StartScope( "WholeFrame", false, m_mainDeviceContext.get() );
    }

    // execute begin frame callbacks - mostly initialization stuff that requires a command list (main context)
    ExecuteBeginFrameCallbacks();

    //AsDX12( *m_mainDeviceContext ).GetCommandList( )->BeginQuery( m_statsHeap.Get( ), D3D12_QUERY_TYPE_PIPELINE_STATISTICS, 0 );
}

void vaRenderDeviceDX12::EndAndPresentFrame( int vsyncInterval )
{
    assert( IsRenderThread() );

    // timers/profiling
    if( vaProfiler::GetInstancePtr( ) != nullptr )
    {
        assert( m_frameProfilingNode != nullptr );
        vaProfiler::GetInstance( ).StopScope( m_frameProfilingNode, m_mainDeviceContext.get( ) );
        m_frameProfilingNode = nullptr;
    }

    VA_SCOPE_CPU_TIMER( EndAndPresentFrame );

    // clear any render targets
    m_mainDeviceContext->SetRenderTarget( nullptr, nullptr, 0 );

    // timers/profiling
    if( vaProfiler::GetInstancePtr( ) != nullptr )
    {
        vaGPUTimerManagerDX12::GetInstance().PIXBeginEvent( L"ResolveGPUTimers" );

        // trigger resolve on the command list (GPU->CPU copy for timer query data) and also collect and use already copied older data
        vaGPUTimerManagerDX12::GetInstance().ResolveQueryDataAndUpdateTimers();

        vaGPUTimerManagerDX12::GetInstance().PIXEndEvent( ); // L"ResolveGPUTimers"
    }

    vaGPUTimerManagerDX12::GetInstance().PIXBeginEvent( L"TransitionToPresentAndFlush" );

    // if( vaProfiler::GetInstancePtr( ) != nullptr )
    // {
    //     assert( m_presentProfilingNode == nullptr );
    //     m_presentProfilingNode = vaProfiler::GetInstance( ).StartScope( "Present", false, m_mainDeviceContext.get() );
    // }

    // Indicate that the back buffer will now be used to present.
    if( m_swapChain != nullptr && GetCurrentBackbuffer() != nullptr )
        AsDX12(*GetCurrentBackbuffer()).TransitionResource( AsDX12(*m_mainDeviceContext), D3D12_RESOURCE_STATE_PRESENT );

    // This will execute and close the command list!
    AsDX12( *m_mainDeviceContext ).EndFrame( );

    vaGPUTimerManagerDX12::GetInstance().PIXEndEvent(); // L"TransitionToPresentAndFlush"

    if( m_swapChain != nullptr )
    {
        UINT flags = 0; //(vsyncInterval==0)?(DXGI_PRESENT_ALLOW_TEARING | DXGI_PRESENT_DO_NOT_WAIT):(0);

        vaGPUTimerManagerDX12::GetInstance().PIXBeginEvent( L"Present!" );
#ifdef ALLOW_DXGI_FULLSCREEN
        BOOL isFullscreen = false;
        m_swapChain->GetFullscreenState( &isFullscreen, nullptr );
        if( m_fullscreenState == vaFullscreenState::Fullscreen && !isFullscreen )
        {
            m_fullscreenState = vaFullscreenState::Windowed;
            VA_WARN( "Fullscreen state changed by external factors (alt+tab or an unexpected issue), readjusting..." );
        }
        else
#endif
        { 
            HRESULT hr = m_swapChain->Present( (UINT)vsyncInterval, flags );
            if( FAILED( hr ) )
            {
                assert( false );
                VA_WARN( "Present failed" );
            }
        }
        vaGPUTimerManagerDX12::GetInstance().PIXEndEvent( ); //L"Present!" );
    }

    // queries require command list - I have to do a light restart of the main context command list and then
    // close and execute it
    // if( vaProfiler::GetInstancePtr( ) != nullptr )
    // {
    //     assert( m_presentProfilingNode != nullptr );
    //     vaProfiler::GetInstance( ).StopScope( m_presentProfilingNode, m_mainDeviceContext.get() );
    //     m_presentProfilingNode = nullptr;
    // }


    HRESULT hr;

    {
        vaGPUTimerManagerDX12::GetInstance().PIXBeginEvent( L"Wait on sync" );

        // don't let anyone schedule any more callbacks until we're done with the frame swap; 
        // should be no risk of deadlocking as none of the code here can try to add callbacks
        std::unique_lock<std::mutex> mutexLock(m_GPUFrameFinishedCallbacksMutex);

        // Schedule a signal command in the queue for this frame (the one being presented).
        const uint64 currentFenceValue = m_fenceValues[ m_currentBackBufferIndex ];
        V( m_commandQueue->Signal(m_fence.Get(), currentFenceValue) );
        
        // Update the frame index to the new one we'll use to render the next frame into.
        m_currentBackBufferIndex = ( m_swapChain != nullptr )?( m_swapChain->GetCurrentBackBufferIndex() ):( 0 );

        // Wait for the oldest frame to be done on the GPU.
        uint64 fenceCompletedValue = m_fence->GetCompletedValue();
        if( fenceCompletedValue < m_fenceValues[m_currentBackBufferIndex] )
        {
            V( m_fence->SetEventOnCompletion( m_fenceValues[m_currentBackBufferIndex], m_fenceEvent ) );
            WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
        }

        // Advance the future fence value.
        m_fenceValues[m_currentBackBufferIndex] = currentFenceValue + 1;

        vaGPUTimerManagerDX12::GetInstance().PIXEndEvent(); // L"Wait on sync"
    }

    vaGPUTimerManagerDX12::GetInstance().PIXBeginEvent( L"vaRenderDevice::EndAndPresentFrame" );
    vaRenderDevice::EndAndPresentFrame( vsyncInterval );
    vaGPUTimerManagerDX12::GetInstance().PIXEndEvent( ); // L"vaRenderDevice::EndAndPresentFrame"

    vaGPUTimerManagerDX12::GetInstance().PIXBeginEvent( L"ExecuteEndFrameCallbacks" );
    // now we know GPU is done with the old frame with this index that we're just about to reuse, so we can 
    // dispose of any resources or similar safely.
    ExecuteEndFrameCallbacks();
    vaGPUTimerManagerDX12::GetInstance().PIXEndEvent( ); // L"ExecuteEndFrameCallbacks"

    // Reset the GPU descriptor heaps for the new frame
    vaGPUTimerManagerDX12::GetInstance().PIXBeginEvent( L"ResetDescriptors" );
    //for( int i = 0; i < m_defaultTransientDescriptorHeaps[m_currentBackBufferIndex].size(); i++ )
        m_defaultTransientDescriptorHeaps[m_currentBackBufferIndex]/*[i]*/.Reset( );
    vaGPUTimerManagerDX12::GetInstance().PIXEndEvent( ); // L"ResetDescriptors"
}

void vaRenderDeviceDX12::ExecuteAtBeginFrame( const std::function<void( vaRenderDeviceDX12 & device )> & callback )
{ 
    assert( !m_beginFrameCallbacksDisable );
    if( m_beginFrameCallbacksDisable )
        return;

    std::unique_lock<std::mutex> mutexLock(m_beginFrameCallbacksMutex);
    m_beginFrameCallbacks.push_back( callback ); 
}

bool vaRenderDeviceDX12::ExecuteBeginFrameCallbacks( )
{
    assert( IsRenderThread() );
    assert( !m_GPUFrameFinishedCallbacksExecuting );    // recursive call to this method is illegal
    assert( !m_beginFrameCallbacksExecuting );          // recursive call to this method is illegal
    m_beginFrameCallbacksExecuting = true;

    bool hadAnyCallbacks = false;

    std::unique_lock<std::mutex> mutexLock(m_beginFrameCallbacksMutex);
    auto & callbacks = m_beginFrameCallbacks;
    while( callbacks.size() > 0 ) 
    {
        auto callback = callbacks.back();
        callbacks.pop_back();
        
        // callbacks are allowed to add more callbacks to the list so unlock the mutex!
        mutexLock.unlock();
        callback( *this );
        mutexLock.lock();
        hadAnyCallbacks = true;
    }

    if( hadAnyCallbacks )
        AsDX12( *m_mainDeviceContext ).ExecuteCommandList( );

    m_beginFrameCallbacksExecuting = false;
    return hadAnyCallbacks;
}

void vaRenderDeviceDX12::ExecuteAfterCurrentGPUFrameDone( const std::function<void( vaRenderDeviceDX12 & device )> & callback )
{
    assert( m_commandQueue != nullptr );

    std::unique_lock<std::mutex> mutexLock(m_GPUFrameFinishedCallbacksMutex);
    m_GPUFrameFinishedCallbacks[m_currentBackBufferIndex].push_back( callback );
}

bool vaRenderDeviceDX12::ExecuteEndFrameCallbacks( )
{
    assert( IsRenderThread() );
    assert( !m_beginFrameCallbacksExecuting );          // recursive call to this method is illegal
    assert( !m_GPUFrameFinishedCallbacksExecuting );    // recursive call to this method is illegal
    m_GPUFrameFinishedCallbacksExecuting = true;

    bool hadAnyCallbacks = false;

    std::unique_lock<std::mutex> mutexLock(m_GPUFrameFinishedCallbacksMutex);
    auto & callbacks = m_GPUFrameFinishedCallbacks[m_currentBackBufferIndex];
    while( callbacks.size() > 0 ) 
    {
        auto callback = callbacks.back();
        callbacks.pop_back();
        
        // callbacks are allowed to add more callbacks to the list so unlock the mutex!
        mutexLock.unlock();
        callback( *this );
        mutexLock.lock();
        hadAnyCallbacks = true;
    }
    m_GPUFrameFinishedCallbacksExecuting = false;
    return hadAnyCallbacks;
}

// Wait for pending GPU work to complete.
void vaRenderDeviceDX12::WaitForGPU( bool executeAfterFrameDoneCallbacks )
{
    assert( IsRenderThread() );
    HRESULT hr;

    // Schedule a Signal command in the queue.
    const uint64 currentFenceValue = m_fenceValues[m_currentBackBufferIndex];
    V( m_commandQueue->Signal(m_fence.Get(), currentFenceValue) );

    // Wait until the fence has been processed.
    UINT64 completedFenceValue = m_fence->GetCompletedValue( );
    if( completedFenceValue < m_fenceValues[m_currentBackBufferIndex] )
    {
        V( m_fence->SetEventOnCompletion( m_fenceValues[m_currentBackBufferIndex], m_fenceEvent ) );
        WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
    }

    // Reset future frame fence values
    for( int i = 0; i < _countof(m_fenceValues); i++ )
        m_fenceValues[i] = currentFenceValue+1;

    if( executeAfterFrameDoneCallbacks )
        ExecuteEndFrameCallbacks( );
}

void vaRenderDeviceDX12::BindDefaultDescriptorHeaps( ID3D12GraphicsCommandList * commandList )
{
    assert( IsRenderThread() );
    assert( m_frameStarted );

    assert( m_defaultPersistentDescriptorHeaps.size() == (int)D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES );
    ID3D12DescriptorHeap* ppHeaps[(int)D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES+1]; 
    int actualCount = 0;
    for( int i = 0; i < m_defaultPersistentDescriptorHeaps.size(); i++ )
    {
        if( m_defaultPersistentDescriptorHeaps[i].GetHeap() == nullptr )
            continue;
        if( (m_defaultPersistentDescriptorHeaps[i].GetDesc().Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) == 0 )
            continue;
        ppHeaps[actualCount++] = m_defaultPersistentDescriptorHeaps[i].GetHeap().Get();
    }
    ppHeaps[actualCount++] = m_defaultTransientDescriptorHeaps[m_currentBackBufferIndex].GetHeap().Get();
    assert( actualCount <= _countof(ppHeaps) );
    commandList->SetDescriptorHeaps( actualCount, ppHeaps );
}

vaRenderDeviceDX12::PersistentDescriptorHeap::~PersistentDescriptorHeap( )
{ 
    assert( m_device != nullptr );
    assert( m_device->IsRenderThread() );

    assert( m_allocatedCount - (int)m_freed.size() == 0 );
}

void vaRenderDeviceDX12::PersistentDescriptorHeap::Initialize( vaRenderDeviceDX12 & _device, const D3D12_DESCRIPTOR_HEAP_DESC & desc )
{
    assert( _device.IsRenderThread() );
    assert( m_device == nullptr );
    m_device = &_device;

    m_capacity = desc.NumDescriptors;
    m_heapDesc = desc;
    auto device = _device.GetPlatformDevice( ).Get();
 
    HRESULT hr;
    V( device->CreateDescriptorHeap( &desc, IID_PPV_ARGS(&m_heap) ) );

    m_descriptorSize = device->GetDescriptorHandleIncrementSize( desc.Type );
}

bool  vaRenderDeviceDX12::PersistentDescriptorHeap::Allocate( int & outIndex, D3D12_CPU_DESCRIPTOR_HANDLE & outCPUHandle, D3D12_GPU_DESCRIPTOR_HANDLE & outGPUHandle )
{ 
    assert( m_device != nullptr );
    std::unique_lock<std::mutex> mutexLock( *m_mutex );

    if( m_freed.size() == 0 && m_allocatedCount >= m_capacity ) 
    {
        VA_ERROR( "Ran out of PersistentDescriptorHeap space - consider initializing with a bigger heap" );
        assert( false );
        return false; 
    }

    // do we have one already freed? return that one
    if( m_freed.size() > 0 ) 
    { 
        int ret = m_freed.back(); 
        m_freed.pop_back(); 
        outIndex = ret;
    }
    else // or allocate the new one
    {
        outIndex = m_allocatedCount++; 
    }

    outCPUHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE( m_heap->GetCPUDescriptorHandleForHeapStart(), outIndex, m_descriptorSize );

    if( (m_heapDesc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) != 0 )
        outGPUHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE( m_heap->GetGPUDescriptorHandleForHeapStart(), outIndex, m_descriptorSize );
    else
        outGPUHandle = D3D12_GPU_DESCRIPTOR_HANDLE{0};

    return true;
}

void vaRenderDeviceDX12::PersistentDescriptorHeap::Release( int index )    
{
    assert( m_device != nullptr );
    assert( index >= 0 && index < m_allocatedCount );

    std::unique_lock<std::mutex> mutexLock( *m_mutex );

    // no defrag, really dumb way but it works
    m_freed.push_back(index); 
}

vaRenderDeviceDX12::TransientGPUDescriptorHeap::~TransientGPUDescriptorHeap( )
{ 
    assert( m_device == nullptr );
    assert( m_heap == nullptr );
    // assert( m_allocatedCount == 0 );
}

void vaRenderDeviceDX12::TransientGPUDescriptorHeap::Initialize( vaRenderDeviceDX12 & _device, const D3D12_DESCRIPTOR_HEAP_DESC & desc )
{
    assert( _device.IsRenderThread() );
    assert( m_device == nullptr );
    m_device = &_device;

    // designed for GPU-only heaps (can and used to actually work for GPU based too but we switched to TransientGPUDescriptorHeap for them and there's no way to have both at the same time)
    assert( (desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) != 0 );

    m_capacity = desc.NumDescriptors;
    m_heapDesc = desc;
    auto device = _device.GetPlatformDevice( ).Get();
 
    HRESULT hr;
    V( device->CreateDescriptorHeap( &desc, IID_PPV_ARGS(&m_heap) ) );

    m_descriptorSize = device->GetDescriptorHandleIncrementSize( desc.Type );
}

void vaRenderDeviceDX12::TransientGPUDescriptorHeap::Deinitialize( )
{
    // assert( m_device->IsRenderThread() );
    m_device            = nullptr;
    m_capacity          = 0;
    m_allocatedCount    = 0;
    m_heapDesc          = { };
    m_heap.Reset();
    m_descriptorSize    = 0;
}

bool  vaRenderDeviceDX12::TransientGPUDescriptorHeap::Allocate( int numberOfDescriptors, int & outIndex )
{ 
    assert( m_device != nullptr );
    std::unique_lock<std::mutex> mutexLock( *m_mutex );

    if( (m_allocatedCount+numberOfDescriptors) > m_capacity ) 
    {
        VA_ERROR( "Ran out of TransientGPUDescriptorHeap space - consider initializing with a bigger heap" );
        assert( false );
        return false; 
    }

    // do the allocation
    outIndex = m_allocatedCount;
    m_allocatedCount += numberOfDescriptors;
    return true;
}

void vaRenderDeviceDX12::TransientGPUDescriptorHeap::Reset( )
{
    if( m_device == nullptr )
        return;
    assert( m_device->IsRenderThread() );
    assert( !m_device->IsFrameStarted() );

    std::unique_lock<std::mutex> mutexLock( *m_mutex );
    m_allocatedCount = 0;
}

vaRenderDeviceDX12::PersistentDescriptorHeap * vaRenderDeviceDX12::GetPersistentDescriptorHeap( D3D12_DESCRIPTOR_HEAP_TYPE type )
{
    assert( m_device != nullptr );

    if( type < D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type >= D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES || type >= m_defaultPersistentDescriptorHeaps.size() )
        { assert( false ); return nullptr; }

    assert( m_defaultDescriptorHeapsInitialized );

    return &m_defaultPersistentDescriptorHeaps[(int)type];
}

vaRenderDeviceDX12::TransientGPUDescriptorHeap * vaRenderDeviceDX12::GetTransientDescriptorHeap( D3D12_DESCRIPTOR_HEAP_TYPE type )
{
    assert( m_device != nullptr );
    assert( IsRenderThread() );
    assert( IsFrameStarted() );

    //if( type < D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type >= D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES || type >= m_defaultTransientDescriptorHeaps[m_currentBackBufferIndex].size() )
    if( type != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV )
        { assert( false ); return nullptr; }

    assert( m_defaultDescriptorHeapsInitialized );

    if( m_defaultTransientDescriptorHeaps[m_currentBackBufferIndex]/*[(int)type]*/.GetHeap( ) == nullptr )
    {
        assert( false );
        return nullptr;
    }

    return &m_defaultTransientDescriptorHeaps[m_currentBackBufferIndex]/*[(int)type]*/;
}

void vaRenderDeviceDX12::ImGuiCreate( )
{
    assert( IsRenderThread() );
    vaRenderDevice::ImGuiCreate();

#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGui_ImplWin32_Init( GetHWND( ) );
    //D3D12_CPU_DESCRIPTOR_HANDLE descCPUHandle; 
    D3D12_GPU_DESCRIPTOR_HANDLE dummyGPUHandle;
    AllocatePersistentResourceView( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_imguiSRVDesc, m_imguiSRVDescCPUHandle, dummyGPUHandle );
    ImGui_ImplDX12_Init( m_device.Get(), c_BackbufferCount, c_DefaultBackbufferFormatRTV, m_imguiSRVDescCPUHandle, {0} );
    ImGui_ImplDX12_CreateDeviceObjects();
#endif

    assert( !m_imguiFrameStarted );
    ImGuiNewFrame( );
}
void vaRenderDeviceDX12::ImGuiDestroy( )
{
    assert( IsRenderThread() );
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGui_ImplDX12_InvalidateDeviceObjects();
    ImGui_ImplDX12_Shutdown( );
    ImGui_ImplWin32_Shutdown( );
    ReleasePersistentResourceView ( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_imguiSRVDesc );
#endif
    vaRenderDevice::ImGuiDestroy();
}

void vaRenderDeviceDX12::ImGuiNewFrame( )
{
    assert( IsRenderThread() );
    assert( !m_imguiFrameStarted ); // forgot to call ImGuiEndFrameAndRender? 
    //assert( ImGuiIsVisible( ) );
    m_imguiFrameStarted = true;

#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGui::GetIO( ).DeltaTime = m_lastDeltaTime;

//            // hacky mouse handling, but good for now
//            bool dontTouchMyCursor = vaInputMouseBase::GetCurrent( )->IsCaptured( );

    // Feed inputs to dear imgui, start new frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
#endif
}

void vaRenderDeviceDX12::ImGuiEndFrameAndRender( vaRenderDeviceContext & renderContext )
{
    assert( &renderContext == GetMainContext() ); renderContext; // at the moment only main context supported

    assert( IsRenderThread() );
    assert( m_imguiFrameStarted ); // forgot to call ImGuiNewFrame? you must not do that!
    //assert( ImGuiIsVisible( ) );

#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGui::Render();

    if( vaUIManager::GetInstance().IsVisible() )
    {
        {
            VA_SCOPE_CPUGPU_TIMER( ImGuiRender, renderContext );

            // unfortunately this is a limitation with the current DirectX12 implementation
            assert( renderContext.GetRenderTarget() == GetCurrentBackbuffer() );

            renderContext.SetRenderTarget( GetCurrentBackbuffer(), nullptr, true );
            AsDX12(renderContext).UpdateViewport( );
            AsDX12(renderContext).UpdateRenderTargetsDepthStencilUAVs( );

            // need to allocate GPU descriptor heap handle for the font resource
            TransientGPUDescriptorHeap * allocator = GetTransientDescriptorHeap( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
            assert( allocator != nullptr );
            int descIndex;
            allocator->Allocate( 1, descIndex );
            m_device->CopyDescriptorsSimple( 1, allocator->ComputeCPUHandle( descIndex ), m_imguiSRVDescCPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
            ImGui_ImplDX12_UpdateGPUDescHandle( allocator->ComputeGPUHandle( descIndex ) );
            ImGui_ImplDX12_RenderDrawData( ImGui::GetDrawData(), AsDX12( *m_mainDeviceContext ).GetCommandList().Get() );
            
            // AsDX12( *m_mainDeviceContext ).Flush( );
            AsDX12( *m_mainDeviceContext ).BindDefaultStates( );
        }

        GetTextureTools().UIDrawImages( *m_mainDeviceContext );
    }
#endif

    m_imguiFrameStarted = false;
}

template< typename PSOType >
static void CleanPSOCache( vaRenderDeviceDX12 & device, vaMemoryBuffer & startKey, std::map<vaMemoryBuffer, shared_ptr<PSOType>, MemoryBufferComparer> & PSOCache )
{
    const int64 currentFrameIndex     = device.GetCurrentFrameIndex();
    const int itemsToVisit          = 5;
    const int unusedAgeThreshold    = 100;

    if( PSOCache.size() == 0 )
        return;

    // get start iterator
    auto & it = (startKey.GetData() == nullptr)?(PSOCache.end()):(PSOCache.find( startKey ));
    if( it == PSOCache.end() )
        it = PSOCache.begin();

    float remainingStepCount = (float)std::min( itemsToVisit, (int)PSOCache.size() );
    do 
    {
        if( currentFrameIndex - it->second->GetLastUsedFrame( ) > unusedAgeThreshold )
        {
            device.ReleasePipelineState( it->second );
            it = PSOCache.erase( it );
            remainingStepCount += 0.8f;   // if there was something to erase, just continue erasing but not indefinitely so we don't cause a frame spike
        }
        else
            it++;

        remainingStepCount -= 1.0f;
        if( it == PSOCache.end() && PSOCache.size() > 0 )
            it = PSOCache.begin();
    } while ( remainingStepCount > 0.0f && PSOCache.size() > 0 );

    if( it != PSOCache.end() )
        startKey = it->first;
    else
        startKey.Clear();
}

void vaRenderDeviceDX12::PSOCachesClearUnusedTick( )
{
    std::unique_lock<std::mutex> mutexLock(m_PSOCacheMutex);
    CleanPSOCache( *this, m_graphicsPSOCacheCleanupLastKey, m_graphicsPSOCache );
    CleanPSOCache( *this, m_computePSOCacheCleanupLastKey, m_computePSOCache );
}

template< typename PSOType, typename PSODescType, int scratchBufferSize >
static shared_ptr<PSOType> FindOrCreateGraphicsPipelineStateTemplated( vaRenderDeviceDX12 & device, std::mutex & mtx, const PSODescType & psoDesc, std::map<vaMemoryBuffer, shared_ptr<PSOType>, MemoryBufferComparer> & PSOCache, ID3D12RootSignature * rootSignature )
{
    assert( device.IsRenderThread() ); // make sure it's really thread-safe 

    uint8 scratchBuffer[scratchBufferSize];

    vaMemoryStream scratchStream( scratchBuffer, scratchBufferSize );
    psoDesc.FillKey( scratchStream );
    vaMemoryBuffer tempKey( scratchStream.GetBuffer(), scratchStream.GetPosition(), vaMemoryBuffer::InitType::View ); // doesn't take the ownership (no allocations)!
    
    shared_ptr<PSOType> retPSO = nullptr;

    // only need to lock now - building the key (above) was the most expensive part
    std::unique_lock<std::mutex> mutexLock(mtx);

    auto it = PSOCache.find( tempKey );
    if( it != PSOCache.end() )
    {
        retPSO = it->second;
    }
    else
    {
        retPSO = std::make_shared<PSOType>( psoDesc );

        HRESULT hr;
        V( retPSO->CreatePSO( device, rootSignature ) );

        PSOCache.insert( std::make_pair( tempKey, retPSO ) );
    }
    assert( retPSO != nullptr );
    retPSO->SetLastUsedFrame( device.GetCurrentFrameIndex() );
    return retPSO;
}

shared_ptr<vaGraphicsPSODX12> vaRenderDeviceDX12::FindOrCreateGraphicsPipelineState( const vaGraphicsPSODescDX12 & psoDesc )
{
    return FindOrCreateGraphicsPipelineStateTemplated<vaGraphicsPSODX12, vaGraphicsPSODescDX12, 256>( *this, m_PSOCacheMutex, psoDesc, m_graphicsPSOCache, GetDefaultGraphicsRootSignature() );
}

void vaRenderDeviceDX12::ReleasePipelineState( shared_ptr<vaGraphicsPSODX12> & pso )
{
    assert( IsRenderThread() ); // upgrade to thread-safe in the future

    if( pso.unique() )
    {
        // Let the resource be removed when we can guarantee GPU has finished using it
        ExecuteAfterCurrentGPUFrameDone( 
        [pso]( vaRenderDeviceDX12 & device )
            { 
                device;
                //if( pso.unique() )
                    pso->m_safelyReleased = true;   // the only place it's safe to delete it from
                // just let capturedPtr go out of scope...
            } );
    }
    pso = nullptr;
}

shared_ptr<vaComputePSODX12> vaRenderDeviceDX12::FindOrCreateComputePipelineState( const vaComputePSODescDX12 & psoDesc )
{
    return FindOrCreateGraphicsPipelineStateTemplated<vaComputePSODX12, vaComputePSODescDX12, 256>( *this, m_PSOCacheMutex, psoDesc, m_computePSOCache, GetDefaultComputeRootSignature() );
}

void vaRenderDeviceDX12::ReleasePipelineState( shared_ptr<vaComputePSODX12> & pso )
{
    assert( IsRenderThread() ); // upgrade to thread-safe in the future

    if( pso.unique() )
    {
        // Let the resource be removed when we can guarantee GPU has finished using it
        ExecuteAfterCurrentGPUFrameDone( 
        [pso]( vaRenderDeviceDX12 & device )
            { 
                device;
                //if( pso.unique() )
                    pso->m_safelyReleased = true;   // the only place it's safe to delete it from
                // just let capturedPtr go out of scope...
            } );
    }
    pso = nullptr;
}

void vaRenderDeviceDX12::StaticEnumerateAdapters( vector<pair<string, string>> & outAdapters )
{
    outAdapters;
    EnsureDirectXAPILoaded( );

    HRESULT hr = S_OK;

    ComPtr<IDXGIFactory5> dxgiFactory;
    UINT dxgiFactoryFlags = 0;
    hr = s_DynamicCreateDXGIFactory2( dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory) );
    {
        if( FAILED( hr ) )
        {
            VA_ERROR( L"Unable to create DXGIFactory; Your Windows 10 probably needs updating" );
            return;
        }
    }

    ComPtr<IDXGIAdapter4> adapter;

    UINT i = 0; 

    IDXGIAdapter1* adapterTemp;
    while( dxgiFactory->EnumAdapters1(i, &adapterTemp) != DXGI_ERROR_NOT_FOUND )
    { 
	    i++; 
        hr = ComPtr<IDXGIAdapter1>(adapterTemp).As(&adapter);
        SAFE_RELEASE( adapterTemp );

        if( SUCCEEDED( hr ) )
        {
            DXGI_ADAPTER_DESC3 desc;
            adapter->GetDesc3( &desc );

            // only hardware devices enumerated
            if( (desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE) != 0 )
                continue;

            // check feature level
            if( FAILED( D3D12CreateDevice(adapter.Get(), c_RequiredFeatureLevel, _uuidof(ID3D12Device), nullptr) ) )
                continue;

            outAdapters.push_back( make_pair( StaticGetAPIName(), FormatAdapterID(desc) ) );
        }
    }

            outAdapters.push_back( make_pair( StaticGetAPIName(), "WARP" ) );

//    hr = dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&adapter));
//    if( !FAILED( hr ) )
//    {
//        if( SUCCEEDED( hr ) )
//        {
//            DXGI_ADAPTER_DESC3 desc;
//            adapter->GetDesc3( &desc );
//
//            // check for feature level should go here
//
//            outAdapters.push_back( make_pair( StaticGetAPIName(), FormatAdapterID(desc) ) );
//        }
//    }

}
