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

#ifndef __INTELLISENSE__
#include "vaASSAOLite_types.h"
#endif

#ifdef SSAO_INTERNAL_SHADER_DEBUG
#include "vaSharedTypes.h"
RWTexture1D<float>              g_ShaderDebugOutputUAV              :   register( U_CONCATENATER( SSAO_DEBUGGING_DUMP_UAV_SLOT ) ); 
#endif

// progressive poisson-like pattern; x, y are in [-1, 1] range, .z is length( float2(x,y) ), .w is log2( z )
#define INTELSSAO_MAIN_DISK_SAMPLE_COUNT (32)

static const float4 g_samplePatternMain[INTELSSAO_MAIN_DISK_SAMPLE_COUNT] =
{
     0.78488064,  0.56661671,  1.500000, -0.126083,        0.26022232, -0.29575172,  1.500000, -1.064030,        0.10459357,  0.08372527,  1.110000, -2.730563,       -0.68286800,  0.04963045,  1.090000, -0.498827,
    -0.13570161, -0.64190155,  1.250000, -0.532765,       -0.26193795, -0.08205118,  0.670000, -1.783245,       -0.61177456,  0.66664219,  0.710000, -0.044234,        0.43675563,  0.25119025,  0.610000, -1.167283,
     0.07884444,  0.86618668,  0.640000, -0.459002,       -0.12790935, -0.29869005,  0.600000, -1.729424,       -0.04031125,  0.02413622,  0.600000, -4.792042,        0.16201244, -0.52851415,  0.790000, -1.067055,
    -0.70991218,  0.47301072,  0.640000, -0.335236,        0.03277707, -0.22349690,  0.600000, -1.982384,        0.68921727,  0.36800742,  0.630000, -0.266718,        0.29251814,  0.37775412,  0.610000, -1.422520,
    -0.12224089,  0.96582592,  0.600000, -0.426142,        0.11071457, -0.16131058,  0.600000, -2.165947,        0.46562141, -0.59747696,  0.600000, -0.189760,       -0.51548797,  0.11804193,  0.600000, -1.246800,
     0.89141309, -0.42090443,  0.600000,  0.028192,       -0.32402530, -0.01591529,  0.600000, -1.543018,        0.60771245,  0.41635221,  0.600000, -0.605411,        0.02379565, -0.08239821,  0.600000, -3.809046,
     0.48951152, -0.23657045,  0.600000, -1.189011,       -0.17611565, -0.81696892,  0.600000, -0.513724,       -0.33930185, -0.20732205,  0.600000, -1.698047,       -0.91974425,  0.05403209,  0.600000,  0.062246,
    -0.15064627, -0.14949332,  0.600000, -1.896062,        0.53180975, -0.35210401,  0.600000, -0.758838,        0.41487166,  0.81442589,  0.600000, -0.505648,       -0.24106961, -0.32721516,  0.600000, -1.665244
};

#if (INTELSSAO_MAIN_DISK_SAMPLE_COUNT) != (SSAO_MAX_TAPS)
    #error ASSAO : main disk number of samples mismatch!
#endif

// these values can be changed (up to SSAO_MAX_TAPS) with no changes required elsewhere; they represent number of taps used by low/medium/high/highest presets
static const uint g_numTaps[4]   = { 3, 5, 11, 22 };


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Optional parts that can be enabled for a required quality preset level and above (0 == Low, 1 == Medium, 2 == High, 3 == Highest/Adaptive, 4 == reference/unused )
// Each has its own cost. To disable just set to 5 or above.
//
// (experimental) tilts the disk (although only half of the samples!) towards surface normal; this helps with effect uniformity between objects but reduces effect distance and has other side-effects
#define SSAO_TILT_SAMPLES_ENABLE_AT_QUALITY_PRESET                      (99)        // to disable simply set to 99 or similar
#define SSAO_TILT_SAMPLES_AMOUNT                                        (0.4)
//
#define SSAO_HALOING_REDUCTION_ENABLE_AT_QUALITY_PRESET                 (1)         // to disable simply set to 99 or similar
#define SSAO_HALOING_REDUCTION_AMOUNT                                   (0.6)      // values from 0.0 - 1.0, 1.0 means max weighting (will cause artifacts, 0.8 is more reasonable)
//
#define SSAO_NORMAL_BASED_EDGES_ENABLE_AT_QUALITY_PRESET                (2)         // to disable simply set to 99 or similar
#define SSAO_NORMAL_BASED_EDGES_DOT_THRESHOLD                           (0.5)       // use 0-0.1 for super-sharp normal-based edges
//
#define SSAO_DETAIL_AO_ENABLE_AT_QUALITY_PRESET                         (1)         // whether to use DetailAOStrength; to disable simply set to 99 or similar
//
#define SSAO_DEPTH_MIPS_ENABLE_AT_QUALITY_PRESET                        (2)         // !!warning!! the MIP generation on the C++ side will be enabled on quality preset 2 regardless of this value, so if changing here, change the C++ side too
#define SSAO_DEPTH_MIPS_GLOBAL_OFFSET                                   (-4.3)      // best noise/quality/performance tradeoff, found empirically
//
// !!warning!! the edge handling is hard-coded to 'disabled' on quality level 0, and enabled above, on the C++ side; while toggling it here will work for 
// testing purposes, it will not yield performance gains (or correct results)
#define SSAO_DEPTH_BASED_EDGES_ENABLE_AT_QUALITY_PRESET                 (1)     
//
#define SSAO_REDUCE_RADIUS_NEAR_SCREEN_BORDER_ENABLE_AT_QUALITY_PRESET  (99)    // 99 simply means disabled; set to 1 to enable at "Medium" or 2 to enable at "High" preset
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

cbuffer ASSAOLiteConstantsBuffer                     : register( B_CONCATENATER( SSAO_CONSTANTSBUFFERSLOT ) )
{
    ASSAOLiteConstants                    g_ASSAOConsts;
}

#ifndef SSAO_MSAA_SAMPLE_COUNT
Texture2D<float>    g_DepthSource               : register( T_CONCATENATER( SSAO_TEXTURE_SLOT0 ) );
#else
Texture2DMS<float>  g_DepthSourceMS             : register( T_CONCATENATER( SSAO_TEXTURE_SLOT0 ) );
#endif
Texture2D           g_NormalmapSource           : register( T_CONCATENATER( SSAO_TEXTURE_SLOT1 ) );

Texture2D<float>    g_ViewspaceDepthSource      : register( T_CONCATENATER( SSAO_TEXTURE_SLOT0 ) );
Texture2D<float>    g_ViewspaceDepthSource1     : register( T_CONCATENATER( SSAO_TEXTURE_SLOT1 ) );
Texture2D<float>    g_ViewspaceDepthSource2     : register( T_CONCATENATER( SSAO_TEXTURE_SLOT2 ) );
Texture2D<float>    g_ViewspaceDepthSource3     : register( T_CONCATENATER( SSAO_TEXTURE_SLOT3 ) );

Texture2D           g_BlurInput                 : register( T_CONCATENATER( SSAO_TEXTURE_SLOT2 ) );

Texture2DArray      g_FinalSSAO                 : register( T_CONCATENATER( SSAO_TEXTURE_SLOT4 ) );

RWTexture2D<unorm float4> g_NormalsOutputUAV    : register( U_CONCATENATER( SSAO_NORMALMAP_OUT_UAV_SLOT ) ); 

Texture2D           g_DebuggingOutputSRV        : register( T_CONCATENATER( SSAO_TEXTURE_SLOT5 ) );
RWTexture2D<unorm float4> g_DebuggingOutputUAV        : register( U_CONCATENATER( SSAO_DEBUGGINGOUTPUT_OUT_UAV_SLOT ) );


// packing/unpacking for edges; 2 bits per edge mean 4 gradient values (0, 0.33, 0.66, 1) for smoother transitions!
float PackEdges( float4 edgesLRTB )
{
//    int4 edgesLRTBi = int4( saturate( edgesLRTB ) * 3.0 + 0.5 );
//    return ( (edgesLRTBi.x << 6) + (edgesLRTBi.y << 4) + (edgesLRTBi.z << 2) + (edgesLRTBi.w << 0) ) / 255.0;

    // optimized, should be same as above
    edgesLRTB = round( saturate( edgesLRTB ) * 3.05 );
    return dot( edgesLRTB, float4( 64.0 / 255.0, 16.0 / 255.0, 4.0 / 255.0, 1.0 / 255.0 ) ) ;
}

float4 UnpackEdges( float _packedVal )
{
    uint packedVal = (uint)(_packedVal * 255.5);
    float4 edgesLRTB;
    edgesLRTB.x = float((packedVal >> 6) & 0x03) / 3.0;          // there's really no need for mask (as it's an 8 bit input) but I'll leave it in so it doesn't cause any trouble in the future
    edgesLRTB.y = float((packedVal >> 4) & 0x03) / 3.0;
    edgesLRTB.z = float((packedVal >> 2) & 0x03) / 3.0;
    edgesLRTB.w = float((packedVal >> 0) & 0x03) / 3.0;

    return saturate( edgesLRTB + g_ASSAOConsts.InvSharpness );
}

