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

#include "vaSharedTypes.h"

#ifndef VA_COMPILED_AS_SHADER_CODE
namespace VertexAsylum
{
#endif

struct ShaderLightDirectional
{
    static const int    MaxLights                       = 16;

    vaVector3           Intensity;
    float               IntensityLength;

    vaVector3           Direction;
    float               Dummy1;
};
//struct ShaderLightPoint - no longer used, just using spot instead
//{
//    static const int    MaxLights                       = 16;
//
//    vaVector3           Intensity;
//    float               Size;                           // distance from which to start attenuating or compute umbra/penumbra/antumbra / compute specular (making this into a 'sphere' light) - useful to avoid near-infinities for when close-to-point lights
//    vaVector3           Position;
//    float               EffectiveRadius;                // distance at which it is considered that it cannot effectively contribute any light and can be culled
//};
struct ShaderLightSpot
{
    static const int    MaxLights                       = 32;

    vaVector3           Intensity;
    float               IntensityLength;
    vaVector3           Position;
    float               EffectiveRadius;                // distance at which it is considered that it cannot effectively contribute any light and can be culled
    vaVector3           Direction;
    float               Size;                           // distance from which to start attenuating or compute umbra/penumbra/antumbra / compute specular (making this into a 'sphere' light) - useful to avoid near-infinities for when close-to-point lights
    float               SpotInnerAngle;                 // angle from Direction below which the spot light has the full intensity (a.k.a. inner cone angle)
    float               SpotOuterAngle;                 // angle from Direction below which the spot light intensity starts dropping (a.k.a. outer cone angle)
    
    float               CubeShadowIndex;                // if used, index of cubemap shadow in the cubemap array texture; otherwise -1
    float               Dummy1;
};

struct LightingShaderConstants
{
    static const int        MaxShadowCubes                  = 8;   // so the number of cube faces is x6 this - lots of RAM

    vaVector4               AmbientLightIntensity;

    // See vaFogSphere for descriptions
    vaVector3               FogCenter;
    int                     FogEnabled;

    vaVector3               FogColor;
    float                   FogRadiusInner;

    float                   FogRadiusOuter;
    float                   FogBlendCurvePow;
    float                   FogBlendMultiplier;
    float                   FogRange;                   // FogRadiusOuter - FogRadiusInner

    vaMatrix4x4             EnvmapRotation;             // ideally we shouldn't need this but at the moment we support this to simplify asset side...
    
    int                     EnvmapEnabled;
    float                   EnvmapMultiplier;
    int                     LightCountDirectional;
    int                     LightCountSpotAndPoint;

    int                     LightCountSpotOnly;
    float                   CubeShadowDirJitterOffset;          // 'sideways' offset when doing PCF jitter for cubemap shadows
    float                   CubeShadowViewDistJitterOffset;     // multiplication offset along cube sampling dir - similar to ViewspaceDepthOffsetFlatMul but only identical at the cube face centers!!
    float                   CubeShadowViewDistJitterOffsetSqrt; // same as above but for use with sqrt-packed cube shadowmaps

    ShaderLightDirectional  LightsDirectional[ShaderLightDirectional::MaxLights];
    //ShaderLightPoint        LightsPoint[ShaderLightPoint::MaxLights];
    ShaderLightSpot         LightsSpotAndPoint[ShaderLightSpot::MaxLights];

    //
    // vaVector4               ShadowCubes[MaxShadowCubes];    // .xyz is cube center and .w is unused at the moment
};

#ifndef VA_COMPILED_AS_SHADER_CODE
} // namespace VertexAsylum
#endif

#ifdef VA_COMPILED_AS_SHADER_CODE

#include "vaShared.hlsl"

TextureCube         g_EnvironmentMap            : register( T_CONCATENATER( SHADERGLOBAL_LIGHTING_ENVMAP_TEXTURESLOT_V ) );
TextureCubeArray    g_CubeShadowmapArray        : register( T_CONCATENATER( SHADERGLOBAL_LIGHTING_CUBE_SHADOW_TEXTURESLOT_V ) );

cbuffer LightingConstantsBuffer                 : register( B_CONCATENATER( LIGHTINGGLOBAL_CONSTANTSBUFFERSLOT_V ) )
{
    LightingShaderConstants     g_Lighting;
}

