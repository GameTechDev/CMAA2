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

#include "vaRenderDeviceDX11.h"

#include "Rendering/DirectX/vaRenderDeviceContextDX11.h"

#include "Rendering/DirectX/vaTextureDX11.h"

#include "Rendering/DirectX/vaDirectXTools.h"
#include "Rendering/DirectX/vaShaderDX11.h"
//#include "Rendering/DirectX/vaDirectXFont.h"

#include "Rendering/DirectX/vaGPUTimerDX11.h"

#include "Rendering/DirectX/vaRenderBuffersDX11.h"

#include "Core/vaUI.h"
#include "IntegratedExternals/vaImguiIntegration.h"

#include "Core/Misc/vaProfiler.h"

#include "Core/vaInput.h"

#ifdef VA_IMGUI_INTEGRATION_ENABLED
#include "IntegratedExternals/imgui/examples/imgui_impl_dx11.h"
#include "IntegratedExternals/imgui/examples/imgui_impl_win32.h"
#endif

using namespace VertexAsylum;

#define ALLOW_DXGI_FULLSCREEN

namespace 
{
    const DXGI_FORMAT                           c_DefaultBackbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    const uint32                                c_DefaultSwapChainFlags = 0; //DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    D3D_FEATURE_LEVEL                           c_RequestedFeatureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };

    typedef HRESULT( WINAPI * LPCREATEDXGIFACTORY )( REFIID, void ** );
    typedef HRESULT( WINAPI * LPD3D11CREATEDEVICE )( IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT32, D3D_FEATURE_LEVEL*, UINT, UINT32, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext** );

    static HMODULE                              s_hModDXGI = NULL;
    static LPCREATEDXGIFACTORY                  s_DynamicCreateDXGIFactory = NULL;
    static HMODULE                              s_hModD3D11 = NULL;
    static LPD3D11CREATEDEVICE                  s_DynamicD3D11CreateDevice = NULL;
}

void RegisterDeviceContextDX11( );
void RegisterSkyDX11( );
void RegisterSkyboxDX11( );
void RegisterSimpleParticleSystemDX11( );
void RegisterRenderGlobalsDX11( );
void RegisterRenderMeshDX11( );
void RegisterPrimitiveShapeRendererDX11( );
void RegisterRenderMaterialDX11( );
void RegisterGBufferDX11( );
void RegisterPostProcessDX11( );
void RegisterLightingDX11( );
void RegisterPostProcessTonemapDX11( );
void RegisterPostProcessBlurDX11( );
void RegisterTextureToolsDX11( );

void RegisterShaderDX11( );

void vaRenderDeviceDX11::RegisterModules( )
{
    RegisterShaderDX11( );
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaTexture, vaTextureDX11 );
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaConstantBuffer, vaConstantBufferDX11 );
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaIndexBuffer, vaIndexBufferDX11 );
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaVertexBuffer, vaVertexBufferDX11 );
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaStructuredBuffer, vaStructuredBufferDX11 );
    //    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaTriangleMesh_PositionColorNormalTangentTexcoord1Vertex, vaTriangleMeshDX11_PositionColorNormalTangentTexcoord1Vertex );

    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaGPUTimer, vaGPUTimerDX11 );

    RegisterDeviceContextDX11( );
    RegisterSkyDX11( );
    RegisterSkyboxDX11( );
    RegisterSimpleParticleSystemDX11( );
    RegisterRenderGlobalsDX11( );
    RegisterRenderMaterialDX11( );
    RegisterRenderMeshDX11( );
    RegisterPrimitiveShapeRendererDX11( );
    RegisterGBufferDX11( );
    RegisterPostProcessDX11( );
    RegisterLightingDX11( );
    RegisterPostProcessTonemapDX11( );
    RegisterPostProcessBlurDX11( );
    RegisterTextureToolsDX11( );
}

vaRenderDeviceDX11::vaRenderDeviceDX11( const string & preferredAdapterNameID, const vector<wstring> & shaderSearchPaths ) : vaRenderDevice( )
{
    static bool modulesRegistered = false;
    if( !modulesRegistered )
    {
        modulesRegistered = true;
        RegisterModules();
    }

    m_preferredAdapterNameID = preferredAdapterNameID;
    m_device = NULL;
    m_deviceImmediateContext = NULL;
    m_DXGIFactory = NULL;
    m_swapChain = NULL;
    m_mainOutput = NULL;
    m_mainRenderTargetView = NULL;

    memset( &m_backbufferTextureDesc, 0, sizeof( m_backbufferTextureDesc ) );

    Initialize( shaderSearchPaths );
    assert( m_shaderManager != nullptr ); // should be created just after device is created but before any contexts are created
    InitializeBase( );
}

