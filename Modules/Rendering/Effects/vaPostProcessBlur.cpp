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

#include "vaPostProcessBlur.h"

using namespace VertexAsylum;

template< class _Alloc = allocator<_Ty> >
static void GenerateSeparableGaussKernel( float sigma, int kernelSize, std::vector<float, _Alloc> & outKernel )
{
    if( (kernelSize % 2) != 1 )
    {
        assert( false ); // kernel size must be odd number
        outKernel.resize(0);
        return;
    }

    int halfKernelSize = kernelSize/2;

    outKernel.resize( kernelSize );

    const double cPI= 3.14159265358979323846;
    double mean     = halfKernelSize;
    double sum      = 0.0;
    for (int x = 0; x < kernelSize; ++x) 
    {
        outKernel[x] = (float)sqrt( exp( -0.5 * (pow((x-mean)/sigma, 2.0) + pow((mean)/sigma,2.0)) )
            / (2 * cPI * sigma * sigma) );
        sum += outKernel[x];
    }
    for (int x = 0; x < kernelSize; ++x) 
        outKernel[x] /= (float)sum;
}

vaPostProcessBlur::vaPostProcessBlur( const vaRenderingModuleParams & params )
 : vaRenderingModule( params ), 
    m_textureSize( vaVector2i( 0, 0 ) ),
    m_constantsBuffer( params.RenderDevice ),
    m_PSGaussHorizontal( params ),
    m_PSGaussVertical( params )
{
    m_texturesUpdatedCounter = 0;
    m_currentGaussKernelRadius = 0;
    m_currentGaussKernelSigma = 0.0f;

    m_constantsBufferNeedsUpdate = true;

    m_PSGaussHorizontal->CreateShaderFromFile( L"vaPostProcessBlur.hlsl", "ps_5_0", "PSGaussHorizontal", m_staticShaderMacros );
    m_PSGaussVertical->CreateShaderFromFile( L"vaPostProcessBlur.hlsl", "ps_5_0", "PSGaussVertical", m_staticShaderMacros );
}

vaPostProcessBlur::~vaPostProcessBlur( )
{
}

void vaPostProcessBlur::UpdateShaders( vaRenderDeviceContext & apiContext )
{
    apiContext; // unreferenced

    //if( newShaderMacros != m_staticShaderMacros )
    //{
    //    m_staticShaderMacros = newShaderMacros;
    //    m_shadersDirty = true;
    //}

    if( m_shadersDirty )
    {
        m_shadersDirty = false;

        // m_PSBlurA.CreateShaderFromFile( L"vaPostProcessBlur.hlsl", "ps_5_0", "PSBlurA", m_staticShaderMacros );
        // m_PSBlurB.CreateShaderFromFile( L"vaPostProcessBlur.hlsl", "ps_5_0", "PSBlurB", m_staticShaderMacros );
    }
}

void vaPostProcessBlur::UpdateGPUConstants( vaRenderDeviceContext & apiContext, float factor0 )
{
    if( !m_constantsBufferNeedsUpdate )
        return;
    m_constantsBufferNeedsUpdate = false;

    // Constants
    {
        PostProcessBlurConstants consts;
        consts.PixelSize    = vaVector2( 1.0f / (float)m_textureSize.x, 1.0f / (float)m_textureSize.y );
        consts.Factor0      = factor0;
        consts.Dummy0       = 0;
        consts.Dummy1       = 0;
        consts.Dummy2       = 0;
        consts.Dummy3       = 0;

        consts.GaussIterationCount = (int)m_currentGaussOffsets.size( );
        assert( consts.GaussIterationCount == (int)m_currentGaussWeights.size( ) );

        for( int i = 0; i < _countof(consts.GaussOffsetsWeights); i++ )
        {
            if( i < consts.GaussIterationCount )
                consts.GaussOffsetsWeights[i] = vaVector4( m_currentGaussOffsets[i], m_currentGaussWeights[i], 0.0f, 0.0f );
            else
                consts.GaussOffsetsWeights[i] = vaVector4( 0.0f, 0.0f, 0.0f, 0.0f );
        }

        m_constantsBuffer.Update( apiContext, consts );
    }
}

