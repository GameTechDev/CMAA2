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

#include "Rendering/DirectX/vaDirectXTools.h"
#include "Rendering/DirectX/vaShaderDX11.h"

#include "Rendering/vaRenderingIncludes.h"

#include "Rendering/DirectX/vaRenderingToolsDX11.h"

#include "Rendering/DirectX/vaTextureDX11.h"
#include "Rendering/DirectX/vaTriangleMeshDX11.h"
#include "Rendering/DirectX/vaRenderDeviceContextDX11.h"

#include "Rendering/DirectX/vaRenderBuffersDX11.h"

using namespace VertexAsylum;


namespace VertexAsylum
{

    class vaSimpleParticleSystemDX11 : public vaSimpleParticleSystem
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    private:
        vaAutoRMI<vaVertexShader>               m_vertexShader;
        vaAutoRMI<vaPixelShader>                m_pixelShader;
        vaAutoRMI<vaPixelShader>                m_pixelShaderShadowed;
        vaAutoRMI<vaGeometryShader>             m_geometryShader;
        vaAutoRMI<vaGeometryShader>             m_geometryShaderGenerateVolumeShadow;
        vaAutoRMI<vaPixelShader>                m_pixelShaderGenerateVolumeShadow;

        bool                                    m_shadersDirty;
        
        vaTypedConstantBufferWrapper< ShaderSimpleParticleSystemConstants >
                                                m_shaderConstants;

        vaTypedVertexBufferWrapper< vaBillboardSprite >
                                                m_dynamicBuffer;
        static const int                        m_dynamicBufferMaxElementCount      = 256 * 1024;
        int                                     m_dynamicBufferCurrentlyUsed;

        std::vector< std::pair< std::string, std::string > > 
                                                m_staticShaderMacros;
        std::vector< std::pair< std::string, std::string > > 
                                                m_staticShaderMacrosGen;

        int                                     m_buffersLastUpdateTickID;

        int                                     m_buffersLastCountToDraw;
        int                                     m_buffersLastOffsetInVertices;


    private:
        explicit vaSimpleParticleSystemDX11( const vaRenderingModuleParams & params );
        virtual ~vaSimpleParticleSystemDX11( );

    public:

    private:
        virtual void                            Draw( vaSceneDrawContext & drawContext ) override;

    };


}

vaSimpleParticleSystemDX11::vaSimpleParticleSystemDX11(  const vaRenderingModuleParams & params )
    : vaSimpleParticleSystem( params ),
    m_dynamicBuffer( params, m_dynamicBufferMaxElementCount, nullptr, true ),
    m_vertexShader( params ),
    m_pixelShader( params ),
    m_pixelShaderShadowed( params ),
    m_geometryShader( params ),
    m_geometryShaderGenerateVolumeShadow( params ),
    m_pixelShaderGenerateVolumeShadow( params ),
    m_shaderConstants( params )
{ 
    params; // unreferenced

    m_shadersDirty = true;

    //m_dynamicBufferMaxElementCount  = 256 * 1024;
    m_dynamicBufferCurrentlyUsed    = 0;

    m_buffersLastUpdateTickID       = 0;

    m_buffersLastCountToDraw        = 0;
    m_buffersLastOffsetInVertices   = 0;

    m_staticShaderMacros.push_back( std::pair<std::string, std::string>( "INVALID", "INVALID" ) );

    m_dynamicBufferCurrentlyUsed    = 0;
}

vaSimpleParticleSystemDX11::~vaSimpleParticleSystemDX11( )
{ 
}

