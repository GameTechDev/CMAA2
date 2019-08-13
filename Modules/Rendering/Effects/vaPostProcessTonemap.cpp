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

#include "vaPostProcessTonemap.h"

#include "IntegratedExternals/vaImguiIntegration.h"

using namespace VertexAsylum;

vaPostProcessTonemap::vaPostProcessTonemap( const vaRenderingModuleParams & params )
 : vaRenderingModule( params ),
    vaUIPanel( "Tonemap", 0, false, vaUIPanel::DockLocation::DockedLeftBottom ),
    m_PSPassThrough( params ),
    m_PSTonemap( params ),
    m_PSAverageLuminance( params ),
    m_PSAverageLuminanceGenerateMIP( params ),
    m_PSDownsampleToHalf( params ),
    m_PSAddBloom( params ),
    m_constantsBuffer( params )
{
    int dims = ( 1 << (c_avgLuminanceMIPs-1) );
    m_avgLuminance = vaTexture::Create2D( params.RenderDevice, vaResourceFormat::R32_FLOAT, dims, dims, c_avgLuminanceMIPs, 1, 1, vaResourceBindSupportFlags::ShaderResource | vaResourceBindSupportFlags::RenderTarget );

    for( int i = 0; i < c_avgLuminanceMIPs; i++ )
        m_avgLuminanceMIPViews[i] = shared_ptr<vaTexture>( vaTexture::CreateView( m_avgLuminance, m_avgLuminance->GetBindSupportFlags( ), vaResourceFormat::Automatic, m_avgLuminance->GetRTVFormat( ), vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaTextureFlags::None, i, 1 ) );

    float initialValue[] = { 0.0f };
    m_avgLuminancePrevLastWrittenIndex = 0;
    for( int i = 0; i < c_backbufferCount; i++ )
    {
        m_avgLuminancePrev[i] = vaTexture::Create2D( params.RenderDevice, vaResourceFormat::R32_FLOAT, 1, 1, 1, 1, 1, vaResourceBindSupportFlags::RenderTarget, vaResourceAccessFlags::Default, vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaTextureFlags::None, vaTextureContentsType::GenericColor, initialValue, 4 );
        m_avgLuminancePrevCPU[i] = vaTexture::Create2D( params.RenderDevice, vaResourceFormat::R32_FLOAT, 1, 1, 1, 1, 1, vaResourceBindSupportFlags::None, vaResourceAccessFlags::CPURead | vaResourceAccessFlags::CPUReadManuallySynced, vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaTextureFlags::None, vaTextureContentsType::GenericColor, initialValue, 4 );
    }
    ResetHistory();

    m_lastAverageLuminance  = 0.0f;

    m_bloomBlur = VA_RENDERING_MODULE_CREATE_SHARED( vaPostProcessBlur, params.RenderDevice );

    m_shadersDirty = true;
    
    // init to defaults
    UpdateShaders( 1, nullptr, nullptr, nullptr, false );
}

vaPostProcessTonemap::~vaPostProcessTonemap( )
{
}

void vaPostProcessTonemap::UIPanelDraw( )
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGui::PushItemWidth( 120.0f );

    ImGui::Checkbox( "Enabled", &m_settings.Enabled );

    ImGui::Text( "Performance/quality settings:" );

    ImGui::InputFloat( "Saturation", &m_settings.Saturation, 0.1f );
    ImGui::Checkbox( "UseAutoExposure", &m_settings.UseAutoExposure );
    ImGui::InputFloat( "Exposure", &m_settings.Exposure, 0.1f );
    ImGui::InputFloat2( "ExposureMinMax", &m_settings.ExposureMin, "%.2f" );
    ImGui::InputFloat( "AutoExposureAdaptationSpeed", &m_settings.AutoExposureAdaptationSpeed, 0.5f );
    ImGui::Checkbox( "UseAutoAutoExposureKeyValue", &m_settings.UseAutoAutoExposureKeyValue );
    ImGui::InputFloat( "AutoExposureKeyValue", &m_settings.AutoExposureKeyValue, 0.05f );
    ImGui::Checkbox( "UseModifiedReinhard", &m_settings.UseModifiedReinhard );    
    ImGui::InputFloat( "ModifiedReinhardWhiteLevel", &m_settings.ModifiedReinhardWhiteLevel, 0.5f );
    //        if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "For Low quality, only one optional simple blur pass can be applied (recommended); settings above 1 are ignored" );

    ImGui::Checkbox( "UseBloom", &m_settings.UseBloom );
    ImGui::InputFloat( "BloomSize", &m_settings.BloomSize, 0.01f );
    ImGui::InputFloat( "BloomThreshold", &m_settings.BloomThreshold, 0.05f );
    ImGui::InputFloat( "BloomMultiplier", &m_settings.BloomMultiplier, 0.05f );

    ImGui::PopItemWidth();
