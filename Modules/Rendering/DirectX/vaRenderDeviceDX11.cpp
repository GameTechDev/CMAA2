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

#include "Core/vaCoreIncludes.h"

#include "vaRenderDeviceDX11.h"

#include "Rendering/DirectX/vaRenderDeviceContextDX11.h"

#include "Rendering/DirectX/vaTextureDX11.h"
#include "Rendering/DirectX/vaRenderDeviceContextDX11.h"

#include "Rendering/DirectX/vaDirectXTools.h"
#include "Rendering/DirectX/vaShaderDX11.h"
//#include "Rendering/DirectX/vaDirectXFont.h"

#include "Rendering/DirectX/vaGPUTimerDX11.h"

#include "Rendering/DirectX/vaRenderBuffersDX11.h"

#include "IntegratedExternals/vaImguiIntegration.h"

#include "Core/Misc/vaProfiler.h"

#include "Core/vaInput.h"

using namespace VertexAsylum;

const DXGI_FORMAT                            c_DefaultBackbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
const uint32                                 c_DefaultSwapChainFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

typedef HRESULT( WINAPI * LPCREATEDXGIFACTORY )( REFIID, void ** );
typedef HRESULT( WINAPI * LPD3D11CREATEDEVICE )( IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT32, D3D_FEATURE_LEVEL*, UINT, UINT32, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext** );

static HMODULE                               s_hModDXGI = NULL;
static LPCREATEDXGIFACTORY                   s_DynamicCreateDXGIFactory = NULL;
static HMODULE                               s_hModD3D11 = NULL;
static LPD3D11CREATEDEVICE                   s_DynamicD3D11CreateDevice = NULL;

void RegisterDeviceContextDX11( );
void RegisterSkyDX11( );
void RegisterSkyboxDX11( );
void RegisterASSAODX11( );
void RegisterSimpleParticleSystemDX11( );
void RegisterRenderingGlobals( );
void RegisterRenderMeshDX11( );
void RegisterPrimitiveShapeRendererDX11( );
void RegisterRenderMaterialDX11( );
void RegisterGBufferDX11( );
void RegisterPostProcessDX11( );
void RegisterLightingDX11( );
void RegisterPostProcessTonemapDX11( );
void RegisterPostProcessBlurDX11( );
void RegisterTextureToolsDX11( );
void RegisterZoomToolDX11( );

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
    RegisterASSAODX11( );
    RegisterSimpleParticleSystemDX11( );
    RegisterRenderingGlobals( );
    RegisterRenderMaterialDX11( );
    RegisterRenderMeshDX11( );
    RegisterPrimitiveShapeRendererDX11( );
    RegisterGBufferDX11( );
    RegisterPostProcessDX11( );
    RegisterLightingDX11( );
    RegisterPostProcessTonemapDX11( );
    RegisterPostProcessBlurDX11( );
    RegisterTextureToolsDX11( );
    RegisterZoomToolDX11( );
}

vaRenderDeviceDX11::vaRenderDeviceDX11( const vector<wstring> & shaderSearchPaths ) : vaRenderDevice( )
{
    static bool modulesRegistered = false;
    if( !modulesRegistered )
    {
        modulesRegistered = true;
        RegisterModules();
    }

    m_device = NULL;
    m_deviceImmediateContext = NULL;
    m_DXGIFactory = NULL;
    m_swapChain = NULL;
    m_mainOutput = NULL;
    m_mainRenderTargetView = NULL;
    m_mainDepthStencil = NULL;
    m_mainDepthStencilView = NULL;
    m_mainDepthSRV = NULL;

    m_application = NULL;

    memset( &m_backbufferTextureDesc, 0, sizeof( m_backbufferTextureDesc ) );

    //assert( s_mainDevice == NULL );
    //s_mainDevice = this;

    m_renderFrameCounter = 0;
    //    m_canvas2D = new vaDebugCanvas2DDX11( m_mainViewport );

    Initialize( shaderSearchPaths );
    assert( m_shaderManager != nullptr ); // should be created just after device is created but before any contexts are created
    InitializeBase( );
}

