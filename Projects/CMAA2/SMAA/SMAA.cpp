/**
 * Copyright (C) 2013 Jorge Jimenez (jorge@iryoku.com)
 * Copyright (C) 2013 Jose I. Echevarria (joseignacioechevarria@gmail.com)
 * Copyright (C) 2013 Belen Masia (bmasia@unizar.es)
 * Copyright (C) 2013 Fernando Navarro (fernandn@microsoft.com)
 * Copyright (C) 2013 Diego Gutierrez (diegog@unizar.es)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to
 * do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software. As clarification, there
 * is no requirement that the copyright notice and permission be included in
 * binary distributions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <cassert>
#include <sstream>
#include <stdexcept>

//#include "Rendering/DirectX/vaDirectXIncludes.h"
// #include <d3d10_1.h>
// #include <d3d9.h>

#include "AreaTex.h"
#include "SearchTex.h"
#include "SMAA.h"
using namespace std;

using namespace VertexAsylum;


// This define is for using the precomputed textures DDS files instead of the
// headers:
#define SMAA_USE_DDS_PRECOMPUTED_TEXTURES 0

// This define is for compressing the precomputed textures:
// (Non-perceptible quality decrease, marginal performance increase)
#if SMAA_USE_DDS_PRECOMPUTED_TEXTURES
#define SMAA_ENABLE_COMPRESSION 0
#endif

#pragma region Useful Macros from DXUT (copy-pasted here as we prefer this to be as self-contained as possible)
#if defined(DEBUG) || defined(_DEBUG)
#ifndef V
#define V(x) { hr = (x); if (FAILED(hr)) { DXTrace(__FILE__, (DWORD)__LINE__, hr, L#x, true); } }
#endif
#ifndef V_RETURN
#define V_RETURN(x) { hr = (x); if (FAILED(hr)) { return DXTrace(__FILE__, (DWORD)__LINE__, hr, L#x, true); } }
#endif
#else
#ifndef V
#define V(x) { hr = (x); }
#endif
#ifndef V_RETURN
#define V_RETURN(x) { hr = (x); if( FAILED(hr) ) { return hr; } }
#endif
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(p) { if (p) { delete (p); (p) = nullptr; } }
#endif
#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if (p) { delete[] (p); (p) = nullptr; } }
#endif
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = nullptr; } }
#endif
#pragma endregion

// #pragma region This stuff is for loading headers from resources
// class ID3D10IncludeResource : public ID3D10Include {
//     public:
//         STDMETHOD(Open)(THIS_ D3D_INCLUDE_TYPE, LPCSTR pFileName, LPCVOID, LPCVOID *ppData, UINT *pBytes)  {
//             wstringstream s;
//             s << pFileName;
//             HRSRC src = FindResource(GetModuleHandle(nullptr), s.str().c_str(), RT_RCDATA);
//             HGLOBAL res = LoadResource(GetModuleHandle(nullptr), src);
// 
//             *pBytes = SizeofResource(GetModuleHandle(nullptr), src);
//             *ppData = (LPCVOID) LockResource(res);
// 
//             return S_OK;
//         }
// 
//         STDMETHOD(Close)(THIS_ LPCVOID)  {
//             return S_OK;
//         }
// };
// #pragma endregion


SMAA::SMAA(ID3D11Device *device, SMAAShaderConstantsInterface * shaderConstantsInterface, SMAATexturesInterface * texturesInterface, SMAATechniqueManagerInterface * techniqueManagerInterface, int width, int height, Preset preset, bool predication, bool reprojection, const DXGI_ADAPTER_DESC *adapterDesc, const ExternalStorage &storage)
        : device(device),
          shaderConstantsInterface(shaderConstantsInterface), texturesInterface(texturesInterface), techniqueManagerInterface(techniqueManagerInterface), 
          width(width),
          height(height),
          preset(preset),
          threshold(0.1f),
          cornerRounding(0.25f),
          maxSearchSteps(16),
          maxSearchStepsDiag(8),
          frameIndex(0) 
{
    //HRESULT hr;
    // // Check for DirectX 10.1 support:
    // ID3D10Device1 *device1;
    // bool dx10_1 = false;
    // if (D3DX10GetFeatureLevel1(device, &device1) != E_FAIL) {
    //     dx10_1 = device1->GetFeatureLevel() == D3D10_FEATURE_LEVEL_10_1;
    //     SAFE_RELEASE(device1);
    // }

    // Setup the defines for compiling the effect:
    std::vector<D3D_SHADER_MACRO> defines;
    stringstream s;
    
    // Setup pixel size macro:
    s << "float4(1.0 / " << width << ", 1.0 / " << height << ", " << width << ", " << height << ")";
    string pixelSizeText = s.str();
    D3D_SHADER_MACRO renderTargetMetricsMacro = { "SMAA_RT_METRICS", pixelSizeText.c_str() };
    defines.push_back(renderTargetMetricsMacro);

    // Setup the preset macro:
    D3D_SHADER_MACRO presetMacros[] = {
        { "SMAA_PRESET_LOW", nullptr },
        { "SMAA_PRESET_MEDIUM", nullptr },
        { "SMAA_PRESET_HIGH", nullptr },
        { "SMAA_PRESET_ULTRA", nullptr },
        { "SMAA_PRESET_CUSTOM", nullptr }
    };
    defines.push_back(presetMacros[int(preset)]);

    // Setup the predicated thresholding macro:
    if (predication) {
        D3D_SHADER_MACRO predicationMacro = { "SMAA_PREDICATION", "1" };
        defines.push_back(predicationMacro);
    }

    // Setup the reprojection macro:
    if (reprojection) {
        D3D_SHADER_MACRO reprojectionMacro = { "SMAA_REPROJECTION", "1" };
        defines.push_back(reprojectionMacro);
    }

    // Setup the target macro:
    //if (dx10_1) {
        D3D_SHADER_MACRO dx101Macro = { "SMAA_HLSL_4_1", "1" };
    //    defines.push_back(dx101Macro);
    //}

    D3D_SHADER_MACRO null = { nullptr, nullptr };
    defines.push_back(null);

    // UINT flags = D3D10_SHADER_ENABLE_STRICTNESS;
    // #if defined(DEBUG) || defined(_DEBUG)
    // flags |= D3D10_SHADER_DEBUG;
    // flags |= D3D10_SHADER_SKIP_OPTIMIZATION;
    // #endif

    /**
     * If your debugger is breaking here is because you have not included the
     * shader as resource (other option is to load it FromFile).
     */
    // ID3D10IncludeResource includeResource;
    // string profile = dx10_1? "fx_4_1" : "fx_4_0";
    // V(D3DX10CreateEffectFromResource(GetModuleHandle(nullptr), L"SMAA.fx", nullptr, &defines.front(), &includeResource, profile.c_str(), flags, 0, device, nullptr, nullptr, &effect, nullptr, nullptr));

    // This is for rendering the typical fullscreen quad later on:
    // D3D10_PASS_DESC desc;
    // V(effect->GetTechniqueByName("NeighborhoodBlending")->GetPassByIndex(0)->GetDesc(&desc));
    triangle = new FullscreenTriangle(device);

    // In NVIDIA cards R8G8 is slower, avoid it:
    bool isNVIDIACard = adapterDesc? adapterDesc->VendorId == 0x10DE : false;
    DXGI_FORMAT format = isNVIDIACard? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R8G8_UNORM;

    // If storage for the edges is not specified we will create it:
    if (storage.edgesRTV != nullptr && storage.edgesSRV != nullptr)
        edgesRT = new RenderTarget(device, storage.edgesRTV, storage.edgesSRV);
    else
        edgesRT = new RenderTarget(device, width, height, format);

    // Same for blending weights:
    if (storage.weightsRTV != nullptr && storage.weightsSRV != nullptr)
        blendRT = new RenderTarget(device, storage.weightsRTV, storage.weightsSRV);
    else
        blendRT = new RenderTarget(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM);

    // Load the pre-computed textures:
    loadAreaTex();
    loadSearchTex();

    // // Create some handles for variables:
    // thresholdVariable = effect->GetVariableByName("threshld")->AsScalar();
    // cornerRoundingVariable = effect->GetVariableByName("cornerRounding")->AsScalar();
    // maxSearchStepsVariable = effect->GetVariableByName("maxSearchSteps")->AsScalar();
    // maxSearchStepsDiagVariable = effect->GetVariableByName("maxSearchStepsDiag")->AsScalar();
    // blendFactorVariable = effect->GetVariableByName("blendFactor")->AsScalar();
    // subsampleIndicesVariable = effect->GetVariableByName("subsampleIndices")->AsVector();
    // areaTexVariable = effect->GetVariableByName("areaTex")->AsShaderResource();
    // searchTexVariable = effect->GetVariableByName("searchTex")->AsShaderResource();
    // colorTexVariable = effect->GetVariableByName("colorTex")->AsShaderResource();
    // colorTexGammaVariable = effect->GetVariableByName("colorTexGamma")->AsShaderResource();
    // colorTexPrevVariable = effect->GetVariableByName("colorTexPrev")->AsShaderResource();
    // colorTexMSVariable = effect->GetVariableByName("colorTexMS")->AsShaderResource();
    // depthTexVariable = effect->GetVariableByName("depthTex")->AsShaderResource();
    // velocityTexVariable = effect->GetVariableByName("velocityTex")->AsShaderResource();
    // edgesTexVariable = effect->GetVariableByName("edgesTex")->AsShaderResource();
    // blendTexVariable = effect->GetVariableByName("blendTex")->AsShaderResource();

    // Create handles for techniques:
    //edgeDetectionTechniques[0] = effect->GetTechniqueByName("LumaEdgeDetection");
    //edgeDetectionTechniques[1] = effect->GetTechniqueByName("ColorEdgeDetection");
    //edgeDetectionTechniques[2] = effect->GetTechniqueByName("DepthEdgeDetection");
    //blendingWeightCalculationTechnique = effect->GetTechniqueByName("BlendingWeightCalculation");
    //neighborhoodBlendingTechnique = effect->GetTechniqueByName("NeighborhoodBlending");
    //resolveTechnique = effect->GetTechniqueByName("Resolve");
    //separateTechnique = effect->GetTechniqueByName("Separate");

    edgeDetectionTechniques[SMAA::INPUT_LUMA]       = techniqueManagerInterface->CreateTechnique("LumaEdgeDetection", defines);
    edgeDetectionTechniques[SMAA::INPUT_LUMA_RAW]   = techniqueManagerInterface->CreateTechnique("LumaRawEdgeDetection", defines);
    edgeDetectionTechniques[SMAA::INPUT_COLOR]      = techniqueManagerInterface->CreateTechnique("ColorEdgeDetection", defines);
    edgeDetectionTechniques[SMAA::INPUT_DEPTH]      = techniqueManagerInterface->CreateTechnique("DepthEdgeDetection", defines);
    blendingWeightCalculationTechnique = techniqueManagerInterface->CreateTechnique("BlendingWeightCalculation", defines);
    neighborhoodBlendingTechnique = techniqueManagerInterface->CreateTechnique("NeighborhoodBlending", defines);
    resolveTechnique = techniqueManagerInterface->CreateTechnique("Resolve", defines);
    separateTechnique = techniqueManagerInterface->CreateTechnique("Separate", defines);

    // ACTUAL SHADER CODE IS IN vaSMAAWrapperDX11::CreateTechnique(...)
    msaaOrderRenderTechnique   = techniqueManagerInterface->CreateTechnique( "detectMSAAOrder_Render", defines );
    msaaOrderLoadTechnique     = techniqueManagerInterface->CreateTechnique( "detectMSAAOrder_Load", defines );
}


