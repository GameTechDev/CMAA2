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

#include "vaPostProcess.h"

#include "Rendering/vaRenderingIncludes.h"

using namespace VertexAsylum;

vaPostProcess::vaPostProcess( const vaRenderingModuleParams & params ) : 
    m_pixelShaderSingleSampleMS{ (params), (params), (params), (params), (params), (params), (params), (params) },
    vaRenderingModule( vaRenderingModuleParams( params ) ), m_constantsBuffer( params ), m_colorProcessHSBC( params ), m_colorProcessLumaForEdges( params ), m_downsample4x4to1x1( params ),
    m_pixelShaderCompare( params ),
    m_pixelShaderCompareInSRGB( params ),
    m_vertexShaderStretchRect( params ),
    m_pixelShaderStretchRectLinear( params ),
    m_pixelShaderStretchRectPoint( params ),
    m_simpleBlurSharpen( params )
{
    m_comparisonResultsGPU = vaTexture::Create2D( GetRenderDevice(), vaResourceFormat::R32_UINT, POSTPROCESS_COMPARISONRESULTS_SIZE*3, 1, 1, 1, 1, vaResourceBindSupportFlags::UnorderedAccess /*| vaResourceBindSupportFlags::ShaderResource*/, vaTextureAccessFlags::None );
    m_comparisonResultsCPU = vaTexture::Create2D( GetRenderDevice(), vaResourceFormat::R32_UINT, POSTPROCESS_COMPARISONRESULTS_SIZE*3, 1, 1, 1, 1, vaResourceBindSupportFlags::None, vaTextureAccessFlags::CPURead );

    m_pixelShaderCompare->CreateShaderFromFile( L"vaPostProcess.hlsl", "ps_5_0", "PSCompareTextures", m_staticShaderMacros );

    vector<std::pair<std::string, std::string>> srgbMacros = m_staticShaderMacros; srgbMacros.push_back( make_pair( string( "POSTPROCESS_COMPARE_IN_SRGB_SPACE" ), string( "1" ) ) );
    m_pixelShaderCompareInSRGB->CreateShaderFromFile( L"vaPostProcess.hlsl", "ps_5_0", "PSCompareTextures", srgbMacros );

    std::vector<vaVertexInputElementDesc> inputElements;
    inputElements.push_back( { "SV_Position", 0, vaResourceFormat::R32G32B32A32_FLOAT, 0, 0, vaVertexInputElementDesc::InputClassification::PerVertexData, 0 } );
    inputElements.push_back( { "TEXCOORD", 0, vaResourceFormat::R32G32_FLOAT, 0, 16, vaVertexInputElementDesc::InputClassification::PerVertexData, 0 } );

    m_vertexShaderStretchRect->CreateShaderAndILFromFile( L"vaPostProcess.hlsl", "vs_5_0", "VSStretchRect", inputElements, m_staticShaderMacros );
    m_pixelShaderStretchRectLinear->CreateShaderFromFile( L"vaPostProcess.hlsl", "ps_5_0", "PSStretchRectLinear", m_staticShaderMacros );
    m_pixelShaderStretchRectPoint->CreateShaderFromFile( L"vaPostProcess.hlsl", "ps_5_0", "PSStretchRectPoint", m_staticShaderMacros );

    for( int i = 0; i < _countof( m_pixelShaderSingleSampleMS ); i++ )
    {
        vector< pair< string, string > > macros;
        macros.push_back( std::pair<std::string, std::string>( "VA_DRAWSINGLESAMPLEFROMMSTEXTURE_SAMPLE", vaStringTools::Format("%d", i ) ) );
        
        m_pixelShaderSingleSampleMS[i]->CreateShaderFromFile( L"vaPostProcess.hlsl", "ps_5_0", "SingleSampleFromMSTexturePS", macros );
    }

    // this still lets the 3 compile in parallel
    m_vertexShaderStretchRect->WaitFinishIfBackgroundCreateActive( );
    m_pixelShaderStretchRectLinear->WaitFinishIfBackgroundCreateActive( );
    m_pixelShaderStretchRectPoint->WaitFinishIfBackgroundCreateActive( );
}

vaPostProcess::~vaPostProcess( )
{
}

