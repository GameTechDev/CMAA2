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

// A basic Blinn-Phong shading model, extended (optionally) with better specular AA (VA_RM_ADVANCED_SPECULAR_SHADER)

#ifndef VA_MATERIAL_BASIC_INCLUDE_AS_UTILITY
#include "vaRenderMesh.hlsl"
#endif

#include "vaRenderingShared.hlsl"

#include "vaLighting.hlsl"

//#ifdef VA_RM_ADVANCED_SPECULAR_SHADER
//#undef VA_RM_ADVANCED_SPECULAR_SHADER
//#endif

#ifndef VA_RM_ADVANCED_SPECULAR_SHADER
#define VA_RM_ADVANCED_SPECULAR_SHADER 0
#endif

struct BasicSurfaceMaterialValues
{
    float3x3    CotangentFrame;         // viewspace (co)tangent frame - perturbed by the normalmap if available! see ComputeCotangentFrame for more info; surface normal is CotangentFrame[2]!
    float3      Albedo;                 // a.k.a. diffuse
    float3      Specular;
    float3      Emissive;
    float2      Roughness;
#if VA_RM_ADVANCED_SPECULAR_SHADER == 0
    float       SpecularPow;
#endif
    float       Opacity;                // a.k.a. alpha (can be used for alpha testing or transparency)
    float       EnvironmentMapMul;      // if environment map enabled
    float       RefractionIndex;
    bool        IsFrontFace;
    float       Noise;
};

#ifndef VA_MATERIAL_BASIC_INCLUDE_AS_UTILITY

float3x3 BasicMaterialGetCotangentFrame( const RenderMaterialShaderInterpolants interpolants, const bool isFrontFace )
{
    float3 viewspaceNormal = normalize( interpolants.ViewspaceNormal.xyz );

    float3 viewDir      = normalize( interpolants.ViewspacePos.xyz );

    // flip for backfacing polys
    viewspaceNormal     = (isFrontFace)?(viewspaceNormal):(-viewspaceNormal);

    // near-perpendicular-to-eye normal filtering (see vertex shader for details)
    float normalAway    = (isFrontFace)?(interpolants.ViewspaceNormal.w):(interpolants.ViewspacePos.w);
    viewspaceNormal     = normalize( viewspaceNormal - normalAway * viewDir );

    float3x3 cotangentFrame;

#if VA_RM_INPUT_Normalmap_IS_TEXTURE
    {
        // load and unpack/decode normalmap from the texture
        float3 normal = VA_RM_INPUT_LOAD_Normalmap( interpolants ).xyz;

    //  figure out how to decode it
    #if VA_RM_INPUT_Normalmap_CONTENTS_TYPE_NORMAL_XYZ_UNORM
        normal.xyz = normal.xyz * 2.0 - 1.0;
        normal = normalize(normal);
    #elif VA_RM_INPUT_Normalmap_CONTENTS_TYPE_NORMAL_XY_UNORM
        normal.xy = normal.xy * 2.0 - 1.0;
        normal.z = sqrt( 1.0001 - normal.x*normal.x - normal.y * normal.y );
    #elif VA_RM_INPUT_Normalmap_CONTENTS_TYPE_NORMAL_WY_UNORM
        normal.xy = normal.wy * 2.0 - 1.0;
        normal.z = sqrt( 1.0001 - normal.x*normal.x - normal.y * normal.y );
    #elif VA_RM_INPUT_Normalmap_CONTENTS_TYPE_NORMAL_XY_LAEA
        normal.xyz = NormalmapDecodeLAEA( normal.xy );
    #else
        #error unrecognized texture type
    #endif
        //normal = normalize( normal );

        // compute (co)tangent frame (since we don't have a good path for vertex cotangent space)
        cotangentFrame = ComputeCotangentFrame( viewspaceNormal, interpolants.ViewspacePos.xyz, VA_RM_INPUT_Normalmap_TEXTURE_UV( interpolants ) );
        
        // // we can re-orthogonalize the frame for bad UV cases but... expensive
        // ReOrthogonalizeFrame( cotangentFrame, true );

        // perturb normal
        viewspaceNormal = normalize( mul( normal, cotangentFrame ) );

        // update (co)tangent frame
        cotangentFrame[2] = viewspaceNormal;

        // re-orthogonalize 
        // when called with 'false', this can mess up handedness which is only important if you rely on being in sync with input UVs, like perturb with another (for ex detail) normalmap
        ReOrthogonalizeFrame( cotangentFrame, false );  
    }
#else
        // if no normal map, just compute (co)tangent frame from the first set of UVs - "should just work"
        cotangentFrame = ComputeCotangentFrame( viewspaceNormal, interpolants.ViewspacePos.xyz, interpolants.Texcoord01.xy );
        
        // this should not be needed but the above step does not "just work" on our broken assets so use this...
        ReOrthogonalizeFrame( cotangentFrame, false );
#endif

    return cotangentFrame;
}

