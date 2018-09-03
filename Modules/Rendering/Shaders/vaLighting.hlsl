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


TextureCube         g_EnvironmentMap            : register( T_CONCATENATER( SHADERGLOBAL_LIGHTING_ENVMAP_TEXTURESLOT ) );
TextureCubeArray    g_CubeShadowmapArray        : register( T_CONCATENATER( SHADERGLOBAL_LIGHTING_CUBE_SHADOW_TEXTURESLOT ) );

cbuffer LightingConstantsBuffer                 : register( B_CONCATENATER( LIGHTINGGLOBAL_CONSTANTSBUFFERSLOT ) )
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