vaDrawResultFlags vaPostProcess::StretchRect( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & srcTexture, const vaVector4 & srcRect, const vaVector4 & dstRect, bool linearFilter, vaBlendMode blendMode, const vaVector4 & colorMul, const vaVector4 & colorAdd )
{
    VA_SCOPE_CPUGPU_TIMER( PP_StretchRect, apiContext );

    // not yet supported / tested
    assert( dstRect.x == 0 );
    assert( dstRect.y == 0 );

    vaVector2 dstPixSize = vaVector2( 1.0f / (dstRect.z-dstRect.x), 1.0f / (dstRect.w-dstRect.y) );

    // Setup
    UpdateShaders( );

    vaVector2 srcPixSize = vaVector2( 1.0f / (float)srcTexture->GetSizeX(), 1.0f / (float)srcTexture->GetSizeY() );

    PostProcessConstants consts;
    consts.Param1.x = dstPixSize.x * dstRect.x * 2.0f - 1.0f;
    consts.Param1.y = 1.0f - dstPixSize.y * dstRect.y * 2.0f;
    consts.Param1.z = dstPixSize.x * dstRect.z * 2.0f - 1.0f;
    consts.Param1.w = 1.0f - dstPixSize.y * dstRect.w * 2.0f;

    consts.Param2.x = srcPixSize.x * srcRect.x;
    consts.Param2.y = srcPixSize.y * srcRect.y;
    consts.Param2.z = srcPixSize.x * srcRect.z;
    consts.Param2.w = srcPixSize.y * srcRect.w;

    consts.Param3   = colorMul;
    consts.Param4   = colorAdd;

    //consts.Param2.x = (float)viewport.Width
    //consts.Param2 = dstRect;

    m_constantsBuffer.Update( apiContext, consts );

    vaRenderItem renderItem;
    apiContext.FillFullscreenPassRenderItem( renderItem );

    renderItem.ConstantBuffers[ POSTPROCESS_CONSTANTS_BUFFERSLOT ]  = m_constantsBuffer.GetBuffer();
    renderItem.ShaderResourceViews[ POSTPROCESS_TEXTURE_SLOT0 ]     = srcTexture;

    renderItem.VertexShader         = m_vertexShaderStretchRect;
    //renderItem.VertexShader->WaitFinishIfBackgroundCreateActive();
    renderItem.PixelShader          = (linearFilter)?(m_pixelShaderStretchRectLinear):(m_pixelShaderStretchRectPoint);
    //renderItem.PixelShader->WaitFinishIfBackgroundCreateActive();
    renderItem.BlendMode            = blendMode;

    return apiContext.ExecuteSingleItem( renderItem );
}

vaDrawResultFlags vaPostProcess::DrawSingleSampleFromMSTexture( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & srcTexture, int sampleIndex )
{
    if( sampleIndex < 0 || sampleIndex >= _countof( m_pixelShaderSingleSampleMS) )
    {
        assert( false );
        return vaDrawResultFlags::UnspecifiedError;
    }

    vaRenderItem renderItem;
    apiContext.FillFullscreenPassRenderItem( renderItem );
    renderItem.ShaderResourceViews[ 0 ] = srcTexture;
    renderItem.PixelShader              = m_pixelShaderSingleSampleMS[sampleIndex];
    return apiContext.ExecuteSingleItem( renderItem );
}

//void vaPostProcess::ColorProcess( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & srcTexture, float someSetting )
//{
//    someSetting;
//    if( !m_colorProcessPS->IsCreated( ) || srcTexture.GetSampleCount( ) != m_colorProcessPSSampleCount )
//    {
//        m_colorProcessPSSampleCount = srcTexture.GetSampleCount();
//        m_colorProcessPS->CreateShaderFromFile( L"vaPostProcess.hlsl", "ps_5_0", "ColorProcessPS", { ( pair< string, string >( "VA_POSTPROCESS_COLORPROCESS", "" ) ), ( pair< string, string >( "VA_POSTPROCESS_COLORPROCESS_MS_COUNT", vaStringTools::Format("%d", m_colorProcessPSSampleCount) ) ) } );
//    }
//    srcTexture.SetToAPISlotSRV( apiContext, 0 );
//    apiContext.FullscreenPassDraw( *m_colorProcessPS );
//    srcTexture.UnsetFromAPISlotSRV( apiContext, 0 );
//}

