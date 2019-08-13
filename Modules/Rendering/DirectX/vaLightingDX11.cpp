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

#include "Rendering/DirectX/vaLightingDX11.h"

#include "Rendering/DirectX/vaDirectXIncludes.h"

#include "Rendering/DirectX/vaRenderDeviceContextDX11.h"
#include "Rendering/DirectX/vaTextureDX11.h"
#include "Rendering/DirectX/vaRenderBuffersDX11.h"

using namespace VertexAsylum;

vaLightingDX11::vaLightingDX11( const vaRenderingModuleParams & params ) : vaLighting( params )
{
    params; // unreferenced
}

vaLightingDX11::~vaLightingDX11( )
{
}

void vaLightingDX11::SetAPIGlobals( vaSceneDrawContext & drawContext )
{
    if( m_envmapTexture != nullptr )
        m_envmapTexture->SafeCast<vaTextureDX11*>()->SetToAPISlotSRV( drawContext.RenderDeviceContext, SHADERGLOBAL_LIGHTING_ENVMAP_TEXTURESLOT );
    if( m_shadowmapTexturesCreated && m_shadowCubeArrayTexture != nullptr )
        m_shadowCubeArrayTexture->SafeCast<vaTextureDX11*>()->SetToAPISlotSRV( drawContext.RenderDeviceContext, SHADERGLOBAL_LIGHTING_CUBE_SHADOW_TEXTURESLOT );

    UpdateShaderConstants( drawContext );

    m_constantsBuffer.GetBuffer()->SafeCast<vaConstantBufferDX11*>()->SetToAPISlot( drawContext.RenderDeviceContext, LIGHTINGGLOBAL_CONSTANTSBUFFERSLOT );
}

void vaLightingDX11::UnsetAPIGlobals( vaSceneDrawContext & drawContext )
{
    if( m_envmapTexture != nullptr )
        m_envmapTexture->SafeCast<vaTextureDX11*>()->UnsetFromAPISlotSRV( drawContext.RenderDeviceContext, SHADERGLOBAL_LIGHTING_ENVMAP_TEXTURESLOT );
    if( m_shadowmapTexturesCreated && m_shadowCubeArrayTexture != nullptr )
        m_shadowCubeArrayTexture->SafeCast<vaTextureDX11*>()->UnsetFromAPISlotSRV( drawContext.RenderDeviceContext, SHADERGLOBAL_LIGHTING_CUBE_SHADOW_TEXTURESLOT );

    m_constantsBuffer.GetBuffer()->SafeCast<vaConstantBufferDX11*>()->UnsetFromAPISlot( drawContext.RenderDeviceContext, LIGHTINGGLOBAL_CONSTANTSBUFFERSLOT );
}

void vaLightingDX11::UpdateResourcesIfNeeded( vaSceneDrawContext & drawContext )
{
    vaLighting::UpdateResourcesIfNeeded( drawContext );
}

// draws provided depthTexture (can be the one obtained using GetDepthBuffer( )) into currently selected RT; relies on settings set in vaRenderGlobals and will assert and return without doing anything if those are not present
void vaLightingDX11::ApplyDirectionalAmbientLighting( vaSceneDrawContext & drawContext, vaGBuffer & GBuffer )
{
    assert( false ); // re-implement
    drawContext;
    GBuffer;

#if 0
    // !!!!!!!!!!! this should be ported to be platform independent....!!!!!!!!!!!!
    // !!!!!!!!!!! this should be ported to be platform independent....!!!!!!!!!!!!
    // !!!!!!!!!!! this should be ported to be platform independent....!!!!!!!!!!!!
    UpdateResourcesIfNeeded( drawContext );
    assert( !m_shadersDirty );                              if( m_shadersDirty ) return;
    
    vaPixelShaderDX11    * pixelShader = m_applyDirectionalAmbientPS->SafeCast<vaPixelShaderDX11*>();

    // // Shadows?
    // vaSimpleShadowMapDX11 * simpleShadowMapDX11 = NULL;
    // if( drawContext.SimpleShadowMap != NULL )
    // {
    //     simpleShadowMapDX11 = vaSaferStaticCast<vaSimpleShadowMapDX11*>( drawContext.SimpleShadowMap );
    //     pixelShader = &m_applyDirectionalAmbientShadowedPS;
    // }

    vaRenderDeviceContextDX11 * apiContext = drawContext.RenderDeviceContext.SafeCast<vaRenderDeviceContextDX11*>( );
    ID3D11DeviceContext * dx11Context = apiContext->GetDXContext( );

    // make sure we're not overwriting someone's stuff
    vaDirectXTools11::AssertSetToD3DContextAllShaderTypes( dx11Context, ( ID3D11ShaderResourceView* )nullptr, LIGHTING_SLOT0 );
    vaDirectXTools11::AssertSetToD3DContextAllShaderTypes( dx11Context, ( ID3D11ShaderResourceView* )nullptr, LIGHTING_SLOT1 );
    vaDirectXTools11::AssertSetToD3DContextAllShaderTypes( dx11Context, ( ID3D11ShaderResourceView* )nullptr, LIGHTING_SLOT2 );

    // gbuffer stuff
    vaDirectXTools11::SetToD3DContextAllShaderTypes( dx11Context, GBuffer.GetDepthBufferViewspaceLinear( )->SafeCast<vaTextureDX11*>( )->GetSRV( ),   LIGHTING_SLOT0 );
    vaDirectXTools11::SetToD3DContextAllShaderTypes( dx11Context, GBuffer.GetAlbedo( )->SafeCast<vaTextureDX11*>( )->GetSRV( ),                       LIGHTING_SLOT1 );
    vaDirectXTools11::SetToD3DContextAllShaderTypes( dx11Context, GBuffer.GetNormalMap( )->SafeCast<vaTextureDX11*>( )->GetSRV( ),                    LIGHTING_SLOT2 );

    // draw but only on things that are already in the zbuffer
    // !!!!!!!!!!! this should be ported to be platform independent....!!!!!!!!!!!!
    apiContext->FullscreenPassDraw( pixelShader->GetShader(), vaDirectXTools11::GetBS_Additive(), vaDirectXTools11::GetDSS_DepthEnabledG_NoDepthWrite(), 0, 1.0f );
    // !!!!!!!!!!! this should be ported to be platform independent....!!!!!!!!!!!!

    //    Reset, leave stuff clean
    vaDirectXTools11::SetToD3DContextAllShaderTypes( dx11Context, (ID3D11ShaderResourceView*) nullptr,  LIGHTING_SLOT0 );
    vaDirectXTools11::SetToD3DContextAllShaderTypes( dx11Context, (ID3D11ShaderResourceView*) nullptr,  LIGHTING_SLOT1 );
    vaDirectXTools11::SetToD3DContextAllShaderTypes( dx11Context, (ID3D11ShaderResourceView*) nullptr,  LIGHTING_SLOT2 );
    // !!!!!!!!!!! this should be ported to be platform independent....!!!!!!!!!!!!
    // !!!!!!!!!!! this should be ported to be platform independent....!!!!!!!!!!!!
    // !!!!!!!!!!! this should be ported to be platform independent....!!!!!!!!!!!!
#endif
}

void vaLightingDX11::ApplyDynamicLighting( vaSceneDrawContext & drawContext, vaGBuffer & GBuffer )
{
drawContext;
GBuffer;
assert( false );
}


void RegisterLightingDX11( )
{
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaLighting, vaLightingDX11 );
}