#endif
}

void vaPostProcessTonemap::UpdateConstants( vaRenderDeviceContext & renderContext, const std::shared_ptr<vaTexture> & srcRadiance )
{
//    vaRenderDeviceContextDX11 * renderContext = drawContext.renderContext.SafeCast<vaRenderDeviceContextDX11*>( );
//    ID3D11DeviceContext * dx11Context = renderContext->GetDXContext( );

    // Constants
    {
        PostProcessTonemapConstants & consts = m_lastShaderConsts;
        consts.AverageLuminanceMIP      = (float)c_avgLuminanceMIPs - 1.0f;
        consts.Exposure                 = m_settings.Exposure;
        consts.WhiteLevel               = (m_settings.UseModifiedReinhard)?(m_settings.ModifiedReinhardWhiteLevel):(FLT_MAX);       // modified Reinhard white level of FLT_MAX equals "regular" Reinhard
        consts.Saturation               = m_settings.Saturation;

        consts.BloomThreshold           = m_settings.BloomThreshold;
        consts.BloomMultiplier          = m_settings.BloomMultiplier;

        consts.FullResPixelSize         = vaVector2( 1.0f / (float)srcRadiance->GetSizeX(), 1.0f / (float)srcRadiance->GetSizeY() );
        consts.BloomSampleUVMul         = (m_halfResRadiance!=nullptr)?( vaVector2((float)m_halfResRadiance->GetSizeX() * 2.0f, (float)m_halfResRadiance->GetSizeY() * 2.0f) ):( vaVector2((float)srcRadiance->GetSizeX(), (float)srcRadiance->GetSizeY()) );
        consts.BloomSampleUVMul         = vaVector2( 1.0f / consts.BloomSampleUVMul.x, 1.0f / consts.BloomSampleUVMul.y );

        consts.Exp2Exposure             = vaMath::Exp2( consts.Exposure );
        consts.WhiteLevelSquared        = consts.WhiteLevel * consts.WhiteLevel;

        m_constantsBuffer.Update( renderContext, consts );
    }
}