float ScreenSpaceToViewSpaceDepth( float screenDepth )
{
    float depthLinearizeMul = g_ASSAOConsts.DepthUnpackConsts.x;
    float depthLinearizeAdd = g_ASSAOConsts.DepthUnpackConsts.y;

    // Optimised version of "-cameraClipNear / (cameraClipFar - projDepth * (cameraClipFar - cameraClipNear)) * cameraClipFar"

    // Set your depthLinearizeMul and depthLinearizeAdd to:
    // depthLinearizeMul = ( cameraClipFar * cameraClipNear) / ( cameraClipFar - cameraClipNear );
    // depthLinearizeAdd = cameraClipFar / ( cameraClipFar - cameraClipNear );

    return depthLinearizeMul / ( depthLinearizeAdd - screenDepth );
}

// from [0, width], [0, height] to [-1, 1], [-1, 1]
float2 ScreenSpaceToClipSpacePositionXY( float2 screenPos )
{
    return screenPos * g_ASSAOConsts.Viewport2xPixelSize.xy - float2( 1.0f, 1.0f );
}

float3 ScreenSpaceToViewSpacePosition( float2 screenPos, float viewspaceDepth )
{
    return float3( g_ASSAOConsts.CameraTanHalfFOV.xy * viewspaceDepth * ScreenSpaceToClipSpacePositionXY( screenPos ), viewspaceDepth );
}

float3 ClipSpaceToViewSpacePosition( float2 clipPos, float viewspaceDepth )
{
    return float3( g_ASSAOConsts.CameraTanHalfFOV.xy * viewspaceDepth * clipPos, viewspaceDepth );
}

float3 NDCToViewspace( float2 pos, float viewspaceDepth )
{
    float3 ret;

    ret.xy = (g_ASSAOConsts.NDCToViewMul * pos.xy + g_ASSAOConsts.NDCToViewAdd) * viewspaceDepth;

    ret.z = viewspaceDepth;

    return ret;
}

// calculate effect radius and fit our screen sampling pattern inside it
void CalculateRadiusParameters( const float pixCenterLength, const float2 pixelDirRBViewspaceSizeAtCenterZ, out float pixLookupRadiusMod, out float effectRadius, out float falloffCalcMulSq )
{
    effectRadius = g_ASSAOConsts.EffectRadius;

//    // leaving this out for performance reasons: use something similar if radius needs to scale based on distance
    effectRadius *= pow( pixCenterLength, g_ASSAOConsts.RadiusDistanceScalingFunctionPow);

    // when too close, on-screen sampling disk will grow beyond screen size; limit this to avoid closeup temporal artifacts
    const float tooCloseLimitMod = saturate( pixCenterLength * g_ASSAOConsts.EffectSamplingRadiusNearLimitRec ) * 0.8 + 0.2;
    
    effectRadius *= tooCloseLimitMod;

    // 0.85 is to reduce the radius to allow for more samples on a slope to still stay within influence
    pixLookupRadiusMod = (0.85 * effectRadius) / pixelDirRBViewspaceSizeAtCenterZ.x;

    // used to calculate falloff (both for AO samples and per-sample weights)
    falloffCalcMulSq= -1.0f / (effectRadius*effectRadius);
}

float4 CalculateEdges( const float centerZ, const float leftZ, const float rightZ, const float topZ, const float bottomZ )
{
    // slope-sensitive depth-based edge detection
    float4 edgesLRTB = float4( leftZ, rightZ, topZ, bottomZ ) - centerZ;
    float4 edgesLRTBSlopeAdjusted = edgesLRTB + edgesLRTB.yxwz;
    edgesLRTB = min( abs( edgesLRTB ), abs( edgesLRTBSlopeAdjusted ) );
    return saturate( ( 1.3 - edgesLRTB / (centerZ * 0.040) ) );

    // cheaper version but has artifacts
    // edgesLRTB = abs( float4( leftZ, rightZ, topZ, bottomZ ) - centerZ; );
    // return saturate( ( 1.3 - edgesLRTB / (pixZ * 0.06 + 0.1) ) );
}

// pass-through vertex shader
void VSMain( inout float4 Pos : SV_POSITION, inout float2 Uv : TEXCOORD0 ) { }

float DepthLoadAndConvertToViewspace( int2 baseCoord, int2 offset )
{
#ifndef SSAO_MSAA_SAMPLE_COUNT
    return ScreenSpaceToViewSpaceDepth( g_DepthSource.Load( int3(baseCoord, 0), offset ).x );
#else

#if 1 // MSAA+offset bug on Intel workaround
    baseCoord += offset;
    offset = 0;
#endif

    float dummyUnused1, dummyUnused2, falloffCalcMulSq, falloffCalcAdd;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TODO: fix this; it's not very optimal - if using just min or max then do it BEFORE ScreenToViewspaceDepth - it requires one big
// ifdef to deal with it though AND it requires either min or max depending on whether invertedZ is used so it's a bit of a mess; even
// then we still need a reference with some kind of averaging for samples 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // should be static precompiled branch if we decide to go with this approach in the future
#if SSAO_MSAA_SAMPLE_COUNT == 2
    {
        float2 depthsV;
        depthsV.x = ScreenSpaceToViewSpaceDepth( g_DepthSourceMS.Load( baseCoord, 0, offset ).x );
        depthsV.y = ScreenSpaceToViewSpaceDepth( g_DepthSourceMS.Load( baseCoord, 1, offset ).x );
//        if( depthsV.x == depthsV.y )
//            return depthsV.x;

        float farthest = max( depthsV.x, depthsV.y );
        return farthest;

        // CalculateRadiusParameters( abs( farthest ), 1.0, dummyUnused1, dummyUnused2, falloffCalcMulSq );
        // 
        // float2 dists = depthsV - farthest.xx;
        // float2 weights = saturate( dists * dists * falloffCalcMulSq + 1.0 );
        // float smartAvg = dot( weights, depthsV ) / dot( weights, float2( 1.0, 1.0 ) );
        // 
        // return smartAvg;
    }
#elif SSAO_MSAA_SAMPLE_COUNT == 4
    {
        float4 depthsV;
        depthsV.x = ScreenSpaceToViewSpaceDepth( g_DepthSourceMS.Load( baseCoord, 0, offset ).x );
        depthsV.y = ScreenSpaceToViewSpaceDepth( g_DepthSourceMS.Load( baseCoord, 1, offset ).x );
        depthsV.z = ScreenSpaceToViewSpaceDepth( g_DepthSourceMS.Load( baseCoord, 2, offset ).x );
        depthsV.w = ScreenSpaceToViewSpaceDepth( g_DepthSourceMS.Load( baseCoord, 3, offset ).x );
        // if( depthsV.x == depthsV.y == depthsV.z == depthsV.w )
        //     return depthsV.x;

        float farthest = max( max( depthsV.x, depthsV.y ), max( depthsV.z, depthsV.w ) );
        return farthest;

        // CalculateRadiusParameters( abs( farthest ), 1.0, dummyUnused1, dummyUnused2, falloffCalcMulSq );
        // 
        // float4 dists = depthsV - farthest.xxxx;
        // float4 weights = saturate( dists * dists * falloffCalcMulSq + 1.0 );
        // float smartAvg = dot( weights, depthsV ) / dot( weights, float4( 1.0, 1.0, 1.0, 1.0 ) );
        // 
        // return smartAvg;
    }
#elif SSAO_MSAA_SAMPLE_COUNT == 8
    {
        float4 depthsV0, depthsV1;
        depthsV0.x = ScreenSpaceToViewSpaceDepth( g_DepthSourceMS.Load( baseCoord, 0, offset ).x );
        depthsV0.y = ScreenSpaceToViewSpaceDepth( g_DepthSourceMS.Load( baseCoord, 1, offset ).x );
        depthsV0.z = ScreenSpaceToViewSpaceDepth( g_DepthSourceMS.Load( baseCoord, 2, offset ).x );
        depthsV0.w = ScreenSpaceToViewSpaceDepth( g_DepthSourceMS.Load( baseCoord, 3, offset ).x );
        depthsV1.x = ScreenSpaceToViewSpaceDepth( g_DepthSourceMS.Load( baseCoord, 4, offset ).x );
        depthsV1.y = ScreenSpaceToViewSpaceDepth( g_DepthSourceMS.Load( baseCoord, 5, offset ).x );
        depthsV1.z = ScreenSpaceToViewSpaceDepth( g_DepthSourceMS.Load( baseCoord, 6, offset ).x );
        depthsV1.w = ScreenSpaceToViewSpaceDepth( g_DepthSourceMS.Load( baseCoord, 7, offset ).x );

    #if 0   // this can get expensive so if you don't want to touch all samples just skip this and use 4 interleaved
        float farthest = max( max( depthsV0.x, depthsV0.y ), max( depthsV0.z, depthsV0.w ) );
        farthest = max( farthest, max( max( depthsV1.x, depthsV1.y ), max( depthsV1.z, depthsV1.w ) ) );
        return farthest;
    #else
        return max( max( depthsV0.x, depthsV1.y ), max( depthsV0.z, depthsV1.w ) );
    #endif
    }
#elif SSAO_MSAA_SAMPLE_COUNT == 16  // skip every 2nd
    {
        float4 depthsV0, depthsV1;
        depthsV0.x = ScreenSpaceToViewSpaceDepth( g_DepthSourceMS.Load( baseCoord, 0, offset ).x );
        depthsV0.y = ScreenSpaceToViewSpaceDepth( g_DepthSourceMS.Load( baseCoord, 2, offset ).x );
        depthsV0.z = ScreenSpaceToViewSpaceDepth( g_DepthSourceMS.Load( baseCoord, 4, offset ).x );
        depthsV0.w = ScreenSpaceToViewSpaceDepth( g_DepthSourceMS.Load( baseCoord, 6, offset ).x );
        depthsV1.x = ScreenSpaceToViewSpaceDepth( g_DepthSourceMS.Load( baseCoord, 8, offset ).x );
        depthsV1.y = ScreenSpaceToViewSpaceDepth( g_DepthSourceMS.Load( baseCoord, 10, offset ).x );
        depthsV1.z = ScreenSpaceToViewSpaceDepth( g_DepthSourceMS.Load( baseCoord, 12, offset ).x );
        depthsV1.w = ScreenSpaceToViewSpaceDepth( g_DepthSourceMS.Load( baseCoord, 14, offset ).x );

        float farthest = max( max( depthsV0.x, depthsV0.y ), max( depthsV0.z, depthsV0.w ) );
        farthest = max( farthest, max( max( depthsV1.x, depthsV1.y ), max( depthsV1.z, depthsV1.w ) ) );
        return farthest;
    }
#else
    #error path not supported yet!
#endif
#endif
}