vaDrawResultFlags vaPostProcess::ColorProcessHSBC( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & srcTexture, float hue, float saturation, float brightness, float contrast )
{
    VA_SCOPE_CPUGPU_TIMER( ColorProcessHueSatBrightContr, apiContext );

    if( !m_colorProcessHSBC->IsCreated( ) )
    {
        m_colorProcessHSBC->CreateShaderFromFile( L"vaPostProcess.hlsl", "ps_5_0", "ColorProcessHSBCPS", { ( pair< string, string >( "VA_POSTPROCESS_COLOR_HSBC", "" ) ) } );
    }

    PostProcessConstants consts; memset( &consts, 0, sizeof(consts) );
    consts.Param1.x = vaMath::Clamp( hue,           -1.0f, 1.0f );
    consts.Param1.y = vaMath::Clamp( saturation,    -1.0f, 1.0f ) + 1.0f;
    consts.Param1.z = vaMath::Clamp( brightness,    -1.0f, 1.0f ) + 1.0f;
    consts.Param1.w = vaMath::Clamp( contrast,      -1.0f, 1.0f );
    // hue goes from [-PI,+PI], saturation goes from [-1, 1], brightness goes from [-1, 1], contrast goes from [-1, 1]
    
    m_constantsBuffer.Update( apiContext, consts);

    vaRenderItem renderItem;
    apiContext.FillFullscreenPassRenderItem( renderItem );
    renderItem.ConstantBuffers[POSTPROCESS_CONSTANTS_BUFFERSLOT]    = m_constantsBuffer.GetBuffer();
    renderItem.ShaderResourceViews[ 0 ]                             = srcTexture;
    renderItem.PixelShader              = m_colorProcessHSBC;
    return apiContext.ExecuteSingleItem( renderItem );
}

// from https://developer.nvidia.com/sites/all/modules/custom/gpugems/books/GPUGems/gpugems_ch24.html
const float BicubicWeight( float x )
{
    const float A = -0.75f;

    x = vaMath::Clamp( x, 0.0f, 2.0f );

    if( x <= 1.0f )
        return (A + 2.0f) * x*x*x - (A + 3.0f) * x*x + 1.0f;
    else
        return A*x*x*x - 5.0f*A*x*x + 8.0f*A*x - 4.0f*A;
}

// all of this is very ad-hoc
vaDrawResultFlags vaPostProcess::SimpleBlurSharpen( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & dstTexture, const shared_ptr<vaTexture> & srcTexture, float sharpen = 0.0f )
{
    VA_SCOPE_CPUGPU_TIMER( SimpleBlurSharpen, apiContext );
    if( !m_simpleBlurSharpen->IsCreated( ) )
    {
        m_simpleBlurSharpen->CreateShaderFromFile( L"vaPostProcess.hlsl", "ps_5_0", "SimpleBlurSharpen", { ( pair< string, string >( "VA_POSTPROCESS_SIMPLE_BLUR_SHARPEN", "" ) ) } );
    }

    assert( (srcTexture->GetSizeX() == dstTexture->GetSizeX() ) && (srcTexture->GetSizeY() == dstTexture->GetSizeY()) );

    vaRenderDeviceContext::RenderOutputsState backupOutputs = apiContext.GetOutputs( );
    apiContext.SetRenderTarget( dstTexture, nullptr, true );

    PostProcessConstants consts; memset( &consts, 0, sizeof(consts) );

    sharpen = vaMath::Clamp( sharpen, -1.0f, 1.0f );

    float blurK     = BicubicWeight( 0.65f );
    float blurDK    = BicubicWeight( 0.65f*vaMath::Sqrt(2.0f) );
    float sharpK    = BicubicWeight( 1.23f );
    float sharpDK   = BicubicWeight( 1.23f*vaMath::Sqrt(2.0f) );

    consts.Param1.x = (sharpen < 0)?(vaMath::Lerp(0.0f, blurK, -sharpen)):(vaMath::Lerp(0.0f, sharpK, sharpen));
    consts.Param1.y = (sharpen < 0)?(vaMath::Lerp(0.0f, blurDK, -sharpen)):(vaMath::Lerp(0.0f, sharpDK, sharpen));
    consts.Param1.z = 0.0f;
    consts.Param1.w = 0.0f;

    m_constantsBuffer.Update( apiContext, consts);

    vaRenderItem renderItem;
    apiContext.FillFullscreenPassRenderItem( renderItem );
    renderItem.ConstantBuffers[POSTPROCESS_CONSTANTS_BUFFERSLOT]    = m_constantsBuffer.GetBuffer();
    renderItem.ShaderResourceViews[ 0 ]                             = srcTexture;
    renderItem.PixelShader              = m_simpleBlurSharpen;
    vaDrawResultFlags renderResults = apiContext.ExecuteSingleItem( renderItem );

    apiContext.SetOutputs( backupOutputs );

    return renderResults;
}

