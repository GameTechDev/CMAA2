// DX11 proxy for using SMAA.hlsl shaders (since there's no DX10 effects/techniques)

#ifndef SMAA_WRAPPER__HLSL
#define SMAA_WRAPPER__HLSL

// only used if using dynamic constants or using MSAA
struct SMAAShaderConstants
{
    /**
     * This is only required for temporal modes (SMAA T2x).
     */
#ifndef INCLUDED_FROM_CPP
    float4 subsampleIndices;
#else
    float subsampleIndices[4];
#endif
    /**
     * This is required for blending the results of previous subsample with the
     * output render target; it's used in SMAA S2x and 4x, for other modes just use
     * 1.0 (no blending).
     */
    float blendFactor;
    /**
     * This can be ignored; its purpose is to support interactive custom parameter
     * tweaking.
     */
    float threshld;
    float maxSearchSteps;
    float maxSearchStepsDiag;
    float cornerRounding;

    float padding0;
    float padding1;
    float padding2;
};

// the rest below is shader only code
#ifndef INCLUDED_FROM_CPP

// this line is VA framework specific (ignore when using outside of VA)
#ifdef VA_COMPILED_AS_SHADER_CODE
#include "MagicMacrosMagicFile.h"
#endif


cbuffer SMAAGlobals : register( b0 )
{
    SMAAShaderConstants g_SMAA;
}


// Use a real macro here for maximum performance!
#ifndef SMAA_RT_METRICS // This is just for compilation-time syntax checking.
#define SMAA_RT_METRICS float4(1.0 / 1280.0, 1.0 / 720.0, 1280.0, 720.0)
#endif

// Set the HLSL version:
#ifndef SMAA_HLSL_4_1
#define SMAA_HLSL_4
#endif

// Set preset defines:
#ifdef SMAA_PRESET_CUSTOM
#define SMAA_THRESHOLD              g_SMAA.threshld
#define SMAA_MAX_SEARCH_STEPS       g_SMAA.maxSearchSteps
#define SMAA_MAX_SEARCH_STEPS_DIAG  g_SMAA.maxSearchStepsDiag
#define SMAA_CORNER_ROUNDING        g_SMAA.cornerRounding
#endif

SamplerState                        LinearSampler                       : register( s0 );
SamplerState                        PointSampler                        : register( s1 );

#include "SMAA.hlsl"

 /**                                                                    
  * Pre-computed area and search textures                               
  */                                                                    
 Texture2D                          areaTex                             : register( t0 );
 Texture2D                          searchTex                           : register( t1 );

/**
  * Input textures
  */
 Texture2D                          colorTex                            : register( t2 );
 Texture2D                          colorTexGamma                       : register( t3 );
 Texture2D                          colorTexPrev                        : register( t4 );
 Texture2DMS<float4, 2>             colorTexMS                          : register( t5 );
 Texture2D                          depthTex                            : register( t6 );
 Texture2D                          velocityTex                         : register( t7 );
                                                                        
 /**                                                                    
  * Temporal textures                                                   
  */                                                                    
 Texture2D                          edgesTex                            : register( t8 );
 Texture2D                          blendTex                            : register( t9 );
                                                                        
 /**
 * Function wrappers
 */
void DX10_SMAAEdgeDetectionVS(float4 position : POSITION,
                              out float4 svPosition : SV_POSITION,
                              inout float2 texcoord : TEXCOORD0,
                              out float4 offset[3] : TEXCOORD1) {
    svPosition = position;
    SMAAEdgeDetectionVS(texcoord, offset);
}

void DX10_SMAABlendingWeightCalculationVS(float4 position : POSITION,
                                          out float4 svPosition : SV_POSITION,
                                          inout float2 texcoord : TEXCOORD0,
                                          out float2 pixcoord : TEXCOORD1,
                                          out float4 offset[3] : TEXCOORD2) {
    svPosition = position;
    SMAABlendingWeightCalculationVS(texcoord, pixcoord, offset);
}