void PSPrepareDepths( in float4 inPos : SV_POSITION, out float out0 : SV_Target0, out float out1 : SV_Target1, out float out2 : SV_Target2, out float out3 : SV_Target3 )
{
#if 0   // gather can be a bit faster but doesn't work with input depth buffers that don't match the working viewport
    float2 gatherUV = inPos.xy * g_ASSAOConsts.Viewport2xPixelSize;
    float4 depths = g_DepthSource.GatherRed( g_samplerPointClamp, gatherUV );
    out0 = ScreenSpaceToViewSpaceDepth( depths.w );
    out1 = ScreenSpaceToViewSpaceDepth( depths.z );
    out2 = ScreenSpaceToViewSpaceDepth( depths.x );
    out3 = ScreenSpaceToViewSpaceDepth( depths.y );
#else
    int2 baseCoord = int2(inPos.xy) * 2;
    out0 = DepthLoadAndConvertToViewspace( baseCoord, int2( 0, 0 ) );
    out1 = DepthLoadAndConvertToViewspace( baseCoord, int2( 1, 0 ) );
    out2 = DepthLoadAndConvertToViewspace( baseCoord, int2( 0, 1 ) );
    out3 = DepthLoadAndConvertToViewspace( baseCoord, int2( 1, 1 ) );
#endif

}

float3 CalculateNormal( const float4 edgesLRTB, float3 pixCenterPos, float3 pixLPos, float3 pixRPos, float3 pixTPos, float3 pixBPos )
{
    // Get this pixel's viewspace normal
    float4 acceptedNormals  = float4( edgesLRTB.x*edgesLRTB.z, edgesLRTB.z*edgesLRTB.y, edgesLRTB.y*edgesLRTB.w, edgesLRTB.w*edgesLRTB.x );

    pixLPos = normalize(pixLPos - pixCenterPos);
    pixRPos = normalize(pixRPos - pixCenterPos);
    pixTPos = normalize(pixTPos - pixCenterPos);
    pixBPos = normalize(pixBPos - pixCenterPos);

    float3 pixelNormal = float3( 0, 0, -0.0005 );
    pixelNormal += ( acceptedNormals.x ) * cross( pixLPos, pixTPos );
    pixelNormal += ( acceptedNormals.y ) * cross( pixTPos, pixRPos );
    pixelNormal += ( acceptedNormals.z ) * cross( pixRPos, pixBPos );
    pixelNormal += ( acceptedNormals.w ) * cross( pixBPos, pixLPos );
    pixelNormal = normalize( pixelNormal );
    
    return pixelNormal;
}

void PSPrepareDepthsAndNormals( in float4 inPos : SV_POSITION, out float out0 : SV_Target0, out float out1 : SV_Target1, out float out2 : SV_Target2, out float out3 : SV_Target3 )
{
    int2 baseCoord = int2(inPos.xy) * 2;
    // gather can be a bit faster but doesn't work with input depth buffers that don't match the working viewport
    out0 = DepthLoadAndConvertToViewspace( baseCoord, int2( 0, 0 ) );
    out1 = DepthLoadAndConvertToViewspace( baseCoord, int2( 1, 0 ) );
    out2 = DepthLoadAndConvertToViewspace( baseCoord, int2( 0, 1 ) );
    out3 = DepthLoadAndConvertToViewspace( baseCoord, int2( 1, 1 ) );

    float pixZs[4][4];

    // middle 4
    pixZs[1][1] = out0;
    pixZs[2][1] = out1;
    pixZs[1][2] = out2;
    pixZs[2][2] = out3;
    // left 2
    pixZs[0][1] = DepthLoadAndConvertToViewspace( baseCoord, int2( -1, 0 ) ); 
    pixZs[0][2] = DepthLoadAndConvertToViewspace( baseCoord, int2( -1, 1 ) ); 
    // right 2
    pixZs[3][1] = DepthLoadAndConvertToViewspace( baseCoord, int2(  2, 0 ) ); 
    pixZs[3][2] = DepthLoadAndConvertToViewspace( baseCoord, int2(  2, 1 ) ); 
    // top 2
    pixZs[1][0] = DepthLoadAndConvertToViewspace( baseCoord, int2(  0, -1 ) );
    pixZs[2][0] = DepthLoadAndConvertToViewspace( baseCoord, int2(  1, -1 ) );
    // bottom 2
    pixZs[1][3] = DepthLoadAndConvertToViewspace( baseCoord, int2(  0,  2 ) );
    pixZs[2][3] = DepthLoadAndConvertToViewspace( baseCoord, int2(  1,  2 ) );

    float4 edges0 = CalculateEdges( pixZs[1][1], pixZs[0][1], pixZs[2][1], pixZs[1][0], pixZs[1][2] );
    float4 edges1 = CalculateEdges( pixZs[2][1], pixZs[1][1], pixZs[3][1], pixZs[2][0], pixZs[2][2] );
    float4 edges2 = CalculateEdges( pixZs[1][2], pixZs[0][2], pixZs[2][2], pixZs[1][1], pixZs[1][3] );
    float4 edges3 = CalculateEdges( pixZs[2][2], pixZs[1][2], pixZs[3][2], pixZs[2][1], pixZs[2][3] );

    float3 pixPos[4][4];

    // there is probably a way to optimize the math below; however no approximation will work, has to be precise.
    float2 upperLeftUV = (inPos.xy - 0.25) * g_ASSAOConsts.Viewport2xPixelSize;

    // middle 4
    pixPos[1][1] = NDCToViewspace( upperLeftUV + g_ASSAOConsts.ViewportPixelSize * float2( 0.0,  0.0 ), pixZs[1][1] );
    pixPos[2][1] = NDCToViewspace( upperLeftUV + g_ASSAOConsts.ViewportPixelSize * float2( 1.0,  0.0 ), pixZs[2][1] );
    pixPos[1][2] = NDCToViewspace( upperLeftUV + g_ASSAOConsts.ViewportPixelSize * float2( 0.0,  1.0 ), pixZs[1][2] );
    pixPos[2][2] = NDCToViewspace( upperLeftUV + g_ASSAOConsts.ViewportPixelSize * float2( 1.0,  1.0 ), pixZs[2][2] );
    // left 2
    pixPos[0][1] = NDCToViewspace( upperLeftUV + g_ASSAOConsts.ViewportPixelSize * float2( -1.0,  0.0), pixZs[0][1] );
    pixPos[0][2] = NDCToViewspace( upperLeftUV + g_ASSAOConsts.ViewportPixelSize * float2( -1.0,  1.0), pixZs[0][2] );
    // right 2                                                                                     
    pixPos[3][1] = NDCToViewspace( upperLeftUV + g_ASSAOConsts.ViewportPixelSize * float2(  2.0,  0.0), pixZs[3][1] );
    pixPos[3][2] = NDCToViewspace( upperLeftUV + g_ASSAOConsts.ViewportPixelSize * float2(  2.0,  1.0), pixZs[3][2] );
    // top 2                                                                                       
    pixPos[1][0] = NDCToViewspace( upperLeftUV + g_ASSAOConsts.ViewportPixelSize * float2( 0.0, -1.0 ), pixZs[1][0] );
    pixPos[2][0] = NDCToViewspace( upperLeftUV + g_ASSAOConsts.ViewportPixelSize * float2( 1.0, -1.0 ), pixZs[2][0] );
    // bottom 2                                                                                    
    pixPos[1][3] = NDCToViewspace( upperLeftUV + g_ASSAOConsts.ViewportPixelSize * float2( 0.0,  2.0 ), pixZs[1][3] );
    pixPos[2][3] = NDCToViewspace( upperLeftUV + g_ASSAOConsts.ViewportPixelSize * float2( 1.0,  2.0 ), pixZs[2][3] );

    float3 norm0 = CalculateNormal( edges0, pixPos[1][1], pixPos[0][1], pixPos[2][1], pixPos[1][0], pixPos[1][2] );
    float3 norm1 = CalculateNormal( edges1, pixPos[2][1], pixPos[1][1], pixPos[3][1], pixPos[2][0], pixPos[2][2] );
    float3 norm2 = CalculateNormal( edges2, pixPos[1][2], pixPos[0][2], pixPos[2][2], pixPos[1][1], pixPos[1][3] );
    float3 norm3 = CalculateNormal( edges3, pixPos[2][2], pixPos[1][2], pixPos[3][2], pixPos[2][1], pixPos[2][3] );

    g_NormalsOutputUAV[ baseCoord.xy + int2( 0, 0 ) ] = float4( norm0 * 0.5 + 0.5, 0.0 );
    g_NormalsOutputUAV[ baseCoord.xy + int2( 1, 0 ) ] = float4( norm1 * 0.5 + 0.5, 0.0 );
    g_NormalsOutputUAV[ baseCoord.xy + int2( 0, 1 ) ] = float4( norm2 * 0.5 + 0.5, 0.0 );
    g_NormalsOutputUAV[ baseCoord.xy + int2( 1, 1 ) ] = float4( norm3 * 0.5 + 0.5, 0.0 );
}

