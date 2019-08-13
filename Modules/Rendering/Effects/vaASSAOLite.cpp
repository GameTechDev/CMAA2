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

#include "vaASSAOLite.h"

#include "Core/vaInput.h"

#include "IntegratedExternals/vaImguiIntegration.h"

using namespace VertexAsylum;

namespace
{
    static const int cMaxBlurPassCount = 6;
}

vaASSAOLite::vaASSAOLite( const vaRenderingModuleParams & params ) : 
    vaRenderingModule( params ),
    vaUIPanel( "ASSAOLite", 10, false, vaUIPanel::DockLocation::DockedLeftBottom ),
    m_vertexShader( params ),
    m_pixelShaderPrepareDepthsMSAA                          { (params), (params), (params), (params), (params) },
    m_pixelShaderPrepareDepthsAndNormalsMSAA                { (params), (params), (params), (params), (params) },
    m_pixelShaderPrepareDepthMip                            { (params), (params), (params) },   // [SSAO_DEPTH_MIP_LEVELS-1]
    m_pixelShaderGenerate                                   { (params), (params), (params), (params) },//(params), (params) },
    m_pixelShaderSmartBlur(params),
    m_pixelShaderSmartBlurWide(params),
    m_pixelShaderApply(params),
    m_pixelShaderNonSmartBlur(params),
    m_pixelShaderNonSmartApply(params),
    m_constantsBuffer( params )

{ 
    m_allShaders.push_back( m_vertexShader.get() );
    for( auto sh : m_pixelShaderPrepareDepthsMSAA )                 m_allShaders.push_back( sh.get() );
    for( auto sh : m_pixelShaderPrepareDepthsAndNormalsMSAA )       m_allShaders.push_back( sh.get() );
    for( auto sh : m_pixelShaderPrepareDepthMip )                   m_allShaders.push_back( sh.get() );
    for( auto sh : m_pixelShaderGenerate )                          m_allShaders.push_back( sh.get() );
    m_allShaders.push_back( m_pixelShaderSmartBlur.get() );
    m_allShaders.push_back( m_pixelShaderSmartBlurWide.get() );
    m_allShaders.push_back( m_pixelShaderApply.get() );
    m_allShaders.push_back( m_pixelShaderNonSmartBlur.get() );
    m_allShaders.push_back( m_pixelShaderNonSmartApply.get() );
    
    m_size = m_halfSize = m_quarterSize = vaVector2i( 0, 0 ); 
    m_prevQualityLevel              = -1;
    m_debugShowNormals              = false; 
    m_debugShowEdges                = false;
    //m_debugOpaqueOutput             = false;
}

