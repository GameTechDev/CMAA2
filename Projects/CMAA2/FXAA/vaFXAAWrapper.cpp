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

#include "vaFXAAWrapper.h"

using namespace VertexAsylum;

vaFXAAWrapper::vaFXAAWrapper( const vaRenderingModuleParams & params ) : vaRenderingModule( params ), m_constantsBuffer( params.RenderDevice )
{ 
//    assert( vaRenderingCore::IsInitialized() );

    //m_debugShowEdges = false;
//    memset( &m_constants, 0, sizeof(m_constants) );
}

vaFXAAWrapper::~vaFXAAWrapper( )
{
}

void vaFXAAWrapper::IHO_Draw( )
{
    ImGui::PushItemWidth( 120.0f );


    ImGuiEx_Combo( "Quality preset", (int&)m_settings.Preset, { string("P10 (LOW)"), string("P12 (MEDIUM)"), string("P25 (HIGH)"), string("P39 (ULTRA)") } );

    ImGui::InputFloat( "'fxaaQualitySubpix'", &m_settings.fxaaQualitySubpix, 0.05f );
    ImGui::InputFloat( "'fxaaQualityEdgeThreshold'", &m_settings.fxaaQualityEdgeThreshold, 0.05f );
    ImGui::InputFloat( "'fxaaQualityEdgeThresholdMin'", &m_settings.fxaaQualityEdgeThresholdMin, 0.05f );
    m_settings.fxaaQualitySubpix            = vaMath::Clamp( m_settings.fxaaQualitySubpix          , 0.0f, 1.0f );
    m_settings.fxaaQualityEdgeThreshold     = vaMath::Clamp( m_settings.fxaaQualityEdgeThreshold   , 0.0f, 1.0f );
    m_settings.fxaaQualityEdgeThresholdMin  = vaMath::Clamp( m_settings.fxaaQualityEdgeThresholdMin, 0.0f, 1.0f );


    //ImGui::Checkbox( "Show edges", &m_debugShowEdges );

    ImGui::PopItemWidth();
}

