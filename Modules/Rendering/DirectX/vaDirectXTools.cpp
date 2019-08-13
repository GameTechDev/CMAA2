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

#include "vaDirectXTools.h"
//#include "DirectX\vaDirectXCanvas.h"
#include "IntegratedExternals/DirectXTex/DDSTextureLoader/DDSTextureLoader.h"
#include "IntegratedExternals/DirectXTex/WICTextureLoader/WICTextureLoader.h"
#include "IntegratedExternals/DirectXTex/DDSTextureLoader/DDSTextureLoader12.h"
#include "IntegratedExternals/DirectXTex/WICTextureLoader/WICTextureLoader12.h"
#include "IntegratedExternals/DirectXTex/DirectXTex/DirectXTex.h"
//#include "DirectXTex\DirectXTex.h"

#include "Rendering/Shaders/vaSharedTypes.h"

#include "Rendering/DirectX/vaTextureDX11.h"

#include "Rendering/DirectX/vaRenderDeviceDX12.h"
#include "Rendering/DirectX/vaRenderDeviceContextDX12.h"

#include "Rendering/DirectX/vaShaderDX12.h"

#include "Core/System/vaFileTools.h"

#include <stdarg.h>

#include <dxgiformat.h>
#include <assert.h>
#include <memory>
#include <algorithm>

#include "Core/Misc/vaXXHash.h"

//#include "IntegratedExternals/DirectXTex/ScreenGrab/ScreenGrab.h"

#pragma warning (default : 4995)
#pragma warning ( disable : 4238 )  //  warning C4238: nonstandard extension used: class rvalue used as lvalue

using namespace std;
using namespace VertexAsylum;

static ID3D11RasterizerState *      g_RS_CullNone_Fill = nullptr;
static ID3D11RasterizerState *      g_RS_CullCCW_Fill = nullptr;
static ID3D11RasterizerState *      g_RS_CullCW_Fill = nullptr;
static ID3D11RasterizerState *      g_RS_CullNone_Wireframe = nullptr;
static ID3D11RasterizerState *      g_RS_CullCW_Wireframe = nullptr;
static ID3D11RasterizerState *      g_RS_CullCCW_Wireframe = nullptr;


static ID3D11DepthStencilState *    g_DSS_DepthEnabledL_DepthWrite = nullptr;
static ID3D11DepthStencilState *    g_DSS_DepthEnabledG_DepthWrite = nullptr;
static ID3D11DepthStencilState *    g_DSS_DepthEnabledLE_NoDepthWrite = nullptr;
static ID3D11DepthStencilState *    g_DSS_DepthEnabledL_NoDepthWrite = nullptr;
static ID3D11DepthStencilState *    g_DSS_DepthEnabledGE_NoDepthWrite = nullptr;
static ID3D11DepthStencilState *    g_DSS_DepthEnabledG_NoDepthWrite = nullptr;
static ID3D11DepthStencilState *    g_DSS_DepthDisabled_NoDepthWrite = nullptr;
static ID3D11DepthStencilState *    g_DSS_DepthPassAlways_DepthWrite = nullptr;
static ID3D11DepthStencilState *    g_DSS_DepthDisabled_StencilCreateMask = nullptr;
static ID3D11DepthStencilState *    g_DSS_DepthDisabled_StencilUseMask = nullptr;

static ID3D11BlendState *           g_BS_Opaque = nullptr;
static ID3D11BlendState *           g_BS_Additive = nullptr;
static ID3D11BlendState *           g_BS_AlphaBlend = nullptr;
static ID3D11BlendState *           g_BS_PremultAlphaBlend = nullptr;
static ID3D11BlendState *           g_BS_Mult = nullptr;

static ID3D11ShaderResourceView *   g_Texture2D_SRV_Noise2D = nullptr;
static ID3D11ShaderResourceView *   g_Texture2D_SRV_Noise3D = nullptr;

static ID3D11SamplerState *         g_samplerStatePointClamp = nullptr;
static ID3D11SamplerState *         g_samplerStatePointWrap = nullptr;
static ID3D11SamplerState *         g_samplerStatePointMirror = nullptr;
static ID3D11SamplerState *         g_samplerStateLinearClamp = nullptr;
static ID3D11SamplerState *         g_samplerStateLinearWrap = nullptr;
static ID3D11SamplerState *         g_samplerStateAnisotropicClamp = nullptr;
static ID3D11SamplerState *         g_samplerStateAnisotropicWrap = nullptr;
static ID3D11SamplerState *         g_samplerStateShadowCmp = nullptr;

template<typename T>
struct AnyStructComparer
{
    bool operator()( const T & Left, const T & Right ) const
    {
        // comparison logic goes here
        return memcmp( &Left, &Right, sizeof( Right ) ) < 0;
    }
};

class vaDirectXTools_RasterizerStatesLib
{
    ID3D11Device *                  m_device;
    std::map< D3D11_RASTERIZER_DESC, ID3D11RasterizerState *, AnyStructComparer<D3D11_RASTERIZER_DESC> >
                                    m_items;
public:
    vaDirectXTools_RasterizerStatesLib( ID3D11Device* device )
    {
        m_device = device;
    }
    ~vaDirectXTools_RasterizerStatesLib( )
    {
        for( auto it = m_items.begin( ); it != m_items.end( ); it++ )
        {
            SAFE_RELEASE( it->second );
        }
    }
    ID3D11RasterizerState * FindOrCreateRasterizerState( ID3D11Device * device, const D3D11_RASTERIZER_DESC & desc );
};

class vaDirectXTools_DepthStencilStatesLib
{
    ID3D11Device *                  m_device;
    std::map< D3D11_DEPTH_STENCIL_DESC, ID3D11DepthStencilState *, AnyStructComparer<D3D11_DEPTH_STENCIL_DESC> >
                                    m_items;
public:
    vaDirectXTools_DepthStencilStatesLib( ID3D11Device* device )
    {
        m_device = device;
    }
    ~vaDirectXTools_DepthStencilStatesLib( )
    {
        for( auto it = m_items.begin( ); it != m_items.end( ); it++ )
        {
            SAFE_RELEASE( it->second );
        }
    }
    ID3D11DepthStencilState * FindOrCreateDepthStencilState( ID3D11Device * device, const D3D11_DEPTH_STENCIL_DESC & desc );
};

static vaDirectXTools_DepthStencilStatesLib * g_depthStencilStatesLib = nullptr;
static vaDirectXTools_RasterizerStatesLib * g_rasterizerStatesLib = nullptr;

void vaDirectXTools_OnDevice11Created( ID3D11Device* device, IDXGISwapChain* swapChain )
{
    device;     // unreferenced
    swapChain;  // unreferenced
    HRESULT hr;

    g_rasterizerStatesLib = new vaDirectXTools_RasterizerStatesLib( device );
    g_depthStencilStatesLib = new vaDirectXTools_DepthStencilStatesLib( device );

    {
        CD3D11_RASTERIZER_DESC desc = CD3D11_RASTERIZER_DESC( CD3D11_DEFAULT( ) );

        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_NONE;
        device->CreateRasterizerState( &desc, &g_RS_CullNone_Fill );
        desc.FillMode = D3D11_FILL_WIREFRAME;
        device->CreateRasterizerState( &desc, &g_RS_CullNone_Wireframe );

        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_BACK;
        desc.FrontCounterClockwise = true;

        device->CreateRasterizerState( &desc, &g_RS_CullCW_Fill );
        desc.FillMode = D3D11_FILL_WIREFRAME;
        device->CreateRasterizerState( &desc, &g_RS_CullCW_Wireframe );

        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_BACK;
        desc.FrontCounterClockwise = false;

        device->CreateRasterizerState( &desc, &g_RS_CullCCW_Fill );
        desc.FillMode = D3D11_FILL_WIREFRAME;
        device->CreateRasterizerState( &desc, &g_RS_CullCCW_Wireframe );
    }

    {
        CD3D11_DEPTH_STENCIL_DESC desc = CD3D11_DEPTH_STENCIL_DESC( CD3D11_DEFAULT( ) );

        desc.DepthEnable = TRUE;
        desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
        V( device->CreateDepthStencilState( &desc, &g_DSS_DepthPassAlways_DepthWrite ) );

        desc.DepthEnable = TRUE;
        desc.DepthFunc = D3D11_COMPARISON_LESS;
        V( device->CreateDepthStencilState( &desc, &g_DSS_DepthEnabledL_DepthWrite ) );

        desc.DepthFunc = D3D11_COMPARISON_GREATER;
        V( device->CreateDepthStencilState( &desc, &g_DSS_DepthEnabledG_DepthWrite ) );

        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        desc.DepthFunc = D3D11_COMPARISON_LESS;
        V( device->CreateDepthStencilState( &desc, &g_DSS_DepthEnabledL_NoDepthWrite ) );

        desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        V( device->CreateDepthStencilState( &desc, &g_DSS_DepthEnabledLE_NoDepthWrite ) );

        desc.DepthFunc = D3D11_COMPARISON_GREATER;
        V( device->CreateDepthStencilState( &desc, &g_DSS_DepthEnabledG_NoDepthWrite ) );

        desc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
        V( device->CreateDepthStencilState( &desc, &g_DSS_DepthEnabledGE_NoDepthWrite ) );

        desc.DepthEnable = FALSE;
        desc.DepthFunc = D3D11_COMPARISON_LESS;
        V( device->CreateDepthStencilState( &desc, &g_DSS_DepthDisabled_NoDepthWrite ) );

        desc.StencilEnable = true;
        desc.StencilReadMask = 0xFF;
        desc.StencilWriteMask = 0xFF;
        desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
        desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        desc.BackFace = desc.FrontFace;
        V( device->CreateDepthStencilState( &desc, &g_DSS_DepthDisabled_StencilCreateMask ) );

        desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        desc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
        desc.BackFace = desc.FrontFace;
        V( device->CreateDepthStencilState( &desc, &g_DSS_DepthDisabled_StencilUseMask ) );
    }

    {
        CD3D11_BLEND_DESC desc = CD3D11_BLEND_DESC( CD3D11_DEFAULT( ) );

        desc.RenderTarget[0].BlendEnable = false;

        V( device->CreateBlendState( &desc, &g_BS_Opaque ) );

        desc.RenderTarget[0].BlendEnable = true;
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        V( device->CreateBlendState( &desc, &g_BS_Additive ) );

        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        V( device->CreateBlendState( &desc, &g_BS_AlphaBlend ) );

        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        V( device->CreateBlendState( &desc, &g_BS_PremultAlphaBlend ) );

        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ZERO;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_SRC_COLOR;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_SRC_ALPHA;
        V( device->CreateBlendState( &desc, &g_BS_Mult ) );

    }

    // samplers
    {
        CD3D11_SAMPLER_DESC desc = CD3D11_SAMPLER_DESC( CD3D11_DEFAULT( ) );

        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        desc.AddressU = desc.AddressV = desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        V( device->CreateSamplerState( &desc, &g_samplerStatePointClamp ) );
        desc.AddressU = desc.AddressV = desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        V( device->CreateSamplerState( &desc, &g_samplerStatePointWrap ) );
        desc.AddressU = desc.AddressV = desc.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR;
        V( device->CreateSamplerState( &desc, &g_samplerStatePointMirror ) );

        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU = desc.AddressV = desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        V( device->CreateSamplerState( &desc, &g_samplerStateLinearClamp ) );
        desc.AddressU = desc.AddressV = desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        V( device->CreateSamplerState( &desc, &g_samplerStateLinearWrap ) );

        desc.Filter = D3D11_FILTER_ANISOTROPIC;
        desc.MaxAnisotropy = 16;
        desc.AddressU = desc.AddressV = desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        V( device->CreateSamplerState( &desc, &g_samplerStateAnisotropicClamp ) );
        desc.AddressU = desc.AddressV = desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        V( device->CreateSamplerState( &desc, &g_samplerStateAnisotropicWrap ) );


        desc = CD3D11_SAMPLER_DESC( CD3D11_DEFAULT( ) );
        desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;   // D3D11_FILTER_COMPARISON_ANISOTROPIC
        desc.AddressU = desc.AddressV = desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        V( device->CreateSamplerState( &desc, &g_samplerStateShadowCmp ) );
    }

    {
        //SAFE_RELEASE( m_noiseTexture );

        //V( GetDevice()->CreateTexture( m_noiseTextureResolution, m_noiseTextureResolution, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &m_noiseTexture, NULL) );

        //int levelCount = m_noiseTexture->GetLevelCount();
        //int resolution = m_noiseTextureResolution;

        //for( int level = 0; level < levelCount; level++ )
        //{
        //   D3DLOCKED_RECT noiseLockedRect; 
        //   V( m_noiseTexture->LockRect( level, &noiseLockedRect, NULL, 0 ) );

        //   unsigned int * texRow = (unsigned int *)noiseLockedRect.pBits;

        //   for( int y = 0; y < resolution; y++ )
        //   {
        //      for( int x = 0; x < resolution; x++ )
        //      {
        //         texRow[x] = (0xFF & (int)(randf() * 256.0f));
        //         texRow[x] |= (0xFF & (int)(randf() * 256.0f)) << 8;

        //         float ang = randf();
        //         float fx = sinf( ang * (float)PI * 2.0f ) * 0.5f + 0.5f;
        //         float fy = sinf( ang * (float)PI * 2.0f ) * 0.5f + 0.5f;

        //         texRow[x] |= (0xFF & (int)(fx * 256.0f)) << 16;
        //         texRow[x] |= (0xFF & (int)(fy * 256.0f)) << 24;
        //      }
        //      texRow += noiseLockedRect.Pitch / sizeof(*texRow);
        //   }
        //   V( m_noiseTexture->UnlockRect(level) );
        //   resolution /= 2;
        //vaDirectXCore::NameObject( texture2D, "g_Texture2D_SRV_White1x1" );
        //}
    }
}

void vaDirectXTools_OnDevice11Destroyed( )
{
    SAFE_RELEASE( g_RS_CullNone_Fill );
    SAFE_RELEASE( g_RS_CullCCW_Fill );
    SAFE_RELEASE( g_RS_CullCW_Fill );
    SAFE_RELEASE( g_RS_CullNone_Wireframe );
    SAFE_RELEASE( g_RS_CullCW_Wireframe );
    SAFE_RELEASE( g_RS_CullCCW_Wireframe );


    SAFE_RELEASE( g_DSS_DepthEnabledL_DepthWrite );
    SAFE_RELEASE( g_DSS_DepthEnabledG_DepthWrite );
    SAFE_RELEASE( g_DSS_DepthEnabledL_NoDepthWrite );
    SAFE_RELEASE( g_DSS_DepthEnabledLE_NoDepthWrite );
    SAFE_RELEASE( g_DSS_DepthEnabledG_NoDepthWrite );
    SAFE_RELEASE( g_DSS_DepthEnabledGE_NoDepthWrite );
    SAFE_RELEASE( g_DSS_DepthDisabled_NoDepthWrite );
    SAFE_RELEASE( g_DSS_DepthPassAlways_DepthWrite );
    SAFE_RELEASE( g_DSS_DepthDisabled_StencilCreateMask );
    SAFE_RELEASE( g_DSS_DepthDisabled_StencilUseMask );

    SAFE_RELEASE( g_BS_Opaque );
    SAFE_RELEASE( g_BS_Additive );
    SAFE_RELEASE( g_BS_AlphaBlend );
    SAFE_RELEASE( g_BS_PremultAlphaBlend );
    SAFE_RELEASE( g_BS_Mult );

    SAFE_RELEASE( g_Texture2D_SRV_Noise2D );
    SAFE_RELEASE( g_Texture2D_SRV_Noise3D );

    SAFE_RELEASE( g_samplerStatePointClamp );
    SAFE_RELEASE( g_samplerStatePointWrap );
    SAFE_RELEASE( g_samplerStatePointMirror );
    SAFE_RELEASE( g_samplerStateLinearClamp );
    SAFE_RELEASE( g_samplerStateLinearWrap );
    SAFE_RELEASE( g_samplerStateAnisotropicClamp );
    SAFE_RELEASE( g_samplerStateAnisotropicWrap );
    SAFE_RELEASE( g_samplerStateShadowCmp );

    SAFE_DELETE( g_rasterizerStatesLib );
    SAFE_DELETE( g_depthStencilStatesLib );
}