vaRenderDeviceDX11::~vaRenderDeviceDX11( void )
{
//    vaDirectXFont::DeinitializeFontGlobals( );

    // have to release these first!
    m_mainDeviceContext->SetRenderTarget( NULL, NULL, false );

    ImGuiDestroy( );
    m_mainDeviceContext = nullptr;
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

void vaDirectXTools_OnDeviceCreated( ID3D11Device* device, IDXGISwapChain* swapChain );
void vaDirectXTools_OnDeviceDestroyed( );

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

    vaCOMSmartPtr<IDXGIAdapter1> adapter;

    D3D_DRIVER_TYPE ddt = D3D_DRIVER_TYPE_HARDWARE;
    //D3D_DRIVER_TYPE ddt = D3D_DRIVER_TYPE_WARP;
    //D3D_DRIVER_TYPE ddt = D3D_DRIVER_TYPE_REFERENCE;

    // create IDXGIAdapter1
    {
        int  adapterOrdinal = 0;

        if( ddt == D3D_DRIVER_TYPE_HARDWARE )
        {
            IDXGIAdapter1* pRet;
            hr = m_DXGIFactory->EnumAdapters1( adapterOrdinal, &pRet );
            if( FAILED( hr ) )
                VA_ERROR( L"Error trying to EnumAdapters1" );
            adapter = pRet;
            ddt = D3D_DRIVER_TYPE_UNKNOWN;
        }
        else if( ddt == D3D_DRIVER_TYPE_WARP )
        {
            assert( false );
        }
        else if( ddt == D3D_DRIVER_TYPE_REFERENCE )
        {
            assert( false );
        }
    }

    // create ID3D11Device and ID3D11DeviceContext
    {
        UINT32 flags = 0;

#ifdef _DEBUG 
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        D3D_FEATURE_LEVEL requestedFeatureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 }; // D3D_FEATURE_LEVEL_11_1, 
        D3D_FEATURE_LEVEL createdFeatureLevel;

        ID3D11Device *         device = nullptr;
        ID3D11DeviceContext *  deviceImmediateContext = nullptr;

        //s_DynamicD3D11CreateDevice
        hr = s_DynamicD3D11CreateDevice( adapter.Get( ), ddt, NULL, flags, requestedFeatureLevels, _countof( requestedFeatureLevels ), D3D11_SDK_VERSION, &device, &createdFeatureLevel, &deviceImmediateContext );

        if( ( ( flags & D3D11_CREATE_DEVICE_DEBUG ) != 0 ) && FAILED( hr ) )
        {
            VA_WARN( L"Error trying to create D3D11 device, might be because of the debug flag and missing Windows 10 SDK, retrying..." );
            flags &= ~D3D11_CREATE_DEVICE_DEBUG;
            hr = s_DynamicD3D11CreateDevice( adapter.Get( ), ddt, NULL, flags, requestedFeatureLevels, _countof( requestedFeatureLevels ), D3D11_SDK_VERSION, &device, &createdFeatureLevel, &deviceImmediateContext );
        }

        if( FAILED( hr ) )
        {
            VA_ERROR( L"Error trying to create D3D11 device" );
            return false;
        }

        m_device = device;
        m_deviceImmediateContext = deviceImmediateContext;

        /*
        if( !SUCCEEDED( device->QueryInterface( IID_ID3D11DeviceContext2, (void**)&m_device ) ) )
        {
            assert( false ); // sorry, this is the min supported
            m_device = nullptr;
        }

        if( !SUCCEEDED( deviceImmediateContext->QueryInterface( IID_ID3D11DeviceContext2, (void**)&m_deviceImmediateContext ) ) )
        {
            assert( false ); // sorry, this is the min supported
            m_deviceImmediateContext = nullptr;
        }
        */

        if( ( m_device == nullptr ) || ( m_deviceImmediateContext == nullptr ) )
        {
            VA_ERROR( L"Unable to create DirectX11 device." )
                return false;
        }
    }

    // enumerate outputs
    {
        int outputCount;
        for( outputCount = 0;; ++outputCount )
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
        m_mainOutput = ppOutputArray[0];
        m_mainOutput->AddRef( );

        // release the rest
        for( int output = 0; output < outputCount; ++output )
            ppOutputArray[output]->Release( );

        delete[] ppOutputArray;
    }

    DXGI_ADAPTER_DESC1 adapterDesc1;
    adapter->GetDesc1( &adapterDesc1 );

    wstring name = wstring( adapterDesc1.Description );
    std::replace( name.begin( ), name.end( ), L' ', L'_' );

    m_adapterNameShort = name;
    m_adapterNameID = vaStringTools::Format( L"%s-%08X_%08X", name.c_str( ), adapterDesc1.DeviceId, adapterDesc1.Revision );
    m_adapterVendorID = adapterDesc1.VendorId;

    m_shaderManager = shared_ptr<vaShaderManager>( new vaDirectX11ShaderManager( *this ) );
    for( auto s : shaderSearchPaths ) m_shaderManager->RegisterShaderSearchPath( s );

    // main canvas
    {
        m_mainDeviceContext = std::shared_ptr< vaRenderDeviceContext >( vaRenderDeviceContextDX11::Create( *this, m_deviceImmediateContext ) );
    }

    // vaDirectXFont::InitializeFontGlobals( vaRenderingModuleParams( *this ) );

    e_DeviceFullyInitialized.Invoke();

    return true;
}