void PrepareDepthMip( const float4 inPos/*, const float2 inUV*/, int mipLevel, out float outD0, out float outD1, out float outD2, out float outD3 )
{
    int2 baseCoords = int2(inPos.xy) * 2;

    float4 depthsArr[4];
    float depthsOutArr[4];
    
    // how to Gather a specific mip level?
    depthsArr[0].x = g_ViewspaceDepthSource[baseCoords + int2( 0, 0 )].x ;// * g_ASSAOConsts.MaxViewspaceDepth;
    depthsArr[0].y = g_ViewspaceDepthSource[baseCoords + int2( 1, 0 )].x ;// * g_ASSAOConsts.MaxViewspaceDepth;
    depthsArr[0].z = g_ViewspaceDepthSource[baseCoords + int2( 0, 1 )].x ;// * g_ASSAOConsts.MaxViewspaceDepth;
    depthsArr[0].w = g_ViewspaceDepthSource[baseCoords + int2( 1, 1 )].x ;// * g_ASSAOConsts.MaxViewspaceDepth;
    depthsArr[1].x = g_ViewspaceDepthSource1[baseCoords + int2( 0, 0 )].x;// * g_ASSAOConsts.MaxViewspaceDepth;
    depthsArr[1].y = g_ViewspaceDepthSource1[baseCoords + int2( 1, 0 )].x;// * g_ASSAOConsts.MaxViewspaceDepth;
    depthsArr[1].z = g_ViewspaceDepthSource1[baseCoords + int2( 0, 1 )].x;// * g_ASSAOConsts.MaxViewspaceDepth;
    depthsArr[1].w = g_ViewspaceDepthSource1[baseCoords + int2( 1, 1 )].x;// * g_ASSAOConsts.MaxViewspaceDepth;
    depthsArr[2].x = g_ViewspaceDepthSource2[baseCoords + int2( 0, 0 )].x;// * g_ASSAOConsts.MaxViewspaceDepth;
    depthsArr[2].y = g_ViewspaceDepthSource2[baseCoords + int2( 1, 0 )].x;// * g_ASSAOConsts.MaxViewspaceDepth;
    depthsArr[2].z = g_ViewspaceDepthSource2[baseCoords + int2( 0, 1 )].x;// * g_ASSAOConsts.MaxViewspaceDepth;
    depthsArr[2].w = g_ViewspaceDepthSource2[baseCoords + int2( 1, 1 )].x;// * g_ASSAOConsts.MaxViewspaceDepth;
    depthsArr[3].x = g_ViewspaceDepthSource3[baseCoords + int2( 0, 0 )].x;// * g_ASSAOConsts.MaxViewspaceDepth;
    depthsArr[3].y = g_ViewspaceDepthSource3[baseCoords + int2( 1, 0 )].x;// * g_ASSAOConsts.MaxViewspaceDepth;
    depthsArr[3].z = g_ViewspaceDepthSource3[baseCoords + int2( 0, 1 )].x;// * g_ASSAOConsts.MaxViewspaceDepth;
    depthsArr[3].w = g_ViewspaceDepthSource3[baseCoords + int2( 1, 1 )].x;// * g_ASSAOConsts.MaxViewspaceDepth;

    const uint2 SVPosui         = uint2( inPos.xy );
    const uint pseudoRandomA    = (SVPosui.x ) + 2 * (SVPosui.y );

    float dummyUnused1;
    float dummyUnused2;
    float falloffCalcMulSq, falloffCalcAdd;
 
    [unroll]
    for( int i = 0; i < 4; i++ )
    {
        float4 depths = depthsArr[i];

        float closest = min( min( depths.x, depths.y ), min( depths.z, depths.w ) );

        CalculateRadiusParameters( abs( closest ), 1.0, dummyUnused1, dummyUnused2, falloffCalcMulSq );

        float4 dists = depths - closest.xxxx;

        float4 weights = saturate( dists * dists * falloffCalcMulSq + 1.0 );

        float smartAvg = dot( weights, depths ) / dot( weights, float4( 1.0, 1.0, 1.0, 1.0 ) );

        const uint pseudoRandomIndex = ( pseudoRandomA + i ) % 4;

        //depthsOutArr[i] = closest;
        //depthsOutArr[i] = depths[ pseudoRandomIndex ];
        depthsOutArr[i] = smartAvg;
    }

    outD0 = depthsOutArr[0];
    outD1 = depthsOutArr[1];
    outD2 = depthsOutArr[2];
    outD3 = depthsOutArr[3];
}

void PSPrepareDepthMip1( in float4 inPos : SV_POSITION/*, in float2 inUV : TEXCOORD0*/, out float out0 : SV_Target0, out float out1 : SV_Target1, out float out2 : SV_Target2, out float out3 : SV_Target3 )
{
    PrepareDepthMip( inPos/*, inUV*/, 1, out0, out1, out2, out3 );
}

void PSPrepareDepthMip2( in float4 inPos : SV_POSITION/*, in float2 inUV : TEXCOORD0*/, out float out0 : SV_Target0, out float out1 : SV_Target1, out float out2 : SV_Target2, out float out3 : SV_Target3 )
{
    PrepareDepthMip( inPos/*, inUV*/, 2, out0, out1, out2, out3 );
}

void PSPrepareDepthMip3( in float4 inPos : SV_POSITION/*, in float2 inUV : TEXCOORD0*/, out float out0 : SV_Target0, out float out1 : SV_Target1, out float out2 : SV_Target2, out float out3 : SV_Target3 )
{
    PrepareDepthMip( inPos/*, inUV*/, 3, out0, out1, out2, out3 );
}

float3 DecodeNormal( float3 encodedNormal )
{
    float3 normal = encodedNormal * 2.0.xxx - 1.0.xxx;

    // normal = normalize( normal );    // normalize adds around 2.5% cost on High settings but makes little (PSNR 66.7) visual difference when normals are as in the sample (stored in R8G8B8A8_UNORM,
    //                                  // decoded in the shader), however it will likely be required if using different encoding/decoding or the inputs are not normalized, etc.
        
    return normal;
}

float3 LoadNormal( int2 pos )
{
    float3 encodedNormal = g_NormalmapSource.Load( int3( pos, 0 ) ).xyz;
    return DecodeNormal( encodedNormal );
}

float3 LoadNormal( int2 pos, int2 offset )
{
    float3 encodedNormal = g_NormalmapSource.Load( int3( pos, 0 ), offset ).xyz;
    return DecodeNormal( encodedNormal );
}

// all vectors in viewspace
float CalculatePixelObscurance( float3 pixelNormal, float3 hitDelta, float falloffCalcMulSq )
{
  float lengthSq = dot( hitDelta, hitDelta );
  float NdotD = dot(pixelNormal, hitDelta) / sqrt(lengthSq);

  float falloffMult = max( 0.0, lengthSq * falloffCalcMulSq + 1.0 );

  return max( 0, NdotD - g_ASSAOConsts.EffectHorizonAngleThreshold ) * falloffMult;
}

