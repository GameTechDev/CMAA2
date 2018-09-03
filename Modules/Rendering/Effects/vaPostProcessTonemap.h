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
#include "Rendering/vaShader.h"

#include "Rendering/vaTexture.h"

#include "Rendering/Effects/vaPostProcessBlur.h"

#include "Rendering/Shaders/vaSharedTypes.h"

#include "IntegratedExternals/vaImguiIntegration.h"

#include "Rendering/Shaders/vaSharedTypes_PostProcessTonemap.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// at the moment, very simple Reinhard implementation 
// ("High Dynamic Range Imaging, Acquisition, Display, and Image - Based Lighting, 2nd Edition")
//
// for future, read:
//  - http://filmicgames.com/archives/75
//  - https://mynameismjp.wordpress.com/2010/04/30/a-closer-look-at-tone-mapping/
//  - http://gpuopen.com/optimized-reversible-tonemapper-for-resolve/
//  - https://developer.nvidia.com/preparing-real-hdr
//  - https://developer.nvidia.com/implementing-hdr-rise-tomb-raider
///////////////////////////////////////////////////////////////////////////////////////////////////

namespace VertexAsylum
{
    class vaPostProcessTonemap : public vaRenderingModule, public vaImguiHierarchyObject
    {
    protected:
        vaPostProcessTonemap( const vaRenderingModuleParams & params );

    public:
        struct TMSettings
        {
            bool    Enabled;                        // if 'false' then it's just a pass-through, saturate( input )

            float   Saturation;                     // [   0.0, 5.0     ]
            float   Exposure;                       // [ -ExposureMin, +ExposureMax  ]
            float   ExposureMin;                    // [ -20.0, +20.0   ]       // danger, the UI expect these to be consecutive in memory 
            float   ExposureMax;                    // [ -20.0, +20.0   ]       // danger, the UI expect these to be consecutive in memory 
            bool    UseAutoExposure;                // 
            float   AutoExposureAdaptationSpeed;    // [   0.1, FLT_MAX ]
            float   AutoExposureKeyValue;           // [   0.0, 2.0     ]
            bool    UseAutoAutoExposureKeyValue;    // 
            bool    UseModifiedReinhard;            // 
            float   ModifiedReinhardWhiteLevel;     // [   0.0, FLT_MAX ]

            bool    UseBloom;
            float   BloomSize;                      // the gaussian blur sigma used is BloomSize scaled by resolution
            float   BloomThreshold;
            float   BloomMultiplier;

            TMSettings( )
            {
                Enabled                     = true;
                Saturation                  = 1.0f;
                Exposure                    = 0.0f;
                UseAutoExposure             = true;
                AutoExposureAdaptationSpeed = 10.0f;
                AutoExposureKeyValue        = 0.2f;
                UseAutoAutoExposureKeyValue = false;
                UseModifiedReinhard         = true;
                ModifiedReinhardWhiteLevel  = 5.0f;
                UseBloom                    = false;
                BloomSize                   = 0.3f;
                BloomThreshold              = 0.015f;
                BloomMultiplier             = 0.04f;
                ExposureMin                 = -20.0f;
                ExposureMax                 = 20.0f;
            }
        };

        struct AdditionalParams
        {
            bool                                    NoDelayInGettingLuminance           = false;        // for image reproducibility this forces using same-frame luminance but it introduces a CPU<->GPU sync point so not to be used where performance is important
            bool                                    SkipTickOnlyApply                   = false;
            std::shared_ptr<vaTexture>              OutMSTonemappedColor                = nullptr;
            std::shared_ptr<vaTexture>              OutMSTonemappedColorComplexityMask  = nullptr;
            std::shared_ptr<vaTexture>              OutExportLuma                       = nullptr;

            AdditionalParams( )                  { }
            AdditionalParams( bool noDelayInGettingLuminance, bool skipTickOnlyApply, const std::shared_ptr<vaTexture> & outMSTonemappedColor = nullptr, const std::shared_ptr<vaTexture> & outMSTonemappedColorComplexityMask = nullptr )
                : NoDelayInGettingLuminance(noDelayInGettingLuminance), SkipTickOnlyApply( skipTickOnlyApply ), OutMSTonemappedColor( outMSTonemappedColor ), OutMSTonemappedColorComplexityMask( outMSTonemappedColorComplexityMask ) { }
        };

    protected:
        TMSettings                                  m_settings;

        static const int                            c_avgLuminanceMIPs = 11;        // 11 means 1024x1024 size for MIP 0, should be more than enough
        static const int                            c_backbufferCount = vaRenderDevice::c_DefaultBackbufferCount+1;          // also adds c_backbufferCount-1 lag!
        std::shared_ptr<vaTexture>                  m_avgLuminance;
        std::shared_ptr<vaTexture>                  m_avgLuminanceMIPViews[c_avgLuminanceMIPs];

        int                                         m_avgLuminancePrevLastWrittenIndex;
        std::shared_ptr<vaTexture>                  m_avgLuminancePrev[c_backbufferCount];
        std::shared_ptr<vaTexture>                  m_avgLuminancePrevCPU[c_backbufferCount];

        std::shared_ptr<vaTexture>                  m_halfResRadiance;
        std::shared_ptr<vaTexture>                  m_resolvedSrcRadiance;          // for MSAA

        float                                       m_lastAverageLuminance;

        std::shared_ptr<vaPostProcessBlur>          m_bloomBlur;

        vaAutoRMI<vaPixelShader>                    m_PSPassThrough;
        vaAutoRMI<vaPixelShader>                    m_PSTonemap;
        vaAutoRMI<vaPixelShader>                    m_PSAverageLuminance;
        vaAutoRMI<vaPixelShader>                    m_PSAverageLuminanceGenerateMIP;
        vaAutoRMI<vaPixelShader>                    m_PSDownsampleToHalf;
        vaAutoRMI<vaPixelShader>                    m_PSAddBloom;

        bool                                        m_shadersDirty;

        PostProcessTonemapConstants                 m_lastShaderConsts;

        vaTypedConstantBufferWrapper< PostProcessTonemapConstants >
                                                    m_constantsBuffer;

        vector< pair< string, string > >            m_staticShaderMacros;

    public:
        virtual ~vaPostProcessTonemap( );

    public:
        virtual vaDrawResultFlags                   TickAndTonemap( float deltaTime, vaCameraBase & camera, vaRenderDeviceContext & apiContext, const std::shared_ptr<vaTexture> & srcRadiance, const AdditionalParams & additionalParams = AdditionalParams() );

    public:
        TMSettings &                                Settings( )                                 { return m_settings; }

    protected:
        void                                        Tick( float deltaTime );

    protected:
        virtual void                                UpdateConstants( vaRenderDeviceContext & apiContext, const std::shared_ptr<vaTexture> & srcRadiance );
        virtual void                                UpdateShaders( int msaaSampleCount, const std::shared_ptr<vaTexture> & outMSTonemappedColor, const std::shared_ptr<vaTexture> & outMSTonemappedColorControl, const std::shared_ptr<vaTexture> & outExportLuma );

    protected:
        virtual void                                UpdateLastAverageLuminance( vaRenderDeviceContext & apiContext, bool noDelayInGettingLuminance );

    protected:
        // vaImguiHierarchyObject
        virtual string                              IHO_GetInstanceName( ) const                { return "Tonemap"; }
        virtual void                                IHO_Draw( );
    };

}