SMAA::~SMAA() {
    //SAFE_RELEASE(effect);
    SAFE_DELETE(triangle);
    SAFE_DELETE(edgesRT);
    SAFE_DELETE(blendRT);
    SAFE_RELEASE(areaTex);
    SAFE_RELEASE(areaTexSRV);
    SAFE_RELEASE(searchTex);
    SAFE_RELEASE(searchTexSRV);
    techniqueManagerInterface->DestroyAllTechniques( );
}


void SMAA::go(ID3D11DeviceContext * context,
              ID3D11ShaderResourceView *srcGammaSRV,
              ID3D11ShaderResourceView *srcSRV,
              ID3D11ShaderResourceView *depthSRV,
              ID3D11ShaderResourceView *velocitySRV,
              ID3D11RenderTargetView *dstRTV,
              ID3D11DepthStencilView *dsv,
              Input input,
              Mode mode,
              int pass) {
//    HRESULT hr;

    if( !orderDetected )
    {
        // Detect MSAA 2x subsample order:
        detectMSAAOrder( context );
        orderDetected = true;
    }

    // PerfEventScope perfEvent(L"SMAA: Morphological Antialiasing");

    assert(((mode == MODE_SMAA_1X  || mode == MODE_SMAA_T2X) &&  pass == 0) ||
           ((mode == MODE_SMAA_S2X || mode == MODE_SMAA_4X)  && (pass == 0  || pass == 1)));

    // Save the state:
    SaveViewportsScope saveViewport(context);
    SaveRenderTargetsScope saveRenderTargets(context);
    SaveInputLayoutScope saveInputLayout(context);
    SaveBlendStateScope saveBlendState(context);
    SaveDepthStencilScope saveDepthStencil(context);

    // Reset the render target:
    context->OMSetRenderTargets(0, nullptr, nullptr);

    // Setup the viewport and the vertex layout:
    edgesRT->setViewport(context);
    
    //triangle->setInputLayout();
    // assert( false ); // check if you're setting the input layout correctly!

    // Clear render targets:
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    context->ClearRenderTargetView(*edgesRT, clearColor);
    context->ClearRenderTargetView(*blendRT, clearColor);

    // Get the subsample index:
    int subsampleIndex = getSubsampleIndex(mode, pass);

    // Setup variables:
    // if (preset == PRESET_CUSTOM) {
    //     V(thresholdVariable->SetFloat(threshold));
    //     V(cornerRoundingVariable->SetFloat(cornerRounding));
    //     V(maxSearchStepsVariable->SetFloat(float(maxSearchSteps)));
    //     V(maxSearchStepsDiagVariable->SetFloat(float(maxSearchStepsDiag)));
    // }
    // V(blendFactorVariable->SetFloat(pass == 0? 1.0f : 0.5f));
    if (preset == PRESET_CUSTOM)
        shaderConstantsInterface->SetVariablesA( context, threshold, cornerRounding, (float)maxSearchSteps, (float)maxSearchStepsDiag, pass == 0? 1.0f : 0.5f );
    else
        shaderConstantsInterface->SetVariablesA( context, 0.0f, 0.0f, 0.0f, 0.0f, pass == 0? 1.0f : 0.5f );

    // V(edgesTexVariable->SetResource(*edgesRT));
    // V(blendTexVariable->SetResource(*blendRT));
    // V(areaTexVariable->SetResource(areaTexSRV));
    // V(searchTexVariable->SetResource(searchTexSRV));
    // 
    // V(colorTexGammaVariable->SetResource(srcGammaSRV));
    // V(colorTexVariable->SetResource(srcSRV));
    // V(depthTexVariable->SetResource(depthSRV));
    // V(velocityTexVariable->SetResource(velocitySRV));

    texturesInterface->SetResource_areaTex(context, areaTexSRV);
    texturesInterface->SetResource_searchTex(context, searchTexSRV);
    texturesInterface->SetResource_colorTexGamma(context, srcGammaSRV);
    texturesInterface->SetResource_colorTex(context, srcSRV);
    texturesInterface->SetResource_depthTex(context, depthSRV);
    texturesInterface->SetResource_velocityTex(context, velocitySRV);

    // And here we go!
    edgesDetectionPass(context, dsv, input);
    texturesInterface->SetResource_edgesTex(context, *edgesRT);
    blendingWeightsCalculationPass(context, dsv, mode, subsampleIndex);
    texturesInterface->SetResource_blendTex(context, *blendRT);
    neighborhoodBlendingPass(context, dstRTV, dsv);

    // Reset external inputs, to avoid warnings:
    // V(colorTexGammaVariable->SetResource(nullptr));
    // V(colorTexVariable->SetResource(nullptr));
    // V(depthTexVariable->SetResource(nullptr));
    // V(velocityTexVariable->SetResource(nullptr));
    texturesInterface->SetResource_colorTexGamma( context, nullptr );
    texturesInterface->SetResource_colorTex( context, nullptr );
    texturesInterface->SetResource_depthTex( context, nullptr );
    texturesInterface->SetResource_velocityTex( context, nullptr );
    texturesInterface->SetResource_edgesTex( context, nullptr );
    texturesInterface->SetResource_blendTex( context, nullptr );

    // V(resolveTechnique->GetPassByIndex(0)->Apply(0));
    resolveTechnique->ApplyStates(context);
}