float       LightingGetFogK( float3 surfaceViewspacePos )
{
    if( g_Lighting.FogEnabled )
    {
        float dist = length( surfaceViewspacePos - g_Lighting.FogCenter);
        return g_Lighting.FogBlendMultiplier * pow( saturate( (dist - g_Lighting.FogRadiusInner) / g_Lighting.FogRange), g_Lighting.FogBlendCurvePow );
    }
    else
    {
        return 0.0;
    }
}

float3      LightingApplyFog( float3 surfaceViewspacePos, float3 inColor )
{
    return lerp( inColor, g_Lighting.FogColor, LightingGetFogK( surfaceViewspacePos ) );
}

#endif


// #include "vaSimpleShadowMap.hlsl"

// At the moment, LightingGlobalShaderConstants are part of the ShaderGlobalConstants, defined in vaSharedTypes.h, 
// accessed with g_Global.Lighting.
// All of that will probably be decoupled and moved here in the future.



// Deferred lighting removed from here - should be done per-material with the stencil mask but that's something
// for a future implementation.
/*
Texture2D           g_textureSlot0              : register(t0);
Texture2D           g_textureSlot1              : register(t1);
Texture2D           g_textureSlot2              : register(t2);

float4 ApplyDirectionalAmbient( const in float4 Position, uniform const bool shadowed )
{
    float viewspaceDepth    = g_textureSlot0.Load( int3( int2(Position.xy), 0 ) ).x;
    float4 albedoPixelValue = g_textureSlot1.Load( int3( int2(Position.xy), 0 ) ).xyzw;
    float4 normalPixelValue = g_textureSlot2.Load( int3( int2(Position.xy), 0 ) ).xyzw;

    GBufferDecodedPixel gpix = DecodeGBuffer( Position.xy, viewspaceDepth, albedoPixelValue, normalPixelValue );

    //return float4( frac(viewspaceDepth.xxx), 1.0 );
    //return float4( frac(gpix.ViewspacePos.xyz), 1.0 );

    float shadowTerm = 1.0f;
    if( shadowed )
    {
        shadowTerm = SimpleShadowMapSample( gpix.ViewspacePos.xyz );
        //shadowTerm = 
        //return float4( frac(gpix.ViewspacePos.zzz), 1.0 );
        //return float4( frac(gpix.ViewspacePos.xyz), 1.0 );
    }

    float3 albedo   = gpix.Albedo;
    float3 normal   = gpix.Normal;
    float3 viewDir  = normalize( gpix.ViewspacePos.xyz );

//    return float4( DisplayNormalSRGB( normal ), 1.0 );

    const float3 diffuseLightVector     = -g_Lighting.DirectionalLightViewspaceDirection.xyz;

    // start calculating final colour
    float3 lightAccum       = albedo * g_Lighting.AmbientLightIntensity.rgb;

    // directional light
    float nDotL             = dot( normal, diffuseLightVector );

    float3 reflected        = diffuseLightVector - 2.0*nDotL*normal;
	float rDotV             = saturate( dot(reflected, viewDir) );
    float specular          = saturate( pow( saturate( rDotV ), 8.0 ) );

    // facing towards light: front and specular
    float lightFront        = saturate( nDotL ) * shadowTerm;
    lightAccum              += lightFront * albedo.rgb * g_Lighting.DirectionalLightIntensity.rgb;
    //lightAccum += fakeSpecularMul * specular;

    // deferred fog goes into a separate pass!
    // finalColor              = FogForwardApply( finalColor, length( input.ViewspacePos.xyz ) );

    return float4( lightAccum.xyz, 1 );
}

float4 ApplyDirectionalAmbientShadowedPS( const in float4 Position : SV_Position, const in float2 Texcoord : TEXCOORD0 ) : SV_Target
{
    return ApplyDirectionalAmbient( Position, true );
}

float4 ApplyDirectionalAmbientPS( const in float4 Position : SV_Position, const in float2 Texcoord : TEXCOORD0 ) : SV_Target
{
    return ApplyDirectionalAmbient( Position, false );
}
*/

// // just a passthrough stub for now
// float4 ApplyTonemapPS( const in float4 Position : SV_Position, const in float2 Texcoord : TEXCOORD0 ) : SV_Target
// {
//     return g_textureSlot0.Load( int3( int2(Position.xy), 0 ) ).xyzw;
// }