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

#include "vaZoomTool.h"
#include "Core/vaInput.h"

#include "IntegratedExternals/vaImguiIntegration.h"

using namespace VertexAsylum;

vaZoomTool::vaZoomTool( const vaRenderingModuleParams & params ) : 
    vaRenderingModule( params ), 
    m_constantsBuffer( params ),
    m_CSZoomToolFloat( params ),
    m_CSZoomToolUnorm( params ),
    vaUIPanel( "ZoomTool", -1, true, vaUIPanel::DockLocation::DockedLeftBottom )
{ 
//    assert( vaRenderingCore::IsInitialized() );

    m_CSZoomToolFloat->CreateShaderFromFile( L"vaHelperTools.hlsl", "cs_5_0", "ZoomToolCS", { ( pair< string, string >( "VA_ZOOM_TOOL_SPECIFIC", "" ) ) }, false );
    m_CSZoomToolUnorm->CreateShaderFromFile( L"vaHelperTools.hlsl", "cs_5_0", "ZoomToolCS", { ( pair< string, string >( "VA_ZOOM_TOOL_SPECIFIC", "" ) ), pair< string, string >( "VA_ZOOM_TOOL_USE_UNORM_FLOAT", "" ) }, false );
}

vaZoomTool::~vaZoomTool( )
{
}

void vaZoomTool::HandleMouseInputs( vaInputMouseBase & mouseInput )
{
    if( m_settings.Enabled )
    {
        if( mouseInput.IsKeyClicked( MK_Left ) )
        {
            m_settings.BoxPos = mouseInput.GetCursorClientPos() - vaVector2i( m_settings.BoxSize.x / 2, m_settings.BoxSize.y / 2 );
        }
    }
}

void vaZoomTool::Draw( vaRenderDeviceContext & renderContext, const shared_ptr<vaTexture> & colorInOut )
{
    if( !m_settings.Enabled )
        return;

    // backup & remove outputs just in case colorInOut is in them - we don't care about overhead for this debugging tool
    vaRenderDeviceContext::RenderOutputsState rtState = renderContext.GetOutputs( );
    renderContext.SetRenderTarget( nullptr, nullptr, false );

    UpdateConstants( renderContext );

    vaComputeItem computeItem;

    computeItem.ConstantBuffers[ ZOOMTOOL_CONSTANTSBUFFERSLOT ] = m_constantsBuffer;
    computeItem.UnorderedAccessViews[ 0 ] = colorInOut;
    
    int threadGroupCountX = ( colorInOut->GetSizeX( ) + 16 - 1 ) / 16;
    int threadGroupCountY = ( colorInOut->GetSizeY( ) + 16 - 1 ) / 16;

    computeItem.ComputeShader = ( vaResourceFormatHelpers::IsFloat( colorInOut->GetUAVFormat() ) ) ? ( m_CSZoomToolFloat.get() ) : ( m_CSZoomToolUnorm.get() );
    computeItem.SetDispatch( threadGroupCountX, threadGroupCountY, 1 );

    renderContext.ExecuteSingleItem( computeItem );

    // restore previous RTs
    renderContext.SetOutputs( rtState );
}

void vaZoomTool::UIPanelDraw( )
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGui::PushItemWidth( 120.0f );

    ImGui::Checkbox( "Enabled", &m_settings.Enabled );
    ImGui::InputInt( "ZoomFactor", &m_settings.ZoomFactor, 1 );
    m_settings.ZoomFactor = vaMath::Clamp( m_settings.ZoomFactor, 2, 32 );

    ImGui::InputInt2( "BoxPos", &m_settings.BoxPos.x );
    ImGui::InputInt2( "BoxSize", &m_settings.BoxSize.x );

    ImGui::PopItemWidth();
#endif
}

void vaZoomTool::UpdateConstants( vaRenderDeviceContext & renderContext )
{
    ZoomToolShaderConstants consts;

    consts.SourceRectangle  = vaVector4( (float)m_settings.BoxPos.x, (float)m_settings.BoxPos.y, (float)m_settings.BoxPos.x+m_settings.BoxSize.x, (float)m_settings.BoxPos.y+m_settings.BoxSize.y );
    consts.ZoomFactor       = m_settings.ZoomFactor;

    m_constantsBuffer.Update( renderContext, consts );
}


