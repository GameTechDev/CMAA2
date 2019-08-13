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

#ifndef VA_RENDERINGSHARED_HLSL_INCLUDED
#define VA_RENDERINGSHARED_HLSL_INCLUDED

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Basic Phong reflection model

void ComputePhongDiffuseBRDF( float3 viewDir, float3 surfaceNormal, float3 surfaceToLightDir, out float outDiffuseTerm )
{
    outDiffuseTerm = max( 0.0, dot(surfaceNormal, surfaceToLightDir) );
}

void ComputePhongSpecularBRDF( float3 viewDir, float3 surfaceNormal, float3 surfaceToLightDir, float specularPower, out float outSpecularTerm )
{
    float3 reflectDir = reflect( surfaceToLightDir, surfaceNormal );
    float RdotV = max( 0.0, dot( reflectDir, viewDir ) );
    outSpecularTerm = pow(RdotV, specularPower);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ggx + specular AA; see links below
// http://www.neilblevins.com/cg_education/ggx/ggx.htm
// http://research.nvidia.com/sites/default/files/pubs/2016-06_Filtering-Distributions-of/NDFFiltering.pdf - http://blog.selfshadow.com/sandbox/specaa.html
// for future optimization: http://filmicworlds.com/blog/optimizing-ggx-shaders-with-dotlh/
// Specular antialiasing part
float2 ComputeFilterRegion(float3 h)
{
    // Compute half-vector derivatives
    float2 hpp   = h.xy/h.z;
    float2 hppDx = ddx(hpp);
    float2 hppDy = ddy(hpp);

    // Compute filtering region
    float2 rectFp = (abs(hppDx) + abs(hppDy)) * 0.5;

    // For grazing angles where the first-order footprint goes to very high values
    // Usually you don’t need such high values and the maximum value of 1.0 or even 0.1
    // is enough for filtering.
    return min(float2(0.2, 0.2), rectFp);
}
// Self-contained roughness filtering function, to be used in forward shading
void FilterRoughness(float3 h, inout float2 roughness)
{
    float2 rectFp = ComputeFilterRegion(h);

    // Covariance matrix of pixel filter's Gaussian (remapped in roughness units)
    // Need to x2 because roughness = sqrt(2) * pixel_sigma_hpp
    float2 covMx = rectFp * rectFp * 2.0;

    roughness = sqrt( roughness*roughness + covMx ); // Beckmann proxy convolution for GGX
}
// Self-contained roughness filtering function, to be used in the G-Buffer generation pass with deferred shading
void FilterRoughnessDeferred(float2 pixelScreenPos, float3x3 cotangentFrame, in float2 globalPixelScale, inout float2 roughness)
{
    // Estimate the approximate half vector w/o knowing the light position
    float3 approxHW = cotangentFrame[2]; // normal
    approxHW -= globalPixelScale.xxx * ddx( approxHW ) * ( GLSL_mod( pixelScreenPos.x, 2.0 ) - 0.5 );
    approxHW -= globalPixelScale.yyy * ddy( approxHW ) * ( GLSL_mod( pixelScreenPos.y, 2.0 ) - 0.5 );

    // Transform to the local space
    float3 approxH = normalize( mul( cotangentFrame, approxHW ) );

    // Do the regular filtering
    FilterRoughness(approxH, roughness);
}
//
float GGX(float3 h, float2 rgns)
{
    rgns = max( float2( 1e-3, 1e-3 ), rgns );
    float NoH2 = h.z * h.z;
    float2 Hproj = h.xy;
    float exponent = dot(Hproj / (rgns*rgns), Hproj);
    float root = NoH2 + exponent;
    return 1.0 / (VA_PI * rgns.x * rgns.y * root * root);
}
//
void ComputeGGXSpecularBRDF( float3 viewDir, float3x3 cotangentFrame, float3 surfaceToLightDir, float2 roughness, out float outSpecularTerm )
{
    // compute half-way vector
    float3 halfWayVec = normalize( -viewDir + surfaceToLightDir );

    // convert half-way vector to tangent space
    float3 h = normalize( mul( cotangentFrame, halfWayVec ) );

    // assuming input roughness has this already applied 
    // FilterRoughness( h, roughness );

    outSpecularTerm = GGX( h, roughness );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GGX/Beckmann <-> Phong - from https://gist.github.com/Kuranes/e43ec6f4b64c7303ef6e
//
// ================================================================================================
// Converts a Beckmann roughness parameter to a Phong specular power
// ================================================================================================
float RoughnessToSpecPower(in float m)
{
    return 2.0 / (m * m) - 2.0;
}
//
// ================================================================================================
// Converts a Blinn-Phong specular power to a Beckmann roughness parameter
// ================================================================================================
float SpecPowerToRoughness(in float s)
{
    return sqrt(2.0 / (s + 2.0));
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Various utility
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ReOrthogonalizeFrame( inout float3x3 frame, uniform const bool preserveHandedness )
{
    frame[1] = normalize( frame[1] - frame[2]*dot(frame[1], frame[2]) );
    // ^ same as normalize( cross( frame[2], frame[1] ) ) - check if faster
    
    if( preserveHandedness )   // this path works for both right handed and left handed cases
    {
        float3 newB = cross(frame[2], frame[1]);
        frame[0] = sign(dot(newB, frame[0])) * newB;
    }
    else
    {
        frame[0] = cross(frame[2], frame[1]);
    }
}

// from http://www.thetenthplanet.de/archives/1180
float3x3 ComputeCotangentFrame( float3 N, float3 p, float2 uv )
{
    // get edge vectors of the pixel triangle
    float3 dp1  = ddx( p );
    float3 dp2  = ddy( p );
    float2 duv1 = ddx( uv );
    float2 duv2 = ddy( uv );
 
    // solve the linear system
    float3 dp2perp = cross( dp2, N );
    float3 dp1perp = cross( N, dp1 );
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;
 
    // construct a scale-invariant frame
    float invmax = rsqrt( max( dot(T,T), dot(B,B) ) );
    return float3x3( T * invmax, B * invmax, N );
}

// from https://graphics.pixar.com/library/OrthonormalB/paper.pdf
void ComputeOrthonormalBasis( float3 n, out float3 b1, out float3 b2 )
{
    float sign = (n.z >= 0.0) * 2.0 - 1.0; //copysignf(1.0f, n.z); <- any better way to do this?
    const float a = -1.0 / (sign + n.z);
    const float b = n.x * n.y * a;
    b1 = float3(1.0f + sign * n.x * n.x * a, sign * b, -sign * n.x);
    b2 = float3(b, sign + n.y * n.y * a, -n.y);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#endif // #ifdef VA_RENDERINGSHARED_HLSL_INCLUDED