vaRenderDeviceDX11::~vaRenderDeviceDX11( void )
{
    assert( !m_frameStarted );

    if( m_fullscreenState != vaFullscreenState::Windowed )
        SetWindowed();

    // did we miss to call these? some logic could be broken but lambdas shouldn't hold any references anyway so it might be safe!
    if( m_beginFrameCallbacks.size( ) != 0 )
    {
        int count = (int)m_beginFrameCallbacks.size( );
        VA_WARN( "vaRenderDeviceDX12::Deinitialize() - m_beginFrameCallbacks still has %d elements; this likely means that some resources were created just before shutdown which is probably safe but inefficient", count );

        // call them so any cleanup can be performed
        if( m_beginFrameCallbacks.size() > 0 )
        {
            for( auto callback : m_beginFrameCallbacks )
                callback( *this );
        }
        m_beginFrameCallbacks.clear();
    }

    e_DeviceAboutToBeDestroyed.Invoke();

    // have to release these first!
    m_mainDeviceContext->SetRenderTarget( NULL, NULL, false );

    ImGuiDestroy( );
    m_mainDeviceContext = nullptr;
    vaGPUTimerDX11::OnDeviceAboutToBeDestroyed( );
    DeinitializeBase( );
    Deinitialize( );
    //    if( m_canvas2D != NULL )
    //        delete m_canvas2D;
    //    m_canvas2D = NULL;
}

static void EnsureDirectXAPILoaded( )
{
    if( s_hModDXGI == NULL )
    {
        s_hModDXGI = LoadLibrary( L"dxgi.dll" );
        if( s_hModDXGI == NULL )
            VA_ERROR( L"Unable to load dxgi.dll; Vista SP2, Win7 or above required" );
    }

    if( s_DynamicCreateDXGIFactory == NULL && s_hModDXGI != NULL )
    {
        s_DynamicCreateDXGIFactory = (LPCREATEDXGIFACTORY)GetProcAddress( s_hModDXGI, "CreateDXGIFactory1" );
        if( s_hModDXGI == NULL )
            VA_ERROR( L"Unable to create CreateDXGIFactory1 proc; Vista SP2, Win7 or above required" );
    }

    if( s_hModD3D11 == NULL )
    {
        s_hModD3D11 = LoadLibrary( L"d3d11.dll" );
        if( s_hModD3D11 == NULL )
            VA_ERROR( L"Unable to load d3d11.dll; please install the latest DirectX." );
    }

    if( s_DynamicD3D11CreateDevice == NULL && s_hModD3D11 != NULL )
    {
        s_DynamicD3D11CreateDevice = (LPD3D11CREATEDEVICE)GetProcAddress( s_hModD3D11, "D3D11CreateDevice" );
        if( s_DynamicD3D11CreateDevice == NULL )
            VA_ERROR( L"D3D11CreateDevice proc not found" );
    }
}

// void vaApplicationWin::RegisterShaderSearchPath( const wchar_t * path, bool pushBack )
// {
//     vaDirectX11ShaderManager::GetInstance( ).RegisterShaderSearchPath( path, pushBack );
// }

void vaDirectXTools_OnDevice11Created( ID3D11Device* device, IDXGISwapChain* swapChain );
void vaDirectXTools_OnDevice11Destroyed( );

string FormatAdapterID( const DXGI_ADAPTER_DESC1 & desc )
{
    string strID = vaStringTools::SimpleNarrow(wstring(desc.Description)) + vaStringTools::Format( " (%#010x)", desc.SubSysId );
    //std::replace( strID.begin( ), strID.end( ), ' ', '_' );
    return strID;
}