void vaPostProcessBlur::UpdateFastKernelWeightsAndOffsets( )
{
    std::vector<float> & inputKernel = m_currentGaussKernel;
    int kernelSize = (int)inputKernel.size();
    if( kernelSize == 0 )
    {
        return;
    }

    assert( (kernelSize % 2) == 1 );
//    assert( (((kernelSize/2)+1) % 2) == 0 );

    vaStackVector< float, 4096 > oneSideInputsStackVector;
    auto & oneSideInputs = oneSideInputsStackVector.container();

    for( int i = (kernelSize/2); i >= 0; i-- )
    {
        if( i == (kernelSize/2) )
            oneSideInputs.push_back( (float)inputKernel[i] * 0.5f );
        else
            oneSideInputs.push_back( (float)inputKernel[i] );
    }
    if( (oneSideInputs.size() % 2) == 1 )
        oneSideInputs.push_back( 0.0f );

    int numSamples = (int)oneSideInputs.size()/2;

    std::vector<float> & weights = m_currentGaussWeights;
    weights.clear();

    float weightSum = 0.0f;

    for( int i = 0; i < numSamples; i++ )
    {
        float sum = oneSideInputs[i*2+0] + oneSideInputs[i*2+1];
        weights.push_back(sum);

        weightSum += sum;
    }

    std::vector<float> & offsets = m_currentGaussOffsets;
    offsets.clear();

    for( int i = 0; i < numSamples; i++ )
    {
        offsets.push_back( i*2.0f + oneSideInputs[i*2+1] / weights[i] );
    }

    assert( m_currentGaussOffsets.size() == m_currentGaussWeights.size( ) );


    // std::string indent = "    ";
    // 
    // std::string shaderCode = (forPreprocessorDefine)?(""):("");
    // std::string eol = (forPreprocessorDefine)?("\\\n"):("\n");
    // if( !forPreprocessorDefine) shaderCode += indent + "//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////;" + eol;
    // if( !forPreprocessorDefine) shaderCode += indent + stringFormatA( "// Kernel width %d x %d", kernelSize, kernelSize ) + eol;
    // if( !forPreprocessorDefine) shaderCode += indent + "//" + eol;
    // shaderCode += indent + stringFormatA( "const int stepCount = %d;", numSamples ) + eol;
    // 
    // if( !workaroundForNoCLikeArrayInitialization )
    // {
    //     if( !forPreprocessorDefine) shaderCode += indent + "//" + eol;
    //     shaderCode += indent + "const float gWeights[stepCount] ={" + eol;
    //     for( int i = 0; i < numSamples; i++ )
    //         shaderCode += indent + stringFormatA( "   %.5f", weights[i] ) + ((i!=(numSamples-1))?(","):("")) + eol;
    //     shaderCode += indent + "};"+eol;
    //     shaderCode += indent + "const float gOffsets[stepCount] ={"+eol;
    //     for( int i = 0; i < numSamples; i++ )
    //         shaderCode += indent + stringFormatA( "   %.5f", offsets[i] ) + ((i!=(numSamples-1))?(","):("")) + eol;
    //     shaderCode += indent + "};" + eol;
    // }
    // else
    // {
    //     if( !forPreprocessorDefine) shaderCode += indent + "//" + eol;
    //     shaderCode += indent + "float gWeights[stepCount];" + eol;
    //     for( int i = 0; i < numSamples; i++ )
    //         shaderCode += indent + stringFormatA( " gWeights[%d] = %.5f;", i, weights[i] ) + eol;
    //     shaderCode += indent + eol;
    //     shaderCode += indent + "float gOffsets[stepCount];"+eol;
    //     for( int i = 0; i < numSamples; i++ )
    //         shaderCode += indent + stringFormatA( " gOffsets[%d] = %.5f;", i, offsets[i] ) + eol;
    //     shaderCode += indent + eol;
    // }
    // 
    // if( !forPreprocessorDefine) shaderCode += indent + "//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////;" + eol;
    // 
    // return shaderCode;
}

bool vaPostProcessBlur::UpdateKernel( float gaussSigma, int gaussRadius )
{
    gaussSigma = vaMath::Clamp( gaussSigma, 0.1f, 256.0f );
    if( gaussRadius == -1 )
    {
        // The '* 5.0f' is a very ad-hoc heuristic for computing the default kernel (actual kernel is radius * 2 + 1) size so the
        // precision remains good for HDR range of roughly [0, 1000] for sensible sigmas (i.e. up to 100-ish).
        // To do it properly (either compute kernel size so that required % of the curve is within the discrete kernel area, or so 
        // that the edge weight is below min required precision threshold), refer to:
        // http://dev.theomader.com/gaussian-kernel-calculator/ and/or http://reference.wolfram.com/language/ref/GaussianMatrix.html
        gaussRadius = (int)vaMath::Ceil( gaussSigma * 5.0f );
    }
    if( gaussRadius <= 0 )
    {
        assert( false );
        return false;
    }
    if( gaussRadius > 2048 )
    {
        assert( false );    // too large, not supported
        return false;
    }

    // no need to update
    if( ( gaussRadius == m_currentGaussKernelRadius ) && (vaMath::Abs( gaussSigma - m_currentGaussKernelSigma ) < 1e-5f) )
        return true;

    m_constantsBufferNeedsUpdate = true;

    m_currentGaussKernelRadius  = gaussRadius; 
    m_currentGaussKernelSigma   = gaussSigma;

    // just ensure sensible values
    assert( (gaussRadius > gaussSigma) && (gaussRadius < gaussSigma*12.0f) );

    int kernelSize = (int)gaussRadius * 2 + 1;

    {
        vaStackVector< float, 4096 > tmpKernelArray;
        GenerateSeparableGaussKernel( m_currentGaussKernelSigma, kernelSize, tmpKernelArray.container() );
        m_currentGaussKernel.resize( tmpKernelArray.container().size() );
        for( size_t i = 0; i < tmpKernelArray.container().size(); i++ )
            m_currentGaussKernel[i] = (float)tmpKernelArray[i];
    }

    UpdateFastKernelWeightsAndOffsets( );

    return true;
}