void vaSimpleParticleSystemDX11::Draw( vaSceneDrawContext & drawContext )
{
    drawContext;

    assert( false );
    // update below to vaRenderItem path

#if 0
    if( m_particles.size() == 0 )
        return;

    std::vector< std::pair< std::string, std::string > > newStaticShaderMacros;

    // if( (drawContext.SimpleShadowMap != NULL) && (drawContext.SimpleShadowMap->GetVolumeShadowMapPlugin() != NULL) )
    //     newStaticShaderMacros.insert( newStaticShaderMacros.end(), drawContext.SimpleShadowMap->GetVolumeShadowMapPlugin()->GetShaderMacros().begin(), drawContext.SimpleShadowMap->GetVolumeShadowMapPlugin()->GetShaderMacros().end() );

    if( newStaticShaderMacros != m_staticShaderMacros )
    {
        m_staticShaderMacros = newStaticShaderMacros;
        m_staticShaderMacrosGen = m_staticShaderMacros;
        m_staticShaderMacrosGen.push_back( std::pair<std::string, std::string>( "SHADOWS_GENERATE", "" ) );
        m_shadersDirty = true;
    }

    if( m_shadersDirty )
    {
        m_shadersDirty = false;

        std::vector<vaVertexInputElementDesc> inputElements;
        inputElements.push_back( { "SV_Position", 0,    vaResourceFormat::R32G32B32A32_FLOAT, 0, vaVertexInputElementDesc::AppendAlignedElement, vaVertexInputElementDesc::InputClassification::PerVertexData, 0 } );
        inputElements.push_back( { "TEXCOORD", 0,       vaResourceFormat::R32G32B32A32_FLOAT, 0, vaVertexInputElementDesc::AppendAlignedElement, vaVertexInputElementDesc::InputClassification::PerVertexData, 0 } );
        inputElements.push_back( { "COLOR", 0,          vaResourceFormat::R8G8B8A8_UNORM, 0,     vaVertexInputElementDesc::AppendAlignedElement, vaVertexInputElementDesc::InputClassification::PerVertexData, 0 } );

        m_vertexShader->CreateShaderAndILFromFile( L"vaSimpleParticles.hlsl", "vs_5_0", "SimpleParticleVS", inputElements, m_staticShaderMacros );
        m_geometryShader->CreateShaderFromFile( L"vaSimpleParticles.hlsl", "gs_5_0", "SimpleParticleGS", m_staticShaderMacros );
        m_pixelShader->CreateShaderFromFile( L"vaSimpleParticles.hlsl", "ps_5_0", "SimpleParticlePS", m_staticShaderMacros );
        m_pixelShaderShadowed->CreateShaderFromFile( L"vaSimpleParticles.hlsl", "ps_5_0", "SimpleParticleShadowedPS", m_staticShaderMacros );
        m_geometryShaderGenerateVolumeShadow->CreateShaderFromFile( L"vaSimpleParticles.hlsl", "gs_5_0", "SimpleParticleGS", m_staticShaderMacrosGen );
        m_pixelShaderGenerateVolumeShadow->CreateShaderFromFile( L"vaSimpleParticles.hlsl", "ps_5_0", "SimpleParticleGenerateVolumeShadowPS", m_staticShaderMacrosGen );
    }
    
    // if( (m_vertexShader->IsCreated() == NULL) || (m_geometryShader->.GetShader() == NULL) )
    //     return;

    // if( (drawType != vaDrawType::Transparent) ) //&& (drawType != vaDrawType::ShadowmapGenerate) )
    // {
    //     // not yet supported
    //     assert( false );
    //     return;
    // }

    // if( /*(drawType == vaDrawType::ShadowmapGenerate) &&*/ ((drawContext.SimpleShadowMap == NULL) || (drawContext.SimpleShadowMap->GetVolumeShadowMapPlugin() == NULL ) ) )
    // {
    //     // volumetric shadow map plugin must be present in case generating shadowmap!
    //     assert( false );
    //     return;
    // }

    ID3D11DeviceContext * dx11Context = vaSaferStaticCast< vaRenderDeviceContextDX11 * >( &drawContext.APIContext )->GetDXContext( );

    if( m_buffersLastUpdateTickID != GetLastTickID() )
    {
        const std::vector<vaSimpleParticle> & particles = GetParticles( );

        m_buffersLastCountToDraw = (int)particles.size( );

        if( m_buffersLastCountToDraw > m_dynamicBufferMaxElementCount )
        {
            // buffer not even big enough for one draw
            assert( false );
            return;
        }
        if( m_buffersLastCountToDraw > (m_dynamicBufferMaxElementCount * 0.4) )
        {
            // you might want to consider increasing the m_dynamicBufferMaxElementCount
            assert( false );
        }

        D3D11_MAPPED_SUBRESOURCE map;
        HRESULT hr;

        ID3D11Buffer * vertexBufferDX11 = m_dynamicBuffer.GetBuffer( )->SafeCast<vaVertexBufferDX11*>( )->GetBuffer( );

        // discard?
        if( (m_dynamicBufferCurrentlyUsed + m_buffersLastCountToDraw) > m_dynamicBufferMaxElementCount )
        {
            hr = dx11Context->Map( vertexBufferDX11, 0, D3D11_MAP_WRITE_DISCARD, 0, &map );
            m_dynamicBufferCurrentlyUsed = 0;
        }
        else
        {
            hr = dx11Context->Map( vertexBufferDX11, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &map );
        }
        assert( SUCCEEDED( hr ) );
        if( !SUCCEEDED( hr ) )
            return;

        vaBillboardSprite * destinationBuffer = &( (vaBillboardSprite *)map.pData )[m_dynamicBufferCurrentlyUsed];

        m_buffersLastOffsetInVertices = m_dynamicBufferCurrentlyUsed;

        m_dynamicBufferCurrentlyUsed += m_buffersLastCountToDraw;

        if( !delegate_drawBufferUpdateShader.empty( ) )
            delegate_drawBufferUpdateShader( *this, particles, GetSortedIndices(), destinationBuffer, m_buffersLastCountToDraw * sizeof(vaBillboardSprite) );

        dx11Context->Unmap( vertexBufferDX11, 0 );
    }
    
//    return;
    // make sure we're not overwriting anything else, and set our constant buffers
    m_shaderConstants.SetToAPISlot( drawContext.APIContext, SIMPLEPARTICLESYSTEM_CONSTANTS_BUFFERSLOT );

    // update per-instance constants
    {
        //const Tree::Settings & settings = m_tree.GetSettings( );
        ShaderSimpleParticleSystemConstants consts;

        vaMatrix4x4 trans = GetTransform( );

        consts.World        = trans;

        // if( drawType == vaDrawType::ShadowmapGenerate )
        // {
        //     consts.WorldView    = consts.World * drawContext.SimpleShadowMap->GetViewMatrix();
        //     consts.Proj         = drawContext.SimpleShadowMap->GetProjMatrix();
        // }
        // else
        {
            consts.WorldView    = consts.World * drawContext.Camera.GetViewMatrix( );
            consts.Proj         = drawContext.Camera.GetProjMatrix( );
        }

        m_shaderConstants.Update( drawContext.APIContext, consts );
    }

    // float blendFactor[4] = { 0, 0, 0, 0 };

    m_vertexShader->SetToAPI( drawContext.APIContext );

    // if( drawType == vaDrawType::ShadowmapGenerate )
    // {
    //     dx11Context->GSSetShader( m_geometryShaderGenerateVolumeShadow.GetShader( ), NULL, 0 );
    //     dx11Context->PSSetShader( m_pixelShaderGenerateVolumeShadow.GetShader( ), NULL, 0 );
    // }
    // else
    {
        dx11Context->GSSetShader( m_geometryShader->SafeCast<vaGeometryShaderDX11*>()->GetShader( ), NULL, 0 );
        //if( drawContext.SimpleShadowMap != NULL )
        //    dx11Context->PSSetShader( m_pixelShaderShadowed.GetShader( ), NULL, 0 );
        //else
            dx11Context->PSSetShader( m_pixelShader->SafeCast<vaPixelShaderDX11*>()->GetShader( ), NULL, 0 );
    }

    assert( false );
    /*
    if( (drawContext.Flags & vaDrawContextFlags::TransparencyPass) != 0 )
    {
        dx11Context->OMSetBlendState( vaDirectXTools::GetBS_AlphaBlend( ), blendFactor, 0xFFFFFFFF );
        dx11Context->OMSetDepthStencilState( (drawContext.Camera.GetUseReversedZ())?( vaDirectXTools::GetDSS_DepthEnabledG_NoDepthWrite( ) ):( vaDirectXTools::GetDSS_DepthEnabledL_NoDepthWrite( ) ), 0 );
    }
    else
    {
        dx11Context->OMSetBlendState( vaDirectXTools::GetBS_Opaque( ), blendFactor, 0xFFFFFFFF );
        dx11Context->OMSetDepthStencilState( (drawContext.Camera.GetUseReversedZ())?( vaDirectXTools::GetDSS_DepthEnabledG_NoDepthWrite( ) ):( vaDirectXTools::GetDSS_DepthEnabledL_NoDepthWrite( ) ), 0 );
    }

    if( (drawContext.Flags & vaDrawContextFlags::DebugWireframePass) != 0 )
    {
        assert( false );
        dx11Context->OMSetDepthStencilState( (drawContext.Camera.GetUseReversedZ())?( vaDirectXTools::GetDSS_DepthEnabledGE_NoDepthWrite( ) ):( vaDirectXTools::GetDSS_DepthEnabledLE_NoDepthWrite( ) ), 0 );
    }//
    */

    // Samplers
    dx11Context->PSSetSamplers( 0, 1, vaDirectXTools::GetSamplerStatePtrAnisotropicClamp( ) );
    dx11Context->PSSetSamplers( 1, 1, vaDirectXTools::GetSamplerStatePtrAnisotropicWrap( ) );

    dx11Context->RSSetState( vaDirectXTools::GetRS_CullNone_Fill( ) );

//        vaWindingOrder backfaceCull = renderableSceneMesh->GetBackfaceCull( );
//        if( drawContext.Wireframe )
//        {
//            switch( backfaceCull )
//            {
//            case VertexAsylum::vaWindingOrder::None:             dx11Context->RSSetState( vaDirectXTools::GetRS_CullNone_Wireframe( ) );  break;
//            case VertexAsylum::vaWindingOrder::Clockwise:        dx11Context->RSSetState( vaDirectXTools::GetRS_CullCW_Wireframe( ) );    break;
//            case VertexAsylum::vaWindingOrder::CounterClockwise: dx11Context->RSSetState( vaDirectXTools::GetRS_CullCCW_Wireframe( ) );   break;
//            default: break;
//            }
//        }
//        else
//        {
//            switch( backfaceCull )
//            {
//            case VertexAsylum::vaWindingOrder::None:             dx11Context->RSSetState( vaDirectXTools::GetRS_CullNone_Fill( ) );  break;
//            case VertexAsylum::vaWindingOrder::Clockwise:        dx11Context->RSSetState( vaDirectXTools::GetRS_CullCW_Fill( ) );    break;
//            case VertexAsylum::vaWindingOrder::CounterClockwise: dx11Context->RSSetState( vaDirectXTools::GetRS_CullCCW_Fill( ) );   break;
//            default: break;
//            }
//        }

    // Textures
    const std::shared_ptr<vaTexture> &  textureColor     = GetTexture();
    std::shared_ptr<vaTexture>          textureHeight    = NULL;
    std::shared_ptr<vaTexture>          textureNormalmap = NULL;
    
    ID3D11ShaderResourceView *      textures[3] = { NULL, NULL, NULL };
    if( textureColor != NULL )      textures[0] = vaSaferStaticCast<vaTextureDX11*>(textureColor.get())->GetSRV( );
    if( textureHeight != NULL )     textures[1] = vaSaferStaticCast<vaTextureDX11*>(textureHeight.get())->GetSRV( );
    if( textureNormalmap!= NULL )   textures[2] = vaSaferStaticCast<vaTextureDX11*>(textureNormalmap.get())->GetSRV( );
        
    dx11Context->PSSetShaderResources( 0, 3, textures );

    m_dynamicBuffer.SetToAPI( drawContext.APIContext, 0, 0 ); 

    dx11Context->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_POINTLIST );

    int countToDraw = (m_debugParticleDrawCountLimit>0)?(m_debugParticleDrawCountLimit):(m_buffersLastCountToDraw);

    while( countToDraw > 0 )
    {
        int countToReallyDraw = vaMath::Min( m_buffersLastCountToDraw, countToDraw );
        dx11Context->Draw( countToReallyDraw, m_buffersLastOffsetInVertices );
        countToDraw -= m_buffersLastCountToDraw;
    }

    // Reset
    ID3D11ShaderResourceView * nullTextures[3] = { NULL, NULL, NULL };
    dx11Context->GSSetShader( NULL, NULL, 0 );
    dx11Context->GSSetShaderResources( 0, 3, nullTextures );
    dx11Context->PSSetShader( NULL, NULL, 0 );
    dx11Context->PSSetShaderResources( 0, 3, nullTextures );
    //ID3D11Buffer * nullBuffers[1] = { NULL };

    m_vertexShader->UnsetFromAPI( drawContext.APIContext );

    m_buffersLastUpdateTickID = GetLastTickID( );

    // make sure nothing messed with our constant buffers and nothing uses them after
    m_shaderConstants.UnsetFromAPISlot( drawContext.APIContext, SIMPLEPARTICLESYSTEM_CONSTANTS_BUFFERSLOT );

#endif
}

void RegisterSimpleParticleSystemDX11( )
{
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaSimpleParticleSystem, vaSimpleParticleSystemDX11 );
}