ID3D11RasterizerState * vaDirectXTools11::GetRS_CullNone_Fill( )
{
    return g_RS_CullNone_Fill;
}

ID3D11RasterizerState * vaDirectXTools11::GetRS_CullCCW_Fill( )
{
    return g_RS_CullCCW_Fill;
}

ID3D11RasterizerState * vaDirectXTools11::GetRS_CullCW_Fill( )
{
    return g_RS_CullCW_Fill;
}

ID3D11RasterizerState * vaDirectXTools11::GetRS_CullNone_Wireframe( )
{
    return g_RS_CullNone_Wireframe;
}

ID3D11RasterizerState * vaDirectXTools11::GetRS_CullCCW_Wireframe( )
{
    return g_RS_CullCCW_Wireframe;
}

ID3D11RasterizerState * vaDirectXTools11::GetRS_CullCW_Wireframe( )
{
    return g_RS_CullCW_Wireframe;
}

ID3D11DepthStencilState * vaDirectXTools11::GetDSS_DepthEnabledL_DepthWrite( )
{
    return g_DSS_DepthEnabledL_DepthWrite;
}

ID3D11DepthStencilState * vaDirectXTools11::GetDSS_DepthEnabledG_DepthWrite( )
{
    return g_DSS_DepthEnabledG_DepthWrite;
}

ID3D11DepthStencilState * vaDirectXTools11::GetDSS_DepthEnabledL_NoDepthWrite( )
{
    return g_DSS_DepthEnabledL_NoDepthWrite;
}

ID3D11DepthStencilState * vaDirectXTools11::GetDSS_DepthEnabledLE_NoDepthWrite( )
{
    return g_DSS_DepthEnabledLE_NoDepthWrite;
}

ID3D11DepthStencilState * vaDirectXTools11::GetDSS_DepthEnabledG_NoDepthWrite( )
{
    return g_DSS_DepthEnabledG_NoDepthWrite;
}

ID3D11DepthStencilState * vaDirectXTools11::GetDSS_DepthEnabledGE_NoDepthWrite( )
{
    return g_DSS_DepthEnabledGE_NoDepthWrite;
}

ID3D11DepthStencilState * vaDirectXTools11::GetDSS_DepthDisabled_NoDepthWrite( )
{
    return g_DSS_DepthDisabled_NoDepthWrite;
}

ID3D11DepthStencilState * vaDirectXTools11::GetDSS_DepthPassAlways_DepthWrite( )
{
    return g_DSS_DepthPassAlways_DepthWrite;
}

ID3D11DepthStencilState * vaDirectXTools11::GetDSS_DepthDisabled_NoDepthWrite_StencilCreateMask( )
{
    return g_DSS_DepthDisabled_StencilCreateMask;
}

ID3D11DepthStencilState * vaDirectXTools11::GetDSS_DepthDisabled_NoDepthWrite_StencilUseMask( )
{
    return g_DSS_DepthDisabled_StencilUseMask;
}

ID3D11SamplerState * vaDirectXTools11::GetSamplerStatePointClamp( ) { return g_samplerStatePointClamp; }
ID3D11SamplerState * vaDirectXTools11::GetSamplerStatePointWrap( ) { return g_samplerStatePointWrap; }
ID3D11SamplerState * vaDirectXTools11::GetSamplerStatePointMirror( ) { return g_samplerStatePointMirror; }
ID3D11SamplerState * vaDirectXTools11::GetSamplerStateLinearClamp( ) { return g_samplerStateLinearClamp; }
ID3D11SamplerState * vaDirectXTools11::GetSamplerStateLinearWrap( ) { return g_samplerStateLinearWrap; }
ID3D11SamplerState * vaDirectXTools11::GetSamplerStateAnisotropicClamp( ) { return g_samplerStateAnisotropicClamp; }
ID3D11SamplerState * vaDirectXTools11::GetSamplerStateAnisotropicWrap( ) { return g_samplerStateAnisotropicWrap; }
ID3D11SamplerState * vaDirectXTools11::GetSamplerStateShadowCmp( ) { return g_samplerStateShadowCmp; }

ID3D11SamplerState * * vaDirectXTools11::GetSamplerStatePtrPointClamp( ) { return &g_samplerStatePointClamp; }
ID3D11SamplerState * * vaDirectXTools11::GetSamplerStatePtrPointWrap( ) { return &g_samplerStatePointWrap; }
ID3D11SamplerState * * vaDirectXTools11::GetSamplerStatePtrLinearClamp( ) { return &g_samplerStateLinearClamp; }
ID3D11SamplerState * * vaDirectXTools11::GetSamplerStatePtrLinearWrap( ) { return &g_samplerStateLinearWrap; }
ID3D11SamplerState * * vaDirectXTools11::GetSamplerStatePtrAnisotropicClamp( ) { return &g_samplerStateAnisotropicClamp; }
ID3D11SamplerState * * vaDirectXTools11::GetSamplerStatePtrAnisotropicWrap( ) { return &g_samplerStateAnisotropicWrap; }
ID3D11SamplerState * * vaDirectXTools11::GetSamplerStatePtrShadowCmp( ) { return &g_samplerStateShadowCmp; }

ID3D11RasterizerState * vaDirectXTools11::CreateRasterizerState( ID3D11Device * device, const D3D11_RASTERIZER_DESC & desc )
{
    ID3D11RasterizerState * retVal = NULL;

    HRESULT hr;

    hr = device->CreateRasterizerState( &desc, &retVal );

    assert( SUCCEEDED( hr ) );
    if( !SUCCEEDED( hr ) )
    {
        return NULL;
    }

    return retVal;
}

ID3D11RasterizerState * vaDirectXTools_RasterizerStatesLib::FindOrCreateRasterizerState( ID3D11Device * device, const D3D11_RASTERIZER_DESC & desc )
{
    auto it = m_items.find( desc );
    if( it != m_items.end( ) )
    {
        return it->second;
    }
    else
    {
        ID3D11RasterizerState * newRS = vaDirectXTools11::CreateRasterizerState( device, desc );
        m_items.insert( std::make_pair( desc, newRS ) );
        return newRS;
    }
}

ID3D11RasterizerState * vaDirectXTools11::FindOrCreateRasterizerState( ID3D11Device * device, const D3D11_RASTERIZER_DESC & desc )
{
    assert( g_rasterizerStatesLib != nullptr );
    return g_rasterizerStatesLib->FindOrCreateRasterizerState( device, desc );
}

ID3D11DepthStencilState * vaDirectXTools_DepthStencilStatesLib::FindOrCreateDepthStencilState( ID3D11Device * device, const D3D11_DEPTH_STENCIL_DESC & desc )
{
    auto it = m_items.find( desc );
    if( it != m_items.end( ) )
    {
        return it->second;
    }
    else
    {
        ID3D11DepthStencilState * newRS = nullptr;
        HRESULT hr = device->CreateDepthStencilState( &desc, &newRS );

        assert( SUCCEEDED( hr ) );
        if( !SUCCEEDED( hr ) )
            return nullptr;

        m_items.insert( std::make_pair( desc, newRS ) );
        return newRS;
    }
}

ID3D11DepthStencilState * vaDirectXTools11::FindOrCreateDepthStencilState( ID3D11Device * device, const D3D11_DEPTH_STENCIL_DESC & desc )
{
    assert( g_depthStencilStatesLib != nullptr );
    return g_depthStencilStatesLib->FindOrCreateDepthStencilState( device, desc );
}

ID3D11BlendState * vaDirectXTools11::GetBS_Opaque( )
{
    return g_BS_Opaque;
}

ID3D11BlendState * vaDirectXTools11::GetBS_Additive( )
{
    return g_BS_Additive;
}

ID3D11BlendState * vaDirectXTools11::GetBS_AlphaBlend( )
{
    return g_BS_AlphaBlend;
}

ID3D11BlendState * vaDirectXTools11::GetBS_PremultAlphaBlend( )
{
    return g_BS_PremultAlphaBlend;
}

ID3D11BlendState * vaDirectXTools11::GetBS_Mult( )
{
    return g_BS_Mult;
}

ID3D11ShaderResourceView * vaDirectXTools11::CreateShaderResourceView( ID3D11Resource * texture, DXGI_FORMAT format, int mipSliceMin, int mipSliceCount, int arraySliceMin, int arraySliceCount )
{
    ID3D11ShaderResourceView * ret = NULL;

    D3D11_SHADER_RESOURCE_VIEW_DESC desc;
    bool descInitialized = false;

    {
        D3D11_SRV_DIMENSION dimension = D3D11_SRV_DIMENSION_UNKNOWN;

        if( !descInitialized )
        {
            ID3D11Texture2D * texture2D = NULL;
            if( SUCCEEDED( texture->QueryInterface( IID_ID3D11Texture2D, (void**)&texture2D ) ) )
            {
                D3D11_TEXTURE2D_DESC descTex2D;
                texture2D->GetDesc( &descTex2D );

                if( mipSliceCount == -1 )
                    mipSliceCount = descTex2D.MipLevels-mipSliceMin;
                if( arraySliceCount == -1 )
                    arraySliceCount = descTex2D.ArraySize-arraySliceMin;

                assert( mipSliceMin >= 0 && (UINT)mipSliceMin < descTex2D.MipLevels );
                assert( mipSliceMin+mipSliceCount > 0 && (UINT)mipSliceMin+mipSliceCount <= descTex2D.MipLevels );
                assert( arraySliceMin >= 0 && (UINT)arraySliceMin < descTex2D.ArraySize );
                assert( arraySliceMin+arraySliceCount > 0 && (UINT)arraySliceMin+arraySliceCount <= descTex2D.ArraySize );

                if( descTex2D.SampleDesc.Count > 1 )
                    dimension = ( descTex2D.ArraySize == 1 ) ? ( D3D11_SRV_DIMENSION_TEXTURE2DMS ) : ( D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY );
                else
                    dimension = ( descTex2D.ArraySize == 1 ) ? ( D3D11_SRV_DIMENSION_TEXTURE2D ) : ( D3D11_SRV_DIMENSION_TEXTURE2DARRAY );

                desc = CD3D11_SHADER_RESOURCE_VIEW_DESC( texture2D, dimension, format );

                if( dimension == D3D11_SRV_DIMENSION_TEXTURE2D )
                {
                    desc.Texture2D.MostDetailedMip      = mipSliceMin;
                    desc.Texture2D.MipLevels            = mipSliceCount;
                    assert( arraySliceMin == 0 );
                    assert( arraySliceCount == 1 );
                }
                else if( dimension == D3D11_SRV_DIMENSION_TEXTURE2DARRAY )
                {
                    desc.Texture2DArray.MostDetailedMip = mipSliceMin;
                    desc.Texture2DArray.MipLevels       = mipSliceCount;
                    desc.Texture2DArray.FirstArraySlice = arraySliceMin;
                    desc.Texture2DArray.ArraySize       = arraySliceCount;
                }
                else if( dimension == D3D11_SRV_DIMENSION_TEXTURE2DMS )
                {
                    desc.Texture2DMS.UnusedField_NothingToDefine = 42;
                    assert( mipSliceMin == 0 );
                    assert( mipSliceCount == 1 );
                    assert( arraySliceMin == 0 );
                    assert( arraySliceCount == 1 );
                }
                else if( dimension == D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY )
                {
                    assert( mipSliceMin == 0 );
                    assert( arraySliceCount == 1 );
                    desc.Texture2DMSArray.FirstArraySlice   = arraySliceMin;
                    desc.Texture2DMSArray.ArraySize         = arraySliceCount;
                }
                else { assert( false ); }

                descInitialized = true;
                SAFE_RELEASE( texture2D );
            }
        }

        if( !descInitialized )
        {
            ID3D11Texture3D * texture3D = NULL;
            if( SUCCEEDED( texture->QueryInterface( IID_ID3D11Texture3D, (void**)&texture3D ) ) )
            {
                D3D11_TEXTURE3D_DESC descTex3D;
                texture3D->GetDesc( &descTex3D );

                if( mipSliceCount == -1 )
                    mipSliceCount = descTex3D.MipLevels - mipSliceMin;
                assert( mipSliceMin >= 0 && (UINT)mipSliceMin < descTex3D.MipLevels );
                assert( mipSliceMin + mipSliceCount > 0 && (UINT)mipSliceMin + mipSliceCount <= descTex3D.MipLevels );

                // no array slices for 3D textures
                assert( arraySliceMin == 0 );
                assert( arraySliceCount == 1 || arraySliceCount == -1 );

                dimension = D3D11_SRV_DIMENSION_TEXTURE3D;
                desc = CD3D11_SHADER_RESOURCE_VIEW_DESC( texture3D, format );

                desc.Texture3D.MostDetailedMip  = mipSliceMin;
                desc.Texture3D.MipLevels        = mipSliceCount;

                descInitialized = true;
                SAFE_RELEASE( texture3D );
            }
        }

        if( !descInitialized )
        {
            ID3D11Texture1D * texture1D = NULL;
            if( SUCCEEDED( texture->QueryInterface( IID_ID3D11Texture1D, (void**)&texture1D ) ) )
            {
                D3D11_TEXTURE1D_DESC descTex1D;
                texture1D->GetDesc( &descTex1D );

                if( mipSliceCount == -1 )
                    mipSliceCount = descTex1D.MipLevels - mipSliceMin;
                if( arraySliceCount == -1 )
                    arraySliceCount = descTex1D.ArraySize - arraySliceMin;

                assert( mipSliceMin >= 0 && (UINT)mipSliceMin < descTex1D.MipLevels );
                assert( mipSliceMin + mipSliceCount > 0 && (UINT)mipSliceMin + mipSliceCount <= descTex1D.MipLevels );
                assert( arraySliceMin >= 0 && (UINT)arraySliceMin < descTex1D.ArraySize );
                assert( arraySliceMin + arraySliceCount > 0 && (UINT)arraySliceMin + arraySliceCount <= descTex1D.ArraySize );

                dimension = ( descTex1D.ArraySize == 1 ) ? ( D3D11_SRV_DIMENSION_TEXTURE1D ) : ( D3D11_SRV_DIMENSION_TEXTURE1DARRAY );
                desc = CD3D11_SHADER_RESOURCE_VIEW_DESC( texture1D, dimension, format );
                descInitialized = true;
                SAFE_RELEASE( texture1D );

                if( dimension == D3D11_SRV_DIMENSION_TEXTURE1D )
                {
                    desc.Texture1D.MostDetailedMip  = mipSliceMin;
                    desc.Texture1D.MipLevels        = mipSliceCount;
                }
                else if( dimension == D3D11_SRV_DIMENSION_TEXTURE1DARRAY )
                {
                    desc.Texture1DArray.MostDetailedMip = mipSliceMin;
                    desc.Texture1DArray.MipLevels       = mipSliceCount;
                    desc.Texture1DArray.FirstArraySlice = arraySliceMin;
                    desc.Texture1DArray.ArraySize       = arraySliceCount;
                } else { assert( false ); }
            }
        }
    }

    if( !descInitialized )
    {
        assert( false ); // resource not recognized; additional code might be needed above
        return NULL;
    }

    ID3D11Device * device = nullptr;
    texture->GetDevice(&device);
    device->Release(); // yeah, ugly - we know at minimum texture will guarantee device persistence but still...
    if( SUCCEEDED( device->CreateShaderResourceView( texture, &desc, &ret ) ) )
    {
        return ret;
    }
    else
    {
        assert( false );
        return NULL;
    }
}