// Pretty heavy noise function based on world position with 'procedural mip layers' (computed from the viewspace pos using g_Global.ViewInv - so another offsetted 
// pseudo-ViewInv matrix can be used for precision if need be).
// I'm sure there's a better procedural noise somewhere but I couldn't find one that is stable in world space, doesn't flicker too much, doesn't use textures
// TODO: look at https://developer.amd.com/wordpress/media/2012/10/03_Clever_Shader_Tricks.pdf - Shader LOD branching - might have a better LOD compute
float ComputeNoise( const in RenderMaterialShaderInterpolants interpolants )
{
    float3 worldPos = mul( g_Global.ViewInv, float4( interpolants.ViewspacePos.xyz, 1) ).xyz; 
    float wpsx = length( ddx( worldPos ) ) ;
    float wpsy = length( ddy( worldPos ) ) ;
    float worldPixelSize = (8.0 * min( wpsx, wpsy ) + wpsx + wpsy) / 10.0;
    float noiseSuppress = saturate( 0.75 - abs( dot( normalize( interpolants.ViewspacePos.xyz ), interpolants.ViewspaceNormal.xyz ) ) );
    float noiseMip = log( worldPixelSize ); // + pow(1.0 - abs(interpolants.ViewspaceNormal.z), 2) * 2;
    noiseMip *= 1.4; // offset
    float noiseSizeHi0 = pow( 2.0, floor(noiseMip)              );
    float noiseSizeHi1 = pow( 2.0, floor(noiseMip) + 1.0        );
    float noiseSizeLo0 = pow( 2.0, floor(noiseMip+0.5)          );
    float noiseSizeLo1 = pow( 2.0, floor(noiseMip+0.5) + 1.0    );
    float3 subpixelOffsetAdd = float3 ( g_Global.CameraSubpixelOffset.xy, g_Global.CameraSubpixelOffset.x+g_Global.CameraSubpixelOffset.y ); subpixelOffsetAdd *= 10.0;
    float noiseHi0 = Hash3D( floor( worldPos.xyz / noiseSizeHi0 + subpixelOffsetAdd ) );
    float noiseHi1 = Hash3D( floor( worldPos.xyz / noiseSizeHi1 + subpixelOffsetAdd ) );
    float noiseLo0 = Hash3D( floor( worldPos.xyz / noiseSizeLo0 + subpixelOffsetAdd ) );
    float noiseLo1 = Hash3D( floor( worldPos.xyz / noiseSizeLo1 + subpixelOffsetAdd ) );
    float noise = 0.5 * ( lerp( noiseHi0, noiseHi1, noiseMip - floor(noiseMip) ) + lerp( noiseLo0, noiseLo1, noiseMip+0.5 - floor(noiseMip+0.5) ) );
    return saturate( lerp( noise, 0.5, noiseSuppress ) );
}