void SMAA::reproject(ID3D11DeviceContext * context,
                     ID3D11ShaderResourceView *currentSRV,
                     ID3D11ShaderResourceView *previousSRV,
                     ID3D11ShaderResourceView *velocitySRV,
                     ID3D11RenderTargetView *dstRTV) {
    assert( false ); // not ported yet
    context; currentSRV; previousSRV; velocitySRV; dstRTV;
                         
#if 0
    HRESULT hr;

    PerfEventScope perfEvent(L"SMAA: Temporal Antialiasing");

    // Save the state:
    SaveViewportsScope saveViewport(device);
    SaveRenderTargetsScope saveRenderTargets(device);
    SaveInputLayoutScope saveInputLayout(device);
    SaveBlendStateScope saveBlendState(device);
    SaveDepthStencilScope saveDepthStencil(device);

    // Setup the viewport and the vertex layout:
    edgesRT->setViewport();
    triangle->setInputLayout();

    // Setup variables:
    V(colorTexVariable->SetResource(currentSRV));
    V(colorTexPrevVariable->SetResource(previousSRV));
    V(velocityTexVariable->SetResource(velocitySRV));

    // Select the technique accordingly:
    V(resolveTechnique->GetPassByIndex(0)->Apply(0));

    // Do it!
    device->OMSetRenderTargets(1, &dstRTV, nullptr);
    triangle->draw();
    device->OMSetRenderTargets(0, nullptr, nullptr);

    // Reset external inputs, to avoid warnings:
    V(colorTexVariable->SetResource(nullptr));
    V(colorTexPrevVariable->SetResource(nullptr));
    V(velocityTexVariable->SetResource(nullptr));
    V(resolveTechnique->GetPassByIndex(0)->Apply(0));
#endif
}