ID3D11ShaderResourceView * vaDirectXTools11::CreateShaderResourceViewCubemap( ID3D11Resource * texture, DXGI_FORMAT format, int mipSliceMin, int mipSliceCount, int arraySliceMin, int arraySliceCount )
{
    ID3D11ShaderResourceView * ret = NULL;

    D3D11_SHADER_RESOURCE_VIEW_DESC desc;

    ID3D11Texture2D * texture2D = NULL;
    if( SUCCEEDED( texture->QueryInterface( IID_ID3D11Texture2D, (void**)&texture2D ) ) )
    {
        D3D11_TEXTURE2D_DESC descTex2D;
        texture2D->GetDesc( &descTex2D );

        if( ( descTex2D.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE ) == 0 )
        {
            // not a cubemap?
            SAFE_RELEASE( texture2D );
            assert( false );
            return NULL;
        }

        if( mipSliceCount == -1 )
            mipSliceCount = descTex2D.MipLevels - mipSliceMin;
        if( arraySliceCount == -1 )
            arraySliceCount = descTex2D.ArraySize - arraySliceMin;

        assert( mipSliceMin >= 0 && (UINT)mipSliceMin < descTex2D.MipLevels );
        assert( mipSliceMin + mipSliceCount > 0 && (UINT)mipSliceMin + mipSliceCount <= descTex2D.MipLevels );
        assert( arraySliceMin >= 0 && (UINT)arraySliceMin < descTex2D.ArraySize );
        assert( arraySliceMin + arraySliceCount > 0 && (UINT)arraySliceMin + arraySliceCount <= descTex2D.ArraySize );

        D3D11_SRV_DIMENSION dimension = (descTex2D.ArraySize==6)?(D3D11_SRV_DIMENSION_TEXTURECUBE):(D3D11_SRV_DIMENSION_TEXTURECUBEARRAY);
        assert( descTex2D.ArraySize % 6 == 0 );

        desc = CD3D11_SHADER_RESOURCE_VIEW_DESC( texture2D, dimension, format );
        SAFE_RELEASE( texture2D );

        if( dimension == D3D11_SRV_DIMENSION_TEXTURECUBE )
        {
            desc.TextureCube.MostDetailedMip = mipSliceMin;
            desc.TextureCube.MipLevels       = mipSliceCount;
            assert( arraySliceMin == 0 );
            assert( arraySliceCount == 6 );
        }
        else if( dimension == D3D11_SRV_DIMENSION_TEXTURECUBEARRAY )
        {
            desc.TextureCubeArray.MostDetailedMip   = mipSliceMin;
            desc.TextureCubeArray.MipLevels         = mipSliceCount;
            desc.TextureCubeArray.First2DArrayFace  = arraySliceMin/6;
            desc.TextureCubeArray.NumCubes          = arraySliceCount/6;
        }
        else { assert(false); }
    }
    else
    {
        assert( false ); // not a texture2d? must be for a cubemap
        return NULL;
    }

    ID3D11Device * device = nullptr;
    texture->GetDevice(&device);
    device->Release(); // yeah, ugly - we know at minimum texture will guarantee device persistence but still...
    if( SUCCEEDED( device->CreateShaderResourceView( texture, &desc, &ret ) ) )
    {
        return ret;
    }
    else
    {
        assert( false );
        return NULL;
    }

}

ID3D11DepthStencilView * vaDirectXTools11::CreateDepthStencilView( ID3D11Resource * texture, DXGI_FORMAT format, int mipSliceMin, int arraySliceMin, int arraySliceCount )
{
    ID3D11DepthStencilView * ret = NULL;

    D3D11_DEPTH_STENCIL_VIEW_DESC desc;
    bool descInitialized = false;

    if( !descInitialized )
    {
        D3D11_DSV_DIMENSION dimension = D3D11_DSV_DIMENSION_UNKNOWN;
        ID3D11Texture2D * texture2D = NULL;

        if( SUCCEEDED( texture->QueryInterface( IID_ID3D11Texture2D, (void**)&texture2D ) ) )
        {
            D3D11_TEXTURE2D_DESC descTex2D;
            texture2D->GetDesc( &descTex2D );

            if( arraySliceCount == -1 )
                arraySliceCount = descTex2D.ArraySize - arraySliceMin;

            assert( mipSliceMin >= 0 && (UINT)mipSliceMin < descTex2D.MipLevels );
            assert( arraySliceMin >= 0 && (UINT)arraySliceMin < descTex2D.ArraySize );
            assert( arraySliceMin + arraySliceCount > 0 && (UINT)arraySliceMin + arraySliceCount <= descTex2D.ArraySize );

            if( descTex2D.SampleDesc.Count > 1 )
                dimension = ( descTex2D.ArraySize == 1 ) ? ( D3D11_DSV_DIMENSION_TEXTURE2DMS ) : ( D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY );
            else
                dimension = ( descTex2D.ArraySize == 1 ) ? ( D3D11_DSV_DIMENSION_TEXTURE2D ) : ( D3D11_DSV_DIMENSION_TEXTURE2DARRAY );

            desc = CD3D11_DEPTH_STENCIL_VIEW_DESC( texture2D, dimension, format );

            if( dimension == D3D11_DSV_DIMENSION_TEXTURE2D )
            {
                desc.Texture2D.MipSlice             = mipSliceMin;
                assert( arraySliceMin == 0 );
            }
            else if( dimension == D3D11_DSV_DIMENSION_TEXTURE2DARRAY )
            {
                desc.Texture2DArray.MipSlice        = mipSliceMin;
                desc.Texture2DArray.FirstArraySlice = arraySliceMin;
                desc.Texture2DArray.ArraySize       = arraySliceCount;
            }
            else if( dimension == D3D11_DSV_DIMENSION_TEXTURE2DMS )
            {
                desc.Texture2DMS.UnusedField_NothingToDefine = 42;
                assert( mipSliceMin == 0 );
                assert( arraySliceMin == 0 );
            }
            else if( dimension == D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY )
            {
                desc.Texture2DMSArray.FirstArraySlice = arraySliceMin;
                desc.Texture2DMSArray.ArraySize       = arraySliceCount;
                assert( mipSliceMin == 0 );
            }
            else { assert( false ); }

            descInitialized = true;
            SAFE_RELEASE( texture2D );
        }
    }

    // there is no 3D depth stencil view
    //if( !descInitialized )
    // {
    //     ID3D11Texture3D * texture3D = NULL;
    //     if( SUCCEEDED( resource->QueryInterface( IID_ID3D11Texture3D, (void**)&texture3D ) ) )
    //     {
    //         D3D11_TEXTURE3D_DESC descTex3D;
    //         texture3D->GetDesc( &descTex3D );
    // 
    //         if( arraySliceCount == -1 )
    //             arraySliceCount = descTex3D.Depth - arraySliceMin;
    // 
    //         assert( mipSliceMin >= 0 && (UINT)mipSliceMin < descTex3D.MipLevels );
    //         assert( arraySliceMin >= 0 && (UINT)arraySliceMin < descTex3D.Depth );
    //         assert( arraySliceMin + arraySliceCount > 0 && (UINT)arraySliceMin + arraySliceCount <= descTex3D.Depth );
    // 
    //         desc = CD3D11_DEPTH_STENCIL_VIEW_DESC( texture3D, format );
    //         
    //         desc.Texture3D.MipSlice     = mipSlice;
    //         desc.Texture3D.FirstWSlice  = arraySliceMin;
    //         desc.Texture3D.WSize        = arraySliceCount;
    //         
    //         descInitialized = true;
    //         SAFE_RELEASE( texture3D );
    //     }
    // }

    if( !descInitialized )
    {
        assert( false ); // resource not recognized; additional code might be needed above
        return NULL;
    }

    ID3D11Device * device = nullptr;
    texture->GetDevice(&device);
    device->Release(); // yeah, ugly - we know at minimum texture will guarantee device persistence but still...
    if( SUCCEEDED( device->CreateDepthStencilView( texture, &desc, &ret ) ) )
    {
        return ret;
    }
    else
    {
        assert( false );
        return NULL;
    }
}

ID3D11RenderTargetView * vaDirectXTools11::CreateRenderTargetView( ID3D11Resource * resource, DXGI_FORMAT format, int mipSliceMin, int arraySliceMin, int arraySliceCount )
{
    ID3D11RenderTargetView * ret = NULL;

    D3D11_RENDER_TARGET_VIEW_DESC desc;
    bool descInitialized = false;

    if( !descInitialized )
    {
        D3D11_RTV_DIMENSION dimension = D3D11_RTV_DIMENSION_UNKNOWN;

        ID3D11Texture2D * texture2D = NULL;
        if( SUCCEEDED( resource->QueryInterface( IID_ID3D11Texture2D, (void**)&texture2D ) ) )
        {
            D3D11_TEXTURE2D_DESC descTex2D;
            texture2D->GetDesc( &descTex2D );

            if( arraySliceCount == -1 )
                arraySliceCount = descTex2D.ArraySize - arraySliceMin;

            assert( mipSliceMin >= 0 && (UINT)mipSliceMin < descTex2D.MipLevels );
            assert( arraySliceMin >= 0 && (UINT)arraySliceMin < descTex2D.ArraySize );
            assert( arraySliceMin + arraySliceCount > 0 && (UINT)arraySliceMin + arraySliceCount <= descTex2D.ArraySize );

            if( descTex2D.SampleDesc.Count > 1 )
                dimension = ( descTex2D.ArraySize == 1 ) ? ( D3D11_RTV_DIMENSION_TEXTURE2DMS ) : ( D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY );
            else
                dimension = ( descTex2D.ArraySize == 1 ) ? ( D3D11_RTV_DIMENSION_TEXTURE2D ) : ( D3D11_RTV_DIMENSION_TEXTURE2DARRAY );

            desc = CD3D11_RENDER_TARGET_VIEW_DESC( texture2D, dimension, format );

            if( dimension == D3D11_RTV_DIMENSION_TEXTURE2D )
            {
                desc.Texture2D.MipSlice             = mipSliceMin;
                assert( arraySliceMin == 0 );
            }
            else if( dimension == D3D11_RTV_DIMENSION_TEXTURE2DARRAY )
            {
                desc.Texture2DArray.MipSlice        = mipSliceMin;
                desc.Texture2DArray.FirstArraySlice = arraySliceMin;
                desc.Texture2DArray.ArraySize       = arraySliceCount;
            }
            else if( dimension == D3D11_RTV_DIMENSION_TEXTURE2DMS )
            {
                desc.Texture2DMS.UnusedField_NothingToDefine = 42;
                assert( mipSliceMin == 0 );
                assert( arraySliceMin == 0 );
            }
            else if( dimension == D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY )
            {
                desc.Texture2DMSArray.FirstArraySlice = arraySliceMin;
                desc.Texture2DMSArray.ArraySize       = arraySliceCount;
                assert( mipSliceMin == 0 );
            }
            else { assert( false ); }

            descInitialized = true;
            SAFE_RELEASE( texture2D );
        }
    }

    if( !descInitialized )
    {
        ID3D11Texture3D * texture3D = NULL;
        if( SUCCEEDED( resource->QueryInterface( IID_ID3D11Texture3D, (void**)&texture3D ) ) )
        {
            assert( false ); // codepath never tested so check it up first!

            D3D11_TEXTURE3D_DESC descTex3D;
            texture3D->GetDesc( &descTex3D );

            if( arraySliceCount == -1 )
                arraySliceCount = descTex3D.Depth - arraySliceMin;

            assert( mipSliceMin >= 0 && (UINT)mipSliceMin < descTex3D.MipLevels );
            assert( arraySliceMin >= 0 && (UINT)arraySliceMin < descTex3D.Depth );
            assert( arraySliceMin + arraySliceCount > 0 && (UINT)arraySliceMin + arraySliceCount <= descTex3D.Depth );

            desc = CD3D11_RENDER_TARGET_VIEW_DESC( texture3D, format );
            
            desc.Texture3D.MipSlice     = mipSliceMin;
            desc.Texture3D.FirstWSlice  = arraySliceMin;
            desc.Texture3D.WSize        = arraySliceCount;
            
            descInitialized = true;
            SAFE_RELEASE( texture3D );
        }
    }

    if( !descInitialized )
    {
        assert( false ); // resource not recognized; additional code might be needed above
        return NULL;
    }

    ID3D11Device * device = nullptr;
    resource->GetDevice(&device);
    device->Release(); // yeah, ugly - we know at minimum texture will guarantee device persistence but still...
    if( SUCCEEDED( device->CreateRenderTargetView( resource, &desc, &ret ) ) )
    {
        return ret;
    }
    else
    {
        assert( false );
        return NULL;
    }
}