void vaRenderDeviceDX11::Deinitialize( )
{
    if( m_device != NULL )
    {
        vaGPUTimerDX11::OnDeviceAboutToBeDestroyed( );

        ReleaseSwapChainRelatedObjects( );

        //vaDirectXCore::GetInstance( ).PostDeviceDestroyed( );
        vaDirectXTools_OnDeviceDestroyed( );

        m_deviceImmediateContext->ClearState( );

        SAFE_RELEASE( m_mainOutput );
        SAFE_RELEASE( m_deviceImmediateContext );
        SAFE_RELEASE( m_swapChain );

        // more on http://seanmiddleditch.com/direct3d-11-debug-api-tricks/
#if 0//_DEBUG
        ID3D11Debug* debugDevice = nullptr;
        HRESULT hr = m_device->QueryInterface( __uuidof( ID3D11Debug ), reinterpret_cast<void**>( &debugDevice ) );
        assert( SUCCEEDED( hr ) );
        debugDevice->ReportLiveDeviceObjects( D3D11_RLDO_DETAIL );
        SAFE_RELEASE( debugDevice );
#endif

        SAFE_RELEASE( m_device );
        SAFE_RELEASE( m_DXGIFactory );
        m_hwnd = 0;
    }
}

void vaRenderDeviceDX11::CreateSwapChain( int width, int height, bool windowed, HWND hwnd )
{
    m_swapChainSize.x = width;
    m_swapChainSize.y = height;

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

    desc.BufferCount = c_DefaultBackbufferCount;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_BACK_BUFFER; // | DXGI_USAGE_SHADER_INPUT; //DXGI_USAGE_UNORDERED_ACCESS
    desc.OutputWindow = hwnd;
    desc.Flags = c_DefaultSwapChainFlags;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    m_hwnd = hwnd;

    desc.Windowed = windowed;

    if( !windowed )
    {
        DXGI_MODE_DESC closestMatch;
        memset( &closestMatch, 0, sizeof( closestMatch ) );

        HRESULT hr = m_mainOutput->FindClosestMatchingMode( &desc.BufferDesc, &closestMatch, m_device );
        if( FAILED( hr ) )
        {
            VA_ERROR( L"Error trying to find closest matching display mode" );
        }
        desc.BufferDesc = closestMatch;
    }

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

    //if( FAILED( d3dResource->QueryInterface( __uuidof( IDXGISwapChain1 ), (void**)&m_swapChain ) ) )
    //{
    //   VA_ERROR( L"Error trying to cast into IDXGISwapChain1" );
    //}
    //SAFE_RELEASE( pSwapChain );

    //   m_swapChain->GetBuffer

    // Broadcast that the device was created!
    //vaDirectXCore::GetInstance( ).PostDeviceCreated( m_device, m_swapChain );
    vaDirectXTools_OnDeviceCreated( m_device, m_swapChain );

    vaGPUTimerDX11::OnDeviceAndContextCreated( m_device );

    CreateSwapChainRelatedObjects( );

    vaLog::GetInstance( ).Add( LOG_COLORS_NEUTRAL, L"DirectX 11 device and swap chain created" );

    ImGuiCreate( );
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

        m_mainRenderTargetView = vaDirectXTools::CreateRenderTargetView( pBackBuffer );

        m_mainColor = std::shared_ptr< vaTexture >( vaTextureDX11::CreateWrap( *this, pBackBuffer ) );

        SAFE_RELEASE( pBackBuffer );
    }

    // Create depth-stencil
    {
        CD3D11_TEXTURE2D_DESC desc( DXGI_FORMAT_R32G8X24_TYPELESS, m_backbufferTextureDesc.Width, m_backbufferTextureDesc.Height, 1, 1, D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE );
        V( m_device->CreateTexture2D( &desc, nullptr, &m_mainDepthStencil ) );

        m_mainDepthStencilView = vaDirectXTools::CreateDepthStencilView( m_mainDepthStencil, DXGI_FORMAT_D32_FLOAT_S8X24_UINT );
        m_mainDepthSRV = vaDirectXTools::CreateShaderResourceView( m_mainDepthStencil, DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS );

        m_mainDepth = std::shared_ptr< vaTexture >( vaTextureDX11::CreateWrap( *this, m_mainDepthStencil, vaResourceFormat::R32_FLOAT_X8X24_TYPELESS, vaResourceFormat::Automatic, vaResourceFormat::D32_FLOAT_S8X24_UINT ) );

        //delete vaTexture::CreateView( m_mainDepth, vaResourceBindSupportFlags::ShaderResource, vaResourceFormat::R32_FLOAT_X8X24_TYPELESS );
    }

    DXGI_SWAP_CHAIN_DESC scdesc;
    m_swapChain->GetDesc( &scdesc );

    DXGI_SURFACE_DESC sdesc;
    sdesc.Width = scdesc.BufferDesc.Width;
    sdesc.Height = scdesc.BufferDesc.Height;
    sdesc.SampleDesc = scdesc.SampleDesc;
    sdesc.Format = scdesc.BufferDesc.Format;

    m_mainDeviceContext->SetRenderTarget( m_mainColor, m_mainDepth, true );

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
    SAFE_RELEASE( m_mainDepthStencil );
    SAFE_RELEASE( m_mainDepthStencilView );
    SAFE_RELEASE( m_mainDepthSRV );

    if( m_mainDeviceContext != nullptr )
        m_mainDeviceContext->SetRenderTarget( NULL, NULL, false );
    m_mainColor = nullptr;
    m_mainDepth = nullptr;
}

