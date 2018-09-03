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

#include "vaShared.hlsl"

RWTexture2D<uint>   g_AVSMDeferredCounterUAV        : register( U_CONCATENATER( POSTPROCESS_COMPARISONRESULTS_UAV_SLOT ) );

Texture2D           g_sourceTexure0                 : register( T_CONCATENATER( POSTPROCESS_TEXTURE_SLOT0 ) );
Texture2D           g_sourceTexure1                 : register( T_CONCATENATER( POSTPROCESS_TEXTURE_SLOT1 ) );

// TODO: maybe do this in a compute shader, use SLM 
void PSCompareTextures( const in float4 Position : SV_Position, const in float2 Texcoord : TEXCOORD0 )
{
    int2 pixelCoord         = (int2)Position.xy;

#if POSTPROCESS_COMPARE_IN_SRGB_SPACE
    float3 col0 = FLOAT3_to_SRGB( g_sourceTexure0.Load( int3( pixelCoord, 0 ) ).rgb );
    float3 col1 = FLOAT3_to_SRGB( g_sourceTexure1.Load( int3( pixelCoord, 0 ) ).rgb );
#else
    float3 col0 = g_sourceTexure0.Load( int3( pixelCoord, 0 ) ).rgb;
    float3 col1 = g_sourceTexure1.Load( int3( pixelCoord, 0 ) ).rgb;
#endif

    // Mean Squared Error
    float3 diff3 = col0.rgb - col1.rgb;
    diff3 = diff3 * diff3;

    uint3 diff3Int = round(diff3*POSTPROCESS_COMPARISONRESULTS_FIXPOINT_MAX);

    uint sizeX, sizeY;
    g_sourceTexure0.GetDimensions( sizeX, sizeY );
    uint linearAddr = (uint)pixelCoord.x + (uint)pixelCoord.y * sizeX;
    int UAVAddr = linearAddr % (uint)(POSTPROCESS_COMPARISONRESULTS_SIZE);

    InterlockedAdd( g_AVSMDeferredCounterUAV[ int2(UAVAddr*2+0, 0) ], diff3Int.x );
    InterlockedAdd( g_AVSMDeferredCounterUAV[ int2(UAVAddr*2+1, 0) ], diff3Int.y );
    InterlockedAdd( g_AVSMDeferredCounterUAV[ int2(UAVAddr*2+2, 0) ], diff3Int.z );
}

float4 PSDrawTexturedQuad( const in float4 Position : SV_Position, const in float2 Texcoord : TEXCOORD0 ) : SV_Target
{
    int2 pixelCoord         = (int2)Position.xy;
    float4 col = g_sourceTexure0.Load( int3( pixelCoord, 0 ) );
    return col;
}

void VSStretchRect( out float4 xPos : SV_Position, in float2 UV : TEXCOORD0, out float2 outUV : TEXCOORD0 ) 
{ 
    xPos.x = (1.0 - UV.x) * g_PostProcessConsts.Param1.x + UV.x * g_PostProcessConsts.Param1.z;
    xPos.y = (1.0 - UV.y) * g_PostProcessConsts.Param1.y + UV.y * g_PostProcessConsts.Param1.w;
    xPos.z = 0;
    xPos.w = 1;

    outUV.x = (1.0 - UV.x) * g_PostProcessConsts.Param2.x + UV.x * g_PostProcessConsts.Param2.z;
    outUV.y = (1.0 - UV.y) * g_PostProcessConsts.Param2.y + UV.y * g_PostProcessConsts.Param2.w;
}

float4 PSStretchRectLinear( const in float4 Position : SV_Position, const in float2 Texcoord : TEXCOORD0 ) : SV_Target
{
    float4 col = g_sourceTexure0.Sample( g_samplerLinearClamp, Texcoord );
    return float4( (col * g_PostProcessConsts.Param3 + g_PostProcessConsts.Param4).rgb, 1.0 );
}

float4 PSStretchRectPoint( const in float4 Position : SV_Position, const in float2 Texcoord : TEXCOORD0 ) : SV_Target
{
    float4 col = g_sourceTexure0.Sample( g_samplerPointClamp, Texcoord );
    return float4( (col * g_PostProcessConsts.Param3 + g_PostProcessConsts.Param4).rgb, 1.0 );
}

#ifdef VA_DRAWSINGLESAMPLEFROMMSTEXTURE_SAMPLE
Texture2DMS<float4>             g_screenTexture             : register( t0 );
float4 SingleSampleFromMSTexturePS( in float4 inPos : SV_Position ) : SV_Target0
{
    return g_screenTexture.Load( inPos.xy, VA_DRAWSINGLESAMPLEFROMMSTEXTURE_SAMPLE );
}
#endif

