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

#include "vaRenderDeviceContextDX11.h"
#include "vaTextureDX11.h"

#include "vaRenderingToolsDX11.h"

#include "vaRenderBuffersDX11.h"

#include "Rendering/DirectX/vaRenderingGlobalsDX11.h"
#include "Rendering/DirectX/vaLightingDX11.h"

#include "vaResourceFormatsDX11.h"

#ifdef VA_IMGUI_INTEGRATION_ENABLED
#include "IntegratedExternals/imgui/DirectX11/imgui_impl_dx11.h"
#endif

using namespace VertexAsylum;

namespace 
{
    struct CommonSimpleVertex
    {
        float   Position[4];
        float   UV[2];

        CommonSimpleVertex( ) {};
        CommonSimpleVertex( float px, float py, float pz, float pw, float uvx, float uvy ) { Position[0] = px; Position[1] = py; Position[2] = pz; Position[3] = pw; UV[0] = uvx; UV[1] = uvy; }
    };
}

// // used to make Gather using UV slightly off the border (so we get the 0,0 1,0 0,1 1,1 even if there's a minor calc error, without adding the half pixel offset)
// static const float  c_minorUVOffset = 0.00006f;  // less than 0.5/8192

vaRenderDeviceContextDX11::vaRenderDeviceContextDX11( const vaRenderingModuleParams & params )
    : vaRenderDeviceContext( params ), 
    //m_fullscreenVS( params ), 
    m_copyResourcePS( params )
{ 
    //m_fullscreenVB              = nullptr;
    //m_fullscreenVBDynamic       = nullptr;
    m_deviceContext             = nullptr;
    m_device                    = nullptr;
}

vaRenderDeviceContextDX11::~vaRenderDeviceContextDX11( )
{ 
    //SAFE_RELEASE( m_fullscreenVB );
    //SAFE_RELEASE( m_fullscreenVBDynamic );

    SAFE_RELEASE( m_shadowCmpSamplerState );
    SAFE_RELEASE( m_deviceContext );
    SAFE_RELEASE( m_device );

    Destroy();
}

void vaRenderDeviceContextDX11::Initialize( ID3D11DeviceContext * deviceContext )
{
    assert( m_deviceContext == nullptr );
    m_deviceContext = deviceContext;
    m_deviceContext->AddRef();

    assert( m_device == nullptr );
    deviceContext->GetDevice( &m_device );

    ID3D11Device * device = GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice();
    assert( m_device == device );

    HRESULT hr;

    // // Fullscreen pass
    // {
    //     CD3D11_BUFFER_DESC desc( 3 * sizeof( CommonSimpleVertex ), D3D11_BIND_VERTEX_BUFFER );
    // 
    //     // using one big triangle
    //     CommonSimpleVertex fsVertices[3];
    //     fsVertices[0] = CommonSimpleVertex( -1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f );
    //     fsVertices[1] = CommonSimpleVertex( 3.0f, 1.0f, 0.0f, 1.0f, 2.0f, 0.0f );
    //     fsVertices[2] = CommonSimpleVertex( -1.0f, -3.0f, 0.0f, 1.0f, 0.0f, 2.0f );
    // 
    //     D3D11_SUBRESOURCE_DATA initSubresData;
    //     initSubresData.pSysMem = fsVertices;
    //     initSubresData.SysMemPitch = 0;
    //     initSubresData.SysMemSlicePitch = 0;
    //     hr = device->CreateBuffer( &desc, &initSubresData, &m_fullscreenVB );
    // 
    //     desc.Usage = D3D11_USAGE_DYNAMIC;
    //     desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    //     hr = device->CreateBuffer( &desc, &initSubresData, &m_fullscreenVBDynamic );
    // 
    //     std::vector<vaVertexInputElementDesc> inputElements;
    //     inputElements.push_back( { "SV_Position", 0,    vaResourceFormat::R32G32B32A32_FLOAT,    0,  0, vaVertexInputElementDesc::InputClassification::PerVertexData, 0 } );
    //     inputElements.push_back( { "TEXCOORD", 0,       vaResourceFormat::R32G32_FLOAT,          0, 16, vaVertexInputElementDesc::InputClassification::PerVertexData, 0 } );
    // 
    //     //ID3DBlob *pVSBlob = NULL;
    //     const char * pVSString = "void main( inout const float4 xPos : SV_Position, inout float2 UV : TEXCOORD0 ) { }";
    // 
    //     m_fullscreenVS->CreateShaderAndILFromBuffer( pVSString, "vs_5_0", "main", inputElements, vaShaderMacroContaner(), true );
    // }

    // copy resource shader
    {
        string shaderCode = 
            "Texture2D g_source           : register( t0 );                 \n"
            "float4 main( in const float4 xPos : SV_Position ) : SV_Target  \n"
            "{                                                              \n"
            "   return g_source.Load( int3( xPos.xy, 0 ) );                 \n"
            "}                                                              \n";

        m_copyResourcePS->CreateShaderFromBuffer( shaderCode, "ps_5_0", "main", vaShaderMacroContaner(), true );
    }

    // samplers
    {
        CD3D11_SAMPLER_DESC desc = CD3D11_SAMPLER_DESC( CD3D11_DEFAULT( ) );

        desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;   // D3D11_FILTER_COMPARISON_ANISOTROPIC
        desc.AddressU = desc.AddressV = desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;

        V( device->CreateSamplerState( &desc, &m_shadowCmpSamplerState ) );
    }
}