void vaPostProcessTonemap::UpdateShaders( int msaaSampleCount, const std::shared_ptr<vaTexture> & outMSTonemappedColor, const std::shared_ptr<vaTexture> & outMSTonemappedColorComplexityMask, const std::shared_ptr<vaTexture> & outExportLuma, bool waitCompileShaders )
{
    vector< pair< string, string > > newShaderMacros;
    if( msaaSampleCount > 1 )
        newShaderMacros.push_back( std::pair<std::string, std::string>( "POSTPROCESS_TONEMAP_MSAA_SAMPLE_COUNT", vaStringTools::Format("%d", msaaSampleCount ) ) );

    if( outMSTonemappedColor != nullptr )
    {
        newShaderMacros.push_back( std::pair<std::string, std::string>( "POSTPROCESS_TONEMAP_OUTPUT_MS_TONEMAPPED_COLOR", "" ) );

        if( outMSTonemappedColor->GetUAVFormat() == vaResourceFormat::R8G8B8A8_UNORM )
            newShaderMacros.push_back( std::pair<std::string, std::string>( "POSTPROCESS_TONEMAP_OUTPUT_MS_TONEMAPPED_COLOR_FMT_UNORM", "" ) );
        if( outMSTonemappedColor->GetSRVFormat() == vaResourceFormat::R8G8B8A8_UNORM_SRGB )
            newShaderMacros.push_back( std::pair<std::string, std::string>( "POSTPROCESS_TONEMAP_OUTPUT_MS_TONEMAPPED_COLOR_FMT_REQUIRES_SRGB_CONVERSION", "" ) );
    }
    if( outMSTonemappedColorComplexityMask != nullptr )
    {
        newShaderMacros.push_back( std::pair<std::string, std::string>( "POSTPROCESS_TONEMAP_OUTPUT_MS_TONEMAPPED_COLOR_COMPLEXITY_MASK", "" ) );
    }
    if( outExportLuma != nullptr )
    {
        newShaderMacros.push_back( std::pair<std::string, std::string>( "POSTPROCESS_TONEMAP_OUTPUT_LUMA", "" ) );

        // exporting luma not supported in MSAA scenario - not needed for now
        assert( msaaSampleCount == 1 );
    }

    if( newShaderMacros != m_staticShaderMacros )
    {
        m_staticShaderMacros = newShaderMacros;
        m_shadersDirty = true;
    }

    if( m_shadersDirty )
    {
        m_shadersDirty = false;

        m_PSPassThrough->CreateShaderFromFile( L"vaPostProcessTonemap.hlsl", "ps_5_0", "PSPassThrough", m_staticShaderMacros, false );
        m_PSTonemap->CreateShaderFromFile( L"vaPostProcessTonemap.hlsl", "ps_5_0", "PSTonemap", m_staticShaderMacros, false );

        m_PSAverageLuminance->CreateShaderFromFile( L"vaPostProcessTonemap.hlsl", "ps_5_0", "PSAverageLuminance", m_staticShaderMacros, false );
        m_PSAverageLuminanceGenerateMIP->CreateShaderFromFile( L"vaPostProcessTonemap.hlsl", "ps_5_0", "PSAverageLuminanceGenerateMIP", m_staticShaderMacros, false );

        m_PSDownsampleToHalf->CreateShaderFromFile( L"vaPostProcessTonemap.hlsl", "ps_5_0", "PSDownsampleToHalf", m_staticShaderMacros, false );
        m_PSAddBloom->CreateShaderFromFile( L"vaPostProcessTonemap.hlsl", "ps_5_0", "PSAddBloom", m_staticShaderMacros, false );
    }

    if( waitCompileShaders )
    {
        m_PSPassThrough->WaitFinishIfBackgroundCreateActive();
        m_PSTonemap->WaitFinishIfBackgroundCreateActive();
        m_PSAddBloom->WaitFinishIfBackgroundCreateActive();
        m_PSAverageLuminanceGenerateMIP->WaitFinishIfBackgroundCreateActive();
        m_PSDownsampleToHalf->WaitFinishIfBackgroundCreateActive();
        m_PSAddBloom->WaitFinishIfBackgroundCreateActive();
    }
}

void vaPostProcessTonemap::UpdateLastAverageLuminance( vaRenderDeviceContext & renderContext, bool noDelayInGettingLuminance )
{
    VA_SCOPE_CPUGPU_TIMER( UpdateLastAverageLum, renderContext );

    // copy oldest used GPU texture to CPU (will induce sync if still being rendered to, so that's why use the one from
    // _countof( m_avgLuminancePrev )-1 frames behind.
    int oldestLuminanceIndex = ( m_avgLuminancePrevLastWrittenIndex + 1 ) % c_backbufferCount;

    if( noDelayInGettingLuminance )
        oldestLuminanceIndex = m_avgLuminancePrevLastWrittenIndex % c_backbufferCount;

    // we must work on the main context due to mapping limitations
    assert( &renderContext == GetRenderDevice().GetMainContext() );

    if( !m_avgLuminancePrevCPUHasData[oldestLuminanceIndex] )
    {
        m_lastAverageLuminance = m_settings.DefaultAvgLuminanceWhenDataNotAvailable;
        return;
    }

    float data[1] = { 0.0f };
    if( m_avgLuminancePrevCPU[oldestLuminanceIndex]->TryMap( renderContext, vaResourceMapType::Read, false ) )
    {
        auto & mappedData = m_avgLuminancePrevCPU[oldestLuminanceIndex]->GetMappedData();
        memcpy( data, mappedData[0].Buffer, sizeof( data ) );
        m_avgLuminancePrevCPU[oldestLuminanceIndex]->Unmap( renderContext );
        m_lastAverageLuminance = data[0];
    }
    else
    {
        // if we had to wait, something is broken with the algorithm - fix it (temporarily you can remove D3D11_MAP_FLAG_DO_NOT_WAIT above to cause a GPU<->CPU sync point, which is, obviously, bad.)
        // (turns out running out of RAM can cause this as well... )
        VA_LOG_ERROR( "vaPostProcessTonemap::UpdateLastAverageLuminance - unable to map texture to get last luminance data" );
        assert( false );
    }

}