/*
// all vectors in viewspace
float CalculatePixelObscurance( float3 pixelNormal, float3 hitDelta, float falloffCalcMulSq, out float weight )
{
  float lengthSq = dot( hitDelta, hitDelta );
  float NdotD = dot(pixelNormal, hitDelta) / sqrt(lengthSq);

  float falloffMult = saturate( lengthSq * falloffCalcMulSq + 1.0 );

  // reduce weight of samples that fall out of the hemisphere - not really the best way to do it but shows promise;
  // perhaps the better and cheaper way would be to do it in 2d based on the chance of the sampling disk points
  // intersecting the hemisphere
  weight = saturate( 1.2 + NdotD );

  return saturate( NdotD - g_ASSAOConsts.EffectHorizonAngleThreshold ) * falloffMult;
}
*/

void SSAOTapInner( const int qualityLevel, inout float obscuranceSum, inout float weightSum, const float2 samplingUV, const float mipLevel, const float3 pixCenterPos, const float3 negViewspaceDir,float3 pixelNormal, const float falloffCalcMulSq, const float weightMod, const int dbgTapIndex )
{
    // get depth at sample
    float viewspaceSampleZ = g_ViewspaceDepthSource.SampleLevel( g_samplerPointClamp, samplingUV.xy, mipLevel ).x; // * g_ASSAOConsts.MaxViewspaceDepth;

    // convert to viewspace
    float3 hitPos = NDCToViewspace( samplingUV.xy, viewspaceSampleZ ).xyz;
    float3 hitDelta = hitPos - pixCenterPos;
    
    float obscurance = CalculatePixelObscurance( pixelNormal, hitDelta, falloffCalcMulSq );
    float weight = 1.0;

    if( qualityLevel >= SSAO_HALOING_REDUCTION_ENABLE_AT_QUALITY_PRESET )
    {
        //float reduct = max( 0, dot( hitDelta, negViewspaceDir ) );
        float reduct = max( 0, -hitDelta.z ); // cheaper, less correct version
        reduct = saturate( reduct * g_ASSAOConsts.NegRecEffectRadius + 2.0 ); // saturate( 2.0 - reduct / g_ASSAOConsts.EffectRadius );
        weight = SSAO_HALOING_REDUCTION_AMOUNT * reduct + (1.0 - SSAO_HALOING_REDUCTION_AMOUNT);
    }
    weight *= weightMod;
    obscuranceSum += obscurance * weight;
    weightSum += weight;

#ifdef SSAO_INTERNAL_SHADER_DEBUG
    g_ShaderDebugOutputUAV[ 4 + dbgTapIndex*5 + 0 ] = samplingUV.x;
    g_ShaderDebugOutputUAV[ 4 + dbgTapIndex*5 + 1 ] = samplingUV.y;
    g_ShaderDebugOutputUAV[ 4 + dbgTapIndex*5 + 2 ] = clamp( mipLevel, 0.0, float(SSAO_DEPTH_MIP_LEVELS)-1.0 ); // clamp not actually needed for above
    g_ShaderDebugOutputUAV[ 4 + dbgTapIndex*5 + 3 ] = saturate( weight );
    g_ShaderDebugOutputUAV[ 4 + dbgTapIndex*5 + 4 ] = saturate( 1.0 - sqrt( obscurance * 1.3 ) );
#endif
}

void SSAOTap( const int qualityLevel, inout float obscuranceSum, inout float weightSum, const int tapIndex, const float2x2 rotScale, const float3 pixCenterPos, const float3 negViewspaceDir, float3 pixelNormal, const float2 normalizedScreenPos, const float mipOffset, const float falloffCalcMulSq, float weightMod, float2 normXY, float normXYLength )
{
    float2  sampleOffset;
    float   samplePow2Len;

    // patterns
    {
        float4 newSample = g_samplePatternMain[tapIndex];
        sampleOffset    = mul( rotScale, newSample.xy );
        samplePow2Len   = newSample.w;                      // precalculated, same as: samplePow2Len = log2( length( newSample.xy ) );
        weightMod *= newSample.z;
    }

#if 0 // experimental - recovers some detail at distance by preventing use of samples that will be snapped to the center pixel and not have any occlusion; in reality it adds too much cost - better to simply increase the sample count
    {
        float pushOut = saturate( length(sampleOffset.xy) * 0.6 );
        sampleOffset.xy /= pushOut;
    }
#endif

    // snap to pixel center (more correct obscurance math, avoids artifacts)
    sampleOffset                    = round(sampleOffset);

    // calculate MIP based on the sample distance from the centre, similar to as described 
    // in http://graphics.cs.williams.edu/papers/SAOHPG12/.
    float mipLevel = ( qualityLevel < SSAO_DEPTH_MIPS_ENABLE_AT_QUALITY_PRESET )?(0):(samplePow2Len + mipOffset);

    float2 samplingUV = sampleOffset * g_ASSAOConsts.Viewport2xPixelSize + normalizedScreenPos;

    SSAOTapInner( qualityLevel, obscuranceSum, weightSum, samplingUV, mipLevel, pixCenterPos, negViewspaceDir, pixelNormal, falloffCalcMulSq, weightMod, tapIndex * 2 );

    // for the second tap, just use the mirrored offset
    float2 sampleOffsetMirroredUV    = -sampleOffset;

    // tilt the second set of samples so that the disk is effectively rotated by the normal
    // effective at removing one set of artifacts, but too expensive for lower quality settings
    if( qualityLevel >= SSAO_TILT_SAMPLES_ENABLE_AT_QUALITY_PRESET )
    {
        float dotNorm = dot( sampleOffsetMirroredUV, normXY );
        sampleOffsetMirroredUV -= dotNorm * normXYLength * normXY;
        sampleOffsetMirroredUV = round(sampleOffsetMirroredUV);
    }
    
    // snap to pixel center (more correct obscurance math, avoids artifacts)
    float2 samplingMirroredUV = sampleOffsetMirroredUV * g_ASSAOConsts.Viewport2xPixelSize + normalizedScreenPos;

    SSAOTapInner( qualityLevel, obscuranceSum, weightSum, samplingMirroredUV, mipLevel, pixCenterPos, negViewspaceDir, pixelNormal, falloffCalcMulSq, weightMod, tapIndex * 2 + 1 );
}