void vaASSAOLite::UIPanelDraw( )
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED

    // Keyboard input (but let the ImgUI controls have input priority)
    
    // extension for "Lowest"
    int qualityLevelUI = m_settings.QualityLevel;

    ImGui::PushItemWidth( 120.0f );

    ImGui::Text( "Performance/quality settings:" );

    ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.0f, 0.8f, 0.8f, 1.0f ) );
    ImGui::Combo( "Quality level", &qualityLevelUI, "Low\0Medium\0High\0Highest\0\0" );  // Combo using values packed in a single constant string (for really quick combo)

    m_settings.QualityLevel = vaMath::Clamp( qualityLevelUI, 0, 3 );

    if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Each quality level is roughly 1.5x more costly than the previous" );
    ImGui::PopStyleColor( 1 );

    ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 0.75f, 0.75f, 0.75f, 1.0f ) );

    if( m_settings.QualityLevel <= 0 )
    {
        ImGui::InputInt( "Simple blur passes (0-1)", &m_settings.BlurPassCount );
        if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "For Low quality, only one optional simple blur pass can be applied (recommended); settings above 1 are ignored" );
    }
    else
    {
        ImGui::InputInt( "Smart blur passes (0-6)", &m_settings.BlurPassCount );
        if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "The amount of edge-aware smart blur; each additional pass increases blur effect but adds to the cost" );
    }
    m_settings.BlurPassCount = vaMath::Clamp( m_settings.BlurPassCount, 0, 6 );

    ImGui::Separator();
    ImGui::Text( "Visual settings:" );
    ImGui::InputFloat( "Effect radius",                     &m_settings.Radius                          , 0.05f, 0.0f, "%.2f" );
    if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "World (viewspace) effect radius" );
    ImGui::InputFloat( "Occlusion multiplier",              &m_settings.ShadowMultiplier                , 0.05f, 0.0f, "%.2f" );
    if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Effect strength" );
    ImGui::InputFloat( "Occlusion power curve",             &m_settings.ShadowPower                     , 0.05f, 0.0f, "%.2f" );
    if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "occlusion = pow( occlusion, value ) - changes the occlusion curve" );
    ImGui::InputFloat( "Fadeout distance from",             &m_settings.FadeOutFrom                     , 1.0f , 0.0f, "%.1f" );
    if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Distance at which to start fading out the effect" );
    ImGui::InputFloat( "Fadeout distance to",               &m_settings.FadeOutTo                       , 1.0f , 0.0f, "%.1f" );
    if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Distance at which to completely fade out the effect" );
    ImGui::InputFloat( "Sharpness",                         &m_settings.Sharpness                       , 0.01f, 0.0f, "%.2f" );
    if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "How much to bleed over edges; 1: not at all, 0.5: half-half; 0.0: completely ignore edges" );

    ImGui::Separator( );
    ImGui::Text( "Advanced visual settings:" );
    ImGui::InputFloat( "Detail occlusion multiplier",       &m_settings.DetailShadowStrength            , 0.05f, 0.0f, "%.2f" );
    if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Additional small radius / high detail occlusion; too much will create aliasing & temporal instability" );
    ImGui::InputFloat( "Horizon angle threshold",           &m_settings.HorizonAngleThreshold           , 0.01f, 0.0f, "%.2f" );
    if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Reduces precision and tessellation related unwanted occlusion" );
    ImGui::InputFloat( "Occlusion max clamp",               &m_settings.ShadowClamp                     , 0.01f, 0.0f, "%.2f" );
    if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "occlusion = min( occlusion, value ) - limits the occlusion maximum" );    
    ImGui::InputFloat( "Radius distance-based modifier",    &m_settings.RadiusDistanceScalingFunction   , 0.05f, 0.0f, "%.2f" );
    if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Used to modify ""Effect radius"" based on distance from the camera; for 0.0, effect world radius is constant (default);\nfor values above 0.0, the effect radius will grow the more distant from the camera it is ( effectRadius *= pow(distance, scaling) );\nif changed, ""Effect Radius"" often needs to be rebalanced as well" );

    m_settings.Radius                           = vaMath::Clamp( m_settings.Radius                          , 0.0f, 100.0f      );
    m_settings.HorizonAngleThreshold            = vaMath::Clamp( m_settings.HorizonAngleThreshold           , 0.0f, 1.0f        );
    m_settings.ShadowMultiplier                 = vaMath::Clamp( m_settings.ShadowMultiplier                , 0.0f, 5.0f       );
    m_settings.ShadowPower                      = vaMath::Clamp( m_settings.ShadowPower                     , 0.5f, 5.0f        );
    m_settings.ShadowClamp                      = vaMath::Clamp( m_settings.ShadowClamp                     , 0.1f, 1.0f        );
    m_settings.FadeOutFrom                      = vaMath::Clamp( m_settings.FadeOutFrom                     , 0.0f, 1000000.0f  );
    m_settings.FadeOutTo                        = vaMath::Clamp( m_settings.FadeOutTo                       , 0.0f, 1000000.0f  );
    m_settings.Sharpness                        = vaMath::Clamp( m_settings.Sharpness                       , 0.0f, 1.0f        );
    m_settings.DetailShadowStrength             = vaMath::Clamp( m_settings.DetailShadowStrength            , 0.0f, 5.0f        );
    m_settings.RadiusDistanceScalingFunction    = vaMath::Clamp( m_settings.RadiusDistanceScalingFunction   , 0.0f, 2.0f        );

    ImGui::Separator( );
    ImGui::Text( "Debugging and experimental switches:" );

    // ImGui::Checkbox( "Smoothen normals", &m_settings.SmoothenNormals );
    // if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Experimental - smoothens normals; fixes some aliasingartifacts, but makes some worse" );

    ImGui::Checkbox( "Debug: Show normals", &m_debugShowNormals );
    if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Show normals used forSSAO (either generated or provided, and after smoothing if selected)" );

    ImGui::Checkbox( "Debug: Show edges", &m_debugShowEdges );
    if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Show edges used for edge-aware smart blur" );

    ImGui::PopStyleColor( 1 );

    ImGui::Separator( );
    ImGui::Text( m_debugInfo.c_str( ) );

    ImGui::PopItemWidth( );

#endif
}

bool vaASSAOLite::ReCreateIfNeeded( shared_ptr<vaTexture> & inoutTex, vaVector2i size, vaResourceFormat format, float & inoutTotalSizeSum, int mipLevels, int arraySize, bool supportUAVs )
{
    int approxSize = size.x * size.y * vaResourceFormatHelpers::GetPixelSizeInBytes( format );
    if( mipLevels != 1 ) approxSize = approxSize * 2; // is this an overestimate?
    inoutTotalSizeSum += approxSize;

    vaResourceBindSupportFlags bindFlags = vaResourceBindSupportFlags::RenderTarget | vaResourceBindSupportFlags::ShaderResource;

    if( supportUAVs )
        bindFlags |= vaResourceBindSupportFlags::UnorderedAccess;

    if( ( size.x == 0 ) || ( size.y == 0 ) || ( format == vaResourceFormat::Unknown ) )
    {
        inoutTex = nullptr;
    }
    else
    {
        vaResourceFormat resourceFormat = format;
        vaResourceFormat srvFormat = format;
        vaResourceFormat rtvFormat = format;
        vaResourceFormat dsvFormat = vaResourceFormat::Unknown;
        vaResourceFormat uavFormat = (supportUAVs)?(format):(vaResourceFormat::Unknown);

        // handle special cases
        if( format == vaResourceFormat::D32_FLOAT )
        {
            bindFlags = ( bindFlags & ~( vaResourceBindSupportFlags::RenderTarget ) ) | vaResourceBindSupportFlags::DepthStencil;
            resourceFormat = vaResourceFormat::R32_TYPELESS;
            srvFormat = vaResourceFormat::R32_FLOAT;
            rtvFormat = vaResourceFormat::Unknown;
            dsvFormat = vaResourceFormat::D32_FLOAT;
        }
        // handle special cases
        if( arraySize != 1 )
        {
            // render target for arrays not really needed (it will get created anyway using resource format - no mechanism to prevent it at the moment)
            rtvFormat = vaResourceFormat::Unknown;
        }
        if( format == vaResourceFormat::R8G8B8A8_UNORM_SRGB )
        {
            resourceFormat = vaResourceFormat::R8G8B8A8_TYPELESS;
            srvFormat = vaResourceFormat::R8G8B8A8_UNORM_SRGB;
            rtvFormat = vaResourceFormat::R8G8B8A8_UNORM_SRGB;
        }

        if( (inoutTex != nullptr) && (inoutTex->GetSizeX() == size.x) && (inoutTex->GetSizeY()==size.y) &&
            (inoutTex->GetResourceFormat()==resourceFormat) && (inoutTex->GetSRVFormat()==srvFormat) && (inoutTex->GetRTVFormat()==rtvFormat) && (inoutTex->GetDSVFormat()==dsvFormat) && (inoutTex->GetUAVFormat()==uavFormat) )
            return false;

        inoutTex = vaTexture::Create2D( GetRenderDevice(), resourceFormat, size.x, size.y, mipLevels, arraySize, 1, bindFlags, vaResourceAccessFlags::Default, srvFormat, rtvFormat, dsvFormat, uavFormat );
    }

    return true;
}