void vaPostProcessTonemap::Tick( float deltaTime )
{
    m_settings.Saturation                   = vaMath::Clamp( m_settings.Saturation, 0.0f, 5.0f );
    m_settings.Exposure                     = vaMath::Clamp( m_settings.Exposure, m_settings.ExposureMin, m_settings.ExposureMax );
    m_settings.ExposureMin                  = vaMath::Clamp( m_settings.ExposureMin, -20.0f, m_settings.ExposureMax );
    m_settings.ExposureMax                  = vaMath::Clamp( m_settings.ExposureMax, m_settings.ExposureMin, 20.0f );

    m_settings.AutoExposureAdaptationSpeed  = vaMath::Clamp( m_settings.AutoExposureAdaptationSpeed, 0.01f, std::numeric_limits<float>::infinity() );
    m_settings.AutoExposureKeyValue         = vaMath::Clamp( m_settings.AutoExposureKeyValue, 0.0f, 2.0f );
    m_settings.ModifiedReinhardWhiteLevel   = vaMath::Clamp( m_settings.ModifiedReinhardWhiteLevel, 0.0f, VA_FLOAT_HIGHEST );

    m_settings.BloomSize                    = vaMath::Clamp( m_settings.BloomSize, 0.0f, 10.0f     ); 
    m_settings.BloomThreshold               = vaMath::Clamp( m_settings.BloomThreshold,  0.0f, 64.0f    );
    m_settings.BloomMultiplier              = vaMath::Clamp( m_settings.BloomMultiplier, 0.0f, 1.0f    ); 

    if( m_settings.UseAutoExposure && deltaTime > 0 )
    {
        float exposureLerpK = vaMath::TimeIndependentLerpF( deltaTime, m_settings.AutoExposureAdaptationSpeed );
        if( m_settings.AutoExposureAdaptationSpeed == std::numeric_limits<float>::infinity() )
            exposureLerpK = 1.0f;

        m_lastAverageLuminance  = vaMath::Max( 0.00001f, m_lastAverageLuminance );

        if( m_settings.UseAutoAutoExposureKeyValue )
        {
            // from https://mynameismjp.wordpress.com/2010/04/30/a-closer-look-at-tone-mapping/
            m_settings.AutoExposureKeyValue = 1.03f - ( 2.0f / ( 2 + std::log10( m_lastAverageLuminance + 1 ) ) );
        }

        float linearExposure    = vaMath::Max( 0.00001f, ( m_settings.AutoExposureKeyValue / m_lastAverageLuminance ) );
        float newExposure       = std::log2( linearExposure );

        m_settings.Exposure     = vaMath::Lerp( m_settings.Exposure, newExposure, exposureLerpK );
        m_settings.Exposure     = vaMath::Clamp( m_settings.Exposure, m_settings.ExposureMin, m_settings.ExposureMax );
    }
}

void vaPostProcessTonemap::ResetHistory( )
{
    for( int i = 0; i < _countof( m_avgLuminancePrev ); i++ )
    {
        m_avgLuminancePrevCPUHasData[i] = false;
    }
}