void vaFXAAWrapper::UpdateConstants( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & inputColor, const shared_ptr<vaTexture> & optionalInLuma )
{
    optionalInLuma;

    int texWidth    = inputColor->GetSizeX();
    int texHeight   = inputColor->GetSizeY();

    FXAAShaderConstants consts;

    // {x_} = 1.0/screenWidthInPixels
    // {_y} = 1.0/screenHeightInPixels
    consts.rcpFrame.x = 1.0f / (float)texWidth;
    consts.rcpFrame.y = 1.0f / (float)texHeight;

    consts.Packing0   = {0.0f, 0.0f};
    consts.Packing1   = {0.0f, 0.0f};

    // {x___} = 2.0/screenWidthInPixels
    // {_y__} = 2.0/screenHeightInPixels
    // {__z_} = 0.5/screenWidthInPixels
    // {___w} = 0.5/screenHeightInPixels
    consts.rcpFrameOpt.x = 2.0f / (float)texWidth;
    consts.rcpFrameOpt.y = 2.0f / (float)texHeight;
    consts.rcpFrameOpt.z = 0.5f / (float)texWidth;
    consts.rcpFrameOpt.w = 0.5f / (float)texHeight;

    // {x___} = -N/screenWidthInPixels  
    // {_y__} = -N/screenHeightInPixels
    // {__z_} =  N/screenWidthInPixels  
    // {___w} =  N/screenHeightInPixels 
    consts.rcpFrameOpt2.x = 0.5f / (float)texWidth;
    consts.rcpFrameOpt2.y = 0.5f / (float)texHeight;
    consts.rcpFrameOpt2.z = 0.5f / (float)texWidth;
    consts.rcpFrameOpt2.w = 0.5f / (float)texHeight;

    // This can effect sharpness.
    //   1.00 - upper limit (softer)
    //   0.75 - default amount of filtering
    //   0.50 - lower limit (sharper, less sub-pixel aliasing removal)
    //   0.25 - almost off
    //   0.00 - completely off
    consts.fxaaQualitySubpix = m_settings.fxaaQualitySubpix;
    // Only used on FXAA Quality.
    // This used to be the FXAA_QUALITY__EDGE_THRESHOLD define.
    // It is here now to allow easier tuning.
    // The minimum amount of local contrast required to apply algorithm.
    //   0.333 - too little (faster)
    //   0.250 - low quality
    //   0.166 - default
    //   0.125 - high quality 
    //   0.063 - overkill (slower)
    consts.fxaaQualityEdgeThreshold = m_settings.fxaaQualityEdgeThreshold;

    // Only used on FXAA Quality.
    // This used to be the FXAA_QUALITY__EDGE_THRESHOLD_MIN define.
    // It is here now to allow easier tuning.
    // Trims the algorithm from processing darks.
    //   0.0833 - upper limit (default, the start of visible unfiltered edges)
    //   0.0625 - high quality (faster)
    //   0.0312 - visible limit (slower)
    // Special notes when using FXAA_GREEN_AS_LUMA,
    //   Likely want to set this to zero.
    //   As colors that are mostly not-green
    //   will appear very dark in the green channel!
    //   Tune by looking at mostly non-green content,
    //   then start at zero and increase until aliasing is a problem.
    consts.fxaaQualityEdgeThresholdMin = m_settings.fxaaQualityEdgeThresholdMin;
  

    // Only used on FXAA Console.
    // This used to be the FXAA_CONSOLE__EDGE_SHARPNESS define.
    // It is here now to allow easier tuning.
    // This does not effect PS3, as this needs to be compiled in.
    //   Use FXAA_CONSOLE__PS3_EDGE_SHARPNESS for PS3.
    //   Due to the PS3 being ALU bound,
    //   there are only three safe values here: 2 and 4 and 8.
    //   These options use the shaders ability to a free *|/ by 2|4|8.
    // For all other platforms can be a non-power of two.
    //   8.0 is sharper (default!!!)
    //   4.0 is softer
    //   2.0 is really soft (good only for vector graphics inputs)
    consts.fxaaConsoleEdgeSharpness = 8.0;

    // Only used on FXAA Console.
    // This used to be the FXAA_CONSOLE__EDGE_THRESHOLD define.
    // It is here now to allow easier tuning.
    // This does not effect PS3, as this needs to be compiled in.
    //   Use FXAA_CONSOLE__PS3_EDGE_THRESHOLD for PS3.
    //   Due to the PS3 being ALU bound,
    //   there are only two safe values here: 1/4 and 1/8.
    //   These options use the shaders ability to a free *|/ by 2|4|8.
    // The console setting has a different mapping than the quality setting.
    // Other platforms can use other values.
    //   0.125 leaves less aliasing, but is softer (default!!!)
    //   0.25 leaves more aliasing, and is sharper
    consts.fxaaConsoleEdgeThreshold = (1.0/8.0);

    // Only used on FXAA Console.
    // This used to be the FXAA_CONSOLE__EDGE_THRESHOLD_MIN define.
    // It is here now to allow easier tuning.
    // Trims the algorithm from processing darks.
    // The console setting has a different mapping than the quality setting.
    // This only applies when FXAA_EARLY_EXIT is 1.
    // This does not apply to PS3, 
    // PS3 was simplified to avoid more shader instructions.
    //   0.06 - faster but more aliasing in darks
    //   0.05 - default
    //   0.04 - slower and less aliasing in darks
    // Special notes when using FXAA_GREEN_AS_LUMA,
    //   Likely want to set this to zero.
    //   As colors that are mostly not-green
    //   will appear very dark in the green channel!
    //   Tune by looking at mostly non-green content,
    //   then start at zero and increase until aliasing is a problem.
    consts.fxaaConsoleEdgeThresholdMin = 0.05f;

    m_constantsBuffer.Update( apiContext, consts );
}