// Also early out on alpha test
BasicSurfaceMaterialValues FillBasicMaterialValues( const in RenderMaterialShaderInterpolants interpolants, const bool isFrontFace )
{
    BasicSurfaceMaterialValues ret;

    ret.IsFrontFace     = isFrontFace;
    ret.Albedo          = interpolants.Color.rgb;
    ret.Opacity         = interpolants.Color.a;

    ret.CotangentFrame  = BasicMaterialGetCotangentFrame( interpolants, isFrontFace );

    float4 albedo       = VA_RM_INPUT_LOAD_Albedo( interpolants );

    ret.Albedo          = albedo.rgb;
    ret.Opacity         = albedo.a;

//    // debugging: all textures to gray
//    ret.Albedo          = float3( 1.0, 1.0, 1.0 );

    // Do we combine opacity with .a from albedo or ignore .a? Combining means forcing sampling both
    // so let's just overwrite.
#if VA_RM_INPUT_Opacity_IS_TEXTURE
    ret.Opacity         = VA_RM_INPUT_LOAD_Opacity( interpolants );
#elif defined( VA_RM_INPUT_LOAD_Opacity )
    ret.Opacity         = VA_RM_INPUT_LOAD_Opacity( interpolants );
#endif

#if VA_RM_ALPHATEST
    if( ret.Opacity < VA_RM_ALPHATEST_THRESHOLD ) // g_RenderMeshMaterialGlobal.AlphaCutoff
        discard;
#endif

    ret.Emissive            = VA_RM_INPUT_LOAD_Emissive( interpolants ).rgb;
#if !VA_RM_INPUT_Emissive_IS_TEXTURE
    ret.Emissive            *= ret.Albedo; // if doesn't have the emissive texture, mix it with albedo texture
#endif

#ifdef VA_RM_INPUT_LOAD_EmissiveMul
    ret.Emissive            *= VA_RM_INPUT_LOAD_EmissiveMul( interpolants ).xxx;
#endif

    float4 specTex = VA_RM_INPUT_LOAD_Specular( interpolants );

    ret.Specular            = specTex.rgb * VA_RM_INPUT_LOAD_SpecularMul( interpolants );

    float specPow           = clamp( specTex.a * VA_RM_INPUT_LOAD_SpecularPow( interpolants ), 1.0, 2048.0 );
#if VA_RM_ADVANCED_SPECULAR_SHADER == 0
    ret.SpecularPow         = specPow;
#endif
    // completely arbitrary roughness from specular power conversion
    // ret.Roughness           = saturate( 1.0 - (log2( specTex.a * VA_RM_INPUT_LOAD_SpecularPow( interpolants ) ) - 1.0f) / 8.0 );
    ret.Roughness           = SpecPowerToRoughness( specPow ).xx;

    // convert roughness from UI representation 
    ret.Roughness           = clamp( ret.Roughness * ret.Roughness, 0.0025, 1.0 );

    ret.EnvironmentMapMul   = VA_RM_INPUT_LOAD_EnvironmentMapMul( interpolants ).x;

    ret.RefractionIndex     = VA_RM_INPUT_LOAD_RefractionIndex( interpolants ).x;

#if VA_RM_ADVANCED_SPECULAR_SHADER
    FilterRoughnessDeferred( interpolants.Position.xy, ret.CotangentFrame, g_Global.GlobalPixelScale, ret.Roughness );
#endif                                                                     

    ret.Noise               = ComputeNoise( interpolants );

    return ret;
}

#endif // #ifndef VA_MATERIAL_BASIC_INCLUDE_AS_UTILITY