bool vaRenderDeviceDX11::Initialize( const vector<wstring> & shaderSearchPaths )
{
    EnsureDirectXAPILoaded( );

    HRESULT hr = S_OK;

    // create DXGI factory
    {
        hr = s_DynamicCreateDXGIFactory( __uuidof( IDXGIFactory1 ), (LPVOID*)&m_DXGIFactory );
        if( FAILED( hr ) )
            VA_ERROR( L"Unable to create DXGIFactory1; Vista SP2, Win7 or above required" );
    }

    ComPtr<IDXGIAdapter1> adapter;

    bool useWarpDevice = m_preferredAdapterNameID == "WARP";
    D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE_UNKNOWN;

    // create IDXGIAdapter1 based on m_preferredAdapterNameID
    {
        if (useWarpDevice)
        {
            // for DX11 one creates a WARP device with a null adapter (see https://docs.microsoft.com/en-us/windows/desktop/direct3d11/overviews-direct3d-11-devices-create-warp)
            driverType = D3D_DRIVER_TYPE_WARP;
        }
        else
        {
            int i = 0;
            IDXGIAdapter1* adapterTemp;
            ComPtr<IDXGIAdapter1> adapterCandidate;
            while( m_DXGIFactory->EnumAdapters1(i, &adapterTemp) != DXGI_ERROR_NOT_FOUND )
            { 
	            i++; 
                hr = ComPtr<IDXGIAdapter1>(adapterTemp).As(&adapterCandidate);
                SAFE_RELEASE( adapterTemp );

                if( SUCCEEDED( hr ) )
                {
                    DXGI_ADAPTER_DESC1 desc;
                    adapterCandidate->GetDesc1( &desc );

                    // only hardware devices enumerated
                    if( (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0 )
                        continue;

                    // check feature level
                    if( FAILED( s_DynamicD3D11CreateDevice( adapterCandidate.Get( ), D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, c_RequestedFeatureLevels, _countof( c_RequestedFeatureLevels ), D3D11_SDK_VERSION, nullptr, nullptr, nullptr ) ) )
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
    }

    // create ID3D11Device and ID3D11DeviceContext
    {
        UINT32 flags = 0;

#ifdef _DEBUG 
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        D3D_FEATURE_LEVEL createdFeatureLevel;

        ID3D11Device *         device = nullptr;
        ID3D11DeviceContext *  deviceImmediateContext = nullptr;

        hr = s_DynamicD3D11CreateDevice( adapter.Get( ), driverType, NULL, flags, c_RequestedFeatureLevels, _countof( c_RequestedFeatureLevels ), D3D11_SDK_VERSION, &device, &createdFeatureLevel, &deviceImmediateContext );

        if( ( ( flags & D3D11_CREATE_DEVICE_DEBUG ) != 0 ) && FAILED( hr ) )
        {
            VA_WARN( L"Error trying to create D3D11 device, might be because of the debug flag and missing Windows 10 SDK, retrying..." );
            flags &= ~D3D11_CREATE_DEVICE_DEBUG;
            hr = s_DynamicD3D11CreateDevice( adapter.Get( ), driverType, NULL, flags, c_RequestedFeatureLevels, _countof( c_RequestedFeatureLevels ), D3D11_SDK_VERSION, &device, &createdFeatureLevel, &deviceImmediateContext );
        }

        if( FAILED( hr ) )
        {
            VA_ERROR( L"Error trying to create D3D11 device" );
            return false;
        }

        m_device = device;
        m_deviceImmediateContext = deviceImmediateContext;

        if( adapter == nullptr && device != nullptr )
        {
            // case of WARP - maybe somehow get adapter? not yet implemented
        }

        if( ( m_device == nullptr ) || ( m_deviceImmediateContext == nullptr ) )
        {
            VA_ERROR( L"Unable to create DirectX11 device." );
            return false;
        }
    }

    // enumerate outputs
    {
        int outputCount;
        for( outputCount = 0; adapter != nullptr; ++outputCount )
        {
            IDXGIOutput * pOutput;
            if( FAILED( adapter->EnumOutputs( outputCount, &pOutput ) ) )
                break;
            SAFE_RELEASE( pOutput );
        }

        IDXGIOutput** ppOutputArray = new IDXGIOutput*[outputCount];
        VA_ASSERT( ppOutputArray != NULL, L"Out of memory?" );

        for( int output = 0; output < outputCount; ++output )
        {
            adapter->EnumOutputs( output, &ppOutputArray[output] );
        }

        // select first
        m_mainOutput = nullptr;
        if( outputCount > 0 )
        {   
            m_mainOutput = ppOutputArray[0];
            m_mainOutput->AddRef( );
        }

        // release the rest
        for( int output = 0; output < outputCount; ++output )
            ppOutputArray[output]->Release( );

        delete[] ppOutputArray;
    }

    if( adapter != nullptr )
    {
        DXGI_ADAPTER_DESC1 adapterDesc1;
        adapter->GetDesc1( &adapterDesc1 );

        string name = vaStringTools::SimpleNarrow( wstring( adapterDesc1.Description ) );
        // std::replace( name.begin( ), name.end( ), ' ', '_' );

        m_adapterNameShort = name;
        m_adapterNameID = FormatAdapterID(adapterDesc1); //vaStringTools::Format( L"%s-%08X_%08X", name.c_str( ), adapterDesc1.DeviceId, adapterDesc1.Revision );
        m_adapterVendorID = adapterDesc1.VendorId;
    }
    else
    {
        m_adapterNameShort  = "WARP";
        m_adapterNameID     = "WARP";
        m_adapterVendorID   = 0;
    }

    m_shaderManager = shared_ptr<vaShaderManager>( new vaDirectX11ShaderManager( *this ) );
    for( auto s : shaderSearchPaths ) m_shaderManager->RegisterShaderSearchPath( s );

    // main context
    {
        m_mainDeviceContext = std::shared_ptr< vaRenderDeviceContext >( vaRenderDeviceContextDX11::Create( *this, m_deviceImmediateContext ) );
    }

    // vaDirectXFont::InitializeFontGlobals( vaRenderingModuleParams( *this ) );

    // BeginFrame( 0.0f );
    // EndAndPresentFrame( );

    e_DeviceFullyInitialized.Invoke( *this );

    return true;
}

void vaRenderDeviceDX11::Deinitialize( )
{
    if( m_device != NULL )
    {
        ReleaseSwapChainRelatedObjects( );

        //vaDirectXCore::GetInstance( ).PostDeviceDestroyed( );
        vaDirectXTools_OnDevice11Destroyed( );

        m_deviceImmediateContext->ClearState( );

        SAFE_RELEASE( m_mainOutput );
        SAFE_RELEASE( m_deviceImmediateContext );
        SAFE_RELEASE( m_swapChain );
        m_swapChainTextureSize = vaVector2i( 0, 0 );
        SAFE_RELEASE( m_DXGIFactory );

        // more on http://seanmiddleditch.com/direct3d-11-debug-api-tricks/
#if _DEBUG
        ID3D11Debug* debugDevice = nullptr;
        HRESULT hr = m_device->QueryInterface( __uuidof( ID3D11Debug ), reinterpret_cast<void**>( &debugDevice ) );
        assert( SUCCEEDED( hr ) );
        SAFE_RELEASE( m_device );
        debugDevice->ReportLiveDeviceObjects( D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL );
        SAFE_RELEASE( debugDevice );
#else
        SAFE_RELEASE( m_device );
#endif

        m_hwnd = 0;
    }
}

void vaRenderDeviceDX11::CreateSwapChain( int width, int height, HWND hwnd, vaFullscreenState fullscreenState )
{
    m_swapChainTextureSize.x = width;
    m_swapChainTextureSize.y = height;

    DXGI_SWAP_CHAIN_DESC desc;
    memset( &desc, 0, sizeof( desc ) );

    desc.BufferDesc.Format = c_DefaultBackbufferFormat;
    desc.BufferDesc.Width = width;
    desc.BufferDesc.Height = height;
    desc.BufferDesc.RefreshRate.Numerator = 1;
    desc.BufferDesc.RefreshRate.Denominator = 60;
    desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;

    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;

    desc.BufferCount = c_BackbufferCount;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_BACK_BUFFER; // | DXGI_USAGE_SHADER_INPUT; //DXGI_USAGE_UNORDERED_ACCESS
    desc.OutputWindow = hwnd;
    desc.Flags = c_DefaultSwapChainFlags;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD; // DXGI_SWAP_EFFECT_FLIP_DISCARD; 

    m_hwnd = hwnd;

    //bool windowed = true;
    desc.Windowed = true; //!fullscreen;
    //m_fullscreen = fullscreen;
    //m_fullscreenBorderless = false;

//    if( !desc.Windowed )
//    {
//        DXGI_MODE_DESC closestMatch;
//        memset( &closestMatch, 0, sizeof( closestMatch ) );
//
//        HRESULT hr = m_mainOutput->FindClosestMatchingMode( &desc.BufferDesc, &closestMatch, m_device );
//        if( FAILED( hr ) )
//        {
//            VA_ERROR( L"Error trying to find closest matching display mode" );
//        }
//        desc.BufferDesc = closestMatch;
//    }

    //IDXGISwapChain * pSwapChain = NULL;
    HRESULT hr = m_DXGIFactory->CreateSwapChain( m_device, &desc, &m_swapChain ); //&pSwapChain );
    if( FAILED( hr ) )
    {
        VA_ERROR( L"Error trying to create D3D11 swap chain" );
    }

    // stop automatic alt+enter, we'll handle it manually
    {
        hr = m_DXGIFactory->MakeWindowAssociation( hwnd, DXGI_MWA_NO_ALT_ENTER );
    }

    CreateSwapChainRelatedObjects( );

    //if( FAILED( d3dResource->QueryInterface( __uuidof( IDXGISwapChain1 ), (void**)&m_swapChain ) ) )
    //{
    //   VA_ERROR( L"Error trying to cast into IDXGISwapChain1" );
    //}
    //SAFE_RELEASE( pSwapChain );

    //   m_swapChain->GetBuffer

    // Broadcast that the device was created!
    //vaDirectXCore::GetInstance( ).PostDeviceCreated( m_device, m_swapChain );
    vaDirectXTools_OnDevice11Created( m_device, m_swapChain );

    vaGPUTimerDX11::OnDeviceAndContextCreated( m_device );

    vaLog::GetInstance( ).Add( LOG_COLORS_NEUTRAL, L"DirectX 11 device and swap chain created" );

    ImGuiCreate( );

    // now switch to fullscreen if we are in fullscreen
    assert( fullscreenState != vaFullscreenState::Unknown );
    if( fullscreenState == vaFullscreenState::Fullscreen )
        ResizeSwapChain( m_swapChainTextureSize.x, m_swapChainTextureSize.y, fullscreenState );
    else
        m_fullscreenState = fullscreenState;
}

void vaRenderDeviceDX11::CreateSwapChainRelatedObjects( )
{
    HRESULT hr;

    // Create render target view
    {
        // Get the back buffer and desc
        ID3D11Texture2D* pBackBuffer = nullptr;
        hr = m_swapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), (LPVOID*)&pBackBuffer );
        if( FAILED( hr ) )
        {
            VA_ERROR( L"Error trying to get back buffer texture" );
        }
        pBackBuffer->GetDesc( &m_backbufferTextureDesc );

        m_mainRenderTargetView = vaDirectXTools11::CreateRenderTargetView( pBackBuffer );

        m_mainColor = std::shared_ptr< vaTexture >( vaTextureDX11::CreateWrap( *this, pBackBuffer ) );

        SAFE_RELEASE( pBackBuffer );
    }

    /*
    // Create depth-stencil
    {
        CD3D11_TEXTURE2D_DESC desc( DXGI_FORMAT_R32G8X24_TYPELESS, m_backbufferTextureDesc.Width, m_backbufferTextureDesc.Height, 1, 1, D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE );
        V( m_device->CreateTexture2D( &desc, nullptr, &m_mainDepthStencil ) );

        m_mainDepthStencilView = vaDirectXTools11::CreateDepthStencilView( m_mainDepthStencil, DXGI_FORMAT_D32_FLOAT_S8X24_UINT );
        m_mainDepthSRV = vaDirectXTools11::CreateShaderResourceView( m_mainDepthStencil, DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS );

//        m_mainDepth = std::shared_ptr< vaTexture >( vaTextureDX11::CreateWrap( *this, m_mainDepthStencil, vaResourceFormat::R32_FLOAT_X8X24_TYPELESS, vaResourceFormat::Automatic, vaResourceFormat::D32_FLOAT_S8X24_UINT ) );

        //delete vaTexture::CreateView( m_mainDepth, vaResourceBindSupportFlags::ShaderResource, vaResourceFormat::R32_FLOAT_X8X24_TYPELESS );
    }
    */

    DXGI_SWAP_CHAIN_DESC scdesc;
    m_swapChain->GetDesc( &scdesc );

    DXGI_SURFACE_DESC sdesc;
    sdesc.Width = scdesc.BufferDesc.Width;
    sdesc.Height = scdesc.BufferDesc.Height;
    sdesc.SampleDesc = scdesc.SampleDesc;
    sdesc.Format = scdesc.BufferDesc.Format;

    m_mainDeviceContext->SetRenderTarget( m_mainColor, nullptr, true );

    //m_mainViewport.X = 0;
    //m_mainViewport.Y = 0;
    //m_mainViewport.Width = scdesc.BufferDesc.Width;
    //m_mainViewport.Height = scdesc.BufferDesc.Height;

    // vaDirectXCore::GetInstance( ).PostResizedSwapChain( sdesc );

}

void vaRenderDeviceDX11::ReleaseSwapChainRelatedObjects( )
{
    // vaDirectXCore::GetInstance( ).PostReleasingSwapChain( );

    SAFE_RELEASE( m_mainRenderTargetView );
    // SAFE_RELEASE( m_mainDepthStencil );
    // SAFE_RELEASE( m_mainDepthStencilView );
    // SAFE_RELEASE( m_mainDepthSRV );

    if( m_mainDeviceContext != nullptr )
        m_mainDeviceContext->SetRenderTarget( NULL, NULL, false );
    m_mainColor = nullptr;
//    m_mainDepth = nullptr;
}

void vaRenderDeviceDX11::SetWindowed( )
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

#if 0
void vaRenderDeviceDX11::FindClosestFullscreenMode( int & width, int & height )
{
    assert( m_mainOutput != NULL );

    DXGI_MODE_DESC desc;

    desc.Format = c_DefaultBackbufferFormat;
    desc.Width = width;
    desc.Height = height;
    desc.RefreshRate.Numerator = 1;
    desc.RefreshRate.Denominator = 60;
    desc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    desc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;

    DXGI_MODE_DESC closestMatch;
    memset( &closestMatch, 0, sizeof( closestMatch ) );

    HRESULT hr = m_mainOutput->FindClosestMatchingMode( &desc, &closestMatch, m_device );
    if( FAILED( hr ) )
        VA_ERROR( L"Error trying to find closest matching display mode" );

    width = closestMatch.Width;
    height = closestMatch.Height;
}
#endif

bool vaRenderDeviceDX11::ResizeSwapChain( int width, int height, vaFullscreenState fullscreenState )
{
    assert( fullscreenState != vaFullscreenState::Unknown );

    if( m_swapChain == NULL ) return false;

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

void vaRenderDeviceDX11::BeginFrame( float deltaTime )
{
    vaRenderDevice::BeginFrame( deltaTime );

#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGui::GetIO( ).DeltaTime = (float)deltaTime;
#endif

    // m_directXCore.TickInternal( );

    vaGPUTimerDX11::OnFrameStart( );

    assert( m_frameProfilingNode == nullptr );
    m_frameProfilingNode = vaProfiler::GetInstance( ).StartScope( "WholeFrame", false, GetMainContext( ) );

    // execute begin frame callbacks - mostly initialization stuff that requires a command list (main context)
    if( m_beginFrameCallbacks.size() > 0 )
    {
        for( auto callback : m_beginFrameCallbacks )
            callback( *this );
        m_beginFrameCallbacks.clear();
    }
}

void vaRenderDeviceDX11::EndAndPresentFrame( int vsyncInterval )
{
    m_mainDeviceContext->SafeCast<vaRenderDeviceContextDX11*>()->CheckNoStatesSet( );

    assert( m_frameProfilingNode != nullptr );
    vaProfiler::GetInstance( ).StopScope( m_frameProfilingNode, nullptr );
    m_frameProfilingNode = nullptr;

    {
        VA_SCOPE_CPUGPU_TIMER( Present, *m_mainDeviceContext );

        UINT flags = 0;

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
            }
        }
    }

    vaGPUTimerDX11::OnFrameEnd( );

    vaRenderDevice::EndAndPresentFrame( );
}

//
void vaRenderDeviceDX11::NameObject( ID3D11DeviceChild * object, const char * permanentNameString )
{
#ifndef NDEBUG
    // Only works if device is created with the D3D10 or D3D11 debug layer, or when attached to PIX for Windows
    object->SetPrivateData( WKPDID_D3DDebugObjectName, (UINT)strlen( permanentNameString ), permanentNameString );
#else
    object; permanentNameString;
#endif
}
//
void vaRenderDeviceDX11::NameObject( ID3D11Resource * resource, const char * permanentNameString )
{
#ifndef NDEBUG
    // Only works if device is created with the D3D10 or D3D11 debug layer, or when attached to PIX for Windows
    resource->SetPrivateData( WKPDID_D3DDebugObjectName, (UINT)strlen( permanentNameString ), permanentNameString );
#else
    resource; permanentNameString;
#endif
}

vaShaderManager &     vaRenderDeviceDX11::GetShaderManager( )
{
    return *m_shaderManager;
}



void vaRenderDeviceDX11::ImGuiCreate( )
{
    vaRenderDevice::ImGuiCreate();

#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGui_ImplWin32_Init( GetHWND( ) );
    ImGui_ImplDX11_Init(m_device, m_mainDeviceContext->SafeCast<vaRenderDeviceContextDX11*>()->GetDXContext());
    ImGui_ImplDX11_CreateDeviceObjects();
#endif

    assert( !m_imguiFrameStarted );
    ImGuiNewFrame( );
}
void vaRenderDeviceDX11::ImGuiDestroy( )
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGui_ImplDX11_InvalidateDeviceObjects();
    ImGui_ImplDX11_Shutdown( );
    ImGui_ImplWin32_Shutdown( );
#endif
    vaRenderDevice::ImGuiDestroy();
}

