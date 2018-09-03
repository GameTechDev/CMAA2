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

#include "Rendering/vaGBuffer.h"

#include "Rendering/DirectX/vaDirectXIncludes.h"
#include "Rendering/DirectX/vaShaderDX11.h"
#include "Rendering/DirectX/vaDirectXTools.h"
#include "Rendering/DirectX/vaRenderDeviceContextDX11.h"
#include "Rendering/DirectX/vaTextureDX11.h"

#define GBUFFER_SLOT0   0



namespace VertexAsylum
{

    class vaGBufferDX11 : public vaGBuffer
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    private:

    protected:
        explicit vaGBufferDX11( const vaRenderingModuleParams & params ) : vaGBuffer( params.RenderDevice ) { }
        ~vaGBufferDX11( ) { }

    private:
        const wchar_t *                         GetShaderFilePath( ) const      { return m_shaderFileToUse.c_str();  };

    private:
        virtual void                            RenderDebugDraw( vaSceneDrawContext & drawContext );

        virtual void                            DepthToViewspaceLinear( vaSceneDrawContext & drawContext, vaTexture & depthTexture );

    public:
    };

}

using namespace VertexAsylum;

// draws provided depthTexture (can be the one obtained using GetDepthBuffer( )) into currently selected RT; relies on settings set in vaRenderingGlobals and will assert and return without doing anything if those are not present
void vaGBufferDX11::DepthToViewspaceLinear( vaSceneDrawContext & drawContext, vaTexture & depthTexture )
{
    assert( !m_shadersDirty );                              if( m_shadersDirty ) return;

    drawContext; depthTexture;

    assert( false ); // TODO: port to platform independent code

#if 0

    vaRenderDeviceContextDX11 * apiContext = drawContext.APIContext.SafeCast<vaRenderDeviceContextDX11*>( );
    ID3D11DeviceContext * dx11Context = apiContext->GetDXContext( );

    vaDirectXTools::AssertSetToD3DContextAllShaderTypes( dx11Context, ( ID3D11ShaderResourceView* )nullptr, GBUFFER_SLOT0 );

    vaDirectXTools::SetToD3DContextAllShaderTypes( dx11Context, depthTexture.SafeCast<vaTextureDX11*>( )->GetSRV( ), GBUFFER_SLOT0 );
    apiContext->FullscreenPassDraw( *m_depthToViewspaceLinearPS );

    // Reset
    vaDirectXTools::SetToD3DContextAllShaderTypes( dx11Context, ( ID3D11ShaderResourceView* )nullptr, GBUFFER_SLOT0 );

        //vaRenderItem renderItem;
        //apiContext.FillFullscreenPassRenderItem( renderItem );
        //renderItem.ConstantBuffers[ IMAGE_COMPARE_TOOL_BUFFERSLOT ]         = m_constants.GetBuffer();
        //renderItem.ShaderResourceViews[ GBUFFER_SLOT0 ]                   = m_referenceTexture;
        //renderItem.ShaderResourceViews[ IMAGE_COMPARE_TOOL_TEXTURE_SLOT1 ]  = m_helperTexture;
        //renderItem.PixelShader          = m_visualizationPS;
        //apiContext.ExecuteSingleItem( renderItem );
#endif
}

void vaGBufferDX11::RenderDebugDraw( vaSceneDrawContext & drawContext )
{
    assert( !m_shadersDirty );                              if( m_shadersDirty ) return;

    drawContext;

    assert( false ); // TODO: port to platform independent code

#if 0
    if( m_debugSelectedTexture == -1 )
        return;

    vaRenderDeviceContextDX11 * apiContext = drawContext.APIContext.SafeCast<vaRenderDeviceContextDX11*>( );
    ID3D11DeviceContext * dx11Context = apiContext->GetDXContext( );

    vaDirectXTools::AssertSetToD3DContextAllShaderTypes( dx11Context, (ID3D11ShaderResourceView*)nullptr, GBUFFER_SLOT0 );

    if( m_debugSelectedTexture == 0 )
    {
        vaDirectXTools::SetToD3DContextAllShaderTypes( dx11Context, m_depthBuffer->SafeCast<vaTextureDX11*>()->GetSRV(), GBUFFER_SLOT0 );
        apiContext->FullscreenPassDraw( *m_debugDrawDepthPS );
    }
    else if( m_debugSelectedTexture == 1 )
    {
        vaDirectXTools::SetToD3DContextAllShaderTypes( dx11Context, m_depthBufferViewspaceLinear->SafeCast<vaTextureDX11*>( )->GetSRV( ), GBUFFER_SLOT0 );
        apiContext->FullscreenPassDraw( *m_debugDrawDepthViewspaceLinearPS );
    }
    else if( m_debugSelectedTexture == 2 )
    {
        vaDirectXTools::SetToD3DContextAllShaderTypes( dx11Context, m_normalMap->SafeCast<vaTextureDX11*>( )->GetSRV( ), GBUFFER_SLOT0 );
        apiContext->FullscreenPassDraw( *m_debugDrawNormalMapPS );
    }
    else if( m_debugSelectedTexture == 3 )
    {
        vaDirectXTools::SetToD3DContextAllShaderTypes( dx11Context, m_albedo->SafeCast<vaTextureDX11*>( )->GetSRV( ), GBUFFER_SLOT0 );
        apiContext->FullscreenPassDraw( *m_debugDrawAlbedoPS );
    }
    else if( m_debugSelectedTexture == 4 )
    {
        vaDirectXTools::SetToD3DContextAllShaderTypes( dx11Context, m_radiance->SafeCast<vaTextureDX11*>( )->GetSRV( ), GBUFFER_SLOT0 );
        apiContext->FullscreenPassDraw( *m_debugDrawRadiancePS );
    }

    // Reset
    vaDirectXTools::SetToD3DContextAllShaderTypes( dx11Context, (ID3D11ShaderResourceView*)nullptr, GBUFFER_SLOT0 );

    // make sure nothing messed with our constant buffers and nothing uses them after
    // vaDirectXTools::AssertSetToD3DContextAllShaderTypes( dx11Context, m_constantsBuffer.GetBuffer( ), RENDERMESH_CONSTANTS_BUFFERSLOT );
    //vaDirectXTools::SetToD3DContextAllShaderTypes( dx11Context, (ID3D11Buffer*)NULL, RENDERMESH_CONSTANTS_BUFFERSLOT );
#endif
}

void RegisterGBufferDX11( )
{
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaGBuffer, vaGBufferDX11 );
}