//#ifdef VA_POSTPROCESS_COLORPROCESS // just a laceholder for debugging stuff ATM
//#if VA_POSTPROCESS_COLORPROCESS_MS_COUNT == 1
//Texture2D<float>                g_srcTexture              : register( t0 );
//#else
//Texture2DMS<float>              g_srcTexture              : register( t0 );
//#endif
//float4 ColorProcessPS( in float4 inPos : SV_Position ) : SV_Target0
//{
//    float4 val, valRef;
//#if VA_POSTPROCESS_COLORPROCESS_MS_COUNT == 1
//    valRef   = g_srcTexture.Load( int3( inPos.xy + int2( 1, 0 ), 0 ) );
//    val      = g_srcTexture.Load( int3( inPos.xy, 0 ), int2( 1, 0 ) );
//#else
////    val = 1.0.xxxx;
////    for( uint i = 0; i < VA_POSTPROCESS_COLORPROCESS_MS_COUNT; i++ )
////        val = min( val, g_srcTexture.Load( int2( inPos.xy ), i, int2( 5, 0 ) ) );
//
//    valRef   = g_srcTexture.Load( int2( inPos.xy ) + int2( 1, 0 ), 0 );
//    val      = g_srcTexture.Load( int2( inPos.xy ), 0, int2( 1, 0 ) );
//
//#endif
//
//    bool thisIsWrong = val.x != valRef.x;
//
//    return float4( thisIsWrong, frac(val.x*10), frac(val.x*100), 1.0 );
//}
//#endif

#ifdef VA_POSTPROCESS_COLOR_HSBC
Texture2D<float4>                g_srcTexture              : register( t0 );
float4 ColorProcessHSBCPS( in float4 inPos : SV_Position ) : SV_Target0
{
    float3 color = g_srcTexture.Load( int3( inPos.xy, 0 ) ).rgb;
    //return float4( color.bgr, 1 );

    const float modHue      = g_PostProcessConsts.Param1.x;
    const float modSat      = g_PostProcessConsts.Param1.y;
    const float modBrig     = g_PostProcessConsts.Param1.z;
    const float modContr    = g_PostProcessConsts.Param1.w;

    float3 colorHSV = RGBtoHSV(color);

    colorHSV.x = GLSL_mod( colorHSV.x + modHue, 1.0 );
    colorHSV.y = saturate( colorHSV.y * modSat );
    colorHSV.z = colorHSV.z * modBrig;

    color = HSVtoRGB(colorHSV);

    color = lerp( color, float3( 0.5, 0.5, 0.5 ), -modContr );
    
    color = saturate(color);
    
    return float4( color.rgb, 1 );
}
#endif

#ifdef VA_POSTPROCESS_SIMPLE_BLUR_SHARPEN
Texture2D<float4>                g_srcTexture              : register( t0 );
float4 SimpleBlurSharpen( in float4 inPos : SV_Position ) : SV_Target0
{
    float3 colorSum = float3( 0, 0, 0 );
    float weights[3][3] = {    { g_PostProcessConsts.Param1.x, g_PostProcessConsts.Param1.y,   g_PostProcessConsts.Param1.x },
                                { g_PostProcessConsts.Param1.y, 1.0,                            g_PostProcessConsts.Param1.y },
                                { g_PostProcessConsts.Param1.x, g_PostProcessConsts.Param1.y,   g_PostProcessConsts.Param1.x } };
    float weightSum = g_PostProcessConsts.Param1.x * 4 + g_PostProcessConsts.Param1.y * 4 + 1;

    [unroll]
    for( int y = 0; y < 3; y++ )
        [unroll]
        for( int x = 0; x < 3; x++ )
            colorSum += g_srcTexture.Load( int3( inPos.xy, 0 ), int2( x-1, y-1 ) ).rgb * weights[x][y];

    colorSum /= weightSum;
    
    return float4( colorSum.rgb, 1 );
}
#endif

#ifdef VA_POSTPROCESS_LUMA_FOR_EDGES
Texture2D<float4>                g_srcTexture              : register( t0 );
float4 ColorProcessLumaForEdges( in float4 inPos : SV_Position ) : SV_Target0
{
    float3 color = g_srcTexture.Load( int3( inPos.xy, 0 ) ).rgb;

    return RGBToLumaForEdges( color ).xxxx;
}
#endif

#ifdef VA_POSTPROCESS_DOWNSAMPLE
Texture2D<float4>                g_srcTexture              : register( t0 );
float4 Downsample4x4to1x1( in float4 inPos : SV_Position, const in float2 Texcoord : TEXCOORD0 ) : SV_Target0
{
    float4 colorAccum = float4( 0, 0, 0, 0 );
    
    // // TODO: use bilinear filter to do it in 1/4 samples...
    // [unroll] for( int x = 0; x < 4; x++ )
    //     [unroll] for( int y = 0; y < 4; y++ )
    //         colorAccum += g_srcTexture.Load( int3( inPos.xy, 0 )*4, int2( x, y ) ).rgba;
    // 
    // colorAccum /= 16.0;

    float2 pixelSize    = g_PostProcessConsts.Param1.xy;
    float2 centerUV     = Texcoord; // inPos.xy * 4 * pixelSize
    float sharpenScale  = g_PostProcessConsts.Param1.z;

    colorAccum += g_srcTexture.Sample( g_samplerLinearClamp, centerUV + sharpenScale * float2( +pixelSize.x, +pixelSize.y ) );
    colorAccum += g_srcTexture.Sample( g_samplerLinearClamp, centerUV + sharpenScale * float2( -pixelSize.x, +pixelSize.y ) );
    colorAccum += g_srcTexture.Sample( g_samplerLinearClamp, centerUV + sharpenScale * float2( +pixelSize.x, -pixelSize.y ) );
    colorAccum += g_srcTexture.Sample( g_samplerLinearClamp, centerUV + sharpenScale * float2( -pixelSize.x, -pixelSize.y ) );

    colorAccum /= 4.0f;

    return colorAccum;
}
#endif