vaDrawResultFlags vaPostProcess::ComputeLumaForEdges( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & srcTexture )
{
    VA_SCOPE_CPUGPU_TIMER( ComputeLumaForEdges, apiContext );

    if( !m_colorProcessLumaForEdges->IsCreated( ) )
    {
        m_colorProcessLumaForEdges->CreateShaderFromFile( L"vaPostProcess.hlsl", "ps_5_0", "ColorProcessLumaForEdges", { ( pair< string, string >( "VA_POSTPROCESS_LUMA_FOR_EDGES", "" ) ) } );
    }

    vaRenderItem renderItem;
    apiContext.FillFullscreenPassRenderItem( renderItem );
    renderItem.ShaderResourceViews[ 0 ] = srcTexture;
    renderItem.PixelShader              = m_colorProcessLumaForEdges;
    return apiContext.ExecuteSingleItem( renderItem );
}

vaDrawResultFlags vaPostProcess::Downsample4x4to1x1( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & dstTexture, const shared_ptr<vaTexture> & srcTexture, float sharpen )
{
    VA_SCOPE_CPUGPU_TIMER( Downsample4x4to1x1, apiContext );

    sharpen = vaMath::Clamp( sharpen, 0.0f, 1.0f );

    assert( (srcTexture->GetSizeX() % 4 == 0) && (srcTexture->GetSizeY() % 4 == 0) );
    assert( (srcTexture->GetSizeX() / 4 == dstTexture->GetSizeX() ) && (srcTexture->GetSizeY() / 4 == dstTexture->GetSizeY()) );

    vaRenderDeviceContext::RenderOutputsState backupOutputs = apiContext.GetOutputs( );
    apiContext.SetRenderTarget( dstTexture, nullptr, true );

    PostProcessConstants consts; memset( &consts, 0, sizeof(consts) );
    consts.Param1.x = 1.0f / (float)srcTexture->GetSizeX();
    consts.Param1.y = 1.0f / (float)srcTexture->GetSizeY();
    consts.Param1.z = 1.0f - sharpen * 0.5f;
    consts.Param1.w = 1.0f - sharpen * 0.5f;

    if( !m_downsample4x4to1x1->IsCreated( ) )
    {
        m_downsample4x4to1x1->CreateShaderFromFile( L"vaPostProcess.hlsl", "ps_5_0", "Downsample4x4to1x1", { ( pair< string, string >( "VA_POSTPROCESS_DOWNSAMPLE", "" ) ) } );
    }

    m_constantsBuffer.Update( apiContext, consts );

    vaRenderItem renderItem;
    apiContext.FillFullscreenPassRenderItem( renderItem );
    renderItem.ConstantBuffers[POSTPROCESS_CONSTANTS_BUFFERSLOT]    = m_constantsBuffer.GetBuffer();
    renderItem.ShaderResourceViews[ 0 ]                             = srcTexture;
    renderItem.PixelShader              = m_downsample4x4to1x1;
    vaDrawResultFlags renderResults = apiContext.ExecuteSingleItem( renderItem );

    apiContext.SetOutputs( backupOutputs );
    return renderResults;
}

void vaPostProcess::UpdateShaders( )
{
    //if( newShaderMacros != m_staticShaderMacros )
    //{
    //    m_staticShaderMacros = newShaderMacros;
    //    m_shadersDirty = true;
    //}

    if( m_shadersDirty )
    {
        m_shadersDirty = false;
    }
}