void vaRenderDeviceDX11::ImGuiNewFrame( )
{
    assert( !m_imguiFrameStarted ); // forgot to call ImGuiEndFrameAndRender?
    m_imguiFrameStarted = true;

#ifdef VA_IMGUI_INTEGRATION_ENABLED
//            // hacky mouse handling, but good for now
//            bool dontTouchMyCursor = vaInputMouseBase::GetCurrent( )->IsCaptured( );

    // Feed inputs to dear imgui, start new frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
#endif
}

void vaRenderDeviceDX11::ImGuiEndFrameAndRender( vaRenderDeviceContext & renderContext )
{
    assert( &renderContext == GetMainContext() ); renderContext; // at the moment only main context supported

    assert( m_imguiFrameStarted ); // forgot to call ImGuiNewFrame? you must not do that!

    // assert( vaUIManager::GetInstance().IsVisible() );

#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGui::Render();
    IMGUI_API void        ImGui_ImplDX11_RenderDrawData(ImDrawData* draw_data);
    if( vaUIManager::GetInstance().IsVisible() )
    {
        {
            VA_SCOPE_CPUGPU_TIMER( ImGuiRender, renderContext );
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }
        GetTextureTools().UIDrawImages( *m_mainDeviceContext );
    }
#endif

    m_imguiFrameStarted = false;
}