void vaRenderDeviceContextDX11::Destroy( )
{
    SAFE_RELEASE( m_deviceContext );
    SAFE_RELEASE( m_device );
}

vaRenderDeviceContext * vaRenderDeviceContextDX11::Create( vaRenderDevice & device, ID3D11DeviceContext * deviceContext )
{
    vaRenderDeviceContext * canvas = VA_RENDERING_MODULE_CREATE( vaRenderDeviceContext, device );

    vaSaferStaticCast<vaRenderDeviceContextDX11*>( canvas )->Initialize( deviceContext );

    return canvas;
}

void vaRenderDeviceContextDX11::UpdateViewport( )
{
    const vaViewport & vavp = GetViewport();

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX   = (float)vavp.X;
    viewport.TopLeftY   = (float)vavp.Y;
    viewport.Width      = (float)vavp.Width;
    viewport.Height     = (float)vavp.Height;
    viewport.MinDepth   = vavp.MinDepth;
    viewport.MaxDepth   = vavp.MaxDepth;

    m_deviceContext->RSSetViewports( 1, &viewport );

    vaRecti scissorRect;
    bool scissorRectEnabled;
    GetScissorRect( scissorRect, scissorRectEnabled );
    if( scissorRectEnabled )
    {
        D3D11_RECT rect;
        rect.left   = scissorRect.left;
        rect.top    = scissorRect.top;
        rect.right  = scissorRect.right;
        rect.bottom = scissorRect.bottom;
        m_deviceContext->RSSetScissorRects( 1, &rect );
    }
    else
    {
        // set the scissor to viewport size, for rasterizer states that have it enabled
        D3D11_RECT rect;
        rect.left   = vavp.X;
        rect.top    = vavp.Y;
        rect.right  = vavp.Width + vavp.X;
        rect.bottom = vavp.Height + vavp.Y;
        m_deviceContext->RSSetScissorRects( 1, &rect );
    }
}

void vaRenderDeviceContextDX11::UpdateRenderTargetsDepthStencilUAVs( )
{
    //const std::shared_ptr<vaTexture> & renderTarget = GetRenderTarget();
    const std::shared_ptr<vaTexture> & depthStencil = GetDepthStencil();

    ID3D11RenderTargetView *    RTVs[ c_maxRTs ];
    ID3D11UnorderedAccessView * UAVs[ c_maxUAVs];
    ID3D11DepthStencilView *    DSV = NULL;
    for( size_t i = 0; i < c_maxRTs; i++ )  
        RTVs[i] = ( m_outputsState.RenderTargets[i] != nullptr ) ? ( vaSaferStaticCast<vaTextureDX11*>( m_outputsState.RenderTargets[i].get() )->GetRTV( ) ) : ( nullptr );
    for( size_t i = 0; i < c_maxUAVs; i++ )
        UAVs[i] = ( m_outputsState.UAVs[i] != nullptr ) ? ( vaSaferStaticCast<vaTextureDX11*>( m_outputsState.UAVs[i].get( ) )->GetUAV( ) ) : ( nullptr );

    if( depthStencil != NULL )
        DSV = vaSaferStaticCast<vaTextureDX11*>( depthStencil.get() )->GetDSV( );

    m_deviceContext->OMSetRenderTargetsAndUnorderedAccessViews( m_outputsState.RenderTargetCount, (m_outputsState.RenderTargetCount>0)?(RTVs):(nullptr), DSV, m_outputsState.UAVsStartSlot, m_outputsState.UAVCount, UAVs, m_outputsState.UAVInitialCounts );
}

