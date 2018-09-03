///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016, Intel Corporation
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

#include "vaSharedTypes_PostProcessTonemap.h"


Texture2D                       g_sourceTexure0                 : register( T_CONCATENATER( POSTPROCESS_TONEMAP_TEXTURE_SLOT0 ) );
Texture2D                       g_sourceTexure1                 : register( T_CONCATENATER( POSTPROCESS_TONEMAP_TEXTURE_SLOT1 ) );

#ifdef POSTPROCESS_TONEMAP_MSAA_SAMPLE_COUNT
Texture2DMS<float4>             g_sourceTexureMS0               : register( T_CONCATENATER( POSTPROCESS_TONEMAP_TEXTURE_SLOT0 ) );
Texture2DMS<float4>             g_sourceTexureMS1               : register( T_CONCATENATER( POSTPROCESS_TONEMAP_TEXTURE_SLOT1 ) );

#ifdef POSTPROCESS_TONEMAP_OUTPUT_MS_TONEMAPPED_COLOR
#ifdef POSTPROCESS_TONEMAP_OUTPUT_MS_TONEMAPPED_COLOR_FMT_UNORM
RWTexture2DArray<unorm float3>  g_tonemappedMSColor             : register( u1 );
#else
RWTexture2DArray<float3>        g_tonemappedMSColor             : register( u1 );
#endif
#ifdef POSTPROCESS_TONEMAP_OUTPUT_MS_TONEMAPPED_COLOR_COMPLEXITY_MASK
RWTexture2D<unorm float>        g_tonemappedMSColorComplexityMask: register( u2 );
#endif
#endif

#ifdef POSTPROCESS_TONEMAP_OUTPUT_LUMA
#error exporting luma not supported in MSAA scenario - not needed for now
#endif

#endif

#ifdef POSTPROCESS_TONEMAP_OUTPUT_LUMA
RWTexture2D<unorm float>        g_exportLuma                    : register( u3 );
#endif


float CalcLuminance( float3 color )
{
    // https://www.w3.org/TR/AERT#color-contrast
    return max( 0.000001, dot( color, float3( 0.299, 0.587, 0.114 ) ) );
}

float PSAverageLuminance( const in float4 Position : SV_Position, const in float2 Texcoord : TEXCOORD0 ) : SV_Target
{
    float3 val = g_sourceTexure0.SampleLevel( g_samplerLinearClamp, Texcoord, 0.0 ).xyz;

    return CalcLuminance( val );
}

float PSAverageLuminanceGenerateMIP( const in float4 Position : SV_Position, const in float2 Texcoord : TEXCOORD0 ) : SV_Target
{
    int2 baseCoords = int2(Position.xy) * 2;

    float a = g_sourceTexure0[ baseCoords + int2( 0, 0 ) ].x;
    float b = g_sourceTexure0[ baseCoords + int2( 1, 0 ) ].x;
    float c = g_sourceTexure0[ baseCoords + int2( 0, 1 ) ].x;
    float d = g_sourceTexure0[ baseCoords + int2( 1, 1 ) ].x;

    return (a + b + c + d) * 0.25;
}

float3 Tonemap( float3 sourceRadiance )
{
    sourceRadiance *= g_PostProcessTonemapConsts.Exp2Exposure;
    float luminance = CalcLuminance( sourceRadiance );

    // Reinhard's modified tonemapping
	float tonemappedLuminance = luminance * (1.0 + luminance / (g_PostProcessTonemapConsts.WhiteLevelSquared)) / (1.0 + luminance);
	float3 tonemapValue = tonemappedLuminance * pow( max( 0.0, sourceRadiance / luminance ), g_PostProcessTonemapConsts.Saturation);
    return clamp( tonemapValue, 0.0, 1.0 ); // upper range will be higher than 1.0 if this ever gets extended for HDR output / HDR displays!
}