void BasicMaterialAccumulateLights( float3 viewspacePos, BasicSurfaceMaterialValues material, inout float3 diffuseAccum, inout float3 reflectedAccum )
{
    const float3 viewDir        = normalize( viewspacePos );
    const float3 materialNormal = material.CotangentFrame[2];

    // Only apply lights above this brightness - TODO: should be based on current exposure/tonemapping settings!
    const float minViableLightThreshold = 1e-3f;

    int i;
    float diffTerm, specTerm;

    float2 cubeJitterRots[3];
    const float jitterZOffsets[3] = { 1.0, 1.0, 1.0 / g_Lighting.CubeShadowViewDistJitterOffsetSqrt }; //, 1.0 / (g_Lighting.CubeShadowViewDistJitterOffsetSqrt*g_Lighting.CubeShadowViewDistJitterOffsetSqrt) };
    {
        float angle = material.Noise * (3.1415 * 2.0); //g_Global.TimeFmod3600;
        float scale = g_Lighting.CubeShadowDirJitterOffset;
        // 3 taps so 120º angle
        float sa120 = 0.86602540378443864676372317075294;
        float ca120 = -0.5;
        cubeJitterRots[0] = float2( cos( angle ), sin( angle ) );
        cubeJitterRots[1] = float2( cubeJitterRots[0].x * ca120 - cubeJitterRots[0].y * sa120, cubeJitterRots[0].x * sa120 + cubeJitterRots[0].y * ca120 );
        cubeJitterRots[2] = float2( cubeJitterRots[1].x * ca120 - cubeJitterRots[1].y * sa120, cubeJitterRots[1].x * sa120 + cubeJitterRots[1].y * ca120 );
        cubeJitterRots[0] *= scale * 0.75;  // with different scaling we get corkscrew-like sampling points
        cubeJitterRots[1] *= scale * 1.25;  // with different scaling we get corkscrew-like sampling points
        cubeJitterRots[2] *= scale * 2.5;   // with different scaling we get corkscrew-like sampling points
    }

    // directional lights
    [loop]
    for( i = 0; i < g_Lighting.LightCountDirectional; i++ )
    {
        ShaderLightDirectional light = g_Lighting.LightsDirectional[i];

        diffTerm = saturate( dot( materialNormal, -light.Direction ) );

        [branch]    // early out, facing away from the light - both optimization and correctness but there should be a smoother transition to reduce aliasing
        if( diffTerm > 0 )
        {
#if VA_RM_ADVANCED_SPECULAR_SHADER
            ComputeGGXSpecularBRDF( viewDir, material.CotangentFrame, -light.Direction, material.Roughness, specTerm );
#else
            ComputePhongSpecularBRDF( viewDir, materialNormal, -light.Direction, material.SpecularPow, specTerm );
#endif
            diffuseAccum    += diffTerm * light.Intensity;
            reflectedAccum  += specTerm * light.Intensity;
        }
    }

    // point & spot lights combined
    [loop]
    for( i = 0; i < g_Lighting.LightCountSpotAndPoint; i++ )
    {
        ShaderLightSpot light = g_Lighting.LightsSpotAndPoint[i];

        float3 pixelToLight = light.Position - viewspacePos;
        float pixelToLightLength = length( pixelToLight );
        pixelToLight /= pixelToLightLength;

        diffTerm = saturate( dot( materialNormal, pixelToLight ) );

#if VA_RM_SPECIAL_EMISSIVE_LIGHT
        // only add emissive within light sphere, and then scale with light itself; this is to allow emissive materials to be 'controlled' by
        // the light - useful for models that represent light emitters (lamps, etc.)
        if( pixelToLightLength < light.Size )
            diffuseAccum  += material.Emissive.rgb * light.Intensity;
#endif

        // // debugging shadows
        // if( light.CubeShadowIndex >= 0 )
        // {
        //     // do shadow map stuff
        //     float3 cubeShadowDir = -normalize( mul( (float3x3)g_Global.ViewInv, pixelToLight ) );
        //     float value = g_CubeShadowmapArray.Sample( g_samplerPointClamp, float4( cubeShadowDir, light.CubeShadowIndex ) ).x;
        //     // return float4( GradientHeatMap( frac(length(viewspacePos)) ), 1.0 );
        //     // return float4( GradientHeatMap( frac(pixelToLightLength) ), 1.0 );
        //     return float4( GradientHeatMap( frac(value) ), 1.0 );
        // }

        [branch]    // early out, facing away from the light
        if( diffTerm > 0 )
        {
            const float earlyOutAttenuationThreshold = minViableLightThreshold / light.IntensityLength;

            float attenuationSqrt = max( light.Size, pixelToLightLength );
            float attenuation = 1.0 / (attenuationSqrt*attenuationSqrt);

            // spot light attenuation (if spot light) - yeah these could be 2 loops but that would be messy
            //[branch]
            //if( i < g_Lighting.LightCountSpotOnly )
            //{ 
                float angle = acos( dot( light.Direction, -pixelToLight ) );
                float spotAttenuation = saturate( (light.SpotOuterAngle - angle) / (light.SpotOuterAngle - light.SpotInnerAngle) );
        
                // squaring of spot attenuation is just for a softer outer->inner curve that I like more visually
                attenuation *= spotAttenuation*spotAttenuation;
            //}
            
#if 1 // enable shadows
            [branch]
            if( light.CubeShadowIndex >= 0 )
            {
                //[branch]
                //if( attenuation > earlyOutAttenuationThreshold )
                //{
                    // do shadow map stuff
                    float3 cubeShadowDir = -normalize( mul( (float3x3)g_Global.ViewInv, pixelToLight ) );

                    float distToLightCompVal = sqrt(pixelToLightLength / light.EffectiveRadius); // pixelToLightLength
                #if 0   // manual compare
                    float shadowmapCompVal = g_CubeShadowmapArray.Sample( g_samplerPointClamp, float4( cubeShadowDir, light.CubeShadowIndex ) ).x;

                    if( shadowmapCompVal < distToLightCompVal )
                        attenuation *= 0;
                #elif 0 // use cmp sampler, 1-tap
                    attenuation *= g_CubeShadowmapArray.SampleCmp( g_samplerLinearCmpSampler, float4( cubeShadowDir, light.CubeShadowIndex ), distToLightCompVal ).x;
                #else   // use multi-tap with cmp sampler
                    float3 shadows;

                    float3 jitterX, jitterY;
                    ComputeOrthonormalBasis( cubeShadowDir, jitterX, jitterY );
                    
                    [unroll]
                    for( uint jittI = 0; jittI < 3; jittI++ )
                    {
                        float3 jitterOffset = jitterX * cubeJitterRots[jittI].x + jitterY * cubeJitterRots[jittI].y;
                        shadows[jittI] = g_CubeShadowmapArray.SampleCmp( g_samplerLinearCmpSampler, float4( normalize(cubeShadowDir + jitterOffset), light.CubeShadowIndex ), distToLightCompVal * jitterZOffsets[jittI] ).x;
                    }
                    #if 1 // blend in sqrt space trick
                        shadows.xyz = sqrt( shadows.xyz );
                        float shadow = dot( shadows.xyz, 1.0 / 3.0 );
                        shadow = (shadow * shadow);
                        attenuation *= shadow;
                    #else
                        attenuation *= dot( shadows.xyz, 1.0 / 3.0 );
                    #endif
                #endif
                //}
            }
#endif

            [branch]
            if( attenuation > earlyOutAttenuationThreshold )
            {

#if VA_RM_ADVANCED_SPECULAR_SHADER
                ComputeGGXSpecularBRDF( viewDir, material.CotangentFrame, pixelToLight, material.Roughness, specTerm );
#else
                ComputePhongSpecularBRDF( viewDir, materialNormal, pixelToLight, material.SpecularPow, specTerm );
#endif

                diffuseAccum    += (attenuation * diffTerm) * light.Intensity;
                reflectedAccum  += (attenuation * specTerm) * light.Intensity;
            }
        }
    }

#if VA_RM_ADVANCED_SPECULAR_SHADER
    reflectedAccum *= 0.05;   // ugly hack to make 'advanced specular' roughly match Phong in intensity
    //return float4( frac( material.Roughness ).xxx, 1 );
#endif
}