// this function is designed to only work with half/half depth at the moment - there's a couple of hardcoded paths that expect pixel/texel size, so it will not work for full res
void GenerateSSAOShadowsInternal( out float outShadowTerm, out float4 outEdges, out float outWeight, const float2 SVPos/*, const float2 normalizedScreenPos*/, uniform int qualityLevel, bool adaptiveBase )
{
    float2 SVPosRounded = trunc( SVPos );
    uint2 SVPosui = uint2( SVPosRounded ); //same as uint2( SVPos )

    const int numberOfTaps = g_numTaps[qualityLevel]; //(adaptiveBase)?(SSAO_ADAPTIVE_TAP_BASE_COUNT) : ( (qualityLevel==4)?(g_ASSAOConsts.AutoMatchSampleCount):(g_numTaps[qualityLevel]) );


    float pixZ, pixLZ, pixTZ, pixRZ, pixBZ;

    float4 valuesUL     = g_ViewspaceDepthSource.GatherRed( g_samplerPointClamp, SVPosRounded * g_ASSAOConsts.HalfViewportPixelSize );
    float4 valuesBR     = g_ViewspaceDepthSource.GatherRed( g_samplerPointClamp, SVPosRounded * g_ASSAOConsts.HalfViewportPixelSize, int2( 1, 1 ) );

    // get this pixel's viewspace depth
    pixZ = valuesUL.y; //float pixZ = g_ViewspaceDepthSource.SampleLevel( g_samplerPointClamp, normalizedScreenPos, 0.0 ).x; // * g_ASSAOConsts.MaxViewspaceDepth;

    // get left right top bottom neighbouring pixels for edge detection (gets compiled out on qualityLevel == 0)
    pixLZ   = valuesUL.x;
    pixTZ   = valuesUL.z;
    pixRZ   = valuesBR.z;
    pixBZ   = valuesBR.x;

    float2 normalizedScreenPos = SVPosRounded * g_ASSAOConsts.Viewport2xPixelSize + g_ASSAOConsts.Viewport2xPixelSize_x_025;
    float3 pixCenterPos = NDCToViewspace( normalizedScreenPos, pixZ ); // g

    // Load this pixel's viewspace normal
    uint2 fullResCoord = SVPosui * 2 + g_ASSAOConsts.PerPassFullResCoordOffset.xy;
    float3 pixelNormal = LoadNormal( fullResCoord );

    const float2 pixelDirRBViewspaceSizeAtCenterZ = pixCenterPos.z * g_ASSAOConsts.NDCToViewMul * g_ASSAOConsts.Viewport2xPixelSize;  // optimized approximation of:  float2 pixelDirRBViewspaceSizeAtCenterZ = NDCToViewspace( normalizedScreenPos.xy + g_ASSAOConsts.ViewportPixelSize.xy, pixCenterPos.z ).xy - pixCenterPos.xy;

    float pixLookupRadiusMod;
    float falloffCalcMulSq;

    // calculate effect radius and fit our screen sampling pattern inside it
    float effectViewspaceRadius;
    CalculateRadiusParameters( length( pixCenterPos ), pixelDirRBViewspaceSizeAtCenterZ, pixLookupRadiusMod, effectViewspaceRadius, falloffCalcMulSq );

    // calculate samples rotation/scaling
    float2x2 rotScale;
    {
        // reduce effect radius near the screen edges slightly; ideally, one would render a larger depth buffer (5% on each side) instead
        if( qualityLevel >= SSAO_REDUCE_RADIUS_NEAR_SCREEN_BORDER_ENABLE_AT_QUALITY_PRESET )
        {
            float nearScreenBorder = min( min( normalizedScreenPos.x, 1.0 - normalizedScreenPos.x ), min( normalizedScreenPos.y, 1.0 - normalizedScreenPos.y ) );
            nearScreenBorder = saturate( 10.0 * nearScreenBorder + 0.6 );
            pixLookupRadiusMod *= nearScreenBorder;
        }

        // load & update pseudo-random rotation matrix
        uint pseudoRandomIndex = uint( SVPosRounded.y * 2 + SVPosRounded.x ) % 5;
        float4 rs = g_ASSAOConsts.PatternRotScaleMatrices[ pseudoRandomIndex ];
        rotScale = float2x2( rs.x * pixLookupRadiusMod, rs.y * pixLookupRadiusMod, rs.z * pixLookupRadiusMod, rs.w * pixLookupRadiusMod );
    }

    // the main obscurance & sample weight storage
    float obscuranceSum = 0.0;
    float weightSum = 0.0;

    // edge mask for between this and left/right/top/bottom neighbour pixels - not used in quality level 0 so initialize to "no edge" (1 is no edge, 0 is edge)
    float4 edgesLRTB = float4( 1.0, 1.0, 1.0, 1.0 );

    // Move center pixel slightly towards camera to avoid imprecision artifacts due to using of 16bit depth buffer; a lot smaller offsets needed when using 32bit floats
    pixCenterPos *= g_ASSAOConsts.DepthPrecisionOffsetMod;

    if( qualityLevel >= SSAO_DEPTH_BASED_EDGES_ENABLE_AT_QUALITY_PRESET )
    {
        edgesLRTB = CalculateEdges( pixZ, pixLZ, pixRZ, pixTZ, pixBZ );
    }

    // adds a more high definition sharp effect, which gets blurred out (reuses left/right/top/bottom samples that we used for edge detection)
    if( qualityLevel >= SSAO_DETAIL_AO_ENABLE_AT_QUALITY_PRESET )
    {
        //approximate neighbouring pixels positions (actually just deltas or "positions - pixCenterPos" )
        float3 viewspaceDirZNormalized = float3( pixCenterPos.xy / pixCenterPos.zz, 1.0 );
        float3 pixLDelta  = float3( -pixelDirRBViewspaceSizeAtCenterZ.x, 0.0, 0.0 ) + viewspaceDirZNormalized * (pixLZ - pixCenterPos.z); // very close approximation of: float3 pixLPos  = NDCToViewspace( normalizedScreenPos + float2( -g_ASSAOConsts.HalfViewportPixelSize.x, 0.0 ), pixLZ ).xyz - pixCenterPos.xyz;
        float3 pixRDelta  = float3( +pixelDirRBViewspaceSizeAtCenterZ.x, 0.0, 0.0 ) + viewspaceDirZNormalized * (pixRZ - pixCenterPos.z); // very close approximation of: float3 pixRPos  = NDCToViewspace( normalizedScreenPos + float2( +g_ASSAOConsts.HalfViewportPixelSize.x, 0.0 ), pixRZ ).xyz - pixCenterPos.xyz;
        float3 pixTDelta  = float3( 0.0, -pixelDirRBViewspaceSizeAtCenterZ.y, 0.0 ) + viewspaceDirZNormalized * (pixTZ - pixCenterPos.z); // very close approximation of: float3 pixTPos  = NDCToViewspace( normalizedScreenPos + float2( 0.0, -g_ASSAOConsts.HalfViewportPixelSize.y ), pixTZ ).xyz - pixCenterPos.xyz;
        float3 pixBDelta  = float3( 0.0, +pixelDirRBViewspaceSizeAtCenterZ.y, 0.0 ) + viewspaceDirZNormalized * (pixBZ - pixCenterPos.z); // very close approximation of: float3 pixBPos  = NDCToViewspace( normalizedScreenPos + float2( 0.0, +g_ASSAOConsts.HalfViewportPixelSize.y ), pixBZ ).xyz - pixCenterPos.xyz;

        const float rangeReductionConst         = 4.0f;                         // this is to avoid various artifacts
        const float modifiedFalloffCalcMulSq    = rangeReductionConst * falloffCalcMulSq;

        float4 additionalObscurance;
        additionalObscurance.x = CalculatePixelObscurance( pixelNormal, pixLDelta, modifiedFalloffCalcMulSq );
        additionalObscurance.y = CalculatePixelObscurance( pixelNormal, pixRDelta, modifiedFalloffCalcMulSq );
        additionalObscurance.z = CalculatePixelObscurance( pixelNormal, pixTDelta, modifiedFalloffCalcMulSq );
        additionalObscurance.w = CalculatePixelObscurance( pixelNormal, pixBDelta, modifiedFalloffCalcMulSq );

        obscuranceSum += g_ASSAOConsts.DetailAOStrength * dot( additionalObscurance, edgesLRTB );
    }

    // Sharp normals also create edges - but this adds to the cost as well
    if( qualityLevel >= SSAO_NORMAL_BASED_EDGES_ENABLE_AT_QUALITY_PRESET )
    {
        float3 neighbourNormalL  = LoadNormal( fullResCoord, int2( -2,  0 ) );
        float3 neighbourNormalR  = LoadNormal( fullResCoord, int2(  2,  0 ) );
        float3 neighbourNormalT  = LoadNormal( fullResCoord, int2(  0, -2 ) );
        float3 neighbourNormalB  = LoadNormal( fullResCoord, int2(  0,  2 ) );

        const float dotThreshold = SSAO_NORMAL_BASED_EDGES_DOT_THRESHOLD;

        float4 normalEdgesLRTB;
        normalEdgesLRTB.x = saturate( (dot( pixelNormal, neighbourNormalL ) + dotThreshold ) );
        normalEdgesLRTB.y = saturate( (dot( pixelNormal, neighbourNormalR ) + dotThreshold ) );
        normalEdgesLRTB.z = saturate( (dot( pixelNormal, neighbourNormalT ) + dotThreshold ) );
        normalEdgesLRTB.w = saturate( (dot( pixelNormal, neighbourNormalB ) + dotThreshold ) );

//#define SSAO_SMOOTHEN_NORMALS // fixes some aliasing artifacts but kills a lot of high detail and adds to the cost - not worth it probably but feel free to play with it
#ifdef SSAO_SMOOTHEN_NORMALS
        //neighbourNormalL  = LoadNormal( fullResCoord, int2( -1,  0 ) );
        //neighbourNormalR  = LoadNormal( fullResCoord, int2(  1,  0 ) );
        //neighbourNormalT  = LoadNormal( fullResCoord, int2(  0, -1 ) );
        //neighbourNormalB  = LoadNormal( fullResCoord, int2(  0,  1 ) );
        pixelNormal += neighbourNormalL * edgesLRTB.x + neighbourNormalR * edgesLRTB.y + neighbourNormalT * edgesLRTB.z + neighbourNormalB * edgesLRTB.w;
        pixelNormal = normalize( pixelNormal );
#endif

        edgesLRTB *= normalEdgesLRTB;
    }

#ifdef SSAO_DEBUG_SHOWNORMALS
    g_DebuggingOutputUAV[ SVPosui * 2 + g_ASSAOConsts.PerPassFullResCoordOffset ] = float4( abs( pixelNormal.xyz * 0.5 + 0.5 ), 1.0 );
#endif
#ifdef SSAO_DEBUG_SHOWEDGES
    g_DebuggingOutputUAV[ SVPosui * 2 + g_ASSAOConsts.PerPassFullResCoordOffset ] = 1.0 - float4( edgesLRTB.x, edgesLRTB.y * 0.5 + edgesLRTB.w * 0.5, edgesLRTB.z, 0.0 );
#endif

#ifdef SSAO_INTERNAL_SHADER_DEBUG
    outEdges        = 0;
    outShadowTerm   = 0.0;
    outWeight       = 0;

    int2 fullResPos = SVPosui.xy * 2 + g_ASSAOConsts.FullResOffset;
    if( all(fullResPos == g_ASSAOConsts.DebugCursorPos) )
    {
        int __count = (int)((float)(numberOfTaps * 2) + SVPosRounded.x / 16768.0);

        g_ShaderDebugOutputUAV[0]   = asfloat( fullResPos.x ); //asfloat( (int)numberOfTaps );
        g_ShaderDebugOutputUAV[1]   = asfloat( fullResPos.y ); //0.0;
        g_ShaderDebugOutputUAV[2]   = asfloat( __count ); //asfloat( (int)numberOfTaps );
        g_ShaderDebugOutputUAV[3]   = 0;
    }
    else
    {
        discard;
        return;
    }
#endif

    const float globalMipOffset     = SSAO_DEPTH_MIPS_GLOBAL_OFFSET;
    float mipOffset = ( qualityLevel < SSAO_DEPTH_MIPS_ENABLE_AT_QUALITY_PRESET ) ? ( 0 ) : ( log2( pixLookupRadiusMod ) + globalMipOffset );

    // Used to tilt the second set of samples so that the disk is effectively rotated by the normal
    // effective at removing one set of artifacts, but too expensive for lower quality settings
    float2 normXY = float2( pixelNormal.x, pixelNormal.y );
    float normXYLength = length( normXY );
    normXY /= float2( normXYLength, -normXYLength );
    normXYLength *= SSAO_TILT_SAMPLES_AMOUNT;

    const float3 negViewspaceDir = -normalize( pixCenterPos );

    // standard approach (adaptive approach removed in Lite version)
    // [unroll] // <- doesn't seem to help on any platform, although the compilers seem to unroll anyway if const number of tap used!
    for( int i = 0; i < numberOfTaps; i++ )
    {
        SSAOTap( qualityLevel, obscuranceSum, weightSum, i, rotScale, pixCenterPos, negViewspaceDir, pixelNormal, normalizedScreenPos, mipOffset, falloffCalcMulSq, 1.0, normXY, normXYLength );
    }
   
    // calculate weighted average
    float obscurance = obscuranceSum / weightSum;

    // leaving this in as a reference; cool edge aware blur but produces chunky output
    //if( useDDXDDYBlur )
    //{
    //    // smart version
    //    float2 zDeltaXY = abs( float2( ddx( pixCenterPos.z ), ddy( pixCenterPos.z ) ) );
    //    float2 zDeltaXYWeights = saturate( zDeltaXY * 2.0 * falloffCalcMul + falloffCalcAdd );
    //    
    //    float2 interleavedPattern  = (frac( SVPos.xy * 0.5.xx ) - 0.5.xx) * 2.0.xx;
    //    float averageObscurance = obscurance;
    //    averageObscurance -= ddx(averageObscurance) * interleavedPattern.x * zDeltaXYWeights.x * 0.33;
    //    averageObscurance -= ddy(averageObscurance) * interleavedPattern.y * zDeltaXYWeights.y * 0.33;
    //    obscurance = averageObscurance;
    //}

    // calculate fadeout (1 close, gradient, 0 far)
    float fadeOut = saturate( pixCenterPos.z * g_ASSAOConsts.EffectFadeOutMul + g_ASSAOConsts.EffectFadeOutAdd );
  
    // Reduce the SSAO shadowing if we're on the edge to remove artifacts on edges (we don't care for the lower quality one)
    if( qualityLevel >= SSAO_DEPTH_BASED_EDGES_ENABLE_AT_QUALITY_PRESET )
    {
        // float edgeCount = dot( 1.0-edgesLRTB, float4( 1.0, 1.0, 1.0, 1.0 ) );

        // when there's more than 2 opposite edges, start fading out the occlusion to reduce aliasing artifacts
        float edgeFadeoutFactor = saturate( (1.0 - edgesLRTB.x - edgesLRTB.y) * 0.35) + saturate( (1.0 - edgesLRTB.z - edgesLRTB.w) * 0.35 );

        // (experimental) if you want to reduce the effect next to any edge
        // edgeFadeoutFactor += 0.1 * saturate( dot( 1 - edgesLRTB, float4( 1, 1, 1, 1 ) ) );

        fadeOut *= saturate( 1.0 - edgeFadeoutFactor );
    }
    
    // same as a bove, but a lot more conservative version
    // fadeOut *= saturate( dot( edgesLRTB, float4( 0.9, 0.9, 0.9, 0.9 ) ) - 2.6 );

    // strength
    obscurance = g_ASSAOConsts.EffectShadowStrength * obscurance;
    
    // clamp
    obscurance = min( obscurance, g_ASSAOConsts.EffectShadowClamp );
    
    // fadeout
    obscurance *= fadeOut;

    // conceptually switch to occlusion with the meaning being visibility (grows with visibility, occlusion == 1 implies full visibility), 
    // to be in line with what is more commonly used.
    float occlusion = 1.0 - obscurance;

    // modify the gradient
    // note: this cannot be moved to a later pass because of loss of precision after storing in the render target
    occlusion = pow( saturate( occlusion ), g_ASSAOConsts.EffectShadowPow );

    // outputs!
    outShadowTerm   = occlusion;    // Our final 'occlusion' term (0 means fully occluded, 1 means fully lit)
    outEdges        = edgesLRTB;    // These are used to prevent blurring across edges, 1 means no edge, 0 means edge, 0.5 means half way there, etc.
    outWeight       = weightSum;
}

