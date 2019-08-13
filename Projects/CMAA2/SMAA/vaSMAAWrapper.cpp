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

#include "vaSMAAWrapper.h"

#include "IntegratedExternals/vaImguiIntegration.h"

using namespace VertexAsylum;

vaSMAAWrapper::vaSMAAWrapper( const vaRenderingModuleParams & params ) : vaRenderingModule( params ), vaUIPanel("SMAA", 0, false), m_constantsBuffer( params )
{ 
//    assert( vaRenderingCore::IsInitialized() );

    //m_debugShowEdges = false;
    memset( &m_constants, 0, sizeof(m_constants) );
}

vaSMAAWrapper::~vaSMAAWrapper( )
{
}

void vaSMAAWrapper::UIPanelDraw( )
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGui::PushItemWidth( 120.0f );


    ImGuiEx_Combo( "Quality preset", (int&)m_settings.Preset, { string("LOW"), string("MEDIUM"), string("HIGH"), string("ULTRA") } );

    //ImGui::Checkbox( "Show edges", &m_debugShowEdges );

    ImGui::PopItemWidth();
#endif
}

// void vaSMAAWrapper::UpdateConstants( vaRenderDeviceContext & renderContext )
// {
//     apiContext;
// //    CMAA2ShaderConstants consts;
// //    consts.EdgeThreshold                    = m_settings.EdgeDetectionThreshold;
// //    consts.LocalContrastAdaptationAmount    = vaMath::Clamp( m_settings.LocalContrastAdaptationAmount, 0.0f, 0.5f );
// //    consts.SimpleShapeBlurrinessAmount      = m_settings.BlurinessAmount * 0.11f;
// //    consts.Unused                           = 0;
// //
// //    m_constantsBuffer.Update( apiContext, consts );
// }