void BasicMaterialAccumulateReflections( float3 viewspacePos, BasicSurfaceMaterialValues material, inout float3 reflectedAccum )
{
    const float3 viewDir        = normalize( viewspacePos );
    const float3 materialNormal = material.CotangentFrame[2];

    // for future reference: https://placeholderart.wordpress.com/2015/07/28/implementation-notes-runtime-environment-map-filtering-for-image-based-lighting/
    float envMul = g_Lighting.EnvmapMultiplier * material.EnvironmentMapMul;
    [branch]
    if( g_Lighting.EnvmapEnabled && ( envMul > 0.0 ) )
    {
        // viewDir is incidence dir - computing this in viewspace
        float3 reflectionVec = reflect( viewDir, materialNormal );

        // convert to world (or cubemap) space
        // OPT: normalize not needed probably (shouldn't be needed)
        reflectionVec = normalize( mul( (float3x3)g_Lighting.EnvmapRotation, reflectionVec ) );

        // how many mips do we have?
        uint cubeWidth, cubeHeight, cubeNumberOfLevels;
        g_EnvironmentMap.GetDimensions( 0, cubeWidth, cubeHeight, cubeNumberOfLevels );

        float3 envSample = g_EnvironmentMap.SampleBias( g_samplerAnisotropicWrap, reflectionVec, (material.Roughness.x+material.Roughness.y) * 0.5 * 8.0 ).xyz;

        envSample = max( envSample, 0 );

        reflectedAccum += envMul * envSample;
    }
}