vaVector4 vaPostProcess::CompareImages( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & textureA, const shared_ptr<vaTexture> & textureB, bool compareInSRGB )
{
    VA_SCOPE_CPUGPU_TIMER( PP_CompareImages, apiContext );

    assert( textureA->GetSizeX() == textureB->GetSizeX() );
    assert( textureA->GetSizeY() == textureB->GetSizeY() );

    // Setup
    UpdateShaders( );

    // DX states
    // backup render targets
    vaRenderDeviceContext::RenderOutputsState outputsState = apiContext.GetOutputs();
    
    int inputSizeX = textureA->GetSizeX();
    int inputSizeY = textureA->GetSizeY();

    // set output
    apiContext.SetRenderTargetsAndUnorderedAccessViews( 0, nullptr, nullptr, 0, 1, &m_comparisonResultsGPU, false );
    apiContext.SetViewport( vaViewport( inputSizeX, inputSizeY ) );

    // clear results UAV
    m_comparisonResultsGPU->ClearUAV( apiContext, vaVector4ui(0, 0, 0, 0) );

    // Call GPU comparison shader
    vaRenderItem renderItem;
    apiContext.FillFullscreenPassRenderItem( renderItem );
    renderItem.ShaderResourceViews[ POSTPROCESS_TEXTURE_SLOT0 ] = textureA;
    renderItem.ShaderResourceViews[ POSTPROCESS_TEXTURE_SLOT1 ] = textureB;
    renderItem.PixelShader = (compareInSRGB)?(m_pixelShaderCompareInSRGB):(m_pixelShaderCompare);
    renderItem.PixelShader->WaitFinishIfBackgroundCreateActive();
    vaDrawResultFlags renderResults = apiContext.ExecuteSingleItem( renderItem );
    if( renderResults != vaDrawResultFlags::None )
    {
        VA_ERROR( "vaPostProcess::CompareImages - error while rendering" );
    }

    // GPU -> CPU copy ( SYNC POINT HERE!! but it doesn't matter because this is only supposed to be used for unit tests and etc.)
    m_comparisonResultsCPU->CopyFrom( apiContext, m_comparisonResultsGPU );

    uint32 data[POSTPROCESS_COMPARISONRESULTS_SIZE*3];
    if( m_comparisonResultsCPU->TryMap( apiContext, vaResourceMapType::Read, false ) )
    {
        auto & mappedData = m_comparisonResultsCPU->GetMappedData();
        memcpy( data, mappedData[0].Buffer, sizeof( data ) );
        m_comparisonResultsCPU->Unmap( apiContext );
    }
    else
    {
        VA_LOG_ERROR( "vaPostProcess::CompareImages failed to map result data!" );
        assert( false );
    }

    // Reset/restore outputs
    apiContext.SetOutputs( outputsState );

    // calculate results
    vaVector4 ret( 0.0f, 0.0f, 0.0f, 0.0f );

    int totalPixelCount = inputSizeX * inputSizeY;
    uint64 resultsSumR = 0;
    uint64 resultsSumG = 0;
    uint64 resultsSumB = 0;

    //double resultsSumD = 0.0;
    //double resultsSumCount = 0.0;

    for( size_t i = 0; i < _countof( data ); i+=3 )
    {
//        VA_LOG( " %d %x", i, data[i] );

        resultsSumR += data[i+0];
        resultsSumG += data[i+1];
        resultsSumB += data[i+2];
    }
    double resultsSumAvg = ((double)resultsSumR + (double)resultsSumG + (double)resultsSumB) / 3.0; // or use Luma-based weights? like (0.2989, 0.5866, 0.1145)? or apply them before sqr in the shader? no idea

    double MSEVal = ( ((double)resultsSumAvg / POSTPROCESS_COMPARISONRESULTS_FIXPOINT_MAX ) / (double)totalPixelCount);

    ret.x = (float)(MSEVal);                        // Mean Squared Error
    ret.y = 10.0f * (float)log10( 1.0 / MSEVal );   // PSNR - we assume 1 is the max value
    ret.z = (float)(MSEVal * 10000.0);              // just to make it more human readable

    return ret;
}