ID3D11UnorderedAccessView * vaDirectXTools11::CreateUnorderedAccessView( ID3D11Resource * resource, DXGI_FORMAT format, int mipSliceMin, int arraySliceMin, int arraySliceCount )
{
    ID3D11UnorderedAccessView * ret = NULL;

    D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
    bool descInitialized = false;

    {
        D3D11_UAV_DIMENSION dimension = D3D11_UAV_DIMENSION_UNKNOWN;

        if( !descInitialized )
        {
            ID3D11Texture2D * texture2D = NULL;
            if( SUCCEEDED( resource->QueryInterface( IID_ID3D11Texture2D, (void**)&texture2D ) ) )
            {
                D3D11_TEXTURE2D_DESC descTex2D;
                texture2D->GetDesc( &descTex2D );

                if( arraySliceCount == -1 )
                    arraySliceCount = descTex2D.ArraySize - arraySliceMin;

                assert( mipSliceMin >= 0 && (UINT)mipSliceMin < descTex2D.MipLevels );
                assert( arraySliceMin >= 0 && (UINT)arraySliceMin < descTex2D.ArraySize );
                assert( arraySliceMin + arraySliceCount > 0 && (UINT)arraySliceMin + arraySliceCount <= descTex2D.ArraySize );

                dimension = ( descTex2D.ArraySize == 1 ) ? ( D3D11_UAV_DIMENSION_TEXTURE2D ) : ( D3D11_UAV_DIMENSION_TEXTURE2DARRAY );

                desc = CD3D11_UNORDERED_ACCESS_VIEW_DESC( texture2D, dimension, format );

                if( dimension == D3D11_UAV_DIMENSION_TEXTURE2D )
                {
                    desc.Texture2D.MipSlice             = mipSliceMin;
                    assert( arraySliceMin == 0 );
                }
                else if( dimension == D3D11_UAV_DIMENSION_TEXTURE2DARRAY )
                {
                    desc.Texture2DArray.MipSlice        = mipSliceMin;
                    desc.Texture2DArray.FirstArraySlice = arraySliceMin;
                    desc.Texture2DArray.ArraySize       = arraySliceCount;
                }
                else { assert( false ); }

                descInitialized = true;
                SAFE_RELEASE( texture2D );
            }

        }

        if( !descInitialized )
        {
            ID3D11Texture3D * texture3D = NULL;
            if( SUCCEEDED( resource->QueryInterface( IID_ID3D11Texture3D, (void**)&texture3D ) ) )
            {
                D3D11_TEXTURE3D_DESC descTex3D;
                texture3D->GetDesc( &descTex3D );

                if( arraySliceCount == -1 )
                    arraySliceCount = descTex3D.Depth - arraySliceMin;

                assert( mipSliceMin >= 0 && (UINT)mipSliceMin < descTex3D.MipLevels );
                assert( arraySliceMin >= 0 && (UINT)arraySliceMin < descTex3D.Depth );
                assert( arraySliceMin + arraySliceCount > 0 && (UINT)arraySliceMin + arraySliceCount <= descTex3D.Depth );

                dimension = D3D11_UAV_DIMENSION_TEXTURE3D;
                desc = CD3D11_UNORDERED_ACCESS_VIEW_DESC( texture3D, format );
            
                desc.Texture3D.MipSlice     = mipSliceMin;
                desc.Texture3D.FirstWSlice  = arraySliceMin;
                desc.Texture3D.WSize        = arraySliceCount;

                descInitialized = true;
                SAFE_RELEASE( texture3D );
            }
        }

        if( !descInitialized )
        {
            ID3D11Texture1D * texture1D = NULL;
            if( SUCCEEDED( resource->QueryInterface( IID_ID3D11Texture1D, (void**)&texture1D ) ) )
            {
                D3D11_TEXTURE1D_DESC descTex1D;
                texture1D->GetDesc( &descTex1D );

                if( arraySliceCount == -1 )
                    arraySliceCount = descTex1D.ArraySize - arraySliceMin;

                assert( mipSliceMin >= 0 && (UINT)mipSliceMin < descTex1D.MipLevels );
                assert( arraySliceMin >= 0 && (UINT)arraySliceMin < descTex1D.ArraySize );
                assert( arraySliceMin + arraySliceCount > 0 && (UINT)arraySliceMin + arraySliceCount <= descTex1D.ArraySize );

                dimension = ( descTex1D.ArraySize == 1 ) ? ( D3D11_UAV_DIMENSION_TEXTURE1D ) : ( D3D11_UAV_DIMENSION_TEXTURE1DARRAY );
                desc = CD3D11_UNORDERED_ACCESS_VIEW_DESC( texture1D, dimension, format );

                if( dimension == D3D11_UAV_DIMENSION_TEXTURE1D )
                {
                    desc.Texture1D.MipSlice = mipSliceMin;
                }
                else if( dimension == D3D11_UAV_DIMENSION_TEXTURE1DARRAY )
                {
                    desc.Texture1DArray.MipSlice        = mipSliceMin;
                    desc.Texture1DArray.FirstArraySlice = arraySliceMin;
                    desc.Texture1DArray.ArraySize       = arraySliceCount;
                } 
                else { assert( false ); }

                descInitialized = true;
                SAFE_RELEASE( texture1D );
            }
        }
    }

    if( !descInitialized )
    {
        assert( false ); // resource not recognized; additional code might be needed above
        return NULL;
    }

    ID3D11Device * device = nullptr;
    resource->GetDevice(&device);
    device->Release(); // yeah, ugly - we know at minimum texture will guarantee device persistence but still...
    if( SUCCEEDED( device->CreateUnorderedAccessView( resource, &desc, &ret ) ) )
    {
        return ret;
    }
    else
    {
        assert( false );
        return NULL;
    }
}

int vaDirectXTools11::CalcApproxTextureSizeInMemory( DXGI_FORMAT format, int width, int height, int mipCount )
{
    // does this work?
    assert( false );

    int pixels = 0;
    int tw = width, th = height;
    for( int i = 0; i < mipCount; i++ )
    {
        pixels += tw * th;
        tw /= 2; th /= 2;
    }

    return pixels * (int)DirectX::BitsPerPixel( format );
}

ID3D11Resource * vaDirectXTools11::LoadTextureDDS( ID3D11Device * device, void * dataBuffer, int64 dataSize, vaTextureLoadFlags loadFlags, vaResourceBindSupportFlags bindFlags, uint64 * outCRC )
{
    outCRC; // unreferenced, not implemented
    assert( outCRC == nullptr ); // not implemented

    ID3D11Resource * texture = NULL;

    UINT dxBindFlags = BindFlagsDX11FromVA( bindFlags );

    // not actually used, needed internally for mipmap generation
    ID3D11ShaderResourceView * textureSRV = NULL;

    bool forceSRGB = (loadFlags & vaTextureLoadFlags::PresumeDataIsSRGB) != 0;
    if( (loadFlags & vaTextureLoadFlags::PresumeDataIsSRGB) != 0 )
    {
        assert( (loadFlags & vaTextureLoadFlags::PresumeDataIsLinear) == 0 );   // both at the same time don't make sense
    }
    if( (loadFlags & vaTextureLoadFlags::PresumeDataIsLinear) != 0 )
    {
        assert( (loadFlags & vaTextureLoadFlags::PresumeDataIsSRGB) == 0 );   // both at the same time don't make sense
    }
    bool dontAutogenerateMIPs = (loadFlags & vaTextureLoadFlags::AutogenerateMIPsIfMissing) == 0;

    HRESULT hr;
    if( dontAutogenerateMIPs )
    {
        hr = DirectX::CreateDDSTextureFromMemoryEx( device, (const uint8_t *)dataBuffer, (size_t)dataSize, 0, D3D11_USAGE_DEFAULT, dxBindFlags, 0, 0, forceSRGB, &texture, &textureSRV, NULL );
    }
    else
    {
        assert( vaThreading::IsMainThread() );
        ID3D11DeviceContext * immediateContext = nullptr;
        device->GetImmediateContext( &immediateContext );
        hr = DirectX::CreateDDSTextureFromMemoryEx( device, immediateContext, (const uint8_t *)dataBuffer, (size_t)dataSize, 0, D3D11_USAGE_DEFAULT, dxBindFlags, 0, 0, forceSRGB, &texture, &textureSRV, NULL );
        immediateContext->Release();
    }

    if( SUCCEEDED( hr ) )
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC desc;
        textureSRV->GetDesc( &desc );
        if( (loadFlags & vaTextureLoadFlags::PresumeDataIsSRGB) != 0 )
        {
            // wanted sRGB but didn't get it? there's something wrong
            assert( DirectX::IsSRGB( desc.Format ) );
        }
        if( (loadFlags & vaTextureLoadFlags::PresumeDataIsLinear) != 0 )
        {
            // there is no support for this at the moment in DirectX tools so asserting if the result is not as requested; fix in the future
            assert( !DirectX::IsSRGB( desc.Format ) );
        }
        if( (loadFlags & vaTextureLoadFlags::AutogenerateMIPsIfMissing) != 0 )
        {
            // check for mips here maybe?
            // assert( desc. );
        }

        SAFE_RELEASE( textureSRV );

        return texture;
    }
    else
    {
        SAFE_RELEASE( texture );
        SAFE_RELEASE( textureSRV );
        assert( false ); // check hr
        throw "Error creating the texture from the stream";
    }
}

ID3D11Resource * vaDirectXTools11::LoadTextureDDS( ID3D11Device * device, const wchar_t * path, vaTextureLoadFlags loadFlags, vaResourceBindSupportFlags bindFlags, uint64 * outCRC )
{
    outCRC; // unreferenced, not implemented
    assert( outCRC == nullptr ); // not implemented

    auto buffer = vaFileTools::LoadFileToMemoryStream( path );

    if( buffer == nullptr )
        return nullptr;

    return LoadTextureDDS( device, buffer->GetBuffer( ), buffer->GetLength( ), loadFlags, bindFlags, outCRC );
}

ID3D11Resource * vaDirectXTools11::LoadTextureWIC( ID3D11Device * device, void * dataBuffer, int64 dataSize, vaTextureLoadFlags loadFlags, vaResourceBindSupportFlags bindFlags, uint64 * outCRC )
{
    outCRC; // unreferenced, not implemented
    assert( outCRC == nullptr ); // not implemented

    ID3D11DeviceContext * immediateContext = nullptr;
    device->GetImmediateContext( &immediateContext );
    immediateContext->Release(); // yeah, ugly - we know device will guarantee immediate context persistence but still...

    ID3D11Resource * texture = NULL;

    UINT dxBindFlags = BindFlagsDX11FromVA( bindFlags );

    // not actually used, needed internally for mipmap generation
    ID3D11ShaderResourceView * textureSRV = NULL;

    DirectX::WIC_LOADER_FLAGS wicLoaderFlags = DirectX::WIC_LOADER_DEFAULT;
    if( (loadFlags & vaTextureLoadFlags::PresumeDataIsSRGB) != 0 )
    {
        assert( (loadFlags & vaTextureLoadFlags::PresumeDataIsLinear) == 0 );   // both at the same time don't make sense
        wicLoaderFlags = (DirectX::WIC_LOADER_FLAGS)(wicLoaderFlags | DirectX::WIC_LOADER_FORCE_SRGB);
    }
    if( (loadFlags & vaTextureLoadFlags::PresumeDataIsLinear) != 0 )
    {
        assert( (loadFlags & vaTextureLoadFlags::PresumeDataIsSRGB) == 0 );   // both at the same time don't make sense
        wicLoaderFlags = (DirectX::WIC_LOADER_FLAGS)(wicLoaderFlags | DirectX::WIC_LOADER_IGNORE_SRGB);
    }
    bool dontAutogenerateMIPs = (loadFlags & vaTextureLoadFlags::AutogenerateMIPsIfMissing) == 0;

    HRESULT hr;

    if( dontAutogenerateMIPs )
    {
        hr = DirectX::CreateWICTextureFromMemoryEx( device, (const uint8_t *)dataBuffer, (size_t)dataSize, 0, D3D11_USAGE_DEFAULT, dxBindFlags, 0, 0, wicLoaderFlags, &texture, &textureSRV );
    }
    else
    {
        hr = DirectX::CreateWICTextureFromMemoryEx( device, immediateContext, (const uint8_t *)dataBuffer, (size_t)dataSize, 0, D3D11_USAGE_DEFAULT, dxBindFlags, 0, 0, wicLoaderFlags, &texture, &textureSRV );
    }

    if( SUCCEEDED( hr ) )
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC desc;
        textureSRV->GetDesc( &desc );
        if( (loadFlags & vaTextureLoadFlags::PresumeDataIsSRGB) != 0 )
        {
            // wanted sRGB but didn't get it? there's something wrong
            assert( DirectX::IsSRGB( desc.Format ) );
        }
        if( (loadFlags & vaTextureLoadFlags::PresumeDataIsLinear) != 0 )
        {
            // there is no support for this at the moment in DirectX tools so asserting if the result is not as requested; fix in the future
            assert( !DirectX::IsSRGB( desc.Format ) );
        }

        SAFE_RELEASE( textureSRV );
        return texture;
    }
    else
    {
        SAFE_RELEASE( texture );
        SAFE_RELEASE( textureSRV );
        VA_LOG_ERROR_STACKINFO( L"Error loading texture" );
        return nullptr;
    }
}

ID3D11Resource * vaDirectXTools11::LoadTextureWIC( ID3D11Device * device, const wchar_t * path, vaTextureLoadFlags loadFlags, vaResourceBindSupportFlags bindFlags, uint64 * outCRC )
{
#if 1 // load to memory, read from buffer - avoid code duplication
    outCRC; // unreferenced, not implemented
    assert( outCRC == nullptr ); // not implemented

    auto buffer = vaFileTools::LoadFileToMemoryStream( path );

    if( buffer == nullptr )
        return nullptr;

    return LoadTextureWIC( device, buffer->GetBuffer( ), buffer->GetLength( ), loadFlags, bindFlags, outCRC );
#else // old one
    outCRC; // unreferenced, not implemented
    assert( outCRC == nullptr ); // not implemented

    ID3D11Resource * texture = NULL;

    UINT dxBindFlags = BindFlagsDX11FromVA( bindFlags );

    // not actually used, needed internally for mipmap generation
    ID3D11ShaderResourceView * textureSRV = NULL;

    DirectX::WIC_LOADER_FLAGS wicLoaderFlags = DirectX::WIC_LOADER_DEFAULT;
    if( (loadFlags & vaTextureLoadFlags::PresumeDataIsSRGB) != 0 )
    {
        assert( (loadFlags & vaTextureLoadFlags::PresumeDataIsLinear) != 0 );   // both at the same time don't make sense
        wicLoaderFlags = (DirectX::WIC_LOADER_FLAGS)(wicLoaderFlags | DirectX::WIC_LOADER_FORCE_SRGB);
    }
    if( (loadFlags & vaTextureLoadFlags::PresumeDataIsLinear) != 0 )
    {
        assert( (loadFlags & vaTextureLoadFlags::PresumeDataIsSRGB) != 0 );   // both at the same time don't make sense
        wicLoaderFlags = (DirectX::WIC_LOADER_FLAGS)(wicLoaderFlags | DirectX::WIC_LOADER_IGNORE_SRGB);
    }
    bool dontAutogenerateMIPs = (loadFlags & vaTextureLoadFlags::AutogenerateMIPsIfMissing) == 0;

    HRESULT hr;

    if( dontAutogenerateMIPs )
    {
        hr = DirectX::CreateWICTextureFromFileEx( vaDirectXCore::GetDevice( ), path, 0, D3D11_USAGE_DEFAULT, dxBindFlags, 0, 0, wicLoaderFlags, &texture, &textureSRV );
    }
    else
    {
        hr = DirectX::CreateWICTextureFromFileEx( vaDirectXCore::GetDevice( ), vaDirectXCore::GetImmediateContext( ), path, 0, D3D11_USAGE_DEFAULT, dxBindFlags, 0, 0, wicLoaderFlags, &texture, &textureSRV );
    }

    if( SUCCEEDED( hr ) )
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC desc;
        textureSRV->GetDesc( &desc );
        if( (loadFlags & vaTextureLoadFlags::PresumeDataIsSRGB) != 0 )
        {
            // wanted sRGB but didn't get it? there's something wrong
            assert( DirectX::IsSRGB( desc.Format ) );
        }
        if( (loadFlags & vaTextureLoadFlags::PresumeDataIsLinear) != 0 )
        {
            // there is no support for this at the moment in DirectX tools so asserting if the result is not as requested; fix in the future
            assert( !DirectX::IsSRGB( desc.Format ) );
        }

        SAFE_RELEASE( textureSRV );
        return texture;
    }
    else
    {
        SAFE_RELEASE( texture );
        SAFE_RELEASE( textureSRV );
        VA_LOG_ERROR_STACKINFO( L"Error loading texture '%s'", path );
        return nullptr;
    }
#endif
}

