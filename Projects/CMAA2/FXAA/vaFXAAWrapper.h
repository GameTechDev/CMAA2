///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017, Intel Corporation
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

#include "Rendering/vaRenderingIncludes.h"

#define INCLUDED_FROM_CPP
#include "FXAAWrapper.hlsl"

namespace VertexAsylum
{
    class vaFXAAWrapper : public VertexAsylum::vaRenderingModule, public vaImguiHierarchyObject
    {
    public:
        enum Preset { PRESET_LOW, PRESET_MEDIUM, PRESET_HIGH, PRESET_ULTRA };

        struct Settings
        {
            Preset                          Preset;
            float                           fxaaQualitySubpix           = 0.55f;    // 0.55 seems to give best PSNR vs reference and is a lot less blurry!
            float                           fxaaQualityEdgeThreshold    = 0.166f;   // these are at defaults
            float                           fxaaQualityEdgeThresholdMin = 0.0833f;  // these are at defaults

            Settings( )
            {
                // MEDIUM is equal to FXAA's default "FXAA_QUALITY__PRESET 12" (see Fxaa3_11.h line 32 & line 411)
                // (see vaFXAAWrapperDX11.cpp around line 139 for other presets)
                this->Preset        = PRESET_MEDIUM;
            }
        };

    protected:
        Settings                    m_settings;

        vaTypedConstantBufferWrapper<FXAAShaderConstants>
                                    m_constantsBuffer;

    protected:
        vaFXAAWrapper( const vaRenderingModuleParams & params );
    public:
        ~vaFXAAWrapper( );

    public:
        // Applies FXAA to currently selected render target using provided inputs
        virtual vaDrawResultFlags   Draw( vaRenderDeviceContext & deviceContext, const shared_ptr<vaTexture> & inputColor, const shared_ptr<vaTexture> & optionalInLuma = nullptr )  = 0;

        // if FXAA is no longer used make sure it's not reserving any memory
        virtual void                CleanupTemporaryResources( )                                                            = 0;

    protected:
        virtual void                UpdateConstants( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & inputColor, const shared_ptr<vaTexture> & optionalInLuma = nullptr );

    private:
        virtual string              IHO_GetInstanceName( ) const { return "FXAA"; }
        virtual void                IHO_Draw( );
    };

}
