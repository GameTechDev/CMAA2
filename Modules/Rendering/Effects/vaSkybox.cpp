/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// VertexAsylum Codebase, Copyright (c) Filip Strugar.
// Contents of this file are distributed under MIT license (https://en.wikipedia.org/wiki/MIT_License)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Rendering/Effects/vaSkybox.h"

using namespace VertexAsylum;

vaSkybox::vaSkybox( const vaRenderingModuleParams & params ) : vaRenderingModule( params ), m_constantsBuffer( params ),
    m_vertexShader( params ), 
    m_pixelShader( params ),
    m_screenTriangleVertexBuffer( params ),
    m_screenTriangleVertexBufferReversedZ( params )
{ 
//    assert( vaRenderingCore::IsInitialized() );

    std::vector<vaVertexInputElementDesc> inputElements;
    inputElements.push_back( { "SV_Position", 0, vaResourceFormat::R32G32B32_FLOAT, 0, vaVertexInputElementDesc::AppendAlignedElement, vaVertexInputElementDesc::InputClassification::PerVertexData, 0 } );

    m_vertexShader->CreateShaderAndILFromFile( L"vaSkybox.hlsl", "vs_5_0", "SkyboxVS", inputElements );
    m_pixelShader->CreateShaderFromFile( L"vaSkybox.hlsl", "ps_5_0", "SkyboxPS" );

    // Create screen triangle vertex buffer
    {
        const float skyFarZ = 1.0f;
        vaVector3 screenTriangle[4];
        screenTriangle[0] = vaVector3( -1.0f,  1.0f, skyFarZ );
        screenTriangle[1] = vaVector3(  1.0f,  1.0f, skyFarZ );
        screenTriangle[2] = vaVector3( -1.0f, -1.0f, skyFarZ );
        screenTriangle[3] = vaVector3(  1.0f, -1.0f, skyFarZ );

        m_screenTriangleVertexBuffer.Create( _countof(screenTriangle), screenTriangle );
    }

    // Create screen triangle vertex buffer
    {
        const float skyFarZ = 0.0f;
        vaVector3 screenTriangle[4];
        screenTriangle[0] = vaVector3( -1.0f,  1.0f, skyFarZ );
        screenTriangle[1] = vaVector3(  1.0f,  1.0f, skyFarZ );
        screenTriangle[2] = vaVector3( -1.0f, -1.0f, skyFarZ );
        screenTriangle[3] = vaVector3(  1.0f, -1.0f, skyFarZ );

        m_screenTriangleVertexBufferReversedZ.Create( _countof(screenTriangle), screenTriangle );
    }
}

vaSkybox::~vaSkybox( )
{
}

void vaSkybox::UpdateConstants( vaSceneDrawContext & drawContext )
{
    {
        SkyboxConstants consts;

        vaMatrix4x4 view = drawContext.Camera.GetViewMatrix( );
        vaMatrix4x4 proj = drawContext.Camera.GetProjMatrix( );

        view.r3 = vaVector4( 0.0f, 0.0f, 0.0f, 1.0f );

        vaMatrix4x4 viewProj = view * proj;

        consts.ProjToWorld = viewProj.Inverse( );

        consts.CubemapRotate = vaMatrix4x4( m_settings.Rotation );

        consts.ColorMul = vaVector4( m_settings.ColorMultiplier, m_settings.ColorMultiplier, m_settings.ColorMultiplier, 1.0f );

        m_constantsBuffer.Update( drawContext.APIContext, consts );
    }
}

vaDrawResultFlags vaSkybox::Draw( vaSceneDrawContext & drawContext )
{
    if( m_cubemap == nullptr )
        return vaDrawResultFlags::UnspecifiedError;

    UpdateConstants( drawContext );

    vaRenderItem renderItem;
    drawContext.APIContext.FillFullscreenPassRenderItem( renderItem, drawContext.Camera.GetUseReversedZ( ) );

    renderItem.ConstantBuffers[ SKYBOX_CONSTANTS_BUFFERSLOT ]   = m_constantsBuffer.GetBuffer();
    renderItem.ShaderResourceViews[ SKYBOX_TEXTURE_SLOT0 ]      = m_cubemap;

    renderItem.VertexShader         = m_vertexShader.get();
    renderItem.PixelShader          = m_pixelShader.get();
    renderItem.DepthEnable          = true;
    renderItem.DepthWriteEnable     = false;
    renderItem.DepthFunc            = ( drawContext.Camera.GetUseReversedZ() )?( vaComparisonFunc::GreaterEqual ):( vaComparisonFunc::LessEqual );

    return drawContext.APIContext.ExecuteSingleItem( renderItem );
}