bool vaRenderDeviceDX11::IsFullscreen( )
{
    BOOL fullscreen = FALSE;
    if( m_swapChain != NULL )
    {
        m_swapChain->GetFullscreenState( &fullscreen, NULL );
    }

    return fullscreen != FALSE;
}

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

bool vaRenderDeviceDX11::ResizeSwapChain( int width, int height, bool windowed )
{
    if( m_swapChain == NULL ) return false;
    if( (int)m_backbufferTextureDesc.Width == width && (int)m_backbufferTextureDesc.Height == height && windowed == !IsFullscreen( ) ) return false;

    if( (int)m_backbufferTextureDesc.Width != width || (int)m_backbufferTextureDesc.Height != height )
    {
        ReleaseSwapChainRelatedObjects( );

        HRESULT hr = m_swapChain->ResizeBuffers( c_DefaultBackbufferCount, width, height, DXGI_FORMAT_UNKNOWN, c_DefaultSwapChainFlags );
        if( FAILED( hr ) )
        {
            assert( false );
            //VA_ERROR( L"Error trying to m_swapChain->ResizeBuffers" );
            return false;
        }

        CreateSwapChainRelatedObjects( );
        return true;
    }

    //if( windowed != !IsFullscreen( ) )
    //{
    //    HRESULT hr = m_swapChain->SetFullscreenState( !windowed, NULL );
    //    if( FAILED( hr ) )
    //    {
    //        //VA_ERROR( L"Error trying to m_swapChain->SetFullscreenState" );
    //    }
    //
    //    IsFullscreen( );
    //    return true;
    //}
    return true;
}

void vaRenderDeviceDX11::SetMainRenderTargetToImmediateContext( )
{
    m_mainDeviceContext->SetRenderTarget( m_mainColor, m_mainDepth, true );
}

void vaRenderDeviceDX11::BeginFrame( float deltaTime )
{
    vaRenderDevice::BeginFrame( deltaTime );

#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGui::GetIO( ).DeltaTime = (float)deltaTime;

    if( ImGuiIsVisible( ) )
        m_mainDeviceContext->ImGuiNewFrame( );
#endif

    m_renderFrameCounter++;

    // m_directXCore.TickInternal( );

    vaGPUTimerDX11::OnFrameStart( );

    assert( m_frameProfilingNode == nullptr );
    m_frameProfilingNode = vaProfiler::GetInstance( ).StartScope( "FrameDrawAndPresent", GetMainContext( ), false );

    SetMainRenderTargetToImmediateContext( );
}

void vaRenderDeviceDX11::EndAndPresentFrame( int vsyncInterval )
{
    m_mainDeviceContext->SafeCast<vaRenderDeviceContextDX11*>()->CheckNoStatesSet( );

    {
        VA_SCOPE_CPUGPU_TIMER( Present, *m_mainDeviceContext );

        UINT flags = 0;

        HRESULT hr = m_swapChain->Present( (UINT)vsyncInterval, flags );
        if( FAILED( hr ) )
        {
            assert( false );
        }
    }

    assert( m_frameProfilingNode != nullptr );
    vaProfiler::GetInstance( ).StopScope( m_frameProfilingNode );
    m_frameProfilingNode = nullptr;
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