void SMAA::separate(ID3D11DeviceContext * context,
                    ID3D11ShaderResourceView *srcSRV,
                    ID3D11RenderTargetView *dst1RTV,
                    ID3D11RenderTargetView *dst2RTV) {
    assert( false ); // not ported yet
    context; srcSRV; dst1RTV; dst2RTV;
#if 0
    HRESULT hr;

    PerfEventScope perfEvent(L"SMAA: Separate MSAA Samples Pass");

    // Save the state:
    SaveViewportsScope saveViewport(device);
    SaveRenderTargetsScope saveRenderTargets(device);
    SaveInputLayoutScope saveInputLayout(device);
    SaveBlendStateScope saveBlendState(device);
    SaveDepthStencilScope saveDepthStencil(device);

    // Setup the viewport and the vertex layout:
    edgesRT->setViewport();
    triangle->setInputLayout();

    // Setup variables:
    V(colorTexMSVariable->SetResource(srcSRV));

    // Select the technique accordingly:
    V(separateTechnique->GetPassByIndex(0)->Apply(0));

    // Do it!
    ID3D11RenderTargetView *dst[] = { dst1RTV, dst2RTV };
    device->OMSetRenderTargets(2, dst, nullptr);
    triangle->draw();
    device->OMSetRenderTargets(0, nullptr, nullptr);
#endif
}


