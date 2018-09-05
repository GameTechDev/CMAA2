# Conservative Morphological Anti-Aliasing version 2 (CMAA2)
 
This repository contains implementation of CMAA2, a post-process anti-aliasing solution focused on providing good anti-aliasing while minimizing the change (i.e. blurring) of the source image at minimal execution cost.

Details of the implementation as well as quality and performance analysis are provided in the accompanying article at https://software.intel.com/en-us/articles/conservative-morphological-anti-aliasing-20.

Sample code in this repository is a DirectX 11 Compute Shader 5.0 HLSL reference implementation optimized for modern PC GPU hardware. DirectX 12 and Vulkan ports are also in development.

## Sample overview
 
The sample application requires Windows 10 and Visual Studio 2017. Once built and started, the application will load the Amazon Lumberyard Bistro dataset (https://developer.nvidia.com/orca/amazon-lumberyard-bistro).

![Alt text](screenshot.jpg?raw=true "CMAA2 sample application")

UI on the left side provides the 'Scene' selection used to change the 3D scene or a to switch to using a static image as the input ('StaticImage' option). StaticImage option is useful for quickly testing CMAA2 or one of the other provided post-process AA effects on a screenshot captured from any external workload. 
To add your images to the StaticImage list simply copy the image file in the .png format to the \CMAA2\Projects\CMAA2\Media\TestScreenshots\ path and restart the application.

Below is the list of selectable anti-aliasing options ('AA option'). To quickly change between them for comparison, '1' - '9' keyboard keys can also be used.

For more detailed comparison, we provide 'ZoomTool'  which, when enabled, allows zooming on a specific region of pixels for closer inspection.

In addition we also provide 'CompareTool' which can be used to capture a reference image ('Save ref' button) and compare it to the currently rendered image either numerically ('Compare with ref' button provides PSNR between the two) or visually (using 'Visualization' combo box to show the difference).

Finally, tools for automated testing of quality and performance of the various AA options are available in the 'Benchmarking' section.

## Code integration guide

This integration guide covers basic steps needed for integration into an engine and relies on the sample code in this repository. Relevant source code files are CMAA2/CMAA2.hlsl and CMAA2/vaCMAA2DX11.cpp.

### Shaders

The effect uses 3 main compute shader kernels out of which the first one (**EdgesColor2x2CS**) is a standard Dispatch type and the second two (**ProcessCandidatesCS** and **DeferredColorApply2x2CS**) are DispatchIndirect type. There is also one helper kernel (**ComputeDispatchArgsCS**) used to set up indirect dispatch argument buffer.

All the shader code is contained in CMAA2/CMAA2.hlsl and various features and integration options are controlled using macros. The only macros without defaults are related to input/output color format and UAV typed store hardware capabilities and must be set up as explained in next section.

### Inputs/Outputs

CMAA2 takes the single color image and applies anti-aliasing in-place. There is no fullscreen copy, only the required texels are changed. It can also optionally take precomputed luma as input since this is usually available as a byproduct of tonemapping, which allows for a faster execution path (significantly reducing memory bandwidth during edge detection).
The input color Shader Resource View should be in the standard color format used for linear sampling (for ex, R8G8B8A8_UNORM_SRGB) and the UAV in the best matching format supported for typed UAV stores (for ex, R8G8B8A8_UNORM, explained below).

Due to lack of hardware UAV Typed Store format support for certain formats, we use CMAA2_UAV_STORE_TYPED, CMAA2_UAV_STORE_TYPED_UNORM_FLOAT and CMAA2_UAV_STORE_CONVERT_TO_SRGB to define the optimal path with regards to used color format and hardware support.
For example, if the application is using R8G8B8A8_UNORM_SRGB color format, and the hardware does not support UAV typed stores for _SRGB but does support typed stores for R8G8B8A8_UNORM (most common scenario), we will use the following settings:

```
#define CMAA2_UAV_STORE_TYPED               1   // use typed UAV store
#define CMAA2_UAV_STORE_CONVERT_TO_SRGB     1   // manually convert to SRGB so we can use non-SRGB typed store because typed stores for SRGB are not supported
#define CMAA2_UAV_STORE_TYPED_UNORM_FLOAT   1   // required for non-float semantics correctness (RWTexture2D<unorm float4>)
```

In another example, if the application is using R11G11B10_FLOAT, all modern hardware will support direct typed UAV stores in which case the settings are simply:

```
#define CMAA2_UAV_STORE_TYPED               1   // use typed UAV store
#define CMAA2_UAV_STORE_CONVERT_TO_SRGB     0   // no need to convert to SRGB - R11G11B10_FLOAT does not use SRGB encoding
#define CMAA2_UAV_STORE_TYPED_UNORM_FLOAT   0   // not required for non-float semantics correctness (RWTexture2D<float4>)
```

With regards to edge detection, CMAA2 can work in two modes. The first is based on weighted average of per-channel color differences (marginally better quality) and the second is based on the luma difference (faster, default).
These modes are controlled using **CMAA2_EDGE_DETECTION_LUMA_PATH** macro using the following settings:
 * **0** enables color-based edge detection path
 * **1** enables luma-based edge detection path where luma is computed from input colors inplace (simplest)
 * **2** enables luma-based edge detection path where precomputed luma is loaded from a separate R8_UNORM input texture (best performance, ideal)
 * **3** enables luma-based edge detection path where precomputed luma is loaded from alpha channel of the original input color texture (faster than 1 but slower than 2 and not an option for color formats with no alpha channel like R11G11B10_FLOAT)

 The detailed example of how to handle most format and detect hardware support is provided in vaCMAA2DX11.cpp from line 318.

### Temporary working buffers

CMAA2 requires a couple of working buffers for storing intermediate data. This data is only required during CMAA2 execution (there is no required persistence between CMAA2 invocations, so the memory can be used for other purposes if needed).

The amount of temporary storage memory required by default is roughly width * height * 5 bytes. This which can be reduced by processing the image in smaller tiles at the expense of complexity.

These storage buffers are:
 * **working edges texture**            : stores the edges output by EdgeColor2x2CS
 * **working shape candidates buffer**  : stores potential shapes for further processing, filled in EdgesColor2x2CS and read/used by ProcessCandidatesCS
 * **working deferred blend buffers**   : these 3 buffers store a linked list of per-pixel anti-aliased color values that are output by ProcessCandidatesCS and consumed and applied to the final texture by DeferredColorApply2x2CS

For the details on buffer creation, sizes and formats please refer to vaCMAA2DX11.cpp from line 395.

### Compute calls

The execution steps are:

 1. **EdgesColor2x2CS**:         takes color (or luma) texture SRV as the input and computes edges and starting points for further processing as well as clearing some of the temporary buffers
 2. **(ComputeDispatchArgsCS)**: computes the arguments for the next kernel's DispatchIndirect call
 3. **ProcessCandidatesCS**:     takes the output from EdgesColor2x2CS and the source color texture SRV and do all color processing on required location, storing final color results into temporary storage
 4. **(ComputeDispatchArgsCS)**: computes the arguments for the next kernel's DispatchIndirect call
 5. **DeferredColorApply2x2CS**: takes all output values from ProcessCandidatesCS and deterministically blends them back into the source (now output) texture using UAV

 For details on the draw call setup please refer to 'vaCMAA2DX11::Execute' in vaCMAADX11.cpp from line 536.

### Settings

The most relevant quality and performance settings are:
 * **CMAA2_STATIC_QUALITY_PRESET**: sets quality level; acceptable values are 0, 1, 2, 3 (defaults to 2) with 0 being lowest quality higher performance and 3 being highest quality lowest performance option.
 * **CMAA2_EXTRA_SHARPNESS**:       enable/disable (0 - off is default) 'extra sharp' path that further minimizes image changes at the cost of avoiding some anti-aliasing.

There are no dynamic quality settings in this implementation for simplicity & performance reasons; however the edge detection threshold can be changed at runtime if needed for more granular and dynamic quality/performance setup.

## Credits

This sample uses following code and libraries:
* Assimp https://github.com/assimp/assimp
* DirectXTex https://github.com/Microsoft/DirectXTex
* enkiTS https://github.com/dougbinks/enkiTS
* imgui https://github.com/ocornut/imgui
* Remotery https://github.com/Celtoys/Remotery
* tinyxml2 https://github.com/leethomason/tinyxml2
* zlib https://github.com/madler/zlib
* FXAA https://github.com/NVIDIAGameWorks/GraphicsSamples/tree/master/samples/es3-kepler/FXAA
* SMAA https://github.com/iryoku/smaa

## License

CMAA2 is licensed under Apache-2 License, see license.txt for more information.