float4 LoadAndTonemap( const float4 Position, const float2 Texcoord )
{
    int i;
    uint2 pixelCoord    = (uint2)Position.xy;

#ifdef POSTPROCESS_TONEMAP_MSAA_SAMPLE_COUNT
   
    float3 values[POSTPROCESS_TONEMAP_MSAA_SAMPLE_COUNT];
    values[0] = g_sourceTexureMS0.Load( pixelCoord, 0 ).rgb;
    bool allValuesSame = true;  // if we had a way to get this from MS control surface we wouldn't have to compare by value
    i = 1;
#if POSTPROCESS_TONEMAP_MSAA_SAMPLE_COUNT > 2
    for( ; i < POSTPROCESS_TONEMAP_MSAA_SAMPLE_COUNT; i++ )
#endif
    {
        values[i] = g_sourceTexureMS0.Load( pixelCoord, i ).rgb;
        
        #if 0 // determine if all samples are the same
            allValuesSame = allValuesSame && all(values[0] == values[i]);
        #else // alternatively, while we're here, we can determine if all samples are within threshold (0.0003 is, when converted from Linear->sRGB, smaller than 1/256 so pick that as a 'safe' threshold)
            float3 absDiff = abs(values[0]-values[i]);
            allValuesSame = allValuesSame && ( (absDiff.x+absDiff.y+absDiff.z) < 0.0003 );
        #endif
    }
    float3 colorSum = Tonemap( values[0] );
#ifdef POSTPROCESS_TONEMAP_OUTPUT_MS_TONEMAPPED_COLOR
#ifdef POSTPROCESS_TONEMAP_OUTPUT_MS_TONEMAPPED_COLOR_FMT_REQUIRES_SRGB_CONVERSION
    float3 colorSumForUAV = LINEAR_to_SRGB( colorSum );
#else
    float3 colorSumForUAV = colorSum;
#endif
    g_tonemappedMSColor[ uint3(pixelCoord, 0) ] = colorSumForUAV;

    // control surface - 1 means there's more than one value
#ifdef POSTPROCESS_TONEMAP_OUTPUT_MS_TONEMAPPED_COLOR_COMPLEXITY_MASK
    g_tonemappedMSColorComplexityMask[ pixelCoord ] = (allValuesSame)?(0.0):(1.0);
#endif
#endif
    if( allValuesSame )
    {
#ifdef POSTPROCESS_TONEMAP_OUTPUT_MS_TONEMAPPED_COLOR
        i = 1;
    #if POSTPROCESS_TONEMAP_MSAA_SAMPLE_COUNT > 2
        for( ; i < POSTPROCESS_TONEMAP_MSAA_SAMPLE_COUNT; i++ )
    #endif
            g_tonemappedMSColor[ uint3(pixelCoord, i) ] = colorSumForUAV;
#endif
        return float4( colorSum, 1 );
    }
    else
    {
        i = 1;
    #if POSTPROCESS_TONEMAP_MSAA_SAMPLE_COUNT > 2
        for( ; i < POSTPROCESS_TONEMAP_MSAA_SAMPLE_COUNT; i++ )
    #endif
        {
            values[i] = Tonemap( values[i] );
#ifdef POSTPROCESS_TONEMAP_OUTPUT_MS_TONEMAPPED_COLOR
    #ifdef POSTPROCESS_TONEMAP_OUTPUT_MS_TONEMAPPED_COLOR_FMT_REQUIRES_SRGB_CONVERSION
            g_tonemappedMSColor[ uint3(pixelCoord, i) ] = LINEAR_to_SRGB( values[i] );
    #else
            g_tonemappedMSColor[ uint3(pixelCoord, i) ] = values[i];
    #endif
#endif
            colorSum += values[i];
        }
        return float4( colorSum / (float)POSTPROCESS_TONEMAP_MSAA_SAMPLE_COUNT, 1.0 );
    }

    // Code above is same as below but will skip tonemap/resolve if values are the same - if we had access to MS control surface, this could be even faster
    // float3 colorSum = 0;
    // for( int i = 0; i < POSTPROCESS_TONEMAP_MSAA_SAMPLE_COUNT; i++ )
    //     colorSum += Tonemap( g_sourceTexureMS0.Load( pixelCoord, i ).rgb );
    // return float4( colorSum / (float)POSTPROCESS_TONEMAP_MSAA_SAMPLE_COUNT, 1.0 );

#else

    float3 color = Tonemap( g_sourceTexure0.Load( int3( pixelCoord, 0 ) ).rgb );

#ifdef POSTPROCESS_TONEMAP_OUTPUT_LUMA
    g_exportLuma[ pixelCoord ] = RGBToLumaForEdges( color.rgb );
#endif

    return float4( color, 1.0 );
#endif
}

//#ifdef POSTPROCESS_TONEMAP_RT_IS_MSAA
//float4 PSTonemap( const in float4 Position : SV_Position, const in float2 Texcoord : TEXCOORD0, uint sampleIndex : SV_SampleIndex ) : SV_Target
//{
//    return float4( Tonemap( g_sourceTexureMS0.Load( (int2)Position.xy, sampleIndex ).rgb ), 1.0 );
//}
//#else
float4 PSTonemap( const in float4 Position : SV_Position, const in float2 Texcoord : TEXCOORD0 ) : SV_Target
{
    return LoadAndTonemap( Position, Texcoord );
}
//#endif

float4 PSDownsampleToHalf( const in float4 Position : SV_Position, const in float2 Texcoord : TEXCOORD0 ) : SV_Target
{
// #ifdef POSTPROCESS_TONEMAP_MSAA_SAMPLE_COUNT
//     float2 srcUV = Position.xy * 2.0 * g_PostProcessTonemapConsts.FullResPixelSize;
// 
//     return g_sourceTexure0.SampleLevel( g_samplerLinearClamp, srcUV, 0.0 );
// #else
    float2 srcUV = Position.xy * 2.0 * g_PostProcessTonemapConsts.FullResPixelSize;

    return g_sourceTexure0.SampleLevel( g_samplerLinearClamp, srcUV, 0.0 );
//#endif
}

float4 PSAddBloom( const in float4 Position : SV_Position, const in float2 Texcoord : TEXCOORD0 ) : SV_Target
{
    //int2 pixelCoord         = (int2)Position.xy;
    //float3 bloom            = g_sourceTexure0.Load( int3( pixelCoord, 0 ) ).rgb;

    float2 srcUV = Position.xy * g_PostProcessTonemapConsts.BloomSampleUVMul;
    float3 bloom = g_sourceTexure0.SampleLevel( g_samplerLinearClamp, srcUV, 0.0 ).xyz;

    return float4( max( 0.0, bloom - g_PostProcessTonemapConsts.BloomThreshold ) * g_PostProcessTonemapConsts.BloomMultiplier, 0.0 );
}

float4 PSPassThrough( const in float4 Position : SV_Position, const in float2 Texcoord : TEXCOORD0 ) : SV_Target
{
    int2 pixelCoord = (int2)Position.xy;
    return g_sourceTexure0.Load( int3( pixelCoord, 0 ) ).rgba;
}
