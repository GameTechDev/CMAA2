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
#include "Core/vaUI.h"

#include "vaRendering.h"

#include "Rendering/vaShader.h"

#include "Rendering/vaRenderBuffers.h"


namespace VertexAsylum
{
    class vaSky;
    class vaTexture;

    // Manages global shader constants and buffers that are not changed 'too frequently' (not per draw call but usually more than once per frame)
    // These are usually (but not necessarily) shared between all draw items between vaRenderDeviceContext::BeginItems/EndItems
    class vaRenderGlobals : public VertexAsylum::vaRenderingModule, public vaUIPanel
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    public:
        static const int            c_shaderDebugFloatOutputCount = SHADERGLOBAL_DEBUG_FLOAT_OUTPUT_COUNT;
        static const int            c_shaderDebugOutputSyncDelay  = vaRenderDevice::c_BackbufferCount+1;

    private:

    protected:
        float                               m_shaderDebugFloats[ c_shaderDebugFloatOutputCount ];

        shared_ptr<vaTexture>               m_shaderDebugOutputGPUTextures[c_shaderDebugOutputSyncDelay];
        shared_ptr<vaTexture>               m_shaderDebugOutputCPUTextures[c_shaderDebugOutputSyncDelay];

        shared_ptr<vaTexture>               m_cursorCaptureGPUTextures[vaRenderDevice::c_BackbufferCount];
        shared_ptr<vaTexture>               m_cursorCaptureCPUTextures[vaRenderDevice::c_BackbufferCount];
        bool                                m_cursorCaptureCPUTexturesHasData[vaRenderDevice::c_BackbufferCount];
        vaVector4                           m_cursorCapture;

        int64                               m_cursorCaptureLastFrame;

        vaVector2                           m_cursorLastScreenPosition;

        vaTypedConstantBufferWrapper< ShaderGlobalConstants >
                                            m_constantsBuffer;

        bool                                m_debugDrawDepth;
        bool                                m_debugDrawNormalsFromDepth;

        shared_ptr<vaPixelShader>           m_debugDrawDepthPS;
        shared_ptr<vaPixelShader>           m_debugDrawNormalsFromDepthPS;
        
        vaAutoRMI<vaComputeShader>          m_cursorCaptureCS;
        vaAutoRMI<vaComputeShader>          m_cursorCaptureMSCS;

    protected:
        vaRenderGlobals( const vaRenderingModuleParams & params );

    public:
        ~vaRenderGlobals( );

    public:
        // array size is c_shaderDebugFloatOutputCount
        const float *                       GetShaderDebugFloatOutput( )                                                            { return m_shaderDebugFloats; }
        void                                GetShaderDebugFloatOutput( float const * & fltArray, int & fltArrayCount )              { fltArray = m_shaderDebugFloats; fltArrayCount = SHADERGLOBAL_DEBUG_FLOAT_OUTPUT_COUNT; }

    public:
        // if calling more than once per frame make sure c_shaderDebugOutputSyncDelay is big enough to avoid stalls
        virtual void                        UpdateDebugOutputFloats( vaSceneDrawContext & drawContext )                             = 0;

        // will use drawContext.Camera to unproject cursor into worldspace position; last position obtained with GetLast3DCursorInfo; don't call this more than once per frame
        virtual void                        Update3DCursor( vaSceneDrawContext & drawContext, const shared_ptr<vaTexture> & depthSource, const vaVector2 & cursorScreenPosition );
        virtual void                        DebugDraw( vaSceneDrawContext & drawContext );

        // x, y, z are worldspace positions of the pixel depth value that was under the cursor when Update3DCursor was called; w is the raw depth data
        const vaVector4 &                   GetLast3DCursorInfo( )  const                                                           { return m_cursorCapture; }

        void                                UpdateShaderConstants( vaSceneDrawContext & drawContext );
        const shared_ptr<vaConstantBuffer>& GetConstantsBuffer( ) const                                                             { return m_constantsBuffer.GetBuffer(); };

    protected:
        const shared_ptr<vaTexture> &       GetCurrentShaderDebugOutput( )                                                          { return m_shaderDebugOutputGPUTextures[ (int)(GetRenderDevice().GetCurrentFrameIndex() % c_shaderDebugOutputSyncDelay) ]; }

    protected:
        virtual string                      UIPanelGetDisplayName( ) const override                                                   { return "RenderingGlobals"; }
        virtual void                        UIPanelDraw( ) override;
    };

}
