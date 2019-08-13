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

#pragma once

#include "Core/vaCoreIncludes.h"

#include "Rendering/vaRendering.h"

#include "Rendering/vaTexture.h"

#include "Rendering/Shaders/vaSharedTypes.h"
#include "Rendering/vaShader.h"

#include "Rendering/vaRenderBuffers.h"

namespace VertexAsylum
{

    // A collection of post-processing helpers
    //
    // TODO: 
    //   [ ] some of the functions expect the render target to be set to destination while others do not. Switch them all to make no 
    //       assumptions and take destination as parameter; optimize internally to avoid setting RTs if not needed. 
    //       This will make it both more easier to use but also easier to switch to CS implementations if needed.
    class vaPostProcess : public vaRenderingModule
    {
    protected:
        vaPostProcess( const vaRenderingModuleParams & params );

    protected:
        std::shared_ptr<vaTexture>                  m_comparisonResultsGPU;
        std::shared_ptr<vaTexture>                  m_comparisonResultsCPU;

        vaTypedConstantBufferWrapper< PostProcessConstants >
                                                    m_constantsBuffer;

        vaAutoRMI<vaPixelShader>                    m_pixelShaderSingleSampleMS[8];

        // vaAutoRMI<vaPixelShader>                    m_colorProcessPS;
        // int                                         m_colorProcessPSSampleCount = -1;

        vaAutoRMI<vaPixelShader>                    m_colorProcessHSBC;
        vaAutoRMI<vaPixelShader>                    m_simpleBlurSharpen;
        vaAutoRMI<vaPixelShader>                    m_colorProcessLumaForEdges;
        vaAutoRMI<vaPixelShader>                    m_downsample4x4to1x1;

        vaAutoRMI<vaPixelShader>                    m_pixelShaderCompare;
        vaAutoRMI<vaPixelShader>                    m_pixelShaderCompareInSRGB;

        vaAutoRMI<vaVertexShader>                   m_vertexShaderStretchRect;
        vaAutoRMI<vaPixelShader>                    m_pixelShaderStretchRectLinear;
        vaAutoRMI<vaPixelShader>                    m_pixelShaderStretchRectPoint;

        bool                                        m_shadersDirty = true;
        vector< pair< string, string > >            m_staticShaderMacros;

        
    public:
        virtual ~vaPostProcess( );

    protected:
        void UpdateShaders( );

    public:
        virtual vaDrawResultFlags                   DrawSingleSampleFromMSTexture( vaRenderDeviceContext & renderContext, const shared_ptr<vaTexture> & srcTexture, int sampleIndex );

        virtual vaDrawResultFlags                   StretchRect( vaRenderDeviceContext & renderContext, const shared_ptr<vaTexture> & srcTexture, const vaVector4 & srcRect, const vaVector4 & dstRect, bool linearFilter, vaBlendMode blendMode = vaBlendMode::Opaque, const vaVector4 & colorMul = vaVector4( 1.0f, 1.0f, 1.0f, 1.0f ), const vaVector4 & colorAdd = vaVector4( 0.0f, 0.0f, 0.0f, 0.0f ) );

        // virtual void                                ColorProcess( vaRenderDeviceContext & renderContext, const shared_ptr<vaTexture> &  srcTexture, float someSetting );

        // Copies srcTexture into current render target (no stretching) while applying the Hue Saturation Brightness Contrast;
        // hue goes from [-1,1], saturation goes from [-1, 1], brightness goes from [-1, 1], contrast goes from [-1, 1]
        virtual vaDrawResultFlags                   ColorProcessHSBC( vaRenderDeviceContext & renderContext, const shared_ptr<vaTexture> & srcTexture, float hue, float saturation, float brightness, float contrast );

        // simple blur/sharpen filter with a 3x3 kernel; sharpen is in range of [-1, 1] (negative: blur, positive: sharpen); this is not to be used for resampling so it requires source & destination to be of the same size
        virtual vaDrawResultFlags                   SimpleBlurSharpen( vaRenderDeviceContext & renderContext, const shared_ptr<vaTexture> & dstTexture, const shared_ptr<vaTexture> & srcTexture, float sharpen );

        // see RGBToLumaForEdges() in vaShared.hlsl
        virtual vaDrawResultFlags                   ComputeLumaForEdges( vaRenderDeviceContext & renderContext, const shared_ptr<vaTexture> & srcTexture );

        // Returned .x is MSE, .y is PSNR
        virtual vaVector4                           CompareImages( vaRenderDeviceContext & renderContext, const shared_ptr<vaTexture> & textureA, const shared_ptr<vaTexture> & textureB, bool compareInSRGB = true );

        // So far only used for 4x4 supersampling - sharpen goes from 0, 1 (1 uses only 4 inner values, 0 uses all 16 equally)
        virtual vaDrawResultFlags                   Downsample4x4to1x1( vaRenderDeviceContext & renderContext, const shared_ptr<vaTexture> & dstTexture, const shared_ptr<vaTexture> & srcTexture, float sharpen = 0.0f );

//        virtual void                                GenerateNormalmap( vaRenderDeviceContext & renderContext, const shared_ptr<vaTexture> & depthTexture ) = 0;
    };

}
