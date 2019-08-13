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
#include "Core/vaUI.h"

#include "Rendering/vaRenderingIncludes.h"

#define INCLUDED_FROM_CPP
#include "SMAAWrapper.hlsl"

namespace VertexAsylum
{
    class vaSMAAWrapper : public VertexAsylum::vaRenderingModule, public vaUIPanel
    {
    public:
        // enum Mode { MODE_SMAA_1X, MODE_SMAA_T2X, MODE_SMAA_S2X, MODE_SMAA_4X, MODE_SMAA_COUNT = MODE_SMAA_4X };
        enum Preset { PRESET_LOW, PRESET_MEDIUM, PRESET_HIGH, PRESET_ULTRA, PRESET_CUSTOM, PRESET_COUNT = PRESET_CUSTOM };

        struct Settings
        {
            Preset                          Preset;

            Settings( )
            {
                this->Preset        = PRESET_HIGH;
            }
        };

    protected:
        Settings                    m_settings;

        SMAAShaderConstants         m_constants;
        vaTypedConstantBufferWrapper<SMAAShaderConstants>
                                    m_constantsBuffer;

        //bool                        m_debugShowEdges;

    protected:
        vaSMAAWrapper( const vaRenderingModuleParams & params );
    public:
        ~vaSMAAWrapper( );

    public:
        // Applies SMAA to currently selected render target using provided inputs
        virtual vaDrawResultFlags   Draw( vaRenderDeviceContext & deviceContext, const shared_ptr<vaTexture> & inputColor, const shared_ptr<vaTexture> & optionalInLuma = nullptr )  = 0;

        // if SMAA is no longer used make sure it's not reserving any memory
        virtual void                CleanupTemporaryResources( )                                                            = 0;

    protected:
        // virtual void                UpdateConstants( vaRenderDeviceContext & renderContext );

    private:
        virtual void                UIPanelDraw( ) override;
        virtual bool                UIPanelIsListed( ) const override          { return false; }
    };

}