void vaASSAOLite::UpdateTextures( vaSceneDrawContext & drawContext, int width, int height, bool generateNormals, const vaVector4i & scissorRect )
{
    vector< pair< string, string > > newShaderMacros;
    
    if( m_debugShowNormals )
        newShaderMacros.push_back( std::pair<std::string, std::string>( "SSAO_DEBUG_SHOWNORMALS", "" ) );
    if( m_debugShowEdges )
        newShaderMacros.push_back( std::pair<std::string, std::string>( "SSAO_DEBUG_SHOWEDGES", "" ) );

    if( m_specialShaderMacro != pair<string, string>(std::make_pair( "", "" )) )
        newShaderMacros.push_back( m_specialShaderMacro );

    if( newShaderMacros != m_staticShaderMacros )
    {
        m_staticShaderMacros = newShaderMacros;
        m_shadersDirty = true;
    }

    if( m_shadersDirty )
    {
        m_shadersDirty = false;
        
        std::vector<vaVertexInputElementDesc> inputElements;
        inputElements.push_back( { "SV_Position", 0, vaResourceFormat::R32G32B32A32_FLOAT, 0, 0, vaVertexInputElementDesc::InputClassification::PerVertexData, 0 } );
        inputElements.push_back( { "TEXCOORD", 0, vaResourceFormat::R32G32_FLOAT, 0, 16, vaVertexInputElementDesc::InputClassification::PerVertexData, 0 } );
        m_vertexShader->CreateShaderAndILFromFile(   GetShaderFilePath( ), "vs_5_0", "VSMain", inputElements, m_staticShaderMacros, false );

        assert( _countof( m_pixelShaderPrepareDepthsMSAA ) == _countof( m_pixelShaderPrepareDepthsAndNormalsMSAA ) );

        for( int i = 0; i < _countof( m_pixelShaderPrepareDepthsMSAA ); i++ )
        {
            vector< pair< string, string > > macrosWithMSAA = m_staticShaderMacros;
            if( i == 0 )
                { }
            else if( i == 1 )
                macrosWithMSAA.push_back( std::pair<std::string, std::string>( "SSAO_MSAA_SAMPLE_COUNT", "2" ) );
            else if( i == 2 )
                macrosWithMSAA.push_back( std::pair<std::string, std::string>( "SSAO_MSAA_SAMPLE_COUNT", "4" ) );
            else if( i == 3 )
                macrosWithMSAA.push_back( std::pair<std::string, std::string>( "SSAO_MSAA_SAMPLE_COUNT", "8" ) );
            else if( i == 4 )
                macrosWithMSAA.push_back( std::pair<std::string, std::string>( "SSAO_MSAA_SAMPLE_COUNT", "16" ) );
            else
            {
                assert( false );
            }

            m_pixelShaderPrepareDepthsMSAA[i]->CreateShaderFromFile( GetShaderFilePath( ), "ps_5_0", "PSPrepareDepths", macrosWithMSAA, false );
            m_pixelShaderPrepareDepthsAndNormalsMSAA[i]->CreateShaderFromFile( GetShaderFilePath( ), "ps_5_0", "PSPrepareDepthsAndNormals", macrosWithMSAA, false );
        }

        m_pixelShaderPrepareDepthMip[0]->CreateShaderFromFile( GetShaderFilePath( ), "ps_5_0", "PSPrepareDepthMip1", m_staticShaderMacros, false );
        m_pixelShaderPrepareDepthMip[1]->CreateShaderFromFile( GetShaderFilePath( ), "ps_5_0", "PSPrepareDepthMip2", m_staticShaderMacros, false );
        m_pixelShaderPrepareDepthMip[2]->CreateShaderFromFile( GetShaderFilePath( ), "ps_5_0", "PSPrepareDepthMip3", m_staticShaderMacros, false );

        // vector< pair< string, string > > shaderMacrosSmoothNormals = m_staticShaderMacros;
        // shaderMacrosSmoothNormals.push_back( std::pair<std::string, std::string>( "SSAO_SMOOTHEN_NORMALS", "" ) );

        m_pixelShaderGenerate[0]->CreateShaderFromFile( GetShaderFilePath( ), "ps_5_0", "PSGenerateQ0", m_staticShaderMacros, false );
        m_pixelShaderGenerate[1]->CreateShaderFromFile( GetShaderFilePath( ), "ps_5_0", "PSGenerateQ1", m_staticShaderMacros, false );
        m_pixelShaderGenerate[2]->CreateShaderFromFile( GetShaderFilePath( ), "ps_5_0", "PSGenerateQ2", m_staticShaderMacros, false );
        m_pixelShaderGenerate[3]->CreateShaderFromFile( GetShaderFilePath( ), "ps_5_0", "PSGenerateQ3", m_staticShaderMacros, false );

        m_pixelShaderSmartBlur->CreateShaderFromFile( GetShaderFilePath( ), "ps_5_0", "PSSmartBlur", m_staticShaderMacros, false );
        m_pixelShaderSmartBlurWide->CreateShaderFromFile( GetShaderFilePath( ), "ps_5_0", "PSSmartBlurWide", m_staticShaderMacros, false );
        m_pixelShaderNonSmartBlur->CreateShaderFromFile( GetShaderFilePath( ), "ps_5_0", "PSNonSmartBlur", m_staticShaderMacros, false );

        m_pixelShaderNonSmartApply->CreateShaderFromFile( GetShaderFilePath( ), "ps_5_0", "PSNonSmartApply", m_staticShaderMacros, false );
        m_pixelShaderApply->CreateShaderFromFile( GetShaderFilePath( ), "ps_5_0", "PSApply", m_staticShaderMacros, false );
    }

    bool needsUpdate = false;

    if( !generateNormals )
    {
        needsUpdate |= m_normals != nullptr;
        m_normals = nullptr;
    }
    else
    {
        needsUpdate |= m_normals == nullptr;
    }

    if( width == -1 )   width = drawContext.RenderDeviceContext.GetViewport( ).Width;
    if( height == -1 )  height = drawContext.RenderDeviceContext.GetViewport( ).Height;

    needsUpdate |= (m_size.x != width) || (m_size.y != height);

    m_size.x        = width;
    m_size.y        = height;
    m_halfSize.x    = (width+1)/2;
    m_halfSize.y    = (height+1)/2;
    m_quarterSize.x = (m_halfSize.x+1)/2;
    m_quarterSize.y = (m_halfSize.y+1)/2;

    vaRecti prevScissorRect = m_fullResOutScissorRect;

    if( (scissorRect.z == 0) || (scissorRect.w == 0) )
        m_fullResOutScissorRect = vaRecti( 0, 0, width, height );
    else
        m_fullResOutScissorRect = vaRecti( vaMath::Max( 0, scissorRect.x ), vaMath::Max( 0, scissorRect.y ), vaMath::Min( width, scissorRect.z ), vaMath::Min( height, scissorRect.w ) );

    needsUpdate |= prevScissorRect != m_fullResOutScissorRect;

#if SSAO_USE_SEPARATE_LOWQ_AORESULTS_TEXTURE
    vaResourceFormat AOResultsFormat = ( m_settings.QualityLevel <= 0 ) ? m_formats.AOResultLowQ : m_formats.AOResult;

    if( (m_pingPongHalfResultA != nullptr) && (m_pingPongHalfResultA->GetSRVFormat() != AOResultsFormat ) )
        needsUpdate = true;
#else
    vaResourceFormat AOResultsFormat = m_formats.AOResult;
#endif
    if( !needsUpdate )
        return;
    
    m_halfResOutScissorRect = vaRecti( m_fullResOutScissorRect.left/2, m_fullResOutScissorRect.top/2, (m_fullResOutScissorRect.right+1) / 2, (m_fullResOutScissorRect.bottom+1) / 2 );
    int blurEnlarge = cMaxBlurPassCount + vaMath::Max( 0, cMaxBlurPassCount-2 );  // +1 for max normal blurs, +2 for wide blurs
    m_halfResOutScissorRect = vaRecti( vaMath::Max( 0, m_halfResOutScissorRect.left - blurEnlarge ), vaMath::Max( 0, m_halfResOutScissorRect.top - blurEnlarge ), vaMath::Min( m_halfSize.x, m_halfResOutScissorRect.right + blurEnlarge ), vaMath::Min( m_halfSize.y, m_halfResOutScissorRect.bottom + blurEnlarge ) );

    float totalSizeInMB = 0.0f;

    for( int i = 0; i < 4; i++ )
    {
        if( ReCreateIfNeeded( m_halfDepths[i], m_halfSize, m_formats.DepthBufferViewspaceLinear, totalSizeInMB, SSAO_DEPTH_MIP_LEVELS, 1, false ) )
        {
            for( int j = 0; j < SSAO_DEPTH_MIP_LEVELS; j++ )
                m_halfDepthsMipViews[i][j] = nullptr;

            for( int j = 0; j < SSAO_DEPTH_MIP_LEVELS; j++ )
                m_halfDepthsMipViews[i][j] = vaTexture::CreateView( m_halfDepths[i], m_halfDepths[i]->GetBindSupportFlags(), vaResourceFormat::Automatic, m_halfDepths[i]->GetRTVFormat(), vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaTextureFlags::None, j, 1 );
        }
        
    }
    ReCreateIfNeeded( m_pingPongHalfResultA, m_halfSize, AOResultsFormat, totalSizeInMB, 1, 1, false );
    ReCreateIfNeeded( m_pingPongHalfResultB, m_halfSize, AOResultsFormat, totalSizeInMB, 1, 1, false );
    ReCreateIfNeeded( m_finalResults, m_halfSize, AOResultsFormat, totalSizeInMB, 1, 4, true );

    for( int i = 0; i < 4; i++ )
        m_finalResultsArrayViews[i] = vaTexture::CreateView( m_finalResults, vaResourceBindSupportFlags::RenderTarget | vaResourceBindSupportFlags::UnorderedAccess, vaResourceFormat::Automatic, AOResultsFormat, vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaTextureFlags::None, 0, -1, i, 1 );
    
    if( generateNormals )
        ReCreateIfNeeded( m_normals, m_size, m_formats.Normals, totalSizeInMB, 1, 1, true );

    float dummy = 0.0f;
    ReCreateIfNeeded( m_debuggingOutput, m_size, vaResourceFormat::R8G8B8A8_UNORM, dummy, 1, 1, true );

    totalSizeInMB /= 1024 * 1024;

    m_debugInfo = vaStringTools::Format( "Approx. %.2fMB video memory used.", totalSizeInMB );

    // trigger a full buffers clear first time; only really required when using scissor rects
    m_requiresClear = m_fullResOutScissorRect != vaRecti( 0, 0, width, height );
}