void vaPostProcessBlur::UpdateTextures( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & srcInput )
{
    apiContext; // unreferenced

    vaResourceFormat srcFormat = srcInput->GetSRVFormat();

    if( ( srcInput->GetSizeX( ) == m_textureSize.x ) && ( srcInput->GetSizeY( ) == m_textureSize.y ) && ( srcFormat == m_textureFormat ) )
    {
        m_texturesUpdatedCounter = vaMath::Max( 0, m_texturesUpdatedCounter-1 );
        return;
    }

    m_textureSize.x    = srcInput->GetSizeX( );
    m_textureSize.y    = srcInput->GetSizeY( );
    m_textureFormat     = srcFormat;

    m_texturesUpdatedCounter += 2;
    // textures being updated multiple times per frame? that's bad: use separate vaPostProcessBlur instead
    assert( m_texturesUpdatedCounter < 10 );

    m_fullresPingTexture = vaTexture::Create2D( GetRenderDevice(), m_textureFormat, m_textureSize.x, m_textureSize.y, 1, 1, 1, vaResourceBindSupportFlags::RenderTarget | vaResourceBindSupportFlags::ShaderResource, vaTextureAccessFlags::None );
    m_fullresPongTexture = vaTexture::Create2D( GetRenderDevice(), m_textureFormat, m_textureSize.x, m_textureSize.y, 1, 1, 1, vaResourceBindSupportFlags::RenderTarget | vaResourceBindSupportFlags::ShaderResource, vaTextureAccessFlags::None );
    m_lastScratchTexture = nullptr;
}

vaDrawResultFlags vaPostProcessBlur::Blur( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & srcInput, float gaussSigma, int gaussRadius )
{
    if( !UpdateKernel( gaussSigma, gaussRadius ) )
        return vaDrawResultFlags::UnspecifiedError;

    UpdateTextures( apiContext, srcInput );

    m_lastScratchTexture = nullptr;
    return BlurInternal( apiContext, srcInput, false );
}

// output goes into m_lastScratchTexture which remains valid until next call to Blur or BlurToScratch or device reset
vaDrawResultFlags vaPostProcessBlur::BlurToScratch( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & srcInput, float gaussSigma, int gaussRadius )
{
    if( !UpdateKernel( gaussSigma, gaussRadius ) )
        return vaDrawResultFlags::UnspecifiedError;

    UpdateTextures( apiContext, srcInput );

    m_lastScratchTexture = nullptr;
    return BlurInternal( apiContext, srcInput, true );
}

vaDrawResultFlags vaPostProcessBlur::BlurInternal( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & srcInput, bool blurToScratch )
{
    VA_SCOPE_CPUGPU_TIMER( PP_Blur, apiContext );

    vaDrawResultFlags renderResults = vaDrawResultFlags::None;

    vaRenderDeviceContext::RenderOutputsState backupOutputs = apiContext.GetOutputs( );

    // Setup
    UpdateShaders( apiContext );

    vaRenderItem renderItem;
    apiContext.FillFullscreenPassRenderItem( renderItem );

    renderItem.ConstantBuffers[ POSTPROCESS_BLUR_CONSTANTS_BUFFERSLOT ]   = m_constantsBuffer.GetBuffer();

    // Separable Gauss blur
//    if( (int)m_currentGaussOffsets.size( ) > 0 )
    {
        UpdateGPUConstants( apiContext, 0.0f );

        apiContext.BeginItems();

        apiContext.SetRenderTarget( m_fullresPongTexture, nullptr, true );

        renderItem.ShaderResourceViews[ POSTPROCESS_BLUR_TEXTURE_SLOT0 ]    = srcInput;

        renderItem.PixelShader = m_PSGaussHorizontal.get();

        renderResults |= apiContext.ExecuteItem( renderItem );

        renderItem.ShaderResourceViews[ POSTPROCESS_BLUR_TEXTURE_SLOT0 ]    = nullptr;

        if( blurToScratch )
        {
            apiContext.SetRenderTarget( m_fullresPingTexture, nullptr, true );
            m_lastScratchTexture = m_fullresPingTexture;
        }
        else
        {
            apiContext.SetOutputs( backupOutputs );
            m_lastScratchTexture = nullptr;
        }

        renderItem.ShaderResourceViews[ POSTPROCESS_BLUR_TEXTURE_SLOT0 ]    = m_fullresPongTexture;

        renderItem.PixelShader = m_PSGaussVertical.get();

        renderResults |= apiContext.ExecuteItem( renderItem );

        apiContext.EndItems();
    }

    if( blurToScratch )
        apiContext.SetOutputs( backupOutputs );

    return renderResults;
}