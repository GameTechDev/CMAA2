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

#include "vaRendering.h"

#include "Rendering/vaShader.h"

#include "Rendering/vaTextureHelpers.h"

#include "Rendering/vaRenderBuffers.h"


namespace VertexAsylum
{
    class vaSky;
    class vaTexture;

    class vaRenderingGlobals : public VertexAsylum::vaRenderingModule, public vaImguiHierarchyObject
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    public:
        static const int            c_shaderDebugFloatOutputCount = SHADERGLOBAL_DEBUG_FLOAT_OUTPUT_COUNT;
        static const int            c_shaderDebugOutputSyncDelay  = vaRenderDevice::c_DefaultBackbufferCount+2;

    private:

    protected:
        float                               m_shaderDebugFloats[ c_shaderDebugFloatOutputCount ];

        shared_ptr<vaTexture>               m_shaderDebugOutputGPUTextures[c_shaderDebugOutputSyncDelay];
        shared_ptr<vaTexture>               m_shaderDebugOutputCPUTextures[c_shaderDebugOutputSyncDelay];

        shared_ptr<vaTexture>               m_cursorCaptureGPUTextures[c_shaderDebugOutputSyncDelay];
        shared_ptr<vaTexture>               m_cursorCaptureCPUTextures[c_shaderDebugOutputSyncDelay];
        vaVector4                           m_cursorCapture;

        int64                               m_frameIndex;

        int64                               m_cursorCaptureLastFrame;

        vaVector2                           m_cursorLastScreenPosition;

        double                              m_totalTime;

        vaTypedConstantBufferWrapper< ShaderGlobalConstants >
                                            m_constantsBuffer;

        bool                                m_debugDrawDepth;
        bool                                m_debugDrawNormalsFromDepth;

        vaAutoRMI<vaPixelShader>            m_debugDrawDepthPS;
        vaAutoRMI<vaPixelShader>            m_debugDrawNormalsFromDepthPS;
        vaAutoRMI<vaComputeShader>          m_cursorCaptureCS;

    protected:
        vaRenderingGlobals( const vaRenderingModuleParams & params );

    public:
        ~vaRenderingGlobals( );

    public:
        // array size is c_shaderDebugFloatOutputCount
        const float *               GetShaderDebugFloatOutput( )                                                                { return m_shaderDebugFloats; }
        void                        GetShaderDebugFloatOutput( float const * & fltArray, int & fltArrayCount )                  { fltArray = m_shaderDebugFloats; fltArrayCount = SHADERGLOBAL_DEBUG_FLOAT_OUTPUT_COUNT; }


    public:
        void                        Tick( float deltaTime );
        int64                       GetFrameIndex( ) const                                                                      { return m_frameIndex; }

        double                      GetTotalTime( ) const                                                                       { return m_totalTime; }

    public:
        // if calling more than once per frame make sure c_shaderDebugOutputSyncDelay is big enough to avoid stalls
        virtual void                UpdateDebugOutputFloats( vaSceneDrawContext & drawContext )                                      = 0;

        // will use drawContext.Camera to unproject cursor into worldspace position; last position obtained with GetLast3DCursorInfo; don't call this more than once per frame
        virtual void                Update3DCursor( vaSceneDrawContext & drawContext, const shared_ptr<vaTexture> & depthSource, const vaVector2 & cursorScreenPosition );
        virtual void                DebugDraw( vaSceneDrawContext & drawContext );

        // x, y, z are worldspace positions of the pixel depth value that was under the cursor when Update3DCursor was called; w is the raw depth data
        const vaVector4 &           GetLast3DCursorInfo( )  const                                                               { return m_cursorCapture; }

    protected:
        void                        UpdateShaderConstants( vaSceneDrawContext & drawContext );

    protected:
        // vaImguiHierarchyObject
        virtual string                              IHO_GetInstanceName( ) const                { return "RenderingGlobals"; }
        virtual void                                IHO_Draw( );
    };

}
