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

#include "vaCMAA2.h"

#include "IntegratedExternals/vaImguiIntegration.h"

using namespace VertexAsylum;

vaCMAA2::vaCMAA2( const vaRenderingModuleParams & params ) : vaRenderingModule( params ), vaUIPanel( "CMAA2", 0, false )
{ 
    m_debugShowEdges = false;
}

vaCMAA2::~vaCMAA2( )
{
}

void vaCMAA2::UIPanelDraw( )
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGui::PushItemWidth( 120.0f );

    ImGui::Checkbox( "Extra sharp", &m_settings.ExtraSharpness );
    ImGuiEx_Combo( "Quality preset", (int&)m_settings.QualityPreset, { string("LOW"), string("MEDIUM"), string("HIGH"), string("ULTRA") } );
    ImGui::Checkbox( "Show edges", &m_debugShowEdges );

    ImGui::PopItemWidth();
#endif
}