void vaASSAOLite::UpdateConstants( vaSceneDrawContext & drawContext, const vaMatrix4x4 & projMatrix, int pass )
{
    // update constants
    {
        ASSAOLiteConstants consts;

        const vaMatrix4x4 & proj = projMatrix;

        consts.ViewportPixelSize                    = vaVector2( 1.0f / (float)m_size.x, 1.0f / (float)m_size.y );
        consts.HalfViewportPixelSize                = vaVector2( 1.0f / (float)m_halfSize.x, 1.0f / (float)m_halfSize.y );

        consts.Viewport2xPixelSize                  = vaVector2( consts.ViewportPixelSize.x * 2.0f, consts.ViewportPixelSize.y * 2.0f );
        consts.Viewport2xPixelSize_x_025            = consts.Viewport2xPixelSize * 0.25;

        float depthLinearizeMul                     = -proj.m[3][2];         // float depthLinearizeMul = ( clipFar * clipNear ) / ( clipFar - clipNear );
        float depthLinearizeAdd                     = proj.m[2][2];          // float depthLinearizeAdd = clipFar / ( clipFar - clipNear );
        // correct the handedness issue. need to make sure this below is correct, but I think it is.
        if( depthLinearizeMul * depthLinearizeAdd < 0 )
            depthLinearizeAdd = -depthLinearizeAdd;
        consts.DepthUnpackConsts                    = vaVector2( depthLinearizeMul, depthLinearizeAdd );

        float tanHalfFOVY                           = 1.0f / proj.m[1][1];    // = tanf( drawContext.Camera.GetYFOV( ) * 0.5f );
        float tanHalfFOVX                           = 1.0F / proj.m[0][0];    // = tanHalfFOVY * drawContext.Camera.GetAspect( );
        consts.CameraTanHalfFOV                     = vaVector2( tanHalfFOVX, tanHalfFOVY );

        consts.NDCToViewMul                         = vaVector2::ComponentMul( consts.CameraTanHalfFOV, vaVector2( 2.0, -2.0 ) );
        consts.NDCToViewAdd                         = vaVector2::ComponentMul( consts.CameraTanHalfFOV, vaVector2( -1.0, 1.0 ) );

        consts.EffectRadius                         = vaMath::Clamp( m_settings.Radius, 0.0f, 100000.0f );
        consts.EffectShadowStrength                 = vaMath::Clamp( m_settings.ShadowMultiplier * 4.3f, 0.0f, 10.0f );
        consts.EffectShadowPow                      = vaMath::Clamp( m_settings.ShadowPower, 0.0f, 5.0f );
        consts.EffectShadowClamp                    = vaMath::Clamp( m_settings.ShadowClamp, 0.0f, 1.0f );
        consts.EffectFadeOutMul                     = - 1.0f / (m_settings.FadeOutTo - m_settings.FadeOutFrom);
        consts.EffectFadeOutAdd                     = m_settings.FadeOutFrom / (m_settings.FadeOutTo - m_settings.FadeOutFrom) + 1.0f;
        consts.EffectHorizonAngleThreshold          = vaMath::Clamp( m_settings.HorizonAngleThreshold, 0.0f, 1.0f );

        // 1.2 seems to be around the best trade off - 1.0 means on-screen radius will stop/slow growing when the camera is at 1.0 distance, so, depending on FOV, basically filling up most of the screen
        // This setting is viewspace-dependent and not screen size dependent intentionally, so that when you change FOV the effect stays (relatively) similar.
        float effectSamplingRadiusNearLimit        = ( m_settings.Radius * 1.2f );

        // if the depth precision is switched to 32bit float, this can be set to something closer to 1 (0.9999 is fine)
        consts.DepthPrecisionOffsetMod              = 0.9992f;
        
        consts.NegRecEffectRadius                   = -1.0f / consts.EffectRadius;
        
        consts.DetailAOStrength                     = m_settings.DetailShadowStrength;
        
        consts.RadiusDistanceScalingFunctionPow     = m_settings.RadiusDistanceScalingFunction;

        // Special settings for lowest quality level - just nerf the effect a tiny bit
        if( m_settings.QualityLevel <= 0 )
        {
            //consts.EffectShadowStrength         *= 0.9f;
            effectSamplingRadiusNearLimit       *= 1.50f;

            if( m_settings.QualityLevel < 0 )
                consts.EffectRadius             *= 0.8f;
        }
        effectSamplingRadiusNearLimit /= tanHalfFOVY; // to keep the effect same regardless of FOV

        consts.EffectSamplingRadiusNearLimitRec         = 1.0f / effectSamplingRadiusNearLimit;

        consts.PerPassFullResCoordOffset                = vaVector2i( pass % 2, pass / 2 );
        consts.PerPassFullResUVOffset                   = vaVector2( ((pass % 2) - 0.0f) / m_size.x, ((pass / 2) - 0.0f) / m_size.y );

        consts.InvSharpness                             = vaMath::Clamp( 1.0f - m_settings.Sharpness, 0.0f, 1.0f );
        consts.PassIndex                                = pass;
        consts.QuarterResPixelSize                      = vaVector2( 1.0f / (float)m_quarterSize.x, 1.0f / (float)m_quarterSize.y );

        float additionalAngleOffset = m_settings.TemporalSupersamplingAngleOffset;  // if using temporal supersampling approach (like "Progressive Rendering Using Multi-frame Sampling" from GPU Pro 7, etc.)
        float additionalRadiusScale = m_settings.TemporalSupersamplingRadiusOffset; // if using temporal supersampling approach (like "Progressive Rendering Using Multi-frame Sampling" from GPU Pro 7, etc.)
        const int subPassCount = 5;
        for( int subPass = 0; subPass < subPassCount; subPass++ )
        {
            int a = pass;
            int b = subPass;

            int spmap[5] { 0, 1, 4, 3, 2 };
            b = spmap[subPass];

            float ca, sa;
            float angle0 = ( (float)a + (float)b / (float)subPassCount ) * VA_PIf * 0.5f;
            angle0 += additionalAngleOffset;

            ca = vaMath::Cos( angle0 );
            sa = vaMath::Sin( angle0 );

            float scale = 1.0f + (a-1.5f + (b - (subPassCount-1.0f) * 0.5f ) / (float)subPassCount ) * 0.07f;
            scale *= additionalRadiusScale;

            consts.PatternRotScaleMatrices[subPass] = vaVector4( scale * ca, scale * -sa, -scale * sa, -scale * ca );
        }
      
        m_constantsBuffer.Update( drawContext.RenderDeviceContext, consts );
    }
}