float3 BasicMaterialFinalizeLight( float3 viewspacePos, BasicSurfaceMaterialValues material, float3 diffuseAccum, float3 reflectedAccum, uniform bool transparent )
{
    const float3 viewDir        = normalize( viewspacePos );
    const float3 materialNormal = material.CotangentFrame[2];

    // Schlick Approximation for Fresnel term
    const float Rzero = material.RefractionIndex;       // reflection coefficient
    float fresnel = saturate( Rzero + ( 1.0f - Rzero ) * pow( abs( 1.0f - dot( -viewDir, materialNormal ) ), 5.0 ) );

    // (this is not a PBR-based model - PBR is on the todo list :) )

    // add albedo color to diffusely reflected light
    diffuseAccum = diffuseAccum * material.Albedo;

    // add specular color and scale by fresnel to directly reflected light
    reflectedAccum = reflectedAccum * fresnel * material.Specular;

    // start calculating final colour
    float3 lightAccum       = 0;

    // for debugging
    // material.Opacity = 1;

    lightAccum  += material.Albedo.rgb * g_Lighting.AmbientLightIntensity.rgb;

    // for debugging AO and similar - lights everything with white ambient and takes out most of the other lighting
    // lightAccum = 50.0;

    lightAccum  += diffuseAccum;

    // these are not diminished by alpha so "un-alpha" them here (should probably use premult alpha blending mode and multiply above by alpha instead)
    float reflectedFactor = 1.0;
    if( transparent )
        reflectedFactor /= max( 0.001, material.Opacity );
    lightAccum  += material.Emissive.rgb * reflectedFactor;
    lightAccum  += reflectedAccum * reflectedFactor;

    lightAccum  = max( 0, lightAccum );

    return lightAccum;
}

#ifndef VA_MATERIAL_BASIC_INCLUDE_AS_UTILITY

float4 MeshColor( const RenderMaterialShaderInterpolants interpolants, BasicSurfaceMaterialValues material )
{
    float3 diffuseAccum     = 0.0f;
    float3 reflectedAccum   = 0.0f;

    BasicMaterialAccumulateLights( interpolants.ViewspacePos.xyz, material, diffuseAccum, reflectedAccum );

    BasicMaterialAccumulateReflections( interpolants.ViewspacePos.xyz, material, reflectedAccum );

#if VA_RM_SPECIAL_EMISSIVE_LIGHT
    material.Emissive.rgb = 0;
#endif

#if VA_RM_TRANSPARENT
    float3 lightAccum = BasicMaterialFinalizeLight( interpolants.ViewspacePos.xyz, material, diffuseAccum, reflectedAccum, true );
#else
    float3 lightAccum = BasicMaterialFinalizeLight( interpolants.ViewspacePos.xyz, material, diffuseAccum, reflectedAccum, false );
#endif

    lightAccum  = LightingApplyFog( interpolants.ViewspacePos.xyz, lightAccum );

// debug display normalmap texture UVs
//#if VA_RM_INPUT_Normalmap_IS_TEXTURE
//    return float4( frac( VA_RM_INPUT_Normalmap_TEXTURE_UV( interpolants ) ), 0, 1 );
//#else
//    return float4( 0, 0, 1, 1 );
//#endif

#if 0 // debug display normals, etc
    const float3 materialNormal = material.CotangentFrame[2];
    return float4( DisplayNormalSRGB( materialNormal ), 1.0 );
#elif 0
    return float4( DisplayNormalSRGB( material.CotangentFrame[0] ), 1.0 );
#elif 0
    return float4( frac( interpolants.Texcoord01.xy ), 0, 1 );
#endif


    //return float4( abs( 1.0 - abs(noiseMip - floor(noiseMip)), abs( 0.5 - abs(noiseMip - floor(noiseMip)), abs( 0.0 - abs(noiseMip - floor(noiseMip)) ), 1.0 );
    //return float4( material.Noise.xxx, 1 );
    //return float4( 1, 1, 1, 1 );

    return float4( lightAccum, material.Opacity );
}