void SMAA::loadAreaTex() {
    #if SMAA_USE_DDS_PRECOMPUTED_TEXTURES
    areaTex = nullptr;
    HRESULT hr;
    D3DX10_IMAGE_LOAD_INFO info = D3DX10_IMAGE_LOAD_INFO();
    info.MipLevels = 1;
    #if SMAA_ENABLE_COMPRESSION
    info.Format = DXGI_FORMAT_BC5_UNORM;
    #else
    info.Format = DXGI_FORMAT_R8G8_UNORM;
    #endif
    V(D3DX10CreateShaderResourceViewFromResource(device, GetModuleHandle(nullptr), L"AreaTexDX10.dds", &info, nullptr, &areaTexSRV, nullptr));
    #else
    HRESULT hr;

    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = areaTexBytes;
    data.SysMemPitch = AREATEX_PITCH;
    data.SysMemSlicePitch = 0;

    D3D11_TEXTURE2D_DESC descTex;
    descTex.Width = AREATEX_WIDTH;
    descTex.Height = AREATEX_HEIGHT;
    descTex.MipLevels = descTex.ArraySize = 1;
    descTex.Format = DXGI_FORMAT_R8G8_UNORM;
    descTex.SampleDesc.Count = 1;
    descTex.SampleDesc.Quality = 0;
    descTex.Usage = D3D11_USAGE_DEFAULT;
    descTex.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    descTex.CPUAccessFlags = 0;
    descTex.MiscFlags = 0;
    V(device->CreateTexture2D(&descTex, &data, &areaTex));

    D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
    descSRV.Format = descTex.Format;
    descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    descSRV.Texture2D.MostDetailedMip = 0;
    descSRV.Texture2D.MipLevels = 1;
    V(device->CreateShaderResourceView(areaTex, &descSRV, &areaTexSRV));
    #endif
}