void DX10_SMAANeighborhoodBlendingVS(float4 position : POSITION,
                                     out float4 svPosition : SV_POSITION,
                                     inout float2 texcoord : TEXCOORD0,
                                     out float4 offset : TEXCOORD1) {
    svPosition = position;
    SMAANeighborhoodBlendingVS(texcoord, offset);
}

void DX10_SMAAResolveVS(float4 position : POSITION,
                        out float4 svPosition : SV_POSITION,
                        inout float2 texcoord : TEXCOORD0) {
    svPosition = position;
}

void DX10_SMAASeparateVS(float4 position : POSITION,
                         out float4 svPosition : SV_POSITION,
                         inout float2 texcoord : TEXCOORD0) {
    svPosition = position;
}

float2 DX10_SMAALumaRawEdgeDetectionPS(float4 position : SV_POSITION,
                                    float2 texcoord : TEXCOORD0,
                                    float4 offset[3] : TEXCOORD1) : SV_TARGET {
    #if SMAA_PREDICATION
    return SMAALumaRawEdgeDetectionPS(texcoord, offset, colorTexGamma, depthTex);
    #else
    return SMAALumaRawEdgeDetectionPS(texcoord, offset, colorTexGamma);
    #endif
}

float2 DX10_SMAALumaEdgeDetectionPS(float4 position : SV_POSITION,
                                    float2 texcoord : TEXCOORD0,
                                    float4 offset[3] : TEXCOORD1) : SV_TARGET {
    #if SMAA_PREDICATION
    return SMAALumaEdgeDetectionPS(texcoord, offset, colorTexGamma, depthTex);
    #else
    return SMAALumaEdgeDetectionPS(texcoord, offset, colorTexGamma);
    #endif
}

float2 DX10_SMAAColorEdgeDetectionPS(float4 position : SV_POSITION,
                                     float2 texcoord : TEXCOORD0,
                                     float4 offset[3] : TEXCOORD1) : SV_TARGET {
    #if SMAA_PREDICATION
    return SMAAColorEdgeDetectionPS(texcoord, offset, colorTexGamma, depthTex);
    #else
    return SMAAColorEdgeDetectionPS(texcoord, offset, colorTexGamma);
    #endif
}

float2 DX10_SMAADepthEdgeDetectionPS(float4 position : SV_POSITION,
                                     float2 texcoord : TEXCOORD0,
                                     float4 offset[3] : TEXCOORD1) : SV_TARGET {
    return SMAADepthEdgeDetectionPS(texcoord, offset, depthTex);
}

float4 DX10_SMAABlendingWeightCalculationPS(float4 position : SV_POSITION,
                                            float2 texcoord : TEXCOORD0,
                                            float2 pixcoord : TEXCOORD1,
                                            float4 offset[3] : TEXCOORD2) : SV_TARGET {
    return SMAABlendingWeightCalculationPS(texcoord, pixcoord, offset, edgesTex, areaTex, searchTex, g_SMAA.subsampleIndices);
}

float4 DX10_SMAANeighborhoodBlendingPS(float4 position : SV_POSITION,
                                       float2 texcoord : TEXCOORD0,
                                       float4 offset : TEXCOORD1) : SV_TARGET {
    #if SMAA_REPROJECTION
    return SMAANeighborhoodBlendingPS(texcoord, offset, colorTex, blendTex, velocityTex);
    #else
    return SMAANeighborhoodBlendingPS(texcoord, offset, colorTex, blendTex);
    #endif
}

float4 DX10_SMAAResolvePS(float4 position : SV_POSITION,
                          float2 texcoord : TEXCOORD0) : SV_TARGET {
    #if SMAA_REPROJECTION
    return SMAAResolvePS(texcoord, colorTex, colorTexPrev, velocityTex);
    #else
    return SMAAResolvePS(texcoord, colorTex, colorTexPrev);
    #endif
}

void DX10_SMAASeparatePS(float4 position : SV_POSITION,
                         float2 texcoord : TEXCOORD0,
                         out float4 target0 : SV_TARGET0,
                         out float4 target1 : SV_TARGET1) {
    SMAASeparatePS(position, texcoord, target0, target1, colorTexMS);
}

#endif // #ifndef INCLUDED_FROM_CPP

#endif // #ifndef SMAA_WRAPPER__HLSL