void vaASSAOLite::PrepareDepths( vaSceneDrawContext & drawContext, const shared_ptr<vaTexture> & depthTexture, vaGraphicsItem & fullscreenGraphicsItem, bool generateNormals )
{
    assert( m_settings.QualityLevel >= 0 && m_settings.QualityLevel <= 3 );

    vaRenderDeviceContext & renderContext = drawContext.RenderDeviceContext; //.SafeCast<vaRenderDeviceContextDX11*>( );

    fullscreenGraphicsItem.ShaderResourceViews[SSAO_TEXTURE_SLOT0] = depthTexture;

    int msaaShaderIndex = 0;
    if( depthTexture->GetSampleCount() == 1 )        msaaShaderIndex = 0;
    else if( depthTexture->GetSampleCount() == 2 )   msaaShaderIndex = 1;
    else if( depthTexture->GetSampleCount() == 4 )   msaaShaderIndex = 2;
    else if( depthTexture->GetSampleCount() == 8 )   msaaShaderIndex = 3;
    else if( depthTexture->GetSampleCount() == 16 )  msaaShaderIndex = 4;
    else
    {
        assert( false );
    }

    const shared_ptr<vaTexture> fourDepths[] = { m_halfDepths[0], m_halfDepths[1], m_halfDepths[2], m_halfDepths[3] };
    if( !generateNormals )
    {
        VA_SCOPE_CPUGPU_TIMER( PrepareDepths, drawContext.RenderDeviceContext );

        renderContext.SetRenderTargets( 4, fourDepths, nullptr, true );
        fullscreenGraphicsItem.PixelShader = m_pixelShaderPrepareDepthsMSAA[msaaShaderIndex];
        renderContext.ExecuteSingleItem( fullscreenGraphicsItem );
    }
    else
    {
        VA_SCOPE_CPUGPU_TIMER( PrepareDepthsAndNormals, drawContext.RenderDeviceContext );

        shared_ptr<vaTexture> uavs[] = { m_normals };
	    renderContext.SetRenderTargetsAndUnorderedAccessViews( 4, fourDepths, nullptr, SSAO_NORMALMAP_OUT_UAV_SLOT, 1, uavs, true );
        fullscreenGraphicsItem.PixelShader = m_pixelShaderPrepareDepthsAndNormalsMSAA[msaaShaderIndex];
        renderContext.ExecuteSingleItem( fullscreenGraphicsItem );
    }

    // only do mipmaps for higher quality levels - doesn't pay off on low/medium
    if( m_settings.QualityLevel > 1 )
    {
        VA_SCOPE_CPUGPU_TIMER( PrepareDepthMips, drawContext.RenderDeviceContext );

        for( int i = 1; i < SSAO_DEPTH_MIP_LEVELS; i++ )
        {
            const shared_ptr<vaTexture> fourDepthMips[] = { m_halfDepthsMipViews[0][i], m_halfDepthsMipViews[1][i], m_halfDepthsMipViews[2][i], m_halfDepthsMipViews[3][i] };

            renderContext.SetRenderTargets( 4, fourDepthMips, nullptr, true );

            fullscreenGraphicsItem.ShaderResourceViews[SSAO_TEXTURE_SLOT0] = m_halfDepthsMipViews[0][i - 1];
            fullscreenGraphicsItem.ShaderResourceViews[SSAO_TEXTURE_SLOT1] = m_halfDepthsMipViews[1][i - 1];
            fullscreenGraphicsItem.ShaderResourceViews[SSAO_TEXTURE_SLOT2] = m_halfDepthsMipViews[2][i - 1];
            fullscreenGraphicsItem.ShaderResourceViews[SSAO_TEXTURE_SLOT3] = m_halfDepthsMipViews[3][i - 1];
         
            fullscreenGraphicsItem.PixelShader = m_pixelShaderPrepareDepthMip[i-1];
            renderContext.ExecuteSingleItem( fullscreenGraphicsItem );
        }
    }

    // just cleanup
    fullscreenGraphicsItem.ShaderResourceViews[SSAO_TEXTURE_SLOT0] = nullptr;
    fullscreenGraphicsItem.ShaderResourceViews[SSAO_TEXTURE_SLOT1] = nullptr;
    fullscreenGraphicsItem.ShaderResourceViews[SSAO_TEXTURE_SLOT2] = nullptr;
    fullscreenGraphicsItem.ShaderResourceViews[SSAO_TEXTURE_SLOT3] = nullptr;
}