void SMAA::loadSearchTex() {
    #if SMAA_USE_DDS_PRECOMPUTED_TEXTURES
    searchTex = nullptr;
    HRESULT hr;
    D3DX10_IMAGE_LOAD_INFO info = D3DX10_IMAGE_LOAD_INFO();
    info.MipLevels = 1;
    #if SMAA_ENABLE_COMPRESSION
    info.Format = DXGI_FORMAT_BC4_UNORM;
    #else
    info.Format = DXGI_FORMAT_R8_UNORM;
    #endif
    V(D3DX10CreateShaderResourceViewFromResource(device, GetModuleHandle(nullptr), L"SearchTex.dds", &info, nullptr, &searchTexSRV, nullptr));
    #else
    HRESULT hr;

    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = searchTexBytes;
    data.SysMemPitch = SEARCHTEX_PITCH;
    data.SysMemSlicePitch = 0;

    D3D11_TEXTURE2D_DESC descTex;
    descTex.Width = SEARCHTEX_WIDTH;
    descTex.Height = SEARCHTEX_HEIGHT;
    descTex.MipLevels = descTex.ArraySize = 1;
    descTex.Format = DXGI_FORMAT_R8_UNORM;
    descTex.SampleDesc.Count = 1;
    descTex.SampleDesc.Quality = 0;
    descTex.Usage = D3D11_USAGE_DEFAULT;
    descTex.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    descTex.CPUAccessFlags = 0;
    descTex.MiscFlags = 0;
    V(device->CreateTexture2D(&descTex, &data, &searchTex));

    D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
    descSRV.Format = descTex.Format;
    descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    descSRV.Texture2D.MostDetailedMip = 0;
    descSRV.Texture2D.MipLevels = 1;
    V(device->CreateShaderResourceView(searchTex, &descSRV, &searchTexSRV));
    #endif
}


void SMAA::edgesDetectionPass(ID3D11DeviceContext * context, ID3D11DepthStencilView *dsv, Input input) {
    //HRESULT hr;

    //PerfEventScope perfEvent(L"SMAA: Edge Detection Pass");

    // Select the technique accordingly:
    //V(edgeDetectionTechniques[int(input)]->GetPassByIndex(0)->Apply(0));
    edgeDetectionTechniques[int(input)]->ApplyStates(context);

    // Do it!
    context->OMSetRenderTargets(1, *edgesRT, dsv);
    triangle->draw(context);
    context->OMSetRenderTargets(0, nullptr, nullptr);
}