void vaRenderDeviceDX11::StaticEnumerateAdapters( vector<pair<string, string>> & outAdapters )
{
    outAdapters;
    EnsureDirectXAPILoaded( );

    HRESULT hr = S_OK;

    IDXGIFactory1 * dxgiFactory = nullptr;

    // create DXGI factory
    {
        hr = s_DynamicCreateDXGIFactory( __uuidof( IDXGIFactory1 ), (LPVOID*)&dxgiFactory );
        if( FAILED( hr ) )
            VA_ERROR( L"Unable to create DXGIFactory1; Vista SP2, Win7 or above required" );
    }

    ComPtr<IDXGIAdapter1> adapter;

    UINT i = 0; 

    IDXGIAdapter1* adapterTemp;
    while( dxgiFactory->EnumAdapters1(i, &adapterTemp) != DXGI_ERROR_NOT_FOUND )
    { 
	    i++; 
        hr = ComPtr<IDXGIAdapter1>(adapterTemp).As(&adapter);
        SAFE_RELEASE( adapterTemp );

        if( SUCCEEDED( hr ) )
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1( &desc );

            // only hardware devices enumerated
            if( (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0 )
                continue;

            // check feature level
            if( FAILED( s_DynamicD3D11CreateDevice( adapter.Get( ), D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, c_RequestedFeatureLevels, _countof( c_RequestedFeatureLevels ), D3D11_SDK_VERSION, nullptr, nullptr, nullptr ) ) )
                continue;

            outAdapters.push_back( make_pair( StaticGetAPIName(), FormatAdapterID(desc) ) );
        }
    } 
    outAdapters.push_back( make_pair( StaticGetAPIName(), "WARP" ) );

    SAFE_RELEASE( dxgiFactory );
}