//void vaDirectXTools11::CopyDepthStencil( ID3D11DeviceContext * destContext, ID3D11DepthStencilView * dsvSrc )
//{
//    ID3D11DepthStencilView * dsvDest;
//    destContext->OMGetRenderTargets( 0, NULL, &dsvDest );
//    ID3D11Resource * resSrc;
//    ID3D11Resource * resDest;
//    dsvSrc->GetResource( &resSrc );
//    dsvDest->GetResource( &resDest );
//    destContext->CopyResource( resDest, resSrc );
//    SAFE_RELEASE( dsvDest );
//}
//
//void vaDirectXTools11::ClearColorDepthStencil( ID3D11DeviceContext * destContext, bool clearAllColorRTs, bool clearDepth, bool clearStencil, const vaVector4 & clearColor, float depth, uint8 stencil )
//{
//    float clearColorF[4] = { clearColor.x, clearColor.y, clearColor.z, clearColor.w };
//    ID3D11RenderTargetView * RTVs[4];
//    ID3D11DepthStencilView * DSV;
//    destContext->OMGetRenderTargets( _countof( RTVs ), RTVs, &DSV );
//
//    for( int i = 0; i < _countof( RTVs ); i++ )
//    {
//        if( RTVs[i] == NULL ) continue;
//        if( clearAllColorRTs )
//            destContext->ClearRenderTargetView( RTVs[i], clearColorF );
//        SAFE_RELEASE( RTVs[i] );
//    }
//    if( clearDepth || clearStencil )
//    {
//        UINT flags = 0;
//        if( clearDepth )     flags |= D3D11_CLEAR_DEPTH;
//        if( clearStencil )   flags |= D3D11_CLEAR_STENCIL;
//        destContext->ClearDepthStencilView( DSV, flags, depth, stencil );
//    }
//    SAFE_RELEASE( DSV );
//}


//--------------------------------------------------------------------------------------
bool vaDirectXTools11::SaveDDSTexture( ID3D11Device * device, VertexAsylum::vaStream & outStream, ID3D11Resource* pSource )
{
    //fix this properly using 
//    HRESULT hr = DirectX::SaveDDSTextureToFile( vaDirectXCore::GetImmediateContext(), pSource, outStream );
//    return SUCCEEDED( hr );

    DirectX::ScratchImage scratchImage;

    ID3D11DeviceContext * immediateContext = nullptr;
    device->GetImmediateContext( &immediateContext );
    immediateContext->Release(); // yeah, ugly - we know device will guarantee immediate context persistence but still...

    HRESULT hr = DirectX::CaptureTexture( device, immediateContext, pSource, scratchImage );
    if( FAILED( hr ) )
    {
        assert( false );
        return false;
    }

    DirectX::Blob blob;

    hr = DirectX::SaveToDDSMemory( scratchImage.GetImages( ), scratchImage.GetImageCount( ), scratchImage.GetMetadata( ), DirectX::DDS_FLAGS_NONE, blob );
    if( FAILED( hr ) )
    {
        assert( false );
        return false;
    }

    return outStream.Write( blob.GetBufferPointer(), blob.GetBufferSize() );
}

////////////////////////////////////////////////////////////////////////////////

bool vaDirectXTools12::FillShaderResourceViewDesc( D3D12_SHADER_RESOURCE_VIEW_DESC & outDesc, ID3D12Resource * resource, DXGI_FORMAT format, int mipSliceMin, int mipSliceCount, int arraySliceMin, int arraySliceCount, bool isCubemap )
{
    assert( mipSliceMin >= 0 );
    assert( arraySliceMin >= 0 );
    assert( arraySliceCount >= -1 );    // -1 means all

    D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();

    outDesc.Format                  = (format == DXGI_FORMAT_UNKNOWN)?(resourceDesc.Format):(format);
    outDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    if( resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D )
    {
        if( !isCubemap )
        {
            if( mipSliceCount == -1 )
                mipSliceCount = resourceDesc.MipLevels-mipSliceMin;
            if( arraySliceCount == -1 )
                arraySliceCount = resourceDesc.DepthOrArraySize-arraySliceMin;

            assert( mipSliceMin >= 0 && (UINT)mipSliceMin < resourceDesc.MipLevels );
            assert( mipSliceMin+mipSliceCount > 0 && (UINT)mipSliceMin+mipSliceCount <= resourceDesc.MipLevels );
            assert( arraySliceMin >= 0 && (UINT)arraySliceMin < resourceDesc.DepthOrArraySize );
            assert( arraySliceMin+arraySliceCount > 0 && (UINT)arraySliceMin+arraySliceCount <= resourceDesc.DepthOrArraySize );

            if( resourceDesc.SampleDesc.Count > 1 )
                outDesc.ViewDimension = ( resourceDesc.DepthOrArraySize == 1 ) ? ( D3D12_SRV_DIMENSION_TEXTURE2DMS ) : ( D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY );
            else
                outDesc.ViewDimension = ( resourceDesc.DepthOrArraySize == 1 ) ? ( D3D12_SRV_DIMENSION_TEXTURE2D ) : ( D3D12_SRV_DIMENSION_TEXTURE2DARRAY );

            if( outDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2D )
            {
                outDesc.Texture2D.MostDetailedMip       = mipSliceMin;
                outDesc.Texture2D.MipLevels             = mipSliceCount;
                outDesc.Texture2D.PlaneSlice            = 0;
                outDesc.Texture2D.ResourceMinLODClamp   = 0.0f;
                assert( arraySliceMin == 0 );
                assert( arraySliceCount == 1 );
            }
            else if( outDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DARRAY )
            {
                outDesc.Texture2DArray.MostDetailedMip      = mipSliceMin;
                outDesc.Texture2DArray.MipLevels            = mipSliceCount;
                outDesc.Texture2DArray.FirstArraySlice      = arraySliceMin;
                outDesc.Texture2DArray.ArraySize            = arraySliceCount;
                outDesc.Texture2DArray.PlaneSlice           = 0;
                outDesc.Texture2DArray.ResourceMinLODClamp  = 0.0f;
            }
            else if( outDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DMS )
            {
                outDesc.Texture2DMS.UnusedField_NothingToDefine = 42;
                assert( mipSliceMin == 0 );
                assert( mipSliceCount == 1 );
                assert( arraySliceMin == 0 );
                assert( arraySliceCount == 1 );
            }
            else if( outDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY )
            {
                assert( mipSliceMin == 0 );
                assert( arraySliceCount == 1 );
                outDesc.Texture2DMSArray.FirstArraySlice    = arraySliceMin;
                outDesc.Texture2DMSArray.ArraySize          = arraySliceCount;
            }
            else { assert( false ); }
        }
        else // is a cubemap
        {
            if( mipSliceCount == -1 )
                mipSliceCount = resourceDesc.MipLevels - mipSliceMin;
            if( arraySliceCount == -1 )
                arraySliceCount = resourceDesc.DepthOrArraySize - arraySliceMin;

            assert( mipSliceMin >= 0 && (UINT)mipSliceMin < resourceDesc.MipLevels );
            assert( mipSliceMin + mipSliceCount > 0 && (UINT)mipSliceMin + mipSliceCount <= resourceDesc.MipLevels );
            assert( arraySliceMin >= 0 && (UINT)arraySliceMin < resourceDesc.DepthOrArraySize );
            assert( arraySliceMin + arraySliceCount > 0 && (UINT)arraySliceMin + arraySliceCount <= resourceDesc.DepthOrArraySize );

            outDesc.ViewDimension = (resourceDesc.DepthOrArraySize==6)?(D3D12_SRV_DIMENSION_TEXTURECUBE):(D3D12_SRV_DIMENSION_TEXTURECUBEARRAY);
            assert( resourceDesc.DepthOrArraySize % 6 == 0 );

            if( outDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURECUBE )
            {
                outDesc.TextureCube.MostDetailedMip     = mipSliceMin;
                outDesc.TextureCube.MipLevels           = mipSliceCount;
                outDesc.TextureCube.ResourceMinLODClamp = 0.0f;
                assert( arraySliceMin == 0 );
                assert( arraySliceCount == 6 );
            }
            else if( outDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURECUBEARRAY )
            {
                outDesc.TextureCubeArray.MostDetailedMip    = mipSliceMin;
                outDesc.TextureCubeArray.MipLevels          = mipSliceCount;
                outDesc.TextureCubeArray.First2DArrayFace   = arraySliceMin/6;
                outDesc.TextureCubeArray.NumCubes           = arraySliceCount/6;
                outDesc.TextureCubeArray.ResourceMinLODClamp= 0.0f;
            }
            else { assert(false); }
        }
        return true;
    }
    else if( resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D )
    {
        assert( false ); // codepath not tested
        assert( !isCubemap );   // can't be 3D cubemap

        if( mipSliceCount == -1 )
            mipSliceCount = resourceDesc.MipLevels - mipSliceMin;
        assert( mipSliceMin >= 0 && (UINT)mipSliceMin < resourceDesc.MipLevels );
        assert( mipSliceMin + mipSliceCount > 0 && (UINT)mipSliceMin + mipSliceCount <= resourceDesc.MipLevels );

        // no array slices for 3D textures
        assert( arraySliceMin == 0 );
        assert( arraySliceCount == 1 || arraySliceCount == -1 );

        outDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;

        outDesc.Texture3D.MostDetailedMip       = mipSliceMin;
        outDesc.Texture3D.MipLevels             = mipSliceCount;
        outDesc.Texture3D.ResourceMinLODClamp   = 0.0f;
        return true;
    }
    else if( resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D )
    {
        assert( false ); // codepath not tested
        assert( !isCubemap );   // can't be 1D cubemap

        if( mipSliceCount == -1 )
            mipSliceCount = resourceDesc.MipLevels - mipSliceMin;
        if( arraySliceCount == -1 )
            arraySliceCount = resourceDesc.DepthOrArraySize - arraySliceMin;

        assert( mipSliceMin >= 0 && (UINT)mipSliceMin < resourceDesc.MipLevels );
        assert( mipSliceMin + mipSliceCount > 0 && (UINT)mipSliceMin + mipSliceCount <= resourceDesc.MipLevels );
        assert( arraySliceMin >= 0 && (UINT)arraySliceMin < resourceDesc.DepthOrArraySize );
        assert( arraySliceMin + arraySliceCount > 0 && (UINT)arraySliceMin + arraySliceCount <= resourceDesc.DepthOrArraySize );

        outDesc.ViewDimension = ( resourceDesc.DepthOrArraySize == 1 ) ? ( D3D12_SRV_DIMENSION_TEXTURE1D ) : ( D3D12_SRV_DIMENSION_TEXTURE1DARRAY );

        if( outDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE1D )
        {
            outDesc.Texture1D.MostDetailedMip       = mipSliceMin;
            outDesc.Texture1D.MipLevels             = mipSliceCount;
            outDesc.Texture1D.ResourceMinLODClamp   = 0.0f;
        }
        else if( outDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE1DARRAY )
        {
            outDesc.Texture1DArray.MostDetailedMip      = mipSliceMin;
            outDesc.Texture1DArray.MipLevels            = mipSliceCount;
            outDesc.Texture1DArray.FirstArraySlice      = arraySliceMin;
            outDesc.Texture1DArray.ArraySize            = arraySliceCount;
            outDesc.Texture1DArray.ResourceMinLODClamp  = 0.0f;
        } else { assert( false ); }
        return true;
    }
    else if( resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER )
    {
        assert( false ); // not intended for buffers
        return false;
        //outDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        //outDesc.Buffer.
    }
    else
    {
        assert( false ); // resource not recognized; additional code might be needed above
        return false;
    }
}

bool vaDirectXTools12::FillDepthStencilViewDesc( D3D12_DEPTH_STENCIL_VIEW_DESC & outDesc, ID3D12Resource * resource, DXGI_FORMAT format, int mipSliceMin, int arraySliceMin, int arraySliceCount )
{
    assert( mipSliceMin >= 0 );
    assert( arraySliceMin >= 0 );
    assert( arraySliceCount >= -1 );    // -1 means all

    D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();
    outDesc.Format  = (format == DXGI_FORMAT_UNKNOWN)?(resourceDesc.Format):(format);
    outDesc.Flags   = D3D12_DSV_FLAG_NONE;  // D3D12_DSV_FLAG_READ_ONLY_DEPTH / D3D12_DSV_FLAG_READ_ONLY_STENCIL

    if( resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D )
    {
        if( arraySliceCount == -1 )
            arraySliceCount = resourceDesc.DepthOrArraySize - arraySliceMin;

        assert( mipSliceMin >= 0 && (UINT)mipSliceMin < resourceDesc.MipLevels );
        assert( arraySliceMin >= 0 && (UINT)arraySliceMin < resourceDesc.DepthOrArraySize );
        assert( arraySliceMin + arraySliceCount > 0 && (UINT)arraySliceMin + arraySliceCount <= resourceDesc.DepthOrArraySize );

        if( resourceDesc.SampleDesc.Count > 1 )
            outDesc.ViewDimension = ( resourceDesc.DepthOrArraySize == 1 ) ? ( D3D12_DSV_DIMENSION_TEXTURE2DMS ) : ( D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY );
        else
            outDesc.ViewDimension = ( resourceDesc.DepthOrArraySize == 1 ) ? ( D3D12_DSV_DIMENSION_TEXTURE2D ) : ( D3D12_DSV_DIMENSION_TEXTURE2DARRAY );

        if( outDesc.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2D )
        {
            outDesc.Texture2D.MipSlice             = mipSliceMin;
            assert( arraySliceMin == 0 );
        }
        else if( outDesc.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2DARRAY )
        {
            outDesc.Texture2DArray.MipSlice        = mipSliceMin;
            outDesc.Texture2DArray.FirstArraySlice = arraySliceMin;
            outDesc.Texture2DArray.ArraySize       = arraySliceCount;
        }
        else if( outDesc.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2DMS )
        {
            outDesc.Texture2DMS.UnusedField_NothingToDefine = 42;
            assert( mipSliceMin == 0 );
            assert( arraySliceMin == 0 );
        }
        else if( outDesc.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY )
        {
            outDesc.Texture2DMSArray.FirstArraySlice = arraySliceMin;
            outDesc.Texture2DMSArray.ArraySize       = arraySliceCount;
            assert( mipSliceMin == 0 );
        }
        else { assert( false ); return false; }

        return true;
    }
    
    assert( false );    // not implemented / supported
    return false;
}

bool vaDirectXTools12::FillRenderTargetViewDesc( D3D12_RENDER_TARGET_VIEW_DESC & outDesc, ID3D12Resource * resource, DXGI_FORMAT format, int mipSliceMin, int arraySliceMin, int arraySliceCount )
{
    assert( mipSliceMin >= 0 );
    assert( arraySliceMin >= 0 );
    assert( arraySliceCount >= -1 );    // -1 means all

    D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();
    outDesc.Format  = (format == DXGI_FORMAT_UNKNOWN)?(resourceDesc.Format):(format);
        
    if( resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D )
    {
        if( arraySliceCount == -1 )
            arraySliceCount = resourceDesc.DepthOrArraySize - arraySliceMin;

        assert( mipSliceMin >= 0 && (UINT)mipSliceMin < resourceDesc.MipLevels );
        assert( arraySliceMin >= 0 && (UINT)arraySliceMin < resourceDesc.DepthOrArraySize );
        assert( arraySliceMin + arraySliceCount > 0 && (UINT)arraySliceMin + arraySliceCount <= resourceDesc.DepthOrArraySize );

        if( resourceDesc.SampleDesc.Count > 1 )
            outDesc.ViewDimension = ( resourceDesc.DepthOrArraySize == 1 ) ? ( D3D12_RTV_DIMENSION_TEXTURE2DMS ) : ( D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY );
        else
            outDesc.ViewDimension = ( resourceDesc.DepthOrArraySize == 1 ) ? ( D3D12_RTV_DIMENSION_TEXTURE2D ) : ( D3D12_RTV_DIMENSION_TEXTURE2DARRAY );

        if( outDesc.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2D )
        {
            outDesc.Texture2D.MipSlice      = mipSliceMin;
            outDesc.Texture2D.PlaneSlice    = 0;
            assert( arraySliceMin == 0 );
        }
        else if( outDesc.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2DARRAY )
        {
            outDesc.Texture2DArray.MipSlice         = mipSliceMin;
            outDesc.Texture2DArray.FirstArraySlice  = arraySliceMin;
            outDesc.Texture2DArray.ArraySize        = arraySliceCount;
            outDesc.Texture2DArray.PlaneSlice       = 0;
        }
        else if( outDesc.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2DMS )
        {
            outDesc.Texture2DMS.UnusedField_NothingToDefine = 42;
            assert( mipSliceMin == 0 );
            assert( arraySliceMin == 0 );
        }
        else if( outDesc.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY )
        {
            outDesc.Texture2DMSArray.FirstArraySlice = arraySliceMin;
            outDesc.Texture2DMSArray.ArraySize       = arraySliceCount;
            assert( mipSliceMin == 0 );
        }
        else { assert( false ); return false; }

        return true;
    }
    
    assert( false );    // not implemented / supported
    return false;
}

