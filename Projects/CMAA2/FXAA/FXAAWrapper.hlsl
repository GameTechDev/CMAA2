#ifndef FXAA_WRAPPER__HLSL
#define FXAA_WRAPPER__HLSL

// this line is VA framework specific (ignore when using outside of VA)
#ifdef VA_COMPILED_AS_SHADER_CODE
#include "MagicMacrosMagicFile.h"
#endif

#define FXAA_PC 1
#define FXAA_HLSL_5 1

#define FXAA_LINEAR 1

#if (FXAA_LUMA_SEPARATE_R8 == 0)
    #define FXAA_GREEN_AS_LUMA 1
#endif

// #define FXAA_QUALITY__PRESET 39

#ifdef INCLUDED_FROM_CPP
struct float2 { float x, y; };
struct float4 { float x, y, z, w; };
#endif

struct FXAAShaderConstants
{
    // {x_} = 1.0/screenWidthInPixels
    // {_y} = 1.0/screenHeightInPixels
    float2 rcpFrame;

    float2 Packing0;

    // This must be from a constant/uniform.
    // {x___} = 2.0/screenWidthInPixels
    // {_y__} = 2.0/screenHeightInPixels
    // {__z_} = 0.5/screenWidthInPixels
    // {___w} = 0.5/screenHeightInPixels
    float4 rcpFrameOpt;

    // {x___} = -N/screenWidthInPixels  
    // {_y__} = -N/screenHeightInPixels
    // {__z_} =  N/screenWidthInPixels  
    // {___w} =  N/screenHeightInPixels 
    float4 rcpFrameOpt2;

    // This can effect sharpness.
    //   1.00 - upper limit (softer)
    //   0.75 - default amount of filtering
    //   0.50 - lower limit (sharper, less sub-pixel aliasing removal)
    //   0.25 - almost off
    //   0.00 - completely off
    float fxaaQualitySubpix;

    // The minimum amount of local contrast required to apply algorithm.
    //   0.333 - too little (faster)
    //   0.250 - low quality
    //   0.166 - default
    //   0.125 - high quality 
    //   0.063 - overkill (slower)
    float fxaaQualityEdgeThreshold;

    // This used to be the FXAA_QUALITY__EDGE_THRESHOLD_MIN define.
    float fxaaQualityEdgeThresholdMin;

    // This used to be the FXAA_CONSOLE__EDGE_SHARPNESS define.
    float fxaaConsoleEdgeSharpness;

    // This used to be the FXAA_CONSOLE__EDGE_THRESHOLD define.
    float fxaaConsoleEdgeThreshold;

    // This used to be the FXAA_CONSOLE__EDGE_THRESHOLD_MIN define.
    float fxaaConsoleEdgeThresholdMin;

	float2 Packing1;
    //    
    // Extra constants for 360 FXAA Console only.
    // float4 fxaaConsole360ConstDir;
};

#ifndef INCLUDED_FROM_CPP

#include "Fxaa3_11.h"

#include "vaStandardSamplers.hlsl"

cbuffer FXAAGlobals : register(b0)
{
   FXAAShaderConstants g_FXAA;
}


Texture2D           g_screenTexture	            : register( t0 );
Texture2D           g_screenTextureLumaOnly	    : register( t1 );
//SamplerState	    g_samplerBilinearClamp      : register( s0 );

// // copy and place L in alpha, as described in Fxaa3_8.h
// float4 FXAACopyAndLumaPS( float4 screenPos : SV_Position ) : SV_Target
// {
//     float4 color = g_screenTexture.Load( int3( screenPos.xy, 0 ) ).rgba;
//     color.a = dot( sqrt(color.rgb), float3(0.299, 0.587, 0.114) ); // compute luma
//     return color;
// }

float4 FXAAEffectPS( float4 screenPos : SV_Position ) : SV_Target
{
    //float4 texel = g_screenTexture.Load( int3( screenPos.xy, 0 ) ).rgba;
    //return texel.aaaa;

    //float4 texel = g_screenTexture.Sample( g_samplerPointClamp, screenPos.xy * g_FXAA.rcpFrame.xy ).rgba;
    //return float4( texel.r, texel.g, 0, 1 );

    FxaaTex tex;
    tex.smpl = g_samplerLinearClamp;
    tex.tex  = g_screenTexture;

    FxaaTex texLuma;
    texLuma.smpl = g_samplerLinearClamp;
#if (FXAA_LUMA_SEPARATE_R8 == 0)
    texLuma.tex  = g_screenTexture;
#else
    texLuma.tex  = g_screenTextureLumaOnly;
#endif

#ifdef FXAA_GATHER_ISSUE_WORKAROUND
    screenPos.xy = (screenPos.xy + 0.001);
#endif

    return FxaaPixelShader( screenPos.xy * g_FXAA.rcpFrame.xy, float4( 0, 0, 0, 0 ), tex, texLuma, tex, tex, 
        g_FXAA.rcpFrame, 
        g_FXAA.rcpFrameOpt, 
        g_FXAA.rcpFrameOpt2, 
        g_FXAA.rcpFrameOpt2,
	    g_FXAA.fxaaQualitySubpix,
	    g_FXAA.fxaaQualityEdgeThreshold,
        g_FXAA.fxaaQualityEdgeThresholdMin,
        g_FXAA.fxaaConsoleEdgeSharpness,
        g_FXAA.fxaaConsoleEdgeThreshold,
        g_FXAA.fxaaConsoleEdgeThresholdMin,
        float4( 0, 0, 0, 0 ) );
}

#endif // #ifndef INCLUDED_FROM_CPP

#endif // #ifndef FXAA_WRAPPER__HLSL