vaDrawResultFlags vaPostProcessTonemap::TickAndTonemap( float deltaTime, vaCameraBase & camera, vaRenderDeviceContext & renderContext, const std::shared_ptr<vaTexture> & srcRadiance, const AdditionalParams & additionalParams )
{
    vaDrawResultFlags renderResults = vaDrawResultFlags::None;
    bool skipTickOnlyApply = additionalParams.SkipTickOnlyApply;
    const std::shared_ptr<vaTexture> & outMSTonemappedColor = additionalParams.OutMSTonemappedColor;
    const std::shared_ptr<vaTexture> & outMSTonemappedColorComplexityMask = additionalParams.OutMSTonemappedColorComplexityMask;
    const std::shared_ptr<vaTexture> & outExportLuma = additionalParams.OutExportLuma;

    VA_SCOPE_CPUGPU_TIMER( PP_Tonemap, renderContext );

    vaRenderDeviceContext::RenderOutputsState backupOutputs = renderContext.GetOutputs( );

    if( outMSTonemappedColor != nullptr )
    {
        if( outMSTonemappedColorComplexityMask != nullptr )
        {
            assert( ( outMSTonemappedColorComplexityMask->GetSizeX() == outMSTonemappedColor->GetSizeX() ) && ( outMSTonemappedColorComplexityMask->GetSizeY() == outMSTonemappedColor->GetSizeY() ) );
        }
        assert( ( outMSTonemappedColor->GetSizeX() == backupOutputs.RenderTargets[0]->GetSizeX() ) &&  ( outMSTonemappedColor->GetSizeY() == backupOutputs.RenderTargets[0]->GetSizeY() ) );
    }

    UpdateShaders( srcRadiance->GetSampleCount(), outMSTonemappedColor, outMSTonemappedColorComplexityMask, outExportLuma, !additionalParams.SkipTickOnlyApply );

    // to simplify things, resolve radiance into this as an input for bloom and luminance calculation - however, it's not used for final tone mapping + MSAA resolve!
    std::shared_ptr<vaTexture> sourceOrResolvedSourceRadiance;
    if( srcRadiance->GetSampleCount( ) > 1 )
    {
        if( (m_resolvedSrcRadiance == nullptr) || !( ( srcRadiance->GetSizeX() == m_resolvedSrcRadiance->GetSizeX() ) && ( srcRadiance->GetSizeY() == m_resolvedSrcRadiance->GetSizeY() ) && ( srcRadiance->GetSRVFormat() == m_resolvedSrcRadiance->GetSRVFormat() ) ) )
        {
            m_resolvedSrcRadiance = vaTexture::Create2D( GetRenderDevice(), srcRadiance->GetSRVFormat(), srcRadiance->GetSizeX(), srcRadiance->GetSizeY(), 1, 1, 1, vaResourceBindSupportFlags::ShaderResource, vaResourceAccessFlags::Default );
        }
        srcRadiance->ResolveSubresource( renderContext, m_resolvedSrcRadiance, 0, 0, vaResourceFormat::Automatic );
        sourceOrResolvedSourceRadiance = m_resolvedSrcRadiance;
    }
    else
    {
        m_resolvedSrcRadiance = nullptr;
        sourceOrResolvedSourceRadiance = srcRadiance;
    }

    if( m_settings.Enabled )
    {
        // TODO: 
        // instead of using m_halfResRadiance just for bloom, we should create it always and (re)use it for tonemapping luminance input

        static bool useHalfResBloom = true;

        vaVector2i halfSize = vaVector2i( (srcRadiance->GetSizeX() + 1) / 2, (srcRadiance->GetSizeY() + 1) / 2 );
        if( useHalfResBloom && ((m_halfResRadiance == nullptr) || !( ( halfSize.x == m_halfResRadiance->GetSizeX() ) && ( halfSize.y == m_halfResRadiance->GetSizeY() ) && ( srcRadiance->GetSRVFormat() == m_halfResRadiance->GetSRVFormat() ) ) ) )
        {
            m_halfResRadiance = vaTexture::Create2D( GetRenderDevice(), srcRadiance->GetSRVFormat(), halfSize.x, halfSize.y, 1, 1, 1, vaResourceBindSupportFlags::RenderTarget | vaResourceBindSupportFlags::ShaderResource, vaResourceAccessFlags::Default );
        }

        UpdateConstants( renderContext, srcRadiance );

        if( m_settings.UseBloom && m_settings.Enabled )
        {
            float bloomSize = m_settings.BloomSize * (float)((camera.GetYFOVMain())?(srcRadiance->GetSizeY()):(srcRadiance->GetSizeX())) / 100.0f;
            //ImGui::Checkbox( "HALFRESBLOOM", &useHalfResBloom );

            if( useHalfResBloom )
            {
                VA_SCOPE_CPUGPU_TIMER( HalfResBlur, renderContext );

                // Downsample
                vaGraphicsItem renderItem;
                renderContext.FillFullscreenPassRenderItem( renderItem );
                renderItem.ConstantBuffers[POSTPROCESS_TONEMAP_CONSTANTSBUFFERSLOT]= m_constantsBuffer.GetBuffer();
                renderItem.ShaderResourceViews[POSTPROCESS_TONEMAP_TEXTURE_SLOT0]   = sourceOrResolvedSourceRadiance;
                renderItem.PixelShader = m_PSDownsampleToHalf.get();
                renderContext.SetRenderTarget( m_halfResRadiance, nullptr, true );
                renderResults |= renderContext.ExecuteSingleItem( renderItem );

                // Blur
                renderResults |= m_bloomBlur->BlurToScratch( renderContext, m_halfResRadiance, bloomSize / 2.0f );
                renderContext.SetOutputs( backupOutputs );
            }
            else
            {
                VA_SCOPE_CPUGPU_TIMER( FullResBlur, renderContext );
                m_halfResRadiance = nullptr;
                renderResults |= m_bloomBlur->BlurToScratch( renderContext, sourceOrResolvedSourceRadiance, bloomSize );
            }
        }

        vaGraphicsItem renderItem;
        renderContext.FillFullscreenPassRenderItem( renderItem );
        renderItem.ConstantBuffers[POSTPROCESS_TONEMAP_CONSTANTSBUFFERSLOT] = m_constantsBuffer.GetBuffer();

        // Compute average luminance
        // TODO: add bloom here as input as well, as otherwise luminance will ignore it, which isn't really correct - not that just 'adding' bloom is energy-preserving anyway but 
        // adding it before computing luminance will at least give correct tonemapping
        if( !skipTickOnlyApply )
        {
            VA_SCOPE_CPUGPU_TIMER( AverageLuminance, renderContext );
            renderItem.ShaderResourceViews[POSTPROCESS_TONEMAP_TEXTURE_SLOT0]   = sourceOrResolvedSourceRadiance;
            renderItem.PixelShader = m_PSAverageLuminance.get();
            renderContext.SetRenderTarget( m_avgLuminanceMIPViews[0], nullptr, true );
            renderResults |= renderContext.ExecuteSingleItem( renderItem );
        }

        // best time to read luminance copied two+ frames ago!
        if( !additionalParams.NoDelayInGettingLuminance )
        {
            if( !skipTickOnlyApply )
                UpdateLastAverageLuminance( renderContext, additionalParams.NoDelayInGettingLuminance );
        }

        if( !skipTickOnlyApply )
        {
            VA_SCOPE_CPUGPU_TIMER( AverageLumGenMIPs, renderContext );
            for( int i = 1; i < c_avgLuminanceMIPs; i++ )
            {
                renderItem.ShaderResourceViews[POSTPROCESS_TONEMAP_TEXTURE_SLOT0]   = m_avgLuminanceMIPViews[i-1];
                renderItem.PixelShader = m_PSAverageLuminanceGenerateMIP.get();
                renderContext.SetRenderTarget( m_avgLuminanceMIPViews[i], nullptr, true );
                renderResults |= renderContext.ExecuteSingleItem( renderItem );

                // just copy the last results to GPU->CPU transfer textures
                if( i == ( c_avgLuminanceMIPs - 1 ) )
                {
                    m_avgLuminancePrevLastWrittenIndex = ( m_avgLuminancePrevLastWrittenIndex + 1 ) % _countof( m_avgLuminancePrev );

                    renderItem.ShaderResourceViews[POSTPROCESS_TONEMAP_TEXTURE_SLOT0]   = m_avgLuminanceMIPViews[i-1];
                    renderItem.PixelShader = m_PSAverageLuminanceGenerateMIP;
                    renderContext.SetRenderTarget( m_avgLuminancePrev[m_avgLuminancePrevLastWrittenIndex], nullptr, true );
                    renderResults |= renderContext.ExecuteSingleItem( renderItem );

                    m_avgLuminancePrevCPU[m_avgLuminancePrevLastWrittenIndex]->CopyFrom( renderContext, m_avgLuminancePrev[m_avgLuminancePrevLastWrittenIndex] );
                    m_avgLuminancePrevCPUHasData[m_avgLuminancePrevLastWrittenIndex] = renderResults == vaDrawResultFlags::None;

                    //// only first time! this avoids reading unitialized memory and potential numerical differences because of it
                    //if( initializeAll )
                    //    for( int j = 1; j < _countof(m_avgLuminancePrevCPU); j++ )
                    //        m_avgLuminancePrevCPU[j]->CopyFrom( renderContext, m_avgLuminancePrev[m_avgLuminancePrevLastWrittenIndex] );
                }
            }
        }

        if( renderResults == vaDrawResultFlags::None && additionalParams.NoDelayInGettingLuminance )
        {
            if( !skipTickOnlyApply )
                UpdateLastAverageLuminance( renderContext, additionalParams.NoDelayInGettingLuminance );
        }

        if( renderResults == vaDrawResultFlags::None )
            Tick( (!skipTickOnlyApply)?(deltaTime):(0.0f) );

        // Apply bloom
        // TODO: combine into tone mapping / resolve; not nearly as optimal as it could be this way but good enough for now
        if( m_settings.UseBloom )
        {
            VA_SCOPE_CPUGPU_TIMER( AddBloom, renderContext );

            renderItem.ShaderResourceViews[POSTPROCESS_TONEMAP_TEXTURE_SLOT0]   = m_bloomBlur->GetLastScratch();
            renderItem.PixelShader  = m_PSAddBloom.get();
            renderItem.BlendMode    = vaBlendMode::Additive;
            renderContext.SetRenderTarget( srcRadiance, nullptr, true );
            renderResults |= renderContext.ExecuteSingleItem( renderItem );
            renderItem.BlendMode    = vaBlendMode::Opaque;
        }

    }

    if( renderResults == vaDrawResultFlags::None )
    {
        VA_SCOPE_CPUGPU_TIMER( Apply, renderContext );

        UpdateConstants( renderContext, srcRadiance );

        vaRenderDeviceContext::RenderOutputsState backupOutputsCopy = backupOutputs;

        vaGraphicsItem renderItem;
        renderContext.FillFullscreenPassRenderItem( renderItem );
        renderItem.ConstantBuffers[POSTPROCESS_TONEMAP_CONSTANTSBUFFERSLOT] = m_constantsBuffer.GetBuffer();
        
        // special case - we want independent tonemapped MS elements and the control surface
        if( outMSTonemappedColor != nullptr || outExportLuma != nullptr )
        {
            backupOutputsCopy.UAVInitialCounts[0]   = (uint32)-1;
            backupOutputsCopy.UAVInitialCounts[1]   = (uint32)-1;
            backupOutputsCopy.UAVInitialCounts[2]   = (uint32)-1;

            backupOutputsCopy.UAVCount              = 3;
            backupOutputsCopy.UAVsStartSlot         = 1;
            backupOutputsCopy.UAVs[0]               = outMSTonemappedColor;
            backupOutputsCopy.UAVs[1]               = outMSTonemappedColorComplexityMask;
            backupOutputsCopy.UAVs[2]               = outExportLuma;
        }

        renderContext.SetOutputs( backupOutputsCopy );

        renderItem.ShaderResourceViews[POSTPROCESS_TONEMAP_TEXTURE_SLOT0]   = srcRadiance;
        renderItem.ShaderResourceViews[POSTPROCESS_TONEMAP_TEXTURE_SLOT1]   = m_avgLuminance;

        // Apply tonemapping
        if( m_settings.Enabled )
        {
            //renderContext.FullscreenPassDraw( *m_PSTonemap );
            renderItem.PixelShader  = m_PSTonemap.get();
            renderResults |= renderContext.ExecuteSingleItem( renderItem );
        }
        else
        {
            // Just copy the floating point source radiance into output color
            renderItem.ShaderResourceViews[POSTPROCESS_TONEMAP_TEXTURE_SLOT0]   = sourceOrResolvedSourceRadiance;
            renderItem.PixelShader  = m_PSPassThrough.get();
            renderResults |= renderContext.ExecuteSingleItem( renderItem );
        }

        // need to restore outputs
        if( outMSTonemappedColor != nullptr )
        {
            renderContext.SetOutputs( backupOutputs );
        }
    }
    return renderResults;
}