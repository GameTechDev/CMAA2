///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018, Intel Corporation
//
// Licensed under the Apache License, Version 2.0 ( the "License" );
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
// http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Author(s):  Filip Strugar (filip.strugar@intel.com)
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Core/vaCoreIncludes.h"

#include "Rendering/vaRenderingIncludes.h"

#define CMAA2_INCLUDED_FROM_CPP
#include "CMAA2.hlsl"

namespace VertexAsylum
{
    class vaCMAA2 : public VertexAsylum::vaRenderingModule, public vaImguiHierarchyObject
    {
    public:

        enum Preset { PRESET_LOW, PRESET_MEDIUM, PRESET_HIGH, PRESET_ULTRA };

        struct Settings
        {
            bool                            ExtraSharpness;
            Preset                          QualityPreset;                  // 0 - LOW, 1 - MEDIUM, 2 - HIGH (default), 3 - HIGHEST

            Settings( )
            {
                ExtraSharpness                  = false;
                QualityPreset                   = PRESET_HIGH;
            }
        };

    protected:
        Settings                    m_settings;

        bool                        m_debugShowEdges;

    protected:
        vaCMAA2( const vaRenderingModuleParams & params );
    public:
        ~vaCMAA2( );

    public:
        virtual vaDrawResultFlags   Draw( vaRenderDeviceContext & deviceContext, const shared_ptr<vaTexture> & inoutColor, const shared_ptr<vaTexture> & optionalInLuma = nullptr )                  = 0;

        virtual vaDrawResultFlags   DrawMS( vaRenderDeviceContext & deviceContext, const shared_ptr<vaTexture> & inoutColor, const shared_ptr<vaTexture> & inColorMS, const shared_ptr<vaTexture> & inColorMSComplexityMask ) = 0;

        // if CMAA2 is no longer used make sure it's not reserving any memory
        virtual void                CleanupTemporaryResources( )                                                    = 0;

        Settings &                  Settings( )                                                                     { return m_settings; }

    protected:
        virtual void                UpdateConstants( vaRenderDeviceContext & apiContext );

    private:
        virtual string              IHO_GetInstanceName( ) const { return "CMAA 2"; }
        virtual void                IHO_Draw( );
    };

}