// depth test or a simple shadow map
void PS_DepthOnly( const in RenderMaterialShaderInterpolants interpolants, const bool isFrontFace : SV_IsFrontFace )// : SV_Target
{
#if VA_RM_ALPHATEST
    FillBasicMaterialValues( interpolants, isFrontFace );
#endif
    // // if transparent, depth test
    // return float4( 0.0, 1.0, 0.0, 1.0 );
}

// pixel shader outputs custom shadow value(s)
float4 PS_CustomShadow( const in RenderMaterialShaderInterpolants interpolants, const bool isFrontFace : SV_IsFrontFace ) : SV_Target
{
#if VA_RM_ALPHATEST
    FillBasicMaterialValues( interpolants, isFrontFace );
#endif

    if( g_Global.CustomShadowmapShaderType == 1 )
    {
        // offsets added in the vertex shader - look for ViewspaceDepthOffsetFlatAdd etc
        float viewspaceLength = length( interpolants.ViewspacePos.xyz );

        // use sqrt of length for more nearby precision in R16_FLOAT (not ideal but works better than linear)
        return float4( sqrt(viewspaceLength / g_Global.CameraNearFar.y), 0.0, 0.0, 0.0 );
        //return float4( viewspaceLength, 0.0, 0.0, 0.0 );
    }
    else
    {
        return float4( 0.0, 0.0, 0.0, 0.0 );
    }
    // // if transparent, depth test
    // return float4( 0.0, 1.0, 0.0, 1.0 );
}

float4 PS_Forward( const in RenderMaterialShaderInterpolants interpolants, const bool isFrontFace : SV_IsFrontFace ) : SV_Target
{
    BasicSurfaceMaterialValues material = FillBasicMaterialValues( interpolants, isFrontFace );

    float4 finalColor = MeshColor( interpolants, material );

    finalColor.rgb = finalColor.rgb * interpolants.Color.aaa + interpolants.Color.rgb;

    return finalColor;
}













struct GBufferOutput
{
    float4  Albedo              : SV_Target0;
    float4  Normal              : SV_Target1;
};


GBufferOutput PS_Deferred( const in RenderMaterialShaderInterpolants interpolants, const bool isFrontFace : SV_IsFrontFace )
{
    // code stale, new path not implemented yet
#if 0
    LocalMaterialValues lmv = FillBasicMaterialValues( interpolants, isFrontFace );

    // this should be #if VA_RM_INPUT_AlphaTest_CONST_BOOLEAN
#if VA_RM_ALPHATEST
    if( lmv.Albedo.a < 0.5 ) // g_RenderMeshMaterialGlobal.AlphaCutoff
        discard;
#endif

    return EncodeGBuffer( lmv );
#else
    GBufferOutput ret;
    ret.Albedo = 0;
    ret.Normal = 0;
    return ret;
#endif
}

#endif // #ifndef VA_MATERIAL_BASIC_INCLUDE_AS_UTILITY

/*
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GBuffer helpers
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct GBufferDecodedPixel
{
    float3  Albedo;
    float3  Normal;
    float3  ViewspacePos;
};

GBufferOutput EncodeGBuffer( LocalMaterialValues lmv )
{
    GBufferOutput ret;
    
    float3 normal   = (lmv.IsFrontFace)?(lmv.Normal):(-lmv.Normal);

    ret.Albedo = float4( lmv.Albedo.xyz, 0.0 );
    ret.Normal = float4( GBufferEncodeNormal( normal ), 0.0 );

    return ret;
}

GBufferDecodedPixel DecodeGBuffer( float2 SVPos, float viewspaceDepth, float4 albedoPixelValue, float4 normalPixelValue )
{
    GBufferDecodedPixel ret;

    ret.Albedo          = albedoPixelValue.xyz;
    ret.Normal          = GBufferDecodeNormal( normalPixelValue.xyz );

    ret.ViewspacePos    = NDCToViewspacePosition( SVPos.xy, viewspaceDepth );

    return ret;
}
*/