void vaRenderDeviceContextDX11::SetStandardSamplers( )
{
    ID3D11DeviceContext * dx11Context = GetDXContext( );

    ID3D11SamplerState * nullSamplers[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    ID3D11SamplerState * samplers[6] =
    {
        vaDirectXTools::GetSamplerStatePointClamp( ),
        vaDirectXTools::GetSamplerStatePointWrap( ),
        vaDirectXTools::GetSamplerStateLinearClamp( ),
        vaDirectXTools::GetSamplerStateLinearWrap( ),
        vaDirectXTools::GetSamplerStateAnisotropicClamp( ),
        vaDirectXTools::GetSamplerStateAnisotropicWrap( ),
    };
//    // Make sure we're not overwriting someone's old stuff. If this fires, make sure you haven't changed sampler slots without resetting to 0, 
//    // and make sure there no SetAPIGlobals getting called twice in a row, which can happen if you create a second vaSceneDrawContext on the same 
//    // vaRenderDeviceContext API context without deleting the first (manually or simply by nesting/creating two vaSceneDrawContext-es in a scope)
    vaDirectXTools::AssertSetToD3DContextAllShaderTypes( dx11Context, nullSamplers, SHADERGLOBAL_POINTCLAMP_SAMPLERSLOT, _countof( samplers ) );

    // Set default samplers
    vaDirectXTools::SetToD3DContextAllShaderTypes( dx11Context, samplers, SHADERGLOBAL_POINTCLAMP_SAMPLERSLOT, _countof( samplers ) );
    // this fills SHADERGLOBAL_POINTCLAMP_SAMPLERSLOT, SHADERGLOBAL_POINTWRAP_SAMPLERSLOT, SHADERGLOBAL_LINEARCLAMP_SAMPLERSLOT, SHADERGLOBAL_LINEARWRAP_SAMPLERSLOT, SHADERGLOBAL_ANISOTROPICCLAMP_SAMPLERSLOT, SHADERGLOBAL_ANISOTROPICWRAP_SAMPLERSLOT

    vaDirectXTools::AssertSetToD3DContextAllShaderTypes( dx11Context, (ID3D11SamplerState*)NULL, SHADERGLOBAL_SHADOWCMP_SAMPLERSLOT );
    vaDirectXTools::SetToD3DContextAllShaderTypes( dx11Context, m_shadowCmpSamplerState, SHADERGLOBAL_SHADOWCMP_SAMPLERSLOT );
}

void vaRenderDeviceContextDX11::UnsetStandardSamplers( )
{
    ID3D11DeviceContext * dx11Context = GetDXContext( );

    ID3D11SamplerState * nullSamplers[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    ID3D11SamplerState * samplers[6] =
    {
        vaDirectXTools::GetSamplerStatePointClamp( ),
        vaDirectXTools::GetSamplerStatePointWrap( ),
        vaDirectXTools::GetSamplerStateLinearClamp( ),
        vaDirectXTools::GetSamplerStateLinearWrap( ),
        vaDirectXTools::GetSamplerStateAnisotropicClamp( ),
        vaDirectXTools::GetSamplerStateAnisotropicWrap( ),
    };
    // Did anyone mess with samplers?
    vaDirectXTools::AssertSetToD3DContextAllShaderTypes( dx11Context, samplers, SHADERGLOBAL_POINTCLAMP_SAMPLERSLOT, _countof( samplers ) );
    vaDirectXTools::AssertSetToD3DContextAllShaderTypes( dx11Context, m_shadowCmpSamplerState, SHADERGLOBAL_SHADOWCMP_SAMPLERSLOT );

    // Unset default samplers
    vaDirectXTools::SetToD3DContextAllShaderTypes( dx11Context, nullSamplers, SHADERGLOBAL_POINTCLAMP_SAMPLERSLOT, _countof( samplers ) );
    vaDirectXTools::SetToD3DContextAllShaderTypes( dx11Context, (ID3D11SamplerState*)NULL, SHADERGLOBAL_SHADOWCMP_SAMPLERSLOT );
}
/*
void vaRenderDeviceContextDX11::FullscreenPassDraw( ID3D11PixelShader * pixelShader, ID3D11BlendState * blendState, ID3D11DepthStencilState * depthStencilState, UINT stencilRef, float ZDistance )
{
//    if( vertexShader == NULL )
//        vertexShader = m_fullscreenVS.GetShader();

    ID3D11DeviceContext * context = m_deviceContext;

    // Topology
    context->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

    // Vertex buffer
    const uint32 stride = sizeof( CommonSimpleVertex );
    UINT offsetInBytes = 0;
    if( ZDistance != 0.0f )
    {
        // update the vertex buffer dynamically - we should do _NO_OVERWRITE but this codepath doesn't happen often enough for it to matter
        HRESULT hr;

        D3D11_MAPPED_SUBRESOURCE subResMap;
        hr = context->Map( m_fullscreenVBDynamic, 0, D3D11_MAP_WRITE_DISCARD, 0, &subResMap );
        assert( SUCCEEDED( hr ) );

        CommonSimpleVertex  * fsVertices = (CommonSimpleVertex *)subResMap.pData;
        fsVertices[0] = CommonSimpleVertex( -1.0f, 1.0f, ZDistance, 1.0f, 0.0f, 0.0f );
        fsVertices[1] = CommonSimpleVertex( 3.0f, 1.0f, ZDistance, 1.0f, 2.0f, 0.0f );
        fsVertices[2] = CommonSimpleVertex( -1.0f, -3.0f, ZDistance, 1.0f, 0.0f, 2.0f );

        context->Unmap( m_fullscreenVBDynamic, 0 );

        context->IASetVertexBuffers( 0, 1, &m_fullscreenVBDynamic, &stride, &offsetInBytes );
    }
    else
    {
        context->IASetVertexBuffers( 0, 1, &m_fullscreenVB, &stride, &offsetInBytes );
    }

    // Shaders and input layout
    
    context->VSSetShader( m_fullscreenVS->SafeCast<vaVertexShaderDX11*>()->GetShader(), nullptr, 0 );
    context->IASetInputLayout( m_fullscreenVS->SafeCast<vaVertexShaderDX11*>()->GetInputLayout() );

    context->PSSetShader( pixelShader, NULL, 0 );

    float blendFactor[4] = { 0, 0, 0, 0 };
    context->OMSetBlendState( blendState, blendFactor, 0xFFFFFFFF );

    context->OMSetDepthStencilState( depthStencilState, stencilRef );

    context->RSSetState( vaDirectXTools::GetRS_CullNone_Fill() );

    context->Draw( 3, 0 );

    context->PSSetShader( nullptr, nullptr, 0 );

    context->VSSetShader( nullptr, nullptr, 0 );
}

void vaRenderDeviceContextDX11::FullscreenPassDraw( vaPixelShader & pixelShader, vaBlendMode blendMode )
{
    ID3D11BlendState * blendState = DX11BlendModeFromVABlendMode( blendMode );

    FullscreenPassDraw( pixelShader.SafeCast<vaPixelShaderDX11*>( )->GetShader(), blendState );
}
*/

// useful for copying individual MIPs, in which case use Views created with vaTexture::CreateView
void vaRenderDeviceContextDX11::CopySRVToRTV( shared_ptr<vaTexture> destination, shared_ptr<vaTexture> source )
{
    // this could probably be done in platform-independent way in vaRenderDeviceContext

    if( destination->GetType( ) != source->GetType( )
        || destination->GetViewedSliceSizeX( ) != source->GetViewedSliceSizeX( )
        || destination->GetViewedSliceSizeY( ) != source->GetViewedSliceSizeY( )
        || destination->GetViewedSliceSizeZ( ) != source->GetViewedSliceSizeZ( )
        || destination->GetSampleCount( ) != source->GetSampleCount( )
        )
    {
        assert( false );
        VA_ERROR( "vaRenderDeviceContext::CopySRVToRTV - not supported or incorrect parameters" );
        return;
    }

    RenderOutputsState backupOutputs = GetOutputs( );

//    ID3D11RenderTargetView * destRTV = destination.SafeCast<vaTextureDX11*>( )->GetRTV();
//    //ID3D11ShaderResourceView * srcSRV = source.SafeCast<vaTextureDX11*>( )->GetSRV();
//
//    source.SafeCast<vaTextureDX11*>()->SetToAPISlotSRV( *this, 0 );
//
//    m_deviceContext->OMSetRenderTargets( 1, &destRTV, nullptr );
//    SetViewport( vaViewport( destination.GetSizeX(), destination.GetSizeY() ) );
//
//    FullscreenPassDraw( *m_copyResourcePS );
//
//    source.SafeCast<vaTextureDX11*>()->UnsetFromAPISlotSRV( *this, 0 );

    // this updated codepath was never tested - please step through and validate it's ok
    assert( false );

    vaRenderItem renderItem;
    FillFullscreenPassRenderItem( renderItem );
    renderItem.ShaderResourceViews[0]   = source;
    renderItem.PixelShader = m_copyResourcePS;
    //renderItem.PixelShader->WaitFinishIfBackgroundCreateActive();
    SetRenderTarget( destination, nullptr, true );
    ExecuteSingleItem( renderItem );

    SetOutputs( backupOutputs );
}

void vaRenderDeviceContextDX11::BeginItems( )
{
    assert( !m_itemsStarted );
    assert( m_lastSceneDrawContext == nullptr );
    m_itemsStarted = true;
    SetStandardSamplers();
}

static void SetSceneGlobals( vaSceneDrawContext * sceneDrawContext )
{
    assert( sceneDrawContext != nullptr );
    if( sceneDrawContext!= nullptr )
    {
        if( sceneDrawContext->Lighting != nullptr )
            sceneDrawContext->Lighting->SafeCast<vaLightingDX11*>()->SetAPIGlobals( *sceneDrawContext);

        sceneDrawContext->Globals.SafeCast<vaRenderingGlobalsDX11*>()->SetAPISceneGlobals( *sceneDrawContext );
        sceneDrawContext = nullptr;
    }
}

static void UnsetSceneGlobals( vaSceneDrawContext * sceneDrawContext )
{
    assert( sceneDrawContext != nullptr );
    if( sceneDrawContext!= nullptr )
    {
        if( sceneDrawContext->Lighting != nullptr )
            sceneDrawContext->Lighting->SafeCast<vaLightingDX11*>()->UnsetAPIGlobals( *sceneDrawContext);

        sceneDrawContext->Globals.SafeCast<vaRenderingGlobalsDX11*>()->UnsetAPISceneGlobals( *sceneDrawContext );
        sceneDrawContext = nullptr;
    }
}

void vaRenderDeviceContextDX11::EndItems( )
{
    assert( m_itemsStarted );
    m_itemsStarted = false;

    UnsetStandardSamplers();

    // m_lastRenderItem = vaRenderItem( );

    //////////////////////////////////////////////////////////////////////////
    // Maybe just do ClearState instead, this is ugly.. or dirty-based clearing.
    //
    ID3D11Buffer * constantBuffers[ _countof(vaRenderItem::ConstantBuffers) ];
    for( int i = 0; i < _countof(vaRenderItem::ConstantBuffers); i++ )
        constantBuffers[i] = nullptr;
    m_deviceContext->VSSetConstantBuffers( 0, _countof(constantBuffers), constantBuffers );
    m_deviceContext->GSSetConstantBuffers( 0, _countof(constantBuffers), constantBuffers );
    m_deviceContext->HSSetConstantBuffers( 0, _countof(constantBuffers), constantBuffers );
    m_deviceContext->DSSetConstantBuffers( 0, _countof(constantBuffers), constantBuffers );
    m_deviceContext->PSSetConstantBuffers( 0, _countof(constantBuffers), constantBuffers );
    m_deviceContext->CSSetConstantBuffers( 0, _countof(constantBuffers), constantBuffers );
    
    ID3D11ShaderResourceView * SRVs[ _countof(vaRenderItem::ShaderResourceViews) ];
    for( int i = 0; i < _countof(vaRenderItem::ShaderResourceViews); i++ )
        SRVs[i] = nullptr;
    m_deviceContext->VSSetShaderResources( 0, _countof(SRVs), SRVs );
    m_deviceContext->GSSetShaderResources( 0, _countof(SRVs), SRVs );
    m_deviceContext->HSSetShaderResources( 0, _countof(SRVs), SRVs );
    m_deviceContext->DSSetShaderResources( 0, _countof(SRVs), SRVs );
    m_deviceContext->PSSetShaderResources( 0, _countof(SRVs), SRVs );
    m_deviceContext->CSSetShaderResources( 0, _countof(SRVs), SRVs );
    
    ID3D11UnorderedAccessView * UAVs[ _countof(vaComputeItem::UnorderedAccessViews) ];
    for( int i = 0; i < _countof(vaComputeItem::UnorderedAccessViews); i++ )
        UAVs[i] = nullptr;
    m_deviceContext->CSSetUnorderedAccessViews( 0, _countof(UAVs), UAVs, nullptr );

    m_deviceContext->VSSetShader( nullptr, nullptr, 0 );
    m_deviceContext->IASetInputLayout( nullptr );
    m_deviceContext->GSSetShader( nullptr, nullptr, 0 );
    m_deviceContext->HSSetShader( nullptr, nullptr, 0 );
    m_deviceContext->DSSetShader( nullptr, nullptr, 0 );
    m_deviceContext->PSSetShader( nullptr, nullptr, 0 );
    m_deviceContext->CSSetShader( nullptr, nullptr, 0 );

    UpdateSceneDrawContext( nullptr );
}

void vaRenderDeviceContextDX11::UpdateSceneDrawContext( vaSceneDrawContext * sceneDrawContext )
{
    if( m_lastSceneDrawContext != sceneDrawContext )
    {
        if( sceneDrawContext == nullptr )
            UnsetSceneGlobals( m_lastSceneDrawContext );
        else
            SetSceneGlobals( sceneDrawContext );
        m_lastSceneDrawContext = sceneDrawContext;
    }
}

vaDrawResultFlags vaRenderDeviceContextDX11::ExecuteItem( const vaRenderItem & renderItem )
{
    // ExecuteTask can only be called in between BeginTasks and EndTasks - call ExecuteSingleItem 
    assert( m_itemsStarted );

    assert( renderItem.VertexShader != nullptr );

    UpdateSceneDrawContext( renderItem.SceneDrawContext );

    // SHADER(s)
    ID3D11VertexShader *    vertexShader        = ( renderItem.VertexShader     == nullptr )?( nullptr ):(renderItem.VertexShader->SafeCast<vaVertexShaderDX11*>()->GetShader());
    ID3D11InputLayout *     inputLayoutShader   = ( renderItem.VertexShader     == nullptr )?( nullptr ):(renderItem.VertexShader->SafeCast<vaVertexShaderDX11*>()->GetInputLayout());
    ID3D11GeometryShader *  geometryShader      = ( renderItem.GeometryShader   == nullptr )?( nullptr ):(renderItem.GeometryShader->SafeCast<vaGeometryShaderDX11*>()->GetShader());
    ID3D11HullShader *      hullShader          = ( renderItem.HullShader       == nullptr )?( nullptr ):(renderItem.HullShader->SafeCast<vaHullShaderDX11*>()->GetShader());
    ID3D11DomainShader *    domainShader        = ( renderItem.DomainShader     == nullptr )?( nullptr ):(renderItem.DomainShader->SafeCast<vaDomainShaderDX11*>()->GetShader());
    ID3D11PixelShader *     pixelShader         = ( renderItem.PixelShader      == nullptr )?( nullptr ):(renderItem.PixelShader->SafeCast<vaPixelShaderDX11*>()->GetShader());
    
    if( vertexShader   == nullptr       && renderItem.VertexShader  != nullptr && !renderItem.VertexShader->IsEmpty() )  
    { /*VA_WARN( "ExecuteItem() : vaRenderItem shader exists but not compiled (either compile error or still compiling)" );*/  return vaDrawResultFlags::ShadersStillCompiling; }
    if( inputLayoutShader == nullptr    && renderItem.VertexShader  != nullptr && !renderItem.VertexShader->IsEmpty() )  
    { /*VA_WARN( "ExecuteItem() : vaRenderItem shader exists but not compiled (either compile error or still compiling)" );*/  return vaDrawResultFlags::ShadersStillCompiling; }
    if( geometryShader == nullptr       && renderItem.GeometryShader!= nullptr && !renderItem.GeometryShader->IsEmpty() )  
    { /*VA_WARN( "ExecuteItem() : vaRenderItem shader exists but not compiled (either compile error or still compiling)" );*/  return vaDrawResultFlags::ShadersStillCompiling; }
    if( hullShader     == nullptr       && renderItem.HullShader   != nullptr && !renderItem.HullShader->IsEmpty() )  
    { /*VA_WARN( "ExecuteItem() : vaRenderItem shader exists but not compiled (either compile error or still compiling)" );*/  return vaDrawResultFlags::ShadersStillCompiling; }
    if( domainShader   == nullptr       && renderItem.DomainShader != nullptr && !renderItem.DomainShader->IsEmpty()  )  
    { /*VA_WARN( "ExecuteItem() : vaRenderItem shader exists but not compiled (either compile error or still compiling)" );*/  return vaDrawResultFlags::ShadersStillCompiling; }
    if( pixelShader    == nullptr       && renderItem.PixelShader  != nullptr && !renderItem.PixelShader->IsEmpty()   )  
    { /*VA_WARN( "ExecuteItem() : vaRenderItem shader exists but not compiled (either compile error or still compiling)" );*/  return vaDrawResultFlags::ShadersStillCompiling; }
    m_deviceContext->VSSetShader( vertexShader,     nullptr, 0 );
    m_deviceContext->IASetInputLayout( inputLayoutShader );
    m_deviceContext->GSSetShader( geometryShader,   nullptr, 0 );
    m_deviceContext->HSSetShader( hullShader,       nullptr, 0 );
    m_deviceContext->DSSetShader( domainShader,     nullptr, 0 );
    m_deviceContext->PSSetShader( pixelShader,      nullptr, 0 );

    // TOPOLOGY
    D3D_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    switch( renderItem.Topology )
    {   case vaPrimitiveTopology::PointList:        topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;        break;
        case vaPrimitiveTopology::LineList:         topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;         break;
        case vaPrimitiveTopology::TriangleList:     topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;     break;
        case vaPrimitiveTopology::TriangleStrip:    topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;    break;
        default: assert( false ); break;
    }
    m_deviceContext->IASetPrimitiveTopology( topology );

    // BLENDING 
    ID3D11BlendState * blendState = DX11BlendModeFromVABlendMode( renderItem.BlendMode );
    float blendFactor[4] = { 0, 0, 0, 0 };
    m_deviceContext->OMSetBlendState( blendState, blendFactor, 0xFFFFFFFF );

    // DEPTH
    CD3D11_DEPTH_STENCIL_DESC dsdesc = CD3D11_DEPTH_STENCIL_DESC( CD3D11_DEFAULT() );
    dsdesc.DepthEnable      = renderItem.DepthEnable;
    dsdesc.DepthWriteMask   = (renderItem.DepthWriteEnable)?(D3D11_DEPTH_WRITE_MASK_ALL):(D3D11_DEPTH_WRITE_MASK_ZERO);
    dsdesc.DepthFunc        = (D3D11_COMPARISON_FUNC)renderItem.DepthFunc;
    m_deviceContext->OMSetDepthStencilState( vaDirectXTools::FindOrCreateDepthStencilState( m_device, dsdesc ), 0 );
    
    // FILLMODE, CULLMODE, MISC RASTERIZER
    CD3D11_RASTERIZER_DESC rastdesc = CD3D11_RASTERIZER_DESC( CD3D11_DEFAULT() );
    rastdesc.CullMode               = (renderItem.CullMode == vaFaceCull::None)?(D3D11_CULL_NONE):( (renderItem.CullMode == vaFaceCull::Front)?(D3D11_CULL_FRONT):(D3D11_CULL_BACK) );
    rastdesc.FillMode               = (renderItem.FillMode == vaFillMode::Solid)?(D3D11_FILL_SOLID):(D3D11_FILL_WIREFRAME);
    rastdesc.FrontCounterClockwise  = renderItem.FrontCounterClockwise;
    rastdesc.MultisampleEnable      = true;
    rastdesc.ScissorEnable          = true;
    if( m_outputsState.RenderTargets[0] != nullptr )
        rastdesc.MultisampleEnable = m_outputsState.RenderTargets[0]->GetSampleCount() > 1;
    m_deviceContext->RSSetState( vaDirectXTools::FindOrCreateRasterizerState( m_device, rastdesc ) );

    // SAMPLERS
    // set in BeginTasks for now

    // CONSTANT BUFFERS, SRVs
    ID3D11Buffer * constantBuffers[ _countof(vaRenderItem::ConstantBuffers) ];
    for( int i = 0; i < _countof(vaRenderItem::ConstantBuffers); i++ )
        constantBuffers[i] = (renderItem.ConstantBuffers[i]==nullptr)?(nullptr):(renderItem.ConstantBuffers[i]->SafeCast<vaConstantBufferDX11*>()->GetBuffer());
    if( vertexShader    != nullptr )   m_deviceContext->VSSetConstantBuffers( 0, _countof(constantBuffers), constantBuffers );
    if( geometryShader  != nullptr )   m_deviceContext->GSSetConstantBuffers( 0, _countof(constantBuffers), constantBuffers );
    if( hullShader      != nullptr )   m_deviceContext->HSSetConstantBuffers( 0, _countof(constantBuffers), constantBuffers );
    if( domainShader    != nullptr )   m_deviceContext->DSSetConstantBuffers( 0, _countof(constantBuffers), constantBuffers );
    if( pixelShader     != nullptr )   m_deviceContext->PSSetConstantBuffers( 0, _countof(constantBuffers), constantBuffers );
    ID3D11ShaderResourceView * SRVs[ _countof(vaRenderItem::ShaderResourceViews) ];
    for( int i = 0; i < _countof(vaRenderItem::ShaderResourceViews); i++ )
        SRVs[i] = (renderItem.ShaderResourceViews[i]==nullptr)?(nullptr):(renderItem.ShaderResourceViews[i]->SafeCast<vaShaderResourceSRVDX11*>()->GetSRV());
    if( vertexShader    != nullptr )   m_deviceContext->VSSetShaderResources( 0, _countof(SRVs), SRVs );
    if( geometryShader  != nullptr )   m_deviceContext->GSSetShaderResources( 0, _countof(SRVs), SRVs );
    if( hullShader      != nullptr )   m_deviceContext->HSSetShaderResources( 0, _countof(SRVs), SRVs );
    if( domainShader    != nullptr )   m_deviceContext->DSSetShaderResources( 0, _countof(SRVs), SRVs );
    if( pixelShader     != nullptr )   m_deviceContext->PSSetShaderResources( 0, _countof(SRVs), SRVs );

    // VERTICES/INDICES
    if( renderItem.VertexBuffer != nullptr )
    {
        ID3D11Buffer * vertexBuffer = renderItem.VertexBuffer->SafeCast<vaVertexBufferDX11*>()->GetBuffer();
        UINT vertexBufferStride = (renderItem.VertexBufferByteStride==0)?(renderItem.VertexBuffer->GetByteStride()):(renderItem.VertexBufferByteStride);
        UINT vertexBufferOffset = renderItem.VertexBufferByteOffset;
        m_deviceContext->IASetVertexBuffers( 0, 1, &vertexBuffer, &vertexBufferStride, &vertexBufferOffset );
    }
    else
    {
        m_deviceContext->IASetVertexBuffers( 0, 0, nullptr, nullptr, nullptr );
    }
    //
    ID3D11Buffer * indexBuffer = (renderItem.IndexBuffer==nullptr)?(nullptr):(renderItem.IndexBuffer->SafeCast<vaIndexBufferDX11*>()->GetBuffer());
    m_deviceContext->IASetIndexBuffer( indexBuffer, (indexBuffer!=nullptr)?(DXGI_FORMAT_R32_UINT):(DXGI_FORMAT_UNKNOWN), 0 );

    bool continueWithDraw = true;
    if( renderItem.PreDrawHook != nullptr )
        continueWithDraw = renderItem.PreDrawHook( renderItem, *this );

    if( continueWithDraw )
    {
        switch( renderItem.DrawType )
        {
        case( vaRenderItem::DrawSimple ): 
            m_deviceContext->Draw( renderItem.DrawSimpleParams.VertexCount, renderItem.DrawSimpleParams.StartVertexLocation );
            break;
        case( vaRenderItem::DrawIndexed ): 
            m_deviceContext->DrawIndexed( renderItem.DrawIndexedParams.IndexCount, renderItem.DrawIndexedParams.StartIndexLocation, renderItem.DrawIndexedParams.BaseVertexLocation );
            break;
        default:
            assert( false );
            break;
        }
    }

    if( renderItem.PostDrawHook != nullptr )
        renderItem.PostDrawHook( renderItem, *this );

    // for caching - not really needed for now
    // m_lastRenderItem = renderItem;
    return vaDrawResultFlags::None;
}

vaDrawResultFlags vaRenderDeviceContextDX11::ExecuteItem( const vaComputeItem & computeItem )
{
    // ExecuteTask can only be called in between BeginTasks and EndTasks - call ExecuteSingleItem 
    assert( m_itemsStarted );

    assert( computeItem.ComputeShader != nullptr );

    UpdateSceneDrawContext( computeItem.SceneDrawContext );


    // SHADER(s)
    ID3D11ComputeShader *   computeShader       = (computeItem.ComputeShader==nullptr)?(nullptr):(computeItem.ComputeShader->SafeCast<vaComputeShaderDX11*>()->GetShader());
    if( computeShader == nullptr )
    {
        // VA_WARN( "ExecuteItem() : vaComputeItem shader exists but not compiled (either compile error or still compiling)" );
        return vaDrawResultFlags::ShadersStillCompiling;
    }
    m_deviceContext->CSSetShader( computeShader, nullptr, 0 );

    // SAMPLERS
    // set in BeginTasks for now

    // CONSTANT BUFFERS, SRVs, UAVs
    ID3D11Buffer * constantBuffers[ _countof(vaRenderItem::ConstantBuffers) ];
    for( int i = 0; i < _countof(vaRenderItem::ConstantBuffers); i++ )
        constantBuffers[i] = (computeItem.ConstantBuffers[i]==nullptr)?(nullptr):(computeItem.ConstantBuffers[i]->SafeCast<vaConstantBufferDX11*>()->GetBuffer());
    m_deviceContext->CSSetConstantBuffers( 0, _countof(constantBuffers), constantBuffers );
    //
    ID3D11UnorderedAccessView * UAVs[ _countof(vaComputeItem::UnorderedAccessViews) ];
    for( int i = 0; i < _countof(vaComputeItem::UnorderedAccessViews); i++ )
        UAVs[i] = (computeItem.UnorderedAccessViews[i]==nullptr)?(nullptr):(computeItem.UnorderedAccessViews[i]->SafeCast<vaShaderResourceSRVDX11*>()->GetUAV());
    m_deviceContext->CSSetUnorderedAccessViews( 0, _countof(UAVs), UAVs, nullptr );
    //
    ID3D11ShaderResourceView * SRVs[ _countof(vaRenderItem::ShaderResourceViews) ];
    for( int i = 0; i < _countof(vaRenderItem::ShaderResourceViews); i++ )
        SRVs[i] = (computeItem.ShaderResourceViews[i]==nullptr)?(nullptr):(computeItem.ShaderResourceViews[i]->SafeCast<vaShaderResourceSRVDX11*>()->GetSRV());
    m_deviceContext->CSSetShaderResources( 0, _countof(SRVs), SRVs );

    bool continueWithDraw = true;
    if( computeItem.PreComputeHook != nullptr )
    {
        assert( false ); // well this was never tested so... step through, think of any implications - messing up caching of states or something? how to verify that? etc.
        continueWithDraw = computeItem.PreComputeHook( computeItem, *this );
    }

    if( continueWithDraw )
    {
        switch( computeItem.ComputeType )
        {
        case( vaComputeItem::Dispatch ): 
            m_deviceContext->Dispatch( computeItem.DispatchParams.ThreadGroupCountX, computeItem.DispatchParams.ThreadGroupCountY, computeItem.DispatchParams.ThreadGroupCountZ );
            break;
        case( vaComputeItem::DispatchIndirect ): 
            assert( computeItem.DispatchIndirectParams.BufferForArgs != nullptr );
            m_deviceContext->DispatchIndirect( computeItem.DispatchIndirectParams.BufferForArgs->SafeCast<vaShaderResourceSRVDX11*>()->GetBuffer(), computeItem.DispatchIndirectParams.AlignedOffsetForArgs );
            break;
        default:
            assert( false );
            break;
        }
    }

    // // API-SPECIFIC MODIFIER CALLBACKS
    // std::function<bool( RenderItem & )> PreDrawHook;

    // for caching - not really needed for now
    // m_lastRenderItem = renderItem;

    return vaDrawResultFlags::None;
}

void vaRenderDeviceContextDX11::ImGuiCreate( )
{
    vaRenderDeviceContext::ImGuiCreate();

#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ID3D11Device * device = nullptr;
    m_deviceContext->GetDevice(&device);
    ImGui_ImplDX11_Init( GetRenderDevice().SafeCast<vaRenderDeviceDX11*>()->GetHWND( ), device, m_deviceContext );
    SAFE_RELEASE( device );
   ImGui_ImplDX11_CreateDeviceObjects();
#endif
}
void vaRenderDeviceContextDX11::ImGuiDestroy( )
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGui_ImplDX11_InvalidateDeviceObjects();
    ImGui_ImplDX11_Shutdown( );
#endif
    vaRenderDeviceContext::ImGuiDestroy();
}

void vaRenderDeviceContextDX11::ImGuiNewFrame( )
{
    assert( !m_imguiFrameStarted ); // forgot to call ImGuiDraw? you must not do that!
    m_imguiFrameStarted = true;

//            // hacky mouse handling, but good for now
//            bool dontTouchMyCursor = vaInputMouseBase::GetCurrent( )->IsCaptured( );

    ImGui_ImplDX11_NewFrame( ); //dontTouchMyCursor );
}

void vaRenderDeviceContextDX11::ImGuiDraw( )
{
    assert( m_imguiFrameStarted ); // forgot to call ImGuiNewFrame? you must not do that!

    assert( GetRenderDevice().ImGuiIsVisible( ) );

    ImGui::Render();
    IMGUI_API void        ImGui_ImplDX11_RenderDrawData(ImDrawData* draw_data);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    {
        VA_SCOPE_CPUGPU_TIMER( UITextures, *this );
        GetRenderDevice().GetTextureTools().UIDrawImages( *this );
    }
    m_imguiFrameStarted = false;
}

void vaRenderDeviceContextDX11::CheckNoStatesSet( )
{
    // stuff like 
    //vaDirectXTools::AssertSetToD3DContextAllShaderTypes( dx11Context, ( ID3D11ShaderResourceView* )nullptr, RENDERMESH_TEXTURE_SLOT0 );
    //vaDirectXTools::AssertSetToD3DContextAllShaderTypes( dx11Context, ( ID3D11ShaderResourceView* )nullptr, RENDERMESH_TEXTURE_SLOT1 );
    //vaDirectXTools::AssertSetToD3DContextAllShaderTypes( dx11Context, ( ID3D11ShaderResourceView* )nullptr, RENDERMESH_TEXTURE_SLOT2 );
    //vaDirectXTools::AssertSetToD3DContextAllShaderTypes( dx11Context, ( ID3D11ShaderResourceView* )nullptr, RENDERMESH_TEXTURE_SLOT3 );
    //vaDirectXTools::AssertSetToD3DContextAllShaderTypes( dx11Context, ( ID3D11ShaderResourceView* )nullptr, RENDERMESH_TEXTURE_SLOT4 );
    //vaDirectXTools::AssertSetToD3DContextAllShaderTypes( dx11Context, ( ID3D11ShaderResourceView* )nullptr, RENDERMESH_TEXTURE_SLOT5 );
    //vaDirectXTools::AssertSetToD3DContextAllShaderTypes( dx11Context, ( ID3D11Buffer* ) nullptr, RENDERMESH_CONSTANTS_BUFFERSLOT );
    //vaDirectXTools::AssertSetToD3DContextAllShaderTypes( dx11Context, ( ID3D11Buffer* ) nullptr, RENDERMESHMATERIAL_CONSTANTS_BUFFERSLOT );

}


void RegisterDeviceContextDX11( )
{
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaRenderDeviceContext, vaRenderDeviceContextDX11 );
}