bool vaDirectXTools12::FillUnorderedAccessViewDesc( D3D12_UNORDERED_ACCESS_VIEW_DESC & outDesc, ID3D12Resource * resource, DXGI_FORMAT format, int mipSliceMin, int arraySliceMin, int arraySliceCount )
{
    assert( mipSliceMin >= 0 );
    assert( arraySliceMin >= 0 );
    assert( arraySliceCount >= -1 );    // -1 means all

    D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();
    outDesc.Format  = (format == DXGI_FORMAT_UNKNOWN)?(resourceDesc.Format):(format);
        
    if( resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D )
    {
        if( arraySliceCount == -1 )
            arraySliceCount = resourceDesc.DepthOrArraySize - arraySliceMin;

        assert( mipSliceMin >= 0 && (UINT)mipSliceMin < resourceDesc.MipLevels );
        assert( arraySliceMin >= 0 && (UINT)arraySliceMin < resourceDesc.DepthOrArraySize );
        assert( arraySliceMin + arraySliceCount > 0 && (UINT)arraySliceMin + arraySliceCount <= resourceDesc.DepthOrArraySize );

        outDesc.ViewDimension = ( resourceDesc.DepthOrArraySize == 1 ) ? ( D3D12_UAV_DIMENSION_TEXTURE2D ) : ( D3D12_UAV_DIMENSION_TEXTURE2DARRAY );

        if( outDesc.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE2D )
        {
            outDesc.Texture2D.MipSlice              = mipSliceMin;
            outDesc.Texture2D.PlaneSlice            = 0;
            assert( arraySliceMin == 0 );
        }
        else if( outDesc.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE2DARRAY )
        {
            outDesc.Texture2DArray.MipSlice         = mipSliceMin;
            outDesc.Texture2DArray.FirstArraySlice  = arraySliceMin;
            outDesc.Texture2DArray.ArraySize        = arraySliceCount;
            outDesc.Texture2DArray.PlaneSlice       = 0;
        }
        else { assert( false ); }
        return true;
    } 
    else if( resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D )
    {
        assert( false ); // codepath not tested
        if( arraySliceCount == -1 )
            arraySliceCount = resourceDesc.DepthOrArraySize - arraySliceMin;

        assert( mipSliceMin >= 0 && (UINT)mipSliceMin < resourceDesc.MipLevels );
        assert( arraySliceMin >= 0 && (UINT)arraySliceMin < resourceDesc.DepthOrArraySize );
        assert( arraySliceMin + arraySliceCount > 0 && (UINT)arraySliceMin + arraySliceCount <= resourceDesc.DepthOrArraySize );

        outDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
            
        outDesc.Texture3D.MipSlice     = mipSliceMin;
        outDesc.Texture3D.FirstWSlice  = arraySliceMin;
        outDesc.Texture3D.WSize        = arraySliceCount;
        return true;
    }
    else if( resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D )
    {
        if( arraySliceCount == -1 )
            arraySliceCount = resourceDesc.DepthOrArraySize - arraySliceMin;

        assert( mipSliceMin >= 0 && (UINT)mipSliceMin < resourceDesc.MipLevels );
        assert( arraySliceMin >= 0 && (UINT)arraySliceMin < resourceDesc.DepthOrArraySize );
        assert( arraySliceMin + arraySliceCount > 0 && (UINT)arraySliceMin + arraySliceCount <= resourceDesc.DepthOrArraySize );

        outDesc.ViewDimension = ( resourceDesc.DepthOrArraySize == 1 ) ? ( D3D12_UAV_DIMENSION_TEXTURE1D ) : ( D3D12_UAV_DIMENSION_TEXTURE1DARRAY );

        if( outDesc.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE1D )
        {
            outDesc.Texture1D.MipSlice = mipSliceMin;
        }
        else if( outDesc.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE1DARRAY )
        {
            outDesc.Texture1DArray.MipSlice        = mipSliceMin;
            outDesc.Texture1DArray.FirstArraySlice = arraySliceMin;
            outDesc.Texture1DArray.ArraySize       = arraySliceCount;
        } 
        else { assert( false ); return false; }
        return true;
    }

    assert( false ); // resource not recognized; additional code might be needed above
    return false;
}

////////////////////////////////////////////////////////////////////////////////

ID3D11BlendState * VertexAsylum::DX11BlendModeFromVABlendMode( vaBlendMode blendMode )
{
    ID3D11BlendState * blendState = vaDirectXTools11::GetBS_Opaque( );
    switch( blendMode )
    {
    case vaBlendMode::Opaque:               blendState = vaDirectXTools11::GetBS_Opaque( );            break;
    case vaBlendMode::Additive:             blendState = vaDirectXTools11::GetBS_Additive( );          break;
    case vaBlendMode::AlphaBlend:           blendState = vaDirectXTools11::GetBS_AlphaBlend( );        break;
    case vaBlendMode::PremultAlphaBlend:    blendState = vaDirectXTools11::GetBS_PremultAlphaBlend( ); break;
    case vaBlendMode::Mult:                 blendState = vaDirectXTools11::GetBS_Mult( );              break;
    default: assert( false );
    }
    return blendState;
}

void vaResourceStateTransitionHelperDX12::RSTHTransitionSubResUnroll( vaRenderDeviceContextDX12 & context )
{
    // unroll all subres transitions because they are evil
    for( auto subRes : m_rsthSubResStates )
    {
        if( subRes.second != m_rsthCurrent )
            context.GetCommandList( )->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition( m_rsthResource.Get(), subRes.second, m_rsthCurrent, subRes.first ) );
    }
    m_rsthSubResStates.clear();
}

void vaResourceStateTransitionHelperDX12::RSTHTransition( vaRenderDeviceContextDX12 & context, D3D12_RESOURCE_STATES target, uint32 subResIndex )
{
    assert( vaThreading::IsMainThread() );
    assert( AsDX12( context.GetRenderDevice() ).GetMainContext() == &context ); // we must be the main context for now
    assert( m_rsthResource != nullptr );
    if( subResIndex != -1 )
    {
        RSTHTransitionSubRes( context, target, subResIndex );
        return;
    }

    if( m_rsthSubResStates.size() > 0 )
        RSTHTransitionSubResUnroll( context );

    if( m_rsthCurrent == target )
        return;

    auto trans = CD3DX12_RESOURCE_BARRIER::Transition( m_rsthResource.Get(), m_rsthCurrent, target );
    context.GetCommandList( )->ResourceBarrier(1, &trans );
    
    m_rsthCurrent = target;
}

void vaResourceStateTransitionHelperDX12::RSTHTransitionSubRes( vaRenderDeviceContextDX12 & context, D3D12_RESOURCE_STATES target, uint32 subResIndex )
{
    // if already in, just transition that one
    for( auto it = m_rsthSubResStates.begin(); it < m_rsthSubResStates.end(); it++ )
    {
        auto & subRes = *it;
        if( subRes.first == subResIndex )
        {
            if( target != subRes.second )
                context.GetCommandList( )->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition( m_rsthResource.Get(), subRes.second, target, subResIndex ) );
            subRes.second = target;
            if( target == m_rsthCurrent )
                m_rsthSubResStates.erase( it );
            return;
        }
    }
    if( target == m_rsthCurrent )
        return;
    m_rsthSubResStates.push_back( make_pair( subResIndex, target ) ) ;
    context.GetCommandList( )->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition( m_rsthResource.Get(), m_rsthCurrent, target, subResIndex ) );
}

void vaResourceStateTransitionHelperDX12::RSTHAdoptResourceState( vaRenderDeviceContextDX12 & context, D3D12_RESOURCE_STATES target, uint32 subResIndex )
{
    assert( vaThreading::IsMainThread() );
    assert( AsDX12( context.GetRenderDevice() ).GetMainContext() == &context ); // we must be the main context for now
    assert( m_rsthResource != nullptr );
    context;
    if( subResIndex != -1 )
    {
        assert( false ); // not implemented/tested for subresources
        return;
    }
    if( m_rsthSubResStates.size() > 0 )
    {
        assert( false ); // not implemented/tested for subresources
        m_rsthSubResStates.clear();
    }

    if( m_rsthCurrent == target )
        return;

    m_rsthCurrent = target;
}

vaResourceViewDX12::~vaResourceViewDX12( ) 
{ 
    SafeRelease( );
}

void vaResourceViewDX12::Allocate( )   
{ 
    assert( !IsCreated() ); 
    m_device.AllocatePersistentResourceView( m_type, m_heapIndex, m_CPUHandle, m_GPUHandle );
    assert( IsCreated() ); 
}

void vaResourceViewDX12::SafeRelease( )
{
    if( !IsCreated() )
        return;
    auto type = m_type;
    int heapIndex = m_heapIndex;

    if( m_GPUHandle.ptr == D3D12_GPU_DESCRIPTOR_HANDLE{0}.ptr )
    {
        // these are now CPU-side only so we can remove them immediately 
        m_device.ReleasePersistentResourceView( type, heapIndex );
    }
    else
    {
        // let the resource be removed until we can guarantee GPU finished using it
        m_device.ExecuteAfterCurrentGPUFrameDone( 
            [type, heapIndex]( vaRenderDeviceDX12 & device )
                { 
                    device.ReleasePersistentResourceView( type, heapIndex );
                } );
    }
    m_heapIndex = -1;       // mark as destroyed
    m_CPUHandle = { 0 };    // avoid any confusion later
    m_GPUHandle = { 0 };    // avoid any confusion later
}

void vaConstantBufferViewDX12::Create( const D3D12_CONSTANT_BUFFER_VIEW_DESC & desc )
{
    Allocate();
    if( m_heapIndex >= 0 )
    {
        m_desc = desc;
        m_device.GetPlatformDevice()->CreateConstantBufferView( &desc, m_CPUHandle );
    }
    else { assert( false ); }
}

void vaConstantBufferViewDX12::CreateNull( )
{
    D3D12_CONSTANT_BUFFER_VIEW_DESC desc = { 0, 0 };
    Allocate();
    if( m_heapIndex >= 0 )
    {
        m_desc = desc;
        m_device.GetPlatformDevice()->CreateConstantBufferView( &desc, m_CPUHandle );
    }
    else { assert( false ); }
}

void vaShaderResourceViewDX12::Create( ID3D12Resource * resource, const D3D12_SHADER_RESOURCE_VIEW_DESC & desc )
{
    Allocate();
    if( m_heapIndex >= 0 )
    {
        m_desc = desc;
        m_device.GetPlatformDevice()->CreateShaderResourceView( resource, &desc, m_CPUHandle );
    }
    else { assert( false ); }
}

void vaShaderResourceViewDX12::CreateNull( )
{
    D3D12_SHADER_RESOURCE_VIEW_DESC desc = { DXGI_FORMAT_R32_FLOAT, D3D12_SRV_DIMENSION_TEXTURE1D, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING, {0, 0, 0} };
    Allocate();
    if( m_heapIndex >= 0 )
    {
        m_desc = desc;
        m_device.GetPlatformDevice()->CreateShaderResourceView( nullptr, &desc, m_CPUHandle );
    }
    else { assert( false ); }
}

void vaUnorderedAccessViewDX12::Create( ID3D12Resource *resource, ID3D12Resource * counterResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC & desc )
{
    Allocate();
    if( m_heapIndex >= 0 )
    {
        m_desc = desc;
        m_device.GetPlatformDevice()->CreateUnorderedAccessView( resource, counterResource, &desc, m_CPUHandle );
    }
    else { assert( false ); }
}

void vaUnorderedAccessViewDX12::CreateNull( )
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC desc = { DXGI_FORMAT_R32_FLOAT, D3D12_UAV_DIMENSION_TEXTURE1D, { 0 } };
    Allocate();
    if( m_heapIndex >= 0 )
    {
        m_desc = desc;
        m_device.GetPlatformDevice()->CreateUnorderedAccessView( nullptr, nullptr, &desc, m_CPUHandle );
    }
    else { assert( false ); }
}

void vaRenderTargetViewDX12::Create( ID3D12Resource *resource, const D3D12_RENDER_TARGET_VIEW_DESC & desc )
{
    Allocate();
    if( m_heapIndex >= 0 )
    {
        m_desc = desc;
        m_device.GetPlatformDevice()->CreateRenderTargetView( resource, &desc, m_CPUHandle );
    }
    else { assert( false ); }
}

void vaRenderTargetViewDX12::CreateNull( )
{
    D3D12_RENDER_TARGET_VIEW_DESC desc = { DXGI_FORMAT_R32_FLOAT, D3D12_RTV_DIMENSION_TEXTURE1D, { 0 } };
    Allocate();
    if( m_heapIndex >= 0 )
    {
        m_desc = desc;
        m_device.GetPlatformDevice()->CreateRenderTargetView( nullptr, &desc, m_CPUHandle );
    }
    else { assert( false ); }
}

void vaDepthStencilViewDX12::Create( ID3D12Resource *resource, const D3D12_DEPTH_STENCIL_VIEW_DESC & desc )
{
    Allocate();
    if( m_heapIndex >= 0 )
    {
        m_desc = desc;
        m_device.GetPlatformDevice()->CreateDepthStencilView( resource, &desc, m_CPUHandle );
    }
    else { assert( false ); }
}

void vaDepthStencilViewDX12::CreateNull( )
{
    D3D12_DEPTH_STENCIL_VIEW_DESC desc = { DXGI_FORMAT_D32_FLOAT, D3D12_DSV_DIMENSION_TEXTURE1D, D3D12_DSV_FLAG_NONE, { 0 } };
    Allocate();
    if( m_heapIndex >= 0 )
    {
        m_desc = desc;
        m_device.GetPlatformDevice()->CreateDepthStencilView( nullptr, &desc, m_CPUHandle );
    }
    else { assert( false ); }
}

void vaSamplerViewDX12::Create( const D3D12_SAMPLER_DESC & desc )
{
    assert( false ); // never tested
    Allocate();
    if( m_heapIndex >= 0 )
    {
        m_desc = desc;
        m_device.GetPlatformDevice()->CreateSampler( &desc, m_CPUHandle );
    }
    else { assert( false ); }
}