void PSGenerateQ0( in float4 inPos : SV_POSITION/*, in float2 inUV : TEXCOORD0*/, out float2 out0 : SV_Target0 )
{
    float   outShadowTerm;
    float   outWeight;
    float4  outEdges;
    GenerateSSAOShadowsInternal( outShadowTerm, outEdges, outWeight, inPos.xy/*, inUV*/, 0, false );
    out0.x = outShadowTerm;
    out0.y = PackEdges( float4( 1, 1, 1, 1 ) ); // no edges in low quality
}

void PSGenerateQ1( in float4 inPos : SV_POSITION/*, in float2 inUV : TEXCOORD0*/, out float2 out0 : SV_Target0 )
{
    float   outShadowTerm;
    float   outWeight;
    float4  outEdges;
    GenerateSSAOShadowsInternal( outShadowTerm, outEdges, outWeight, inPos.xy/*, inUV*/, 1, false );
    out0.x = outShadowTerm;
    out0.y = PackEdges( outEdges );
}

void PSGenerateQ2( in float4 inPos : SV_POSITION/*, in float2 inUV : TEXCOORD0*/, out float2 out0 : SV_Target0 )
{
    float   outShadowTerm;
    float   outWeight;
    float4  outEdges;
    GenerateSSAOShadowsInternal( outShadowTerm, outEdges, outWeight, inPos.xy/*, inUV*/, 2, false );
    out0.x = outShadowTerm;
    out0.y = PackEdges( outEdges );
}