void SMAA::blendingWeightsCalculationPass(ID3D11DeviceContext * context, ID3D11DepthStencilView *dsv, Mode mode, int subsampleIndex) {
    //HRESULT hr;

    //PerfEventScope perfEvent(L"SMAA: Blending Weights Calculation Pass");

    /**
     * Orthogonal indices:
     *     [0]:  0.0
     *     [1]: -0.25
     *     [2]:  0.25
     *     [3]: -0.125
     *     [4]:  0.125
     *     [5]: -0.375
     *     [6]:  0.375
     *
     * Diagonal indices:
     *     [0]:  0.00,   0.00
     *     [1]:  0.25,  -0.25
     *     [2]: -0.25,   0.25
     *     [3]:  0.125, -0.125
     *     [4]: -0.125,  0.125
     *
     * Indices layout: indices[4] = { |, --,  /, \ }
     */

    switch (mode) {
        case MODE_SMAA_1X: {
            float indices[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            //V(subsampleIndicesVariable->SetFloatVector(indices));
            shaderConstantsInterface->SetVariablesB( context, indices );
            break;
        }
        case MODE_SMAA_T2X:
        case MODE_SMAA_S2X: {
            /***
             * Sample positions (bottom-to-top y axis):
             *   _______
             *  | S1    |  S0:  0.25    -0.25
             *  |       |  S1: -0.25     0.25
             *  |____S0_|
             */
              float indices[][4] = {
                { 1.0f, 1.0f, 1.0f, 0.0f }, // S0
                { 2.0f, 2.0f, 2.0f, 0.0f }  // S1
                // (it's 1 for the horizontal slot of S0 because horizontal
                //  blending is reversed: positive numbers point to the right)
            };
            //V(subsampleIndicesVariable->SetFloatVector(indices[subsampleIndex]));
            shaderConstantsInterface->SetVariablesB( context, indices[subsampleIndex] );
            break;
        }
        case MODE_SMAA_4X: {
            /***
             * Sample positions (bottom-to-top y axis):
             *   ________
             *  |  S1    |  S0:  0.3750   -0.1250
             *  |      S0|  S1: -0.1250    0.3750
             *  |S3      |  S2:  0.1250   -0.3750
             *  |____S2__|  S3: -0.3750    0.1250
             */
            float indices[][4] = {
                { 5.0f, 3.0f, 1.0f, 3.0f }, // S0
                { 4.0f, 6.0f, 2.0f, 3.0f }, // S1
                { 3.0f, 5.0f, 1.0f, 4.0f }, // S2
                { 6.0f, 4.0f, 2.0f, 4.0f }  // S3
            };
            //V(subsampleIndicesVariable->SetFloatVector(indices[subsampleIndex]));
            shaderConstantsInterface->SetVariablesB( context, indices[subsampleIndex] );
            break;
        }
    }

    // Setup the technique (again):
    // V(blendingWeightCalculationTechnique->GetPassByIndex(0)->Apply(0));
    blendingWeightCalculationTechnique->ApplyStates( context );

    // And here we go!
    context->OMSetRenderTargets(1, *blendRT, dsv);
    triangle->draw( context );
    context->OMSetRenderTargets(0, nullptr, nullptr);
}


void SMAA::neighborhoodBlendingPass(ID3D11DeviceContext * context, ID3D11RenderTargetView *dstRTV, ID3D11DepthStencilView *dsv) {
    //HRESULT hr;

    // PerfEventScope perfEvent(L"SMAA: Neighborhood Blending Pass");

    // Setup the technique (once again):
    // V(neighborhoodBlendingTechnique->GetPassByIndex(0)->Apply(0));
    neighborhoodBlendingTechnique->ApplyStates( context );
    
    // Do the final pass!
    context->OMSetRenderTargets(1, &dstRTV, dsv);
    triangle->draw( context );
    context->OMSetRenderTargets(0, nullptr, nullptr);
}


// D3DXMATRIX SMAA::JitteredMatrix(const D3DXMATRIX &worldViewProjection, Mode mode) const {
//     D3DXVECTOR2 jitter = getJitter(mode);
//     D3DXMATRIX translationMatrix;
//     D3DXMatrixTranslation(&translationMatrix, 2.0f * jitter.x / float(width), 2.0f * jitter.y / float(height), 0.0f);
//     return worldViewProjection * translationMatrix;
// }


void SMAA::nextFrame() {
    frameIndex = (frameIndex + 1) % 2;
}


void SMAA::detectMSAAOrder( ID3D11DeviceContext * context ) {
    // not sure this is needed as we could just use D3D11_STANDARD_MULTISAMPLE_PATTERN but let's leave it in here for now
#if 0
    BYTE value = 0;
    msaaOrderMap[0] = int(value == 255);
    msaaOrderMap[1] = int(value != 255);
#else
    HRESULT hr;

    // // Create the effect:
    // string s = "float4 RenderVS(float4 pos : POSITION,    inout float2 coord : TEXCOORD0) : SV_POSITION { pos.x = -0.5 + 0.5 * pos.x; return pos; }"
    //            "float4 RenderPS(float4 pos : SV_POSITION,       float2 coord : TEXCOORD0) : SV_TARGET   { return 1.0; }"
    //            "DepthStencilState DisableDepthStencil { DepthEnable = FALSE; StencilEnable = FALSE; };"
    //            "BlendState NoBlending { AlphaToCoverageEnable = FALSE; BlendEnable[0] = FALSE; };"
    //            "technique10 Render { pass Render {"
    //            "SetVertexShader(CompileShader(vs_4_0, RenderVS())); SetGeometryShader(NULL); SetPixelShader(CompileShader(ps_4_0, RenderPS()));"
    //            "SetDepthStencilState(DisableDepthStencil, 0);"
    //            "SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);"
    //            "}}"
    //            "Texture2DMS<float4, 2> texMS;"
    //            "float4 LoadVS(float4 pos : POSITION,    inout float2 coord : TEXCOORD0) : SV_POSITION { return pos; }"
    //            "float4 LoadPS(float4 pos : SV_POSITION,       float2 coord : TEXCOORD0) : SV_TARGET   { int2 ipos = int2(pos.xy); return texMS.Load(ipos, 0); }"
    //            "technique10 Load { pass Load {"
    //            "SetVertexShader(CompileShader(vs_4_0, LoadVS())); SetGeometryShader(NULL); SetPixelShader(CompileShader(ps_4_0, LoadPS()));"
    //            "SetDepthStencilState(DisableDepthStencil, 0);"
    //            "SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);"
    //            "}}";

    std::vector<D3D_SHADER_MACRO> defines;    defines.push_back( { nullptr, nullptr } );

    // Create the buffers:
    DXGI_SAMPLE_DESC sampleDesc = { 2, 0 };
    RenderTarget *renderTargetMS = new RenderTarget(device, 1, 1, DXGI_FORMAT_R8_UNORM, sampleDesc);
    RenderTarget *renderTarget = new RenderTarget(device, 1, 1, DXGI_FORMAT_R8_UNORM);
    ID3D11Texture2D *stagingTexture = Utils::createStagingTexture(device, *renderTarget);

    // Create a quad:
    //D3D10_PASS_DESC desc;
    //V(effect->GetTechniqueByName("Render")->GetPassByIndex(0)->GetDesc(&desc));
    Quad *quad = new Quad(device);

    // Save DX state:
    SaveViewportsScope saveViewport(context);
    SaveRenderTargetsScope saveRenderTargets(context);
    SaveInputLayoutScope saveInputLayout(context);

    // Set viewport and input layout:
    renderTargetMS->setViewport(context);
    //quad->setInputLayout();

    // Render a quad that fills the left half of a 1x1 buffer:
    //V(effect->GetTechniqueByName("Render")->GetPassByIndex(0)->Apply(0));
    msaaOrderRenderTechnique->ApplyStates(context);
    context->OMSetRenderTargets(1, *renderTargetMS, nullptr);
    quad->draw(context);
    context->OMSetRenderTargets(0, nullptr, nullptr);

    // Load the sample 0 from previous 1x1 buffer:
    //V(effect->GetVariableByName("texMS")->AsShaderResource()->SetResource(*renderTargetMS));
    texturesInterface->SetResource_colorTexMS( context, *renderTargetMS );
    //V(effect->GetTechniqueByName("Load")->GetPassByIndex(0)->Apply(0));
    msaaOrderLoadTechnique->ApplyStates(context);
    context->OMSetRenderTargets(1, *renderTarget, nullptr);
    quad->draw(context);
    context->OMSetRenderTargets(0, nullptr, nullptr);
    texturesInterface->SetResource_colorTexMS( context, nullptr );
    
    // Copy the sample #0 into CPU memory:
    context->CopyResource(stagingTexture, *renderTarget);
    D3D11_MAPPED_SUBRESOURCE map;
    V( context->Map( stagingTexture, 0, D3D11_MAP_READ, 0, &map) );
    BYTE value = ((char *) map.pData)[0];
    context->Unmap( stagingTexture, 0 );

    // Set the map indices:
    msaaOrderMap[0] = int(value == 255);
    msaaOrderMap[1] = int(value != 255);

    // Release memory
    //SAFE_RELEASE(effect);
    SAFE_DELETE(renderTargetMS);
    SAFE_DELETE(renderTarget);
    SAFE_RELEASE(stagingTexture);
    SAFE_DELETE(quad);
#endif
}


D3DXVECTOR2 SMAA::getJitter(Mode mode) const {
    switch (mode) {
        case SMAA::MODE_SMAA_1X:
        case SMAA::MODE_SMAA_S2X:
            return D3DXVECTOR2(0.0f, 0.0f);
        case SMAA::MODE_SMAA_T2X: {
            D3DXVECTOR2 jitters[] = {
                D3DXVECTOR2(-0.25f,  0.25f),
                D3DXVECTOR2( 0.25f, -0.25f)
            };
            return jitters[frameIndex];
        }
        case SMAA::MODE_SMAA_4X: {
            D3DXVECTOR2 jitters[] = {
                D3DXVECTOR2(-0.125f, -0.125f),
                D3DXVECTOR2( 0.125f,  0.125f)
            };
            return jitters[frameIndex];
        }
        default:
            throw logic_error("unexpected problem");
    }
}


int SMAA::getSubsampleIndex(Mode mode, int pass) const {
    switch (mode) {
        case SMAA::MODE_SMAA_1X:
            return 0;
        case SMAA::MODE_SMAA_T2X:
            return frameIndex;
        case SMAA::MODE_SMAA_S2X:
            return msaaReorder(pass);
        case SMAA::MODE_SMAA_4X:
            return 2 * frameIndex + msaaReorder(pass);
        default:
            throw logic_error("unexpected problem");
    }
}