void vaSamplerViewDX12::CreateNull( )
{
    D3D12_SAMPLER_DESC desc = { D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 0.0f, 0, D3D12_COMPARISON_FUNC_NEVER, {0.0f, 0.0f, 0.0f, 0.0f}, 0.0f, 0.0f };
    Allocate();
    if( m_heapIndex >= 0 )
    {
        m_desc = desc;
        m_device.GetPlatformDevice()->CreateSampler( &desc, m_CPUHandle );
    }
    else { assert( false ); }
}


void vaDirectXTools12::FillSamplerStatePointClamp( D3D12_STATIC_SAMPLER_DESC & outDesc )
{
    outDesc = CD3DX12_STATIC_SAMPLER_DESC( );
    outDesc.Filter          = D3D12_FILTER_MIN_MAG_MIP_POINT;
    outDesc.AddressU        = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    outDesc.AddressV        = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    outDesc.AddressW        = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    outDesc.MipLODBias      = 0;
    outDesc.MaxAnisotropy   = 16;
    outDesc.ComparisonFunc  = D3D12_COMPARISON_FUNC_NEVER;
    outDesc.BorderColor     = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    outDesc.MinLOD          = 0.0f;
    outDesc.MaxLOD          = D3D12_FLOAT32_MAX;
    outDesc.ShaderRegister  = SHADERGLOBAL_POINTCLAMP_SAMPLERSLOT;
    outDesc.RegisterSpace   = 0;
    outDesc.ShaderVisibility= D3D12_SHADER_VISIBILITY_ALL;
}

void vaDirectXTools12::FillSamplerStatePointWrap( D3D12_STATIC_SAMPLER_DESC & outDesc )
{
    outDesc = CD3DX12_STATIC_SAMPLER_DESC( );
    outDesc.Filter          = D3D12_FILTER_MIN_MAG_MIP_POINT;
    outDesc.AddressU        = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    outDesc.AddressV        = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    outDesc.AddressW        = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    outDesc.MipLODBias      = 0;
    outDesc.MaxAnisotropy   = 16;
    outDesc.ComparisonFunc  = D3D12_COMPARISON_FUNC_NEVER;
    outDesc.BorderColor     = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    outDesc.MinLOD          = 0.0f;
    outDesc.MaxLOD          = D3D12_FLOAT32_MAX;
    outDesc.ShaderRegister  = SHADERGLOBAL_POINTWRAP_SAMPLERSLOT;
    outDesc.RegisterSpace   = 0;
    outDesc.ShaderVisibility= D3D12_SHADER_VISIBILITY_ALL;
}

void vaDirectXTools12::FillSamplerStateLinearClamp( D3D12_STATIC_SAMPLER_DESC & outDesc )
{
    outDesc = CD3DX12_STATIC_SAMPLER_DESC( );
    outDesc.Filter          = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    outDesc.AddressU        = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    outDesc.AddressV        = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    outDesc.AddressW        = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    outDesc.MipLODBias      = 0;
    outDesc.MaxAnisotropy   = 16;
    outDesc.ComparisonFunc  = D3D12_COMPARISON_FUNC_NEVER;
    outDesc.BorderColor     = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    outDesc.MinLOD          = 0.0f;
    outDesc.MaxLOD          = D3D12_FLOAT32_MAX;
    outDesc.ShaderRegister  = SHADERGLOBAL_LINEARCLAMP_SAMPLERSLOT;
    outDesc.RegisterSpace   = 0;
    outDesc.ShaderVisibility= D3D12_SHADER_VISIBILITY_ALL;
}

void vaDirectXTools12::FillSamplerStateLinearWrap( D3D12_STATIC_SAMPLER_DESC & outDesc )
{
    outDesc = CD3DX12_STATIC_SAMPLER_DESC( );
    outDesc.Filter          = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    outDesc.AddressU        = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    outDesc.AddressV        = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    outDesc.AddressW        = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    outDesc.MipLODBias      = 0;
    outDesc.MaxAnisotropy   = 16;
    outDesc.ComparisonFunc  = D3D12_COMPARISON_FUNC_NEVER;
    outDesc.BorderColor     = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    outDesc.MinLOD          = 0.0f;
    outDesc.MaxLOD          = D3D12_FLOAT32_MAX;
    outDesc.ShaderRegister  = SHADERGLOBAL_LINEARWRAP_SAMPLERSLOT;
    outDesc.RegisterSpace   = 0;
    outDesc.ShaderVisibility= D3D12_SHADER_VISIBILITY_ALL;
}

void vaDirectXTools12::FillSamplerStateAnisotropicClamp( D3D12_STATIC_SAMPLER_DESC & outDesc )
{
    outDesc = CD3DX12_STATIC_SAMPLER_DESC();
    outDesc.Filter          = D3D12_FILTER_ANISOTROPIC;
    outDesc.AddressU        = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    outDesc.AddressV        = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    outDesc.AddressW        = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    outDesc.MipLODBias      = 0;
    outDesc.MaxAnisotropy   = 16;
    outDesc.ComparisonFunc  = D3D12_COMPARISON_FUNC_NEVER;
    outDesc.BorderColor     = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    outDesc.MinLOD          = 0.0f;
    outDesc.MaxLOD          = D3D12_FLOAT32_MAX;
    outDesc.ShaderRegister  = SHADERGLOBAL_ANISOTROPICCLAMP_SAMPLERSLOT;
    outDesc.RegisterSpace   = 0;
    outDesc.ShaderVisibility= D3D12_SHADER_VISIBILITY_ALL;
}

void vaDirectXTools12::FillSamplerStateAnisotropicWrap( D3D12_STATIC_SAMPLER_DESC & outDesc )
{
    outDesc = CD3DX12_STATIC_SAMPLER_DESC( );
    outDesc.Filter          = D3D12_FILTER_ANISOTROPIC;
    outDesc.AddressU        = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    outDesc.AddressV        = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    outDesc.AddressW        = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    outDesc.MipLODBias      = 0;
    outDesc.MaxAnisotropy   = 16;
    outDesc.ComparisonFunc  = D3D12_COMPARISON_FUNC_NEVER;
    outDesc.BorderColor     = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    outDesc.MinLOD          = 0.0f;
    outDesc.MaxLOD          = D3D12_FLOAT32_MAX;
    outDesc.ShaderRegister  = SHADERGLOBAL_ANISOTROPICWRAP_SAMPLERSLOT;
    outDesc.RegisterSpace   = 0;
    outDesc.ShaderVisibility= D3D12_SHADER_VISIBILITY_ALL;
}

void vaDirectXTools12::FillSamplerStateShadowCmp( D3D12_STATIC_SAMPLER_DESC & outDesc )
{
    outDesc = CD3DX12_STATIC_SAMPLER_DESC( );
    outDesc.Filter          = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR; // D3D12_FILTER_COMPARISON_ANISOTROPIC; 
    outDesc.AddressU        = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    outDesc.AddressV        = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    outDesc.AddressW        = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    outDesc.MipLODBias      = 0;
    outDesc.MaxAnisotropy   = 16;
    outDesc.ComparisonFunc  = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    outDesc.BorderColor     = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    outDesc.MinLOD          = 0.0f;
    outDesc.MaxLOD          = D3D12_FLOAT32_MAX;
    outDesc.ShaderRegister  = SHADERGLOBAL_SHADOWCMP_SAMPLERSLOT;
    outDesc.RegisterSpace   = 0;
    outDesc.ShaderVisibility= D3D12_SHADER_VISIBILITY_ALL;
}

void vaDirectXTools12::FillBlendState( D3D12_BLEND_DESC & outDesc, vaBlendMode blendMode )
{
    outDesc.AlphaToCoverageEnable   = false;
    outDesc.IndependentBlendEnable  = false;
    const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
    {
        FALSE, FALSE,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL,
    };
    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        outDesc.RenderTarget[ i ] = defaultRenderTargetBlendDesc;

    switch( blendMode )
    {
    case vaBlendMode::Opaque:
        // already in default
        break;
    case vaBlendMode::Additive:
        outDesc.RenderTarget[0].BlendEnable     = true;
        outDesc.RenderTarget[0].BlendOp         = D3D12_BLEND_OP_ADD;
        outDesc.RenderTarget[0].BlendOpAlpha    = D3D12_BLEND_OP_ADD;
        outDesc.RenderTarget[0].SrcBlend        = D3D12_BLEND_ONE;
        outDesc.RenderTarget[0].SrcBlendAlpha   = D3D12_BLEND_ONE;
        outDesc.RenderTarget[0].DestBlend       = D3D12_BLEND_ONE;
        outDesc.RenderTarget[0].DestBlendAlpha  = D3D12_BLEND_ONE;
        break;
    case vaBlendMode::AlphaBlend:
        outDesc.RenderTarget[0].BlendEnable     = true;
        outDesc.RenderTarget[0].BlendOp         = D3D12_BLEND_OP_ADD;
        outDesc.RenderTarget[0].BlendOpAlpha    = D3D12_BLEND_OP_ADD;
        outDesc.RenderTarget[0].SrcBlend        = D3D12_BLEND_SRC_ALPHA;
        outDesc.RenderTarget[0].SrcBlendAlpha   = D3D12_BLEND_ZERO;
        outDesc.RenderTarget[0].DestBlend       = D3D12_BLEND_INV_SRC_ALPHA;
        outDesc.RenderTarget[0].DestBlendAlpha  = D3D12_BLEND_ONE;
        break;
    case vaBlendMode::PremultAlphaBlend:
        outDesc.RenderTarget[0].BlendEnable     = true;
        outDesc.RenderTarget[0].BlendOp         = D3D12_BLEND_OP_ADD;
        outDesc.RenderTarget[0].BlendOpAlpha    = D3D12_BLEND_OP_ADD;
        outDesc.RenderTarget[0].SrcBlend        = D3D12_BLEND_ONE;
        outDesc.RenderTarget[0].SrcBlendAlpha   = D3D12_BLEND_ZERO;
        outDesc.RenderTarget[0].DestBlend       = D3D12_BLEND_INV_SRC_ALPHA;
        outDesc.RenderTarget[0].DestBlendAlpha  = D3D12_BLEND_ONE;
        break;
    case vaBlendMode::Mult:
        outDesc.RenderTarget[0].BlendEnable     = true;
        outDesc.RenderTarget[0].BlendOp         = D3D12_BLEND_OP_ADD;
        outDesc.RenderTarget[0].BlendOpAlpha    = D3D12_BLEND_OP_ADD;
        outDesc.RenderTarget[0].SrcBlend        = D3D12_BLEND_ZERO;
        outDesc.RenderTarget[0].SrcBlendAlpha   = D3D12_BLEND_ZERO;
        outDesc.RenderTarget[0].DestBlend       = D3D12_BLEND_SRC_COLOR;
        outDesc.RenderTarget[0].DestBlendAlpha  = D3D12_BLEND_SRC_ALPHA;
        break;
    default: assert( false );
    }
}

bool vaDirectXTools12::LoadTexture( ID3D12Device * device, void * dataBuffer, uint64 dataBufferSize, bool isDDS, vaTextureLoadFlags loadFlags, vaResourceBindSupportFlags bindFlags, ID3D12Resource *& outResource, std::vector<D3D12_SUBRESOURCE_DATA> & outSubresources, std::unique_ptr<byte[]> & outDecodedData, bool & outIsCubemap )
{
    D3D12_RESOURCE_FLAGS resourceFlags = ResourceFlagsDX12FromVA( bindFlags );

    bool forceSRGB = (loadFlags & vaTextureLoadFlags::PresumeDataIsSRGB) != 0;
    if( (loadFlags & vaTextureLoadFlags::PresumeDataIsSRGB) != 0 )
        { assert( (loadFlags & vaTextureLoadFlags::PresumeDataIsLinear) == 0 ); }   // both at the same time don't make sense
    if( (loadFlags & vaTextureLoadFlags::PresumeDataIsLinear) != 0 )
        { assert( (loadFlags & vaTextureLoadFlags::PresumeDataIsSRGB) == 0 );  }    // both at the same time don't make sense
    assert( (loadFlags & vaTextureLoadFlags::AutogenerateMIPsIfMissing) == 0 );     // not supported anymore

    assert( outSubresources.size() == 0 );
    outSubresources.clear();

    uint32 dxLoadFlags = (forceSRGB)?(DirectX::DDS_LOADER_FORCE_SRGB):(0);
    HRESULT hr;

    if( isDDS )
    {
        assert( outDecodedData == nullptr || outDecodedData.get() == dataBuffer );   // loading inplace from provided data
        hr = DirectX::LoadDDSTextureFromMemoryEx( device, (const uint8_t *)dataBuffer, (size_t)dataBufferSize, 0, resourceFlags, dxLoadFlags, &outResource, outSubresources, nullptr, &outIsCubemap );
    }
    else
    {
        outIsCubemap = false;
        assert( outDecodedData == nullptr );            // will create data
        outSubresources.resize(1);
        hr = DirectX::LoadWICTextureFromMemoryEx( device, (const uint8_t *)dataBuffer, (size_t)dataBufferSize, 0, resourceFlags, dxLoadFlags, &outResource, outDecodedData, outSubresources[0] );
    }

    if( SUCCEEDED( hr ) )
    {
        D3D12_RESOURCE_DESC desc = outResource->GetDesc();
        if( (loadFlags & vaTextureLoadFlags::PresumeDataIsSRGB) != 0 )
        {
            // wanted sRGB but didn't get it? there's something wrong
            assert( DirectX::IsSRGB( desc.Format ) );
        }
        if( (loadFlags & vaTextureLoadFlags::PresumeDataIsLinear) != 0 )
        {
            // there is no support for this at the moment in DirectX tools so asserting if the result is not as requested; fix in the future
            assert( !DirectX::IsSRGB( desc.Format ) );
        }
        return true;
    }
    outResource = nullptr;
    outDecodedData = nullptr;
    outSubresources.clear();
    return false;
}

bool vaDirectXTools12::LoadTexture( ID3D12Device * device, const wchar_t * filePath, bool isDDS, vaTextureLoadFlags loadFlags, vaResourceBindSupportFlags bindFlags, ID3D12Resource *& outResource, std::vector<D3D12_SUBRESOURCE_DATA> & outSubresources, std::unique_ptr<byte[]> & outDecodedData, bool & outIsCubemap )
{
    auto buffer = vaFileTools::LoadFileToMemoryStream( filePath );

    if( buffer == nullptr )
        return nullptr;

    if( isDDS )
    {
        outDecodedData = std::make_unique<byte[]>( buffer->GetLength() );
        memcpy( outDecodedData.get(), buffer->GetBuffer(), buffer->GetLength() );
        return LoadTexture( device, outDecodedData.get(), buffer->GetLength( ), isDDS, loadFlags, bindFlags, outResource, outSubresources, outDecodedData, outIsCubemap );
    }
    else
    {
        return LoadTexture( device, buffer->GetBuffer( ), buffer->GetLength( ), isDDS, loadFlags, bindFlags, outResource, outSubresources, outDecodedData, outIsCubemap );
    }
}