void PSGenerateQ3( in float4 inPos : SV_POSITION/*, in float2 inUV : TEXCOORD0*/, out float2 out0 : SV_Target0 )
{
    float   outShadowTerm;
    float   outWeight;
    float4  outEdges;
    GenerateSSAOShadowsInternal( outShadowTerm, outEdges, outWeight, inPos.xy/*, inUV*/, 3, false );
    out0.x = outShadowTerm;
    out0.y = PackEdges( outEdges );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Pixel shader that does smart edge-aware blurring that avoid bleeding between foreground and background objects
//
void AddSample( float ssaoValue, float edgeValue, inout float sum, inout float sumWeight )
{
    float weight = edgeValue;    

    sum += (weight * ssaoValue);
    sumWeight += weight;
}
//
float2 SampleBlurredWide( float4 inPos, float2 coord )
{
    float2 vC           = g_BlurInput.SampleLevel( g_samplerPointClamp, coord, 0.0, int2( 0,  0 ) ).xy;
    float2 vL           = g_BlurInput.SampleLevel( g_samplerPointClamp, coord, 0.0, int2( -2, 0 ) ).xy;
    float2 vT           = g_BlurInput.SampleLevel( g_samplerPointClamp, coord, 0.0, int2( 0, -2 ) ).xy;
    float2 vR           = g_BlurInput.SampleLevel( g_samplerPointClamp, coord, 0.0, int2(  2, 0 ) ).xy;
    float2 vB           = g_BlurInput.SampleLevel( g_samplerPointClamp, coord, 0.0, int2( 0,  2 ) ).xy;

    float packedEdges   = vC.y;
    float4 edgesLRTB    = UnpackEdges( packedEdges );
    edgesLRTB.x         *= UnpackEdges( vL.y ).y;
    edgesLRTB.z         *= UnpackEdges( vT.y ).w;
    edgesLRTB.y         *= UnpackEdges( vR.y ).x;
    edgesLRTB.w         *= UnpackEdges( vB.y ).z;

    //this would be more mathematically correct but there's little difference at cost
    //edgesLRTB.x         = sqrt( edgesLRTB.x );
    //edgesLRTB.z         = sqrt( edgesLRTB.z );
    //edgesLRTB.y         = sqrt( edgesLRTB.y );
    //edgesLRTB.w         = sqrt( edgesLRTB.w );

    float ssaoValue     = vC.x;
    float ssaoValueL    = vL.x;
    float ssaoValueT    = vT.x;
    float ssaoValueR    = vR.x;
    float ssaoValueB    = vB.x;

    float sumWeight = 0.8f;
    float sum = ssaoValue * sumWeight;

    AddSample( ssaoValueL, edgesLRTB.x, sum, sumWeight );
    AddSample( ssaoValueR, edgesLRTB.y, sum, sumWeight );
    AddSample( ssaoValueT, edgesLRTB.z, sum, sumWeight );
    AddSample( ssaoValueB, edgesLRTB.w, sum, sumWeight );

    float ssaoAvg = sum / sumWeight;

    ssaoValue = ssaoAvg; //min( ssaoValue, ssaoAvg ) * 0.2 + ssaoAvg * 0.8;

    return float2( ssaoValue, packedEdges );
}
//
float2 SampleBlurred( float4 inPos, float2 coord )
{
    float packedEdges   = g_BlurInput.Load( int3( inPos.xy, 0 ) ).y;
    float4 edgesLRTB    = UnpackEdges( packedEdges );

    float4 valuesUL     = g_BlurInput.GatherRed( g_samplerPointClamp, coord - g_ASSAOConsts.HalfViewportPixelSize * 0.5 );
    float4 valuesBR     = g_BlurInput.GatherRed( g_samplerPointClamp, coord + g_ASSAOConsts.HalfViewportPixelSize * 0.5 );

    float ssaoValue     = valuesUL.y;
    float ssaoValueL    = valuesUL.x;
    float ssaoValueT    = valuesUL.z;
    float ssaoValueR    = valuesBR.z;
    float ssaoValueB    = valuesBR.x;

    float sumWeight = 0.5f;
    float sum = ssaoValue * sumWeight;

    AddSample( ssaoValueL, edgesLRTB.x, sum, sumWeight );
    AddSample( ssaoValueR, edgesLRTB.y, sum, sumWeight );

    AddSample( ssaoValueT, edgesLRTB.z, sum, sumWeight );
    AddSample( ssaoValueB, edgesLRTB.w, sum, sumWeight );

    float ssaoAvg = sum / sumWeight;

    ssaoValue = ssaoAvg; //min( ssaoValue, ssaoAvg ) * 0.2 + ssaoAvg * 0.8;

    return float2( ssaoValue, packedEdges );
}
//
// edge-sensitive blur
float2 PSSmartBlur( in float4 inPos : SV_POSITION, in float2 inUV : TEXCOORD0 ) : SV_Target
{
    return SampleBlurred( inPos, inUV );
}
//
// edge-sensitive blur (wider kernel)
float2 PSSmartBlurWide( in float4 inPos : SV_POSITION, in float2 inUV : TEXCOORD0 ) : SV_Target
{
    return SampleBlurredWide( inPos, inUV );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float4 PSApply( in float4 inPos : SV_POSITION/*, in float2 inUV : TEXCOORD0*/ ) : SV_Target
{
    float ao;
    uint2 pixPos     = (uint2)inPos.xy;
    uint2 pixPosHalf = pixPos / uint2(2, 2);

#if defined(SSAO_DEBUG_SHOWNORMALS) || defined( SSAO_DEBUG_SHOWEDGES ) || defined( SSAO_DEBUG_SHOWSAMPLEHEATMAP )
    return pow( abs( g_DebuggingOutputSRV.Load( int3( pixPos, 0 ) ) ), 2.2 );
#endif

    // calculate index in the four deinterleaved source array texture
    int mx = (pixPos.x % 2);
    int my = (pixPos.y % 2);
    int ic = mx + my * 2;       // center index
    int ih = (1-mx) + my * 2;   // neighbouring, horizontal
    int iv = mx + (1-my) * 2;   // neighbouring, vertical
    int id = (1-mx) + (1-my)*2; // diagonal
    
    float2 centerVal = g_FinalSSAO.Load( int4( pixPosHalf, ic, 0 ) ).xy;
    
    ao = centerVal.x;

#if 1   // change to 0 if you want to disable last pass high-res blur (for debugging purposes, etc.)
    float4 edgesLRTB = UnpackEdges( centerVal.y );

    // convert index shifts to sampling offsets
    float fmx   = (float)mx;
    float fmy   = (float)my;
    
    // in case of an edge, push sampling offsets away from the edge (towards pixel center)
    float fmxe  = (edgesLRTB.y - edgesLRTB.x);
    float fmye  = (edgesLRTB.w - edgesLRTB.z);

    // calculate final sampling offsets and sample using bilinear filter
    float2  uvH = (inPos.xy + float2( fmx + fmxe - 0.5, 0.5 - fmy ) ) * 0.5 * g_ASSAOConsts.HalfViewportPixelSize;
    float   aoH = g_FinalSSAO.SampleLevel( g_samplerLinearClamp, float3( uvH, ih ), 0 ).x;
    float2  uvV = (inPos.xy + float2( 0.5 - fmx, fmy - 0.5 + fmye ) ) * 0.5 * g_ASSAOConsts.HalfViewportPixelSize;
    float   aoV = g_FinalSSAO.SampleLevel( g_samplerLinearClamp, float3( uvV, iv ), 0 ).x;
    float2  uvD = (inPos.xy + float2( fmx - 0.5 + fmxe, fmy - 0.5 + fmye ) ) * 0.5 * g_ASSAOConsts.HalfViewportPixelSize;
    float   aoD = g_FinalSSAO.SampleLevel( g_samplerLinearClamp, float3( uvD, id ), 0 ).x;

    // reduce weight for samples near edge - if the edge is on both sides, weight goes to 0
    float4 blendWeights;
    blendWeights.x = 1.0;
    blendWeights.y = (edgesLRTB.x + edgesLRTB.y) * 0.5;
    blendWeights.z = (edgesLRTB.z + edgesLRTB.w) * 0.5;
    blendWeights.w = (blendWeights.y + blendWeights.z) * 0.5;

    // calculate weighted average
    float blendWeightsSum   = dot( blendWeights, float4( 1.0, 1.0, 1.0, 1.0 ) );
    ao = dot( float4( ao, aoH, aoV, aoD ), blendWeights ) / blendWeightsSum;
#endif

    return float4( ao.xxx, 1 );
}

// edge-ignorant blur in x and y directions, 9 pixels touched (for the lowest quality level 0)
float2 PSNonSmartBlur( in float4 inPos : SV_POSITION, in float2 inUV : TEXCOORD0 ) : SV_Target
{
    float2 halfPixel = g_ASSAOConsts.HalfViewportPixelSize * 0.5f;

    float2 centre = g_BlurInput.SampleLevel( g_samplerLinearClamp, inUV, 0.0 ).xy;

    float4 vals;
    vals.x = g_BlurInput.SampleLevel( g_samplerLinearClamp, inUV + float2( -halfPixel.x * 3, -halfPixel.y ), 0.0 ).x;
    vals.y = g_BlurInput.SampleLevel( g_samplerLinearClamp, inUV + float2( +halfPixel.x, -halfPixel.y * 3 ), 0.0 ).x;
    vals.z = g_BlurInput.SampleLevel( g_samplerLinearClamp, inUV + float2( -halfPixel.x, +halfPixel.y * 3 ), 0.0 ).x;
    vals.w = g_BlurInput.SampleLevel( g_samplerLinearClamp, inUV + float2( +halfPixel.x * 3, +halfPixel.y ), 0.0 ).x;

    return float2(dot( vals, 0.2.xxxx ) + centre.x * 0.2, centre.y);
}

// edge-ignorant blur & apply (for the Low quality level 0)
float4 PSNonSmartApply( in float4 inPos : SV_POSITION, in float2 inUV : TEXCOORD0 ) : SV_Target
{
#if defined(SSAO_DEBUG_SHOWNORMALS) || defined( SSAO_DEBUG_SHOWEDGES ) || defined( SSAO_DEBUG_SHOWSAMPLEHEATMAP )
    return pow( abs( g_DebuggingOutputSRV.Load( int3( (uint2)inPos.xy, 0 ) ) ), 2.2 );
#endif

    float a = g_FinalSSAO.SampleLevel( g_samplerLinearClamp, float3( inUV.xy, 0 ), 0.0 ).x;
    float b = g_FinalSSAO.SampleLevel( g_samplerLinearClamp, float3( inUV.xy, 1 ), 0.0 ).x;
    float c = g_FinalSSAO.SampleLevel( g_samplerLinearClamp, float3( inUV.xy, 2 ), 0.0 ).x;
    float d = g_FinalSSAO.SampleLevel( g_samplerLinearClamp, float3( inUV.xy, 3 ), 0.0 ).x;
    float avg = (a+b+c+d) * 0.25;
    return float4( avg.xxx, 1.0 );
}