void vaASSAOLite::GenerateSSAO( vaSceneDrawContext & drawContext, const vaMatrix4x4 & projMatrix, const shared_ptr<vaTexture> & normalmapTexture, vaGraphicsItem & fullscreenGraphicsItem )
{
    VA_SCOPE_CPUGPU_TIMER( GenerateAndBlur, drawContext.RenderDeviceContext );

    assert( m_settings.QualityLevel >= 0 && m_settings.QualityLevel <= 3 );

    vaRenderDeviceContext & renderContext = drawContext.RenderDeviceContext;

    vaViewport halfResNormalVP( 0, 0, m_halfSize.x, m_halfSize.y );

    renderContext.SetViewportAndScissorRect( halfResNormalVP, m_halfResOutScissorRect );

    int passCount = 4;

    for( int pass = 0; pass < passCount; pass++ )
    {
        if( ( m_settings.QualityLevel < 0 ) && ( ( pass == 1 ) || ( pass == 2 ) ) )
            continue;

        int blurPasses = m_settings.BlurPassCount;
        blurPasses = vaMath::Min( blurPasses, cMaxBlurPassCount );

        if( m_settings.QualityLevel <= 0 )
        {
            // just one blur pass allowed for minimum quality 
            blurPasses = vaMath::Min( 1, m_settings.BlurPassCount );
        }

        UpdateConstants( drawContext, projMatrix, pass );

        shared_ptr<vaTexture> * pPingRT = &m_pingPongHalfResultA;
        shared_ptr<vaTexture> * pPongRT = &m_pingPongHalfResultB;

        // Generate
        {
            // VA_SCOPE_CPUGPU_TIMER( Generate, drawContext.RenderDeviceContext );
            // //VA_SCOPE_CPUGPU_TIMER_NAMED( Generate, vaStringTools::Format( "Generate_quad%d", pass ), drawContext.RenderDeviceContext );

            shared_ptr<vaTexture> rts[] = { *pPingRT }; //, (m_settings.QualityLevel!=0)?(m_edges):(nullptr) };   // the quality level 0 doesn't export edges

            // no blur?
            if( blurPasses == 0 )
                rts[0] = m_finalResultsArrayViews[pass];

            renderContext.SetRenderTargets( _countof(rts), rts, nullptr, false );

            if( m_debugShowNormals || m_debugShowEdges )
            {
                shared_ptr<vaTexture> uavs[] = { m_debuggingOutput };
                renderContext.SetRenderTargetsAndUnorderedAccessViews( _countof(rts), rts, nullptr, SSAO_DEBUGGINGOUTPUT_OUT_UAV_SLOT, 1, uavs, false );
            }

            fullscreenGraphicsItem.ShaderResourceViews[SSAO_TEXTURE_SLOT0] = m_halfDepths[pass];
            fullscreenGraphicsItem.ShaderResourceViews[SSAO_TEXTURE_SLOT1] = normalmapTexture;

            int shaderIndex = vaMath::Max( 0, m_settings.QualityLevel );
            fullscreenGraphicsItem.PixelShader = m_pixelShaderGenerate[shaderIndex];
            renderContext.ExecuteSingleItem( fullscreenGraphicsItem );

            // to avoid API complaints
            fullscreenGraphicsItem.ShaderResourceViews[SSAO_TEXTURE_SLOT0] = nullptr;
            fullscreenGraphicsItem.ShaderResourceViews[SSAO_TEXTURE_SLOT1] = nullptr;
        }
        
        // Blur
        if( blurPasses > 0 )
        {

            int wideBlursRemaining = vaMath::Max( 0, blurPasses-2 );

            for( int i = 0; i < blurPasses; i++ )
            {
                shared_ptr<vaTexture> rts[] = { *pPongRT };

                // last pass?
                if( i == (blurPasses-1) )
                    rts[0] = m_finalResultsArrayViews[pass];

                renderContext.SetRenderTargets( _countof( rts ), rts, nullptr, false );

                fullscreenGraphicsItem.ShaderResourceViews[SSAO_TEXTURE_SLOT2] = (*pPingRT);

                if( m_settings.QualityLevel > 0 )
                {
                    if( wideBlursRemaining > 0 )
                    {
                        fullscreenGraphicsItem.PixelShader = m_pixelShaderSmartBlurWide;
                        wideBlursRemaining--;
                    }
                    else
                    {
                        fullscreenGraphicsItem.PixelShader = m_pixelShaderSmartBlur;
                    }
                }
                else
                {
                    fullscreenGraphicsItem.PixelShader = m_pixelShaderNonSmartBlur;
                }

                renderContext.ExecuteSingleItem( fullscreenGraphicsItem );

                // to avoid API complaints
                fullscreenGraphicsItem.ShaderResourceViews[SSAO_TEXTURE_SLOT2] = nullptr;

                std::swap( pPingRT, pPongRT );
            }
        }
    }
    fullscreenGraphicsItem.ShaderResourceViews[SSAO_TEXTURE_SLOT0] = nullptr;
    fullscreenGraphicsItem.ShaderResourceViews[SSAO_TEXTURE_SLOT1] = nullptr;
    fullscreenGraphicsItem.ShaderResourceViews[SSAO_TEXTURE_SLOT2] = nullptr;
}

