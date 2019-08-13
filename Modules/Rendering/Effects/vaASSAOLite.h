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
// This is a slightly updated and simplified ASSAO implementation based on the original code at
// https://github.com/GameTechDev/ASSAO. Some of the important changes are:
//  * moved to platform-independent layer
//  * removed lowest (half res) quality codepath 
//  * removed adaptive (highest) quality codepath due to complexity and replaced it by High-equivalent with more taps
//    (this slightly hurts performance vs before but the codebase is a lot easier to maintain and upgrade)
//  * re-enabled RadiusDistanceScalingFunction as some users like it
//  * platform independent implementation only (going through vaGraphicsItem path) but since this is compute only
//    implementation, it should be easy to port anyway.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Rendering/Shaders/vaShaderCore.h"

#include "Rendering/vaRendering.h"

#include "Rendering/vaShader.h"
#include "Rendering/vaRenderBuffers.h"

#include "Rendering/vaTexture.h"

#include "Core/vaUI.h"

#define INTEL_SSAO_INCLUDED_FROM_CPP

#include "Rendering\Shaders\vaASSAOLite_types.h"

namespace VertexAsylum
{
    class vaASSAOLite : public vaRenderingModule, public vaUIPanel
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );
    private:

    public:
        struct Settings
        {
            float       Radius                              = 1.2f;     // [0.0,  ~ ]   World (view) space size of the occlusion sphere.
            float       ShadowMultiplier                    = 1.0f;     // [0.0, 5.0]   Effect strength linear multiplier
            float       ShadowPower                         = 1.50f;    // [0.5, 5.0]   Effect strength pow modifier
            float       ShadowClamp                         = 0.98f;    // [0.0, 1.0]   Effect max limit (applied after multiplier but before blur)
            float       HorizonAngleThreshold               = 0.06f;    // [0.0, 0.2]   Limits self-shadowing (makes the sampling area less of a hemisphere, more of a spherical cone, to avoid self-shadowing and various artifacts due to low tessellation and depth buffer imprecision, etc.)
            float       FadeOutFrom                         = 50.0f;    // [0.0,  ~ ]   Distance to start start fading out the effect.
            float       FadeOutTo                           = 300.0f;   // [0.0,  ~ ]   Distance at which the effect is faded out.
            int         QualityLevel                        = 2;        // [ -1,  3 ]   Effect quality; -1 - lowest (low, half res checkerboard), 0 - low, 1 - medium, 2 - high, 3 - very high; each quality level is roughly 1.5x more costly than the previous.
    		int         BlurPassCount                       = 2;        // [  0,   6]   Number of edge-sensitive smart blur passes to apply. Quality 0 is an exception with only one 'dumb' blur pass used.
            float       Sharpness                           = 0.98f;    // [0.0, 1.0]   (How much to bleed over edges; 1: not at all, 0.5: half-half; 0.0: completely ignore edges)
            float       TemporalSupersamplingAngleOffset    = 0.0f;     // [0.0,  PI]   Used to rotate sampling kernel; If using temporal AA / supersampling, suggested to rotate by ( (frame%3)/3.0*PI ) or similar. Kernel is already symmetrical, which is why we use PI and not 2*PI.
            float       TemporalSupersamplingRadiusOffset   = 1.0f;     // [0.0, 2.0]   Used to scale sampling kernel; If using temporal AA / supersampling, suggested to scale by ( 1.0f + (((frame%3)-1.0)/3.0)*0.1 ) or similar.
    		float       DetailShadowStrength                = 0.5f;     // [0.0, 5.0]   Used for high-res detail AO using neighboring depth pixels: adds a lot of detail but also reduces temporal stability (adds aliasing).
            float       RadiusDistanceScalingFunction       = 0.0f;     // [0.0, 1.0]   Use 0 for default behavior (world-space radius always Settings::Radius). Anything above 0 means radius will be dynamically scaled per-pixel based on distance from viewer - this breaks consistency but adds AO on distant areas which might be desireable (for ex, for large open-world outdoor areas).
        };

    protected:
        struct BufferFormats
        {
            vaResourceFormat         DepthBufferViewspaceLinear = vaResourceFormat::R16_FLOAT;       // increase to R32_FLOAT if using very low FOVs (for ex, for sniper scope effect) or similar, or in case you suspect artifacts caused by lack of precision; performance will degrade
            vaResourceFormat         AOResult                   = vaResourceFormat::R8G8_UNORM;
#if SSAO_USE_SEPARATE_LOWQ_AORESULTS_TEXTURE
            vaResourceFormat         AOResultLowQ               = vaResourceFormat::R8_UNORM;
#endif
            vaResourceFormat         Normals                    = vaResourceFormat::R8G8B8A8_UNORM;
        };

    protected:
        string                                      m_debugInfo;
        mutable int                                 m_prevQualityLevel;
        mutable bool                                m_debugShowNormals;
        mutable bool                                m_debugShowEdges;

        BufferFormats                               m_formats;

        vaVector2i                                  m_size;
        vaVector2i                                  m_halfSize;
        vaVector2i                                  m_quarterSize;

        vaRecti                                     m_fullResOutScissorRect;
        vaRecti                                     m_halfResOutScissorRect;

        shared_ptr<vaTexture>                       m_halfDepths[4];
        shared_ptr<vaTexture>                       m_halfDepthsMipViews[4][SSAO_DEPTH_MIP_LEVELS];
        shared_ptr<vaTexture>                       m_pingPongHalfResultA;
        shared_ptr<vaTexture>                       m_pingPongHalfResultB;
        shared_ptr<vaTexture>                       m_finalResults;
        shared_ptr<vaTexture>                       m_finalResultsArrayViews[4];
        shared_ptr<vaTexture>                       m_normals;
        //shared_ptr<vaTexture>                       m_loadCounter;
        //shared_ptr<vaTexture>                       m_importanceMap;
        //shared_ptr<vaTexture>                       m_importanceMapPong;

        shared_ptr<vaTexture>                       m_debuggingOutput;

        mutable Settings                            m_settings;

        pair< string, string >                      m_specialShaderMacro;

        vaAutoRMI<vaVertexShader>                   m_vertexShader;
        
        // MSAA versions include 1-sample for non-MSAA
        vaAutoRMI<vaPixelShader>                    m_pixelShaderPrepareDepthsMSAA[5];
        vaAutoRMI<vaPixelShader>                    m_pixelShaderPrepareDepthsAndNormalsMSAA[5];
        vaAutoRMI<vaPixelShader>                    m_pixelShaderPrepareDepthMip[SSAO_DEPTH_MIP_LEVELS-1];
        vaAutoRMI<vaPixelShader>                    m_pixelShaderGenerate[4];
        vaAutoRMI<vaPixelShader>                    m_pixelShaderSmartBlur;
        vaAutoRMI<vaPixelShader>                    m_pixelShaderSmartBlurWide;
        vaAutoRMI<vaPixelShader>                    m_pixelShaderApply;
        vaAutoRMI<vaPixelShader>                    m_pixelShaderNonSmartBlur;
        vaAutoRMI<vaPixelShader>                    m_pixelShaderNonSmartApply;

        // to allow background compilation but still wait on them on draw - causes a sync point but it's either that or few frames with no effect which can cause subtle difference and mess up automated testing
        vector<shared_ptr<vaShader>>                m_allShaders;

        bool                                        m_shadersDirty = true;

        vaTypedConstantBufferWrapper< ASSAOLiteConstants >
                                                    m_constantsBuffer;

        vector< pair< string, string > >            m_staticShaderMacros;

        wstring                                     m_shaderFileToUse = L"vaASSAOLite.hlsl";

        bool                                        m_requiresClear = false;

    public:
        vaASSAOLite( const vaRenderingModuleParams & params );

    public:
        virtual ~vaASSAOLite( ) { }

    public:
        virtual vaDrawResultFlags                   Draw( vaSceneDrawContext & drawContext, const vaMatrix4x4 & projMatrix, const shared_ptr<vaTexture> & depthTexture, bool blend, const shared_ptr<vaTexture> & normalmapTexture = nullptr, const vaVector4i & scissorRect = vaVector4i( 0, 0, 0, 0 ) );

    public:
        Settings &                                  GetSettings( ) { return m_settings; }

        // used for debugging & optimization tests - just sets a single shader macro for all shaders (and triggers a shader recompile)
        void                                        SetSpecialShaderMacro( const pair< string, string > & ssm ) { m_specialShaderMacro = ssm; }

        bool &                                      DebugShowNormals()                                                      { return m_debugShowNormals;        }
        bool &                                      DebugShowEdges()                                                        { return m_debugShowEdges;          }

    private:
        virtual void                                UIPanelDraw( ) override;

    protected:
        const wchar_t *                             GetShaderFilePath( ) const { return m_shaderFileToUse.c_str( ); };

        bool                                        ReCreateIfNeeded( shared_ptr<vaTexture> & inoutTex, vaVector2i size, vaResourceFormat format, float & inoutTotalSizeSum, int mipLevels, int arraySize, bool supportUAVs );
        void                                        UpdateTextures( vaSceneDrawContext & drawContext, int width, int height, bool generateNormals, const vaVector4i & scissorRect );
        void                                        UpdateConstants( vaSceneDrawContext & drawContext, const vaMatrix4x4 & projMatrix, int pass );

        void                                        PrepareDepths( vaSceneDrawContext & drawContext, const shared_ptr<vaTexture> & depthTexture, vaGraphicsItem & fullscreenGraphicsItem, bool generateNormals );
        void                                        GenerateSSAO( vaSceneDrawContext & drawContext, const vaMatrix4x4 & projMatrix, const shared_ptr<vaTexture> & normalmapTexture, vaGraphicsItem & fullscreenGraphicsItem );
        //
        vaDrawResultFlags                           Draw( vaSceneDrawContext & drawContext, const vaMatrix4x4 & projMatrix, const shared_ptr<vaTexture> & depthTexture, const shared_ptr<vaTexture> & normalmapTexture, bool blend, bool generateNormals );
    };

} // namespace VertexAsylum