#if 0
ID3D12Resource * vaDirectXTools12::LoadTextureDDS( ID3D12Device * device, void * dataBuffer, int64 dataSize, vaTextureLoadFlags loadFlags, vaResourceBindSupportFlags bindFlags, uint64 * outCRC )
{
    outCRC; // unreferenced, not implemented
    assert( outCRC == nullptr ); // not implemented

    ID3D12Resource * texture = nullptr;

    // std::vector<D3D12_SUBRESOURCE_DATA> subresources; <- ok need to output this and isCubemap!
    hr = DirectX::LoadDDSTextureFromMemoryEx( device, (const uint8_t *)dataBuffer, (size_t)dataSize, 0, resourceFlags, dxLoadFlags, &texture, subresources, NULL );

    if( SUCCEEDED( hr ) )
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC desc;
        textureSRV->GetDesc( &desc );
        if( (loadFlags & vaTextureLoadFlags::PresumeDataIsSRGB) != 0 )
        {
            // wanted sRGB but didn't get it? there's something wrong
            assert( DirectX::IsSRGB( desc.Format ) );
        }
        if( (loadFlags & vaTextureLoadFlags::PresumeDataIsLinear) != 0 )
        {
            // there is no support for this at the moment in DirectX tools so asserting if the result is not as requested; fix in the future
            assert( !DirectX::IsSRGB( desc.Format ) );
        }
        if( (loadFlags & vaTextureLoadFlags::AutogenerateMIPsIfMissing) != 0 )
        {
            // check for mips here maybe?
            // assert( desc. );
        }

        SAFE_RELEASE( textureSRV );

        return texture;
    }
    else
    {
        SAFE_RELEASE( texture );
        SAFE_RELEASE( textureSRV );
        assert( false ); // check hr
        throw "Error creating the texture from the stream";
    }
}

ID3D12Resource * vaDirectXTools12::LoadTextureDDS( ID3D12Device * device, const wchar_t * path, vaTextureLoadFlags loadFlags, vaResourceBindSupportFlags bindFlags, uint64 * outCRC )
{
    outCRC; // unreferenced, not implemented
    assert( outCRC == nullptr ); // not implemented

    auto buffer = vaFileTools::LoadFileToMemoryStream( path );

    if( buffer == nullptr )
        return nullptr;

    return LoadTextureDDS( device, buffer->GetBuffer( ), buffer->GetLength( ), loadFlags, bindFlags, outCRC );
}

ID3D12Resource * vaDirectXTools12::LoadTextureWIC( ID3D12Device * device, void * dataBuffer, int64 dataSize, vaTextureLoadFlags loadFlags, vaResourceBindSupportFlags bindFlags, uint64 * outCRC )
{
    outCRC; // unreferenced, not implemented
    assert( outCRC == nullptr ); // not implemented

    ID3D12DeviceContext * immediateContext = nullptr;
    device->GetImmediateContext( &immediateContext );
    immediateContext->Release(); // yeah, ugly - we know device will guarantee immediate context persistence but still...

    ID3D12Resource * texture = NULL;

    UINT dxBindFlags = BindFlagsDX12FromVA( bindFlags );

    // not actually used, needed internally for mipmap generation
    ID3D12ShaderResourceView * textureSRV = NULL;

    DirectX::WIC_LOADER_FLAGS wicLoaderFlags = DirectX::WIC_LOADER_DEFAULT;
    if( (loadFlags & vaTextureLoadFlags::PresumeDataIsSRGB) != 0 )
    {
        assert( (loadFlags & vaTextureLoadFlags::PresumeDataIsLinear) == 0 );   // both at the same time don't make sense
        wicLoaderFlags = (DirectX::WIC_LOADER_FLAGS)(wicLoaderFlags | DirectX::WIC_LOADER_FORCE_SRGB);
    }
    if( (loadFlags & vaTextureLoadFlags::PresumeDataIsLinear) != 0 )
    {
        assert( (loadFlags & vaTextureLoadFlags::PresumeDataIsSRGB) == 0 );   // both at the same time don't make sense
        wicLoaderFlags = (DirectX::WIC_LOADER_FLAGS)(wicLoaderFlags | DirectX::WIC_LOADER_IGNORE_SRGB);
    }
    bool dontAutogenerateMIPs = (loadFlags & vaTextureLoadFlags::AutogenerateMIPsIfMissing) == 0;

    HRESULT hr;

    if( dontAutogenerateMIPs )
    {
        hr = DirectX::CreateWICTextureFromMemoryEx( device, (const uint8_t *)dataBuffer, (size_t)dataSize, 0, D3D11_USAGE_DEFAULT, dxBindFlags, 0, 0, wicLoaderFlags, &texture, &textureSRV );
    }
    else
    {
        hr = DirectX::CreateWICTextureFromMemoryEx( device, immediateContext, (const uint8_t *)dataBuffer, (size_t)dataSize, 0, D3D11_USAGE_DEFAULT, dxBindFlags, 0, 0, wicLoaderFlags, &texture, &textureSRV );
    }

    if( SUCCEEDED( hr ) )
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC desc;
        textureSRV->GetDesc( &desc );
        if( (loadFlags & vaTextureLoadFlags::PresumeDataIsSRGB) != 0 )
        {
            // wanted sRGB but didn't get it? there's something wrong
            assert( DirectX::IsSRGB( desc.Format ) );
        }
        if( (loadFlags & vaTextureLoadFlags::PresumeDataIsLinear) != 0 )
        {
            // there is no support for this at the moment in DirectX tools so asserting if the result is not as requested; fix in the future
            assert( !DirectX::IsSRGB( desc.Format ) );
        }

        SAFE_RELEASE( textureSRV );
        return texture;
    }
    else
    {
        SAFE_RELEASE( texture );
        SAFE_RELEASE( textureSRV );
        VA_LOG_ERROR_STACKINFO( L"Error loading texture" );
        return nullptr;
    }
}

ID3D12Resource * vaDirectXTools12::LoadTextureWIC( ID3D12Device * device, const wchar_t * path, vaTextureLoadFlags loadFlags, vaResourceBindSupportFlags bindFlags, uint64 * outCRC )
{
    outCRC; // unreferenced, not implemented
    assert( outCRC == nullptr ); // not implemented

    auto buffer = vaFileTools::LoadFileToMemoryStream( path );

    if( buffer == nullptr )
        return nullptr;

    return LoadTextureWIC( device, buffer->GetBuffer( ), buffer->GetLength( ), loadFlags, bindFlags, outCRC );
}

#endif

void vaGraphicsPSODescDX12::FillGraphicsPipelineStateDesc( D3D12_GRAPHICS_PIPELINE_STATE_DESC & outDesc, ID3D12RootSignature * pRootSignature ) const
{
    assert( VSBlob != nullptr );
    outDesc.pRootSignature    = pRootSignature;
    outDesc.VS                = (VSBlob != nullptr)?( CD3DX12_SHADER_BYTECODE(VSBlob.Get()) ):(D3D12_SHADER_BYTECODE({0, 0}));
    outDesc.PS                = (PSBlob != nullptr)?( CD3DX12_SHADER_BYTECODE(PSBlob.Get()) ):(D3D12_SHADER_BYTECODE({0, 0}));
    outDesc.DS                = (DSBlob != nullptr)?( CD3DX12_SHADER_BYTECODE(DSBlob.Get()) ):(D3D12_SHADER_BYTECODE({0, 0}));
    outDesc.HS                = (HSBlob != nullptr)?( CD3DX12_SHADER_BYTECODE(HSBlob.Get()) ):(D3D12_SHADER_BYTECODE({0, 0}));
    outDesc.GS                = (GSBlob != nullptr)?( CD3DX12_SHADER_BYTECODE(GSBlob.Get()) ):(D3D12_SHADER_BYTECODE({0, 0}));
    outDesc.StreamOutput      = D3D12_STREAM_OUTPUT_DESC({nullptr, 0, nullptr, 0, 0});
    vaDirectXTools12::FillBlendState( outDesc.BlendState, BlendMode );
    outDesc.SampleMask        = UINT_MAX;
    
    // rasterizer state
    {
        D3D12_RASTERIZER_DESC rastDesc;
        rastDesc.CullMode               = (CullMode == vaFaceCull::None)?(D3D12_CULL_MODE_NONE):( (CullMode == vaFaceCull::Front)?(D3D12_CULL_MODE_FRONT):(D3D12_CULL_MODE_BACK) );
        rastDesc.FillMode               = (FillMode == vaFillMode::Solid)?(D3D12_FILL_MODE_SOLID):(D3D12_FILL_MODE_WIREFRAME);
        rastDesc.FrontCounterClockwise  = FrontCounterClockwise;
        rastDesc.DepthBias              = D3D12_DEFAULT_DEPTH_BIAS;
        rastDesc.DepthBiasClamp         = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        rastDesc.SlopeScaledDepthBias   = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rastDesc.DepthClipEnable        = true;
        rastDesc.MultisampleEnable      = MultisampleEnable;
        //rastDesc.ScissorEnable          = true;
        rastDesc.AntialiasedLineEnable  = false;
        rastDesc.ForcedSampleCount      = 0;
        rastDesc.ConservativeRaster     = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        outDesc.RasterizerState       = rastDesc;
    }

    // depth stencil state
    {
        D3D12_DEPTH_STENCIL_DESC dsDesc;
        dsDesc.DepthEnable              = DepthEnable;
        dsDesc.DepthWriteMask           = (DepthWriteEnable)?(D3D12_DEPTH_WRITE_MASK_ALL):(D3D12_DEPTH_WRITE_MASK_ZERO);
        dsDesc.DepthFunc                = (D3D12_COMPARISON_FUNC)DepthFunc;
        dsDesc.StencilEnable            = false;
        dsDesc.StencilReadMask          = 0;
        dsDesc.StencilWriteMask         = 0;
        dsDesc.FrontFace                = {D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_COMPARISON_FUNC_ALWAYS};
        dsDesc.BackFace                 = {D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_COMPARISON_FUNC_ALWAYS};
        outDesc.DepthStencilState = dsDesc;
    }
    
    // input layout
    {
        if( VSInputLayout != nullptr )
        {
            outDesc.InputLayout.NumElements          = (UINT)VSInputLayout->size();
            outDesc.InputLayout.pInputElementDescs   = &(*VSInputLayout)[0];
        }
        else
            outDesc = { 0, nullptr };
    }

    outDesc.IBStripCutValue   = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

    // topology
    {
        outDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
        // TODO: "If the HS and DS members are specified, the PrimitiveTopologyType member for topology type must be set to D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH."
        assert( HSBlob == nullptr );
        assert( GSBlob == nullptr );
        switch( Topology )
        {   case vaPrimitiveTopology::PointList:        outDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;        break;
            case vaPrimitiveTopology::LineList:         outDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;         break;
            case vaPrimitiveTopology::TriangleList:     outDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;     break;
            case vaPrimitiveTopology::TriangleStrip:    outDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;     break;
            default: assert( false ); break;    // for hull shader
        }
    }

    outDesc.NumRenderTargets  = NumRenderTargets;
    for( int i = 0; i < _countof(outDesc.RTVFormats); i++ )
        outDesc.RTVFormats[i] = DXGIFormatFromVA(RTVFormats[i]);
    outDesc.DSVFormat         = DXGIFormatFromVA(DSVFormat);
    outDesc.SampleDesc        = { SampleDescCount, 0 };
    outDesc.NodeMask          = 0;
    outDesc.CachedPSO         = { nullptr, 0 };
    outDesc.Flags             = D3D12_PIPELINE_STATE_FLAG_NONE; // for warp devices automatically use D3D12_PIPELINE_STATE_FLAG_TOOL_DEBUG?

}

void vaGraphicsPSODescDX12::FillKey( vaMemoryStream & outStream ) const
{
    size_t dbgSizeOfThis = sizeof(*this); dbgSizeOfThis;
    assert( dbgSizeOfThis == 168 ); // size of the structure changed, did you change the key creation too?
    
    assert( outStream.GetPosition() == 0 );

    // add space for the 64bit hash 
    outStream.WriteValue<uint64>( 0ui64 );

    outStream.WriteValue<int64>( VSUniqueContentsID );
    outStream.WriteValue<int64>( PSUniqueContentsID );
    outStream.WriteValue<int64>( DSUniqueContentsID );
    outStream.WriteValue<int64>( HSUniqueContentsID );
    outStream.WriteValue<int64>( GSUniqueContentsID );

    outStream.WriteValue<int32>( static_cast<int32>(this->BlendMode) );
    outStream.WriteValue<int32>( static_cast<int32>(this->FillMode) );
    outStream.WriteValue<int32>( static_cast<int32>(this->CullMode) );
    outStream.WriteValue<bool>( this->FrontCounterClockwise );
    outStream.WriteValue<bool>( this->MultisampleEnable );
    // outStream.WriteValue<bool>( this->ScissorEnable );
    outStream.WriteValue<bool>( this->DepthEnable );
    outStream.WriteValue<bool>( this->DepthWriteEnable );
    outStream.WriteValue<int32>( static_cast<int32>(this->DepthFunc) );
    outStream.WriteValue<int32>( static_cast<int32>(this->Topology) );
    outStream.WriteValue<int32>( this->NumRenderTargets );
    
    for( int i = 0; i < _countof(this->RTVFormats); i++ )
        outStream.WriteValue<int32>( static_cast<int32>(this->RTVFormats[i]) );
    outStream.WriteValue<int32>( static_cast<int32>(this->DSVFormat) );
    outStream.WriteValue<uint32>( this->SampleDescCount );

    *reinterpret_cast<uint64*>(outStream.GetBuffer()) = vaXXHash64::Compute( outStream.GetBuffer() + sizeof(uint64), outStream.GetPosition() - sizeof(uint64), 0 );
}

void vaComputePSODescDX12::FillComputePipelineStateDesc( D3D12_COMPUTE_PIPELINE_STATE_DESC & outDesc, ID3D12RootSignature * pRootSignature ) const
{
    assert( CSBlob != nullptr && CSUniqueContentsID != -1 );
    outDesc.pRootSignature    = pRootSignature;
    outDesc.CS                = (CSBlob != nullptr)?( CD3DX12_SHADER_BYTECODE(CSBlob.Get()) ):(D3D12_SHADER_BYTECODE({0, 0}));
    outDesc.NodeMask          = 0;
    outDesc.CachedPSO         = { nullptr, 0 };
    outDesc.Flags             = D3D12_PIPELINE_STATE_FLAG_NONE; // for warp devices automatically use D3D12_PIPELINE_STATE_FLAG_TOOL_DEBUG?
}

void vaComputePSODescDX12::FillKey( vaMemoryStream & outStream ) const
{
    size_t dbgSizeOfThis = sizeof(*this); dbgSizeOfThis;
    assert( dbgSizeOfThis == 16 ); // size of the structure changed, did you change the key creation too?
    
    assert( outStream.GetPosition() == 0 );

    outStream.WriteValue<int64>( CSUniqueContentsID );
}

HRESULT vaGraphicsPSODX12::CreatePSO( vaRenderDeviceDX12 & device, ID3D12RootSignature * rootSignature )
{ 
    if( m_pso != nullptr )
    {
        assert( false );
        return E_FAIL;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;
    m_desc.FillGraphicsPipelineStateDesc( desc, rootSignature );
    return device.GetPlatformDevice()->CreateGraphicsPipelineState( &desc, IID_PPV_ARGS(&m_pso) );
}
HRESULT vaComputePSODX12::CreatePSO( vaRenderDeviceDX12 & device, ID3D12RootSignature * rootSignature )
{ 
    if( m_pso != nullptr )
    {
        assert( false );
        return E_FAIL;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC desc;
    m_desc.FillComputePipelineStateDesc( desc, rootSignature );
    return device.GetPlatformDevice()->CreateComputePipelineState( &desc, IID_PPV_ARGS(&m_pso) );
}