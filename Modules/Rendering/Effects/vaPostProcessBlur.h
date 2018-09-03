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

#include "Rendering/Shaders/vaSharedTypes_PostProcessBlur.h"

#include "Rendering/vaRenderBuffers.h"
#include "Rendering/vaShader.h"

//////////////////////////////////////////////////////////////////////////
// read:
// - https://community.arm.com/servlet/JiveServlet/download/96891546-19463/siggraph2015-mmg-marius-slides.pdf 
// (dual blur seems like an ideal solution, just check the procedure for fract kernels - probably needs 
// upgrading for that)
//
//////////////////////////////////////////////////////////////////////////

namespace VertexAsylum
{

    class vaPostProcessBlur : public vaRenderingModule //, public vaImguiHierarchyObject
    {
    protected:
        int                                     m_texturesUpdatedCounter;

        vaResourceFormat                         m_textureFormat = vaResourceFormat::Unknown;
        vaVector2i                              m_textureSize;

        shared_ptr<vaTexture>                   m_fullresPingTexture;
        shared_ptr<vaTexture>                   m_fullresPongTexture;
        
        shared_ptr<vaTexture>                   m_lastScratchTexture;

        int                                     m_currentGaussKernelRadius;
        float                                   m_currentGaussKernelSigma;
        vector<float>                           m_currentGaussKernel;
        vector<float>                           m_currentGaussWeights;
        vector<float>                           m_currentGaussOffsets;

        vaTypedConstantBufferWrapper< PostProcessBlurConstants >
                                                m_constantsBuffer;
        bool                                    m_constantsBufferNeedsUpdate;

        vaAutoRMI< vaPixelShader >              m_PSGaussHorizontal;
        vaAutoRMI< vaPixelShader >              m_PSGaussVertical;

    private:
        bool                                    m_shadersDirty = true;
        vector< pair< string, string > >        m_staticShaderMacros;


    protected:
        vaPostProcessBlur( const vaRenderingModuleParams & params );

    public:
        virtual ~vaPostProcessBlur( );

    public:

        void                                    UpdateGPUConstants( vaRenderDeviceContext & apiContext, float factor0 );

        // for HDR images use gaussRadius that is at least 6 * ceil(gaussSigma); for LDR 3 * ceil(gaussSigma) is enough
        // if -1 is used, gaussRadius will be calculated as 6 * ceil(gaussSigma)
        // gaussRadius is the only factor that determines performance; 
        vaDrawResultFlags                       Blur( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & srcInput, float gaussSigma, int gaussRadius = -1 );

        // same as Blur except output goes into m_lastScratchTexture which remains valid until next call to Blur or BlurToScratch or device reset
        vaDrawResultFlags                       BlurToScratch( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & srcInput, float gaussSigma, int gaussRadius = -1 );

        // output from last BlurToScratch or nullptr if invalid
        const shared_ptr<vaTexture> &           GetLastScratch( ) const                 { return m_lastScratchTexture; }

    protected:
        void                                    UpdateTextures( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & srcInput );
        bool                                    UpdateKernel( float gaussSigma, int gaussRadius );

    private:
        void                                    UpdateFastKernelWeightsAndOffsets( );

    public:
        void                                    UpdateShaders( vaRenderDeviceContext & apiContext );

    protected:
        virtual vaDrawResultFlags               BlurInternal( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & srcInput, bool blurToScratch );
    };

}