// draws provided depthTexture (can be the one obtained using GetDepthBuffer( )) into currently selected RT; relies on settings set in vaRenderGlobals and will assert and return without doing anything if those are not present
vaDrawResultFlags vaASSAOLite::Draw( vaSceneDrawContext & drawContext, const vaMatrix4x4 & projMatrix, const shared_ptr<vaTexture> & depthTexture, const shared_ptr<vaTexture> & normalmapTexture, bool blend, bool generateNormals )
{
#if 0   // shaders not ready? skip the effect
    for( auto sh : m_allShaders )
        if( !sh->IsCreated() )
            {   /*VA_WARN( "SMAA: Not all shaders compiled, can't run" );*/ return vaDrawResultFlags::ShadersStillCompiling; }
#else   // shaders not ready? wait until they are!
    for( auto sh : m_allShaders )
        sh->WaitFinishIfBackgroundCreateActive();
#endif

    VA_SCOPE_CPUGPU_TIMER( ASSAO, drawContext.RenderDeviceContext );

//    assert( drawContext.GetRenderingGlobalsUpdated( ) );    if( !drawContext.GetRenderingGlobalsUpdated( ) ) return;
    assert( ((normalmapTexture->GetSizeX() == m_size.x) || (normalmapTexture->GetSizeX() == m_size.x-1)) && ( (normalmapTexture->GetSizeY() == m_size.y) || (normalmapTexture->GetSizeY() == m_size.y-1)) );
    assert( !m_shadersDirty ); if( m_shadersDirty ) return vaDrawResultFlags::UnspecifiedError;

    if( m_debugShowNormals || m_debugShowEdges )
        blend = false;

    vaRenderDeviceContext & renderContext = drawContext.RenderDeviceContext;

    // Only needed when using a scissor rect!
    if( m_requiresClear )
    {
        float fourZeroes[4] = { 0, 0, 0, 0 };
        float fourOnes[4]   = { 1, 1, 1, 1 };
        m_halfDepths[0]->ClearRTV( drawContext.RenderDeviceContext, fourZeroes );
        m_halfDepths[1]->ClearRTV( drawContext.RenderDeviceContext, fourZeroes );
        m_halfDepths[2]->ClearRTV( drawContext.RenderDeviceContext, fourZeroes );
        m_halfDepths[3]->ClearRTV( drawContext.RenderDeviceContext, fourZeroes );
        m_pingPongHalfResultA->ClearRTV( drawContext.RenderDeviceContext, fourOnes );
        m_pingPongHalfResultB->ClearRTV( drawContext.RenderDeviceContext, fourZeroes );
        m_finalResultsArrayViews[0]->ClearRTV( drawContext.RenderDeviceContext, fourOnes );
        m_finalResultsArrayViews[1]->ClearRTV( drawContext.RenderDeviceContext, fourOnes );
        m_finalResultsArrayViews[2]->ClearRTV( drawContext.RenderDeviceContext, fourOnes );
        m_finalResultsArrayViews[3]->ClearRTV( drawContext.RenderDeviceContext, fourOnes );
        if( m_normals != nullptr ) m_normals->ClearRTV( drawContext.RenderDeviceContext, fourZeroes );

        m_requiresClear = false;
    }

    UpdateConstants( drawContext, projMatrix, 0 );

    vaGraphicsItem fullscreenGraphicsItem;
    drawContext.RenderDeviceContext.FillFullscreenPassRenderItem( fullscreenGraphicsItem );
    fullscreenGraphicsItem.ConstantBuffers[SSAO_CONSTANTSBUFFERSLOT] = m_constantsBuffer;

    // backup currently set RTs
    vaRenderDeviceContext::RenderOutputsState rtState = renderContext.GetOutputs();

    // Generate depths
    PrepareDepths( drawContext, depthTexture, fullscreenGraphicsItem, generateNormals );

    // Generate SSAO
    GenerateSSAO( drawContext, projMatrix, normalmapTexture, fullscreenGraphicsItem );

    // restore previous RTs
    renderContext.SetOutputs( rtState );

    // Apply
    {
        VA_SCOPE_CPUGPU_TIMER( Apply, drawContext.RenderDeviceContext );

        // select 4 deinterleaved AO textures (texture array)
        fullscreenGraphicsItem.ShaderResourceViews[SSAO_TEXTURE_SLOT4] = m_finalResults;

        if( m_debugShowNormals || m_debugShowEdges )
            fullscreenGraphicsItem.ShaderResourceViews[SSAO_TEXTURE_SLOT5] = m_debuggingOutput;

        fullscreenGraphicsItem.BlendMode = (!blend)?( vaBlendMode::Opaque ):( vaBlendMode::Mult );

        vaViewport applyVP( 0, 0, m_size.x, m_size.y );
        
        renderContext.SetViewportAndScissorRect( applyVP, m_fullResOutScissorRect );

        if( m_settings.QualityLevel == 0 )
            fullscreenGraphicsItem.PixelShader = m_pixelShaderNonSmartApply;
        else
            fullscreenGraphicsItem.PixelShader = m_pixelShaderApply;
        renderContext.ExecuteSingleItem( fullscreenGraphicsItem );

        // restore VP
        renderContext.SetViewport( rtState.Viewport );
    }

    return vaDrawResultFlags::None;
}

vaDrawResultFlags vaASSAOLite::Draw( vaSceneDrawContext & drawContext, const vaMatrix4x4 & projMatrix, const shared_ptr<vaTexture> & depthTexture, bool blend, const shared_ptr<vaTexture> & normalmapTexture, const vaVector4i & scissorRect )
{
    int width   = depthTexture->GetSizeX( );
    int height  = depthTexture->GetSizeY( );

    // this doesn't work yet - TODO: fix it. rounding is needed for correct mips
    // const int roundUpTo = 1 << SSAO_DEPTH_MIP_LEVELS;
    // width  = ( (width  + roundUpTo-1) / roundUpTo ) * roundUpTo;
    // height = ( (height + roundUpTo-1) / roundUpTo ) * roundUpTo;

    UpdateTextures( drawContext, width, height, normalmapTexture == nullptr, scissorRect );
    if( normalmapTexture != nullptr )
        return Draw( drawContext, projMatrix, depthTexture, normalmapTexture, blend, false );
    else
        return Draw( drawContext, projMatrix, depthTexture, m_normals, blend, true );
}


