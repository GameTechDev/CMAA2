///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019, Intel Corporation
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

#pragma once

#include "Core/System/vaMemoryStream.h"

#include "Rendering/DirectX/vaDirectXIncludes.h"

#include "Rendering/vaTexture.h"

#include "Core/Misc/vaResourceFormats.h"

#include <wrl/client.h>

namespace VertexAsylum
{
    class vaStream;
    class vaSimpleProfiler;

    //class vaDirectXNotifyTarget;

    class vaDirectXTools11 final
    {
        //enum TextureLoadFlags
        //{
        //   TLF_NoFlags                      = 0,
        //   //TLF_InputI
        //};

    public:
        // Helper DirectX functions
  //      static ID3D11Texture1D *              CreateTexture1D(    DXGI_FORMAT format, UINT width, D3D11_SUBRESOURCE_DATA * initialData = NULL, 
  //                                                                UINT arraySize = 1, UINT mipLevels = 0, UINT bindFlags = D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE usage = D3D11_USAGE_DEFAULT,
  //                                                                UINT cpuaccessFlags = 0, UINT miscFlags = 0 );
  //      static ID3D11Texture2D *              CreateTexture2D(    DXGI_FORMAT format, UINT width, UINT height, D3D11_SUBRESOURCE_DATA * initialData = NULL, 
  //                                                                UINT arraySize = 1, UINT mipLevels = 0, UINT bindFlags = D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE usage = D3D11_USAGE_DEFAULT,
  //                                                                UINT cpuaccessFlags = 0, UINT sampleCount = 1, UINT sampleQuality = 0, UINT miscFlags = 0 );
  //      static ID3D11Texture3D *              CreateTexture3D(    DXGI_FORMAT format, UINT width, UINT height, UINT depth, D3D11_SUBRESOURCE_DATA * initialData = NULL, UINT mipLevels = 0,
  //                                                                UINT bindFlags = D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE usage = D3D11_USAGE_DEFAULT, UINT cpuaccessFlags = 0, UINT miscFlags = 0 );

        static ID3D11ShaderResourceView *   CreateShaderResourceView( ID3D11Resource * texture, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN, int mipSliceMin = 0, int mipSliceCount = -1, int arraySliceMin = 0, int arraySliceCount = -1 );
        static ID3D11ShaderResourceView *   CreateShaderResourceViewCubemap( ID3D11Resource * texture, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN, int mipSliceMin = 0, int mipSliceCount = -1, int arraySliceMin = 0, int arraySliceCount = -1 );
        static ID3D11DepthStencilView *     CreateDepthStencilView( ID3D11Resource * texture, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN, int mipSliceMin = 0, int arraySliceMin = 0, int arraySliceCount = -1 );
        static ID3D11RenderTargetView *     CreateRenderTargetView( ID3D11Resource * texture, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN, int mipSliceMin = 0, int arraySliceMin = 0, int arraySliceCount = -1 );
        static ID3D11UnorderedAccessView *  CreateUnorderedAccessView( ID3D11Resource * texture, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN, int mipSliceMin = 0, int arraySliceMin = 0, int arraySliceCount = -1 );
        //static ID3D11Buffer *               CreateBuffer( uint32 sizeInBytes, uint32 bindFlags, D3D11_USAGE usage, uint32 cpuAccessFlags = 0, uint32 miscFlags = 0, uint32 structByteStride = 0, const void * initializeData = NULL );

        // Common depth/stencil states
        static ID3D11DepthStencilState *    GetDSS_DepthEnabledL_DepthWrite( );
        static ID3D11DepthStencilState *    GetDSS_DepthEnabledG_DepthWrite( );
        static ID3D11DepthStencilState *    GetDSS_DepthEnabledL_NoDepthWrite( );
        static ID3D11DepthStencilState *    GetDSS_DepthEnabledLE_NoDepthWrite( );
        static ID3D11DepthStencilState *    GetDSS_DepthEnabledG_NoDepthWrite( );
        static ID3D11DepthStencilState *    GetDSS_DepthEnabledGE_NoDepthWrite( );
        static ID3D11DepthStencilState *    GetDSS_DepthDisabled_NoDepthWrite( );
        static ID3D11DepthStencilState *    GetDSS_DepthPassAlways_DepthWrite( );

        static ID3D11DepthStencilState *    GetDSS_DepthDisabled_NoDepthWrite_StencilCreateMask( );
        static ID3D11DepthStencilState *    GetDSS_DepthDisabled_NoDepthWrite_StencilUseMask( );

        // Common rasterizer states
        static ID3D11RasterizerState *      GetRS_CullNone_Fill( );
        static ID3D11RasterizerState *      GetRS_CullCCW_Fill( );
        static ID3D11RasterizerState *      GetRS_CullCW_Fill( );
        static ID3D11RasterizerState *      GetRS_CullNone_Wireframe( );
        static ID3D11RasterizerState *      GetRS_CullCCW_Wireframe( );
        static ID3D11RasterizerState *      GetRS_CullCW_Wireframe( );

        // Common blend states
        static ID3D11BlendState *           GetBS_Opaque( );
        static ID3D11BlendState *           GetBS_Additive( );
        static ID3D11BlendState *           GetBS_AlphaBlend( );
        static ID3D11BlendState *           GetBS_PremultAlphaBlend( );
        static ID3D11BlendState *           GetBS_Mult( );

        // Common samplers
        static ID3D11SamplerState *         GetSamplerStatePointClamp( );
        static ID3D11SamplerState *         GetSamplerStatePointWrap( );
        static ID3D11SamplerState *         GetSamplerStatePointMirror( );
        static ID3D11SamplerState *         GetSamplerStateLinearClamp( );
        static ID3D11SamplerState *         GetSamplerStateLinearWrap( );
        static ID3D11SamplerState *         GetSamplerStateAnisotropicClamp( );
        static ID3D11SamplerState *         GetSamplerStateAnisotropicWrap( );
        static ID3D11SamplerState *         GetSamplerStateShadowCmp( );

        static ID3D11SamplerState * *       GetSamplerStatePtrPointClamp( );
        static ID3D11SamplerState * *       GetSamplerStatePtrPointWrap( );
        static ID3D11SamplerState * *       GetSamplerStatePtrLinearClamp( );
        static ID3D11SamplerState * *       GetSamplerStatePtrLinearWrap( );
        static ID3D11SamplerState * *       GetSamplerStatePtrAnisotropicClamp( );
        static ID3D11SamplerState * *       GetSamplerStatePtrAnisotropicWrap( );
        static ID3D11SamplerState * *       GetSamplerStatePtrShadowCmp( );

        // Helper state-related functions
        static ID3D11RasterizerState *      CreateRasterizerState( ID3D11Device * device, const D3D11_RASTERIZER_DESC & desc );

        static ID3D11RasterizerState *      FindOrCreateRasterizerState( ID3D11Device * device, const D3D11_RASTERIZER_DESC & desc );

        static ID3D11DepthStencilState *    FindOrCreateDepthStencilState( ID3D11Device * device, const D3D11_DEPTH_STENCIL_DESC & desc );

        // Helper basic rendering functions
        // static void                         ClearColorDepthStencil( ID3D11DeviceContext * destContext, bool clearAllColorRTs, bool clearDepth, bool clearStencil, const vaVector4 & clearColor, float depth, uint8 stencil );
        // static void                         CopyDepthStencil( ID3D11DeviceContext * destContext, ID3D11DepthStencilView * srcDSV );

        // Helper texture-related functions
        static int                          CalcApproxTextureSizeInMemory( DXGI_FORMAT format, int width, int height, int mipCount );
        static ID3D11Resource *             LoadTextureDDS( ID3D11Device * device, void * buffer, int64 bufferSize, vaTextureLoadFlags loadFlags, vaResourceBindSupportFlags bindFlags, uint64 * outCRC = NULL );
        static ID3D11Resource *             LoadTextureDDS( ID3D11Device * device, const wchar_t * path, vaTextureLoadFlags loadFlags, vaResourceBindSupportFlags bindFlags, uint64 * outCRC = NULL );
        static ID3D11Resource *             LoadTextureWIC( ID3D11Device * device, void * buffer, int64 bufferSize, vaTextureLoadFlags loadFlags, vaResourceBindSupportFlags bindFlags, uint64 * outCRC = NULL );
        static ID3D11Resource *             LoadTextureWIC( ID3D11Device * device, const wchar_t * path, vaTextureLoadFlags loadFlags, vaResourceBindSupportFlags bindFlags, uint64 * outCRC = NULL );

        // Misc

        // // Save any DirectX resource wrapped in our own simple format
        // static bool                         SaveResource( VertexAsylum::vaStream & outStream, ID3D11Resource * d3dResource );
        // static ID3D11Resource *             LoadResource( VertexAsylum::vaStream & inStream, uint64 * outCRC = NULL );

        static bool                           SaveDDSTexture( ID3D11Device * device, VertexAsylum::vaStream & outStream, ID3D11Resource* pSource );

        template<typename OutType>
        static inline OutType *               QueryResourceInterface( ID3D11Resource * d3dResource, REFIID riid );

        static void                           SetToD3DContextAllShaderTypes( ID3D11DeviceContext * context, ID3D11Buffer * buffer, int32 slot );
        static void                           AssertSetToD3DContextAllShaderTypes( ID3D11DeviceContext * context, ID3D11Buffer * buffer, int32 slot );

        static void                           SetToD3DContextAllShaderTypes( ID3D11DeviceContext * context, ID3D11SamplerState * sampler, int32 slot );
        static void                           SetToD3DContextAllShaderTypes( ID3D11DeviceContext * context, ID3D11SamplerState ** samplers, int32 slot, int32 count );
        static void                           AssertSetToD3DContextAllShaderTypes( ID3D11DeviceContext * context, ID3D11SamplerState * sampler, int32 slot );
        static void                           AssertSetToD3DContextAllShaderTypes( ID3D11DeviceContext * context, ID3D11SamplerState ** samplers, int32 slot, int32 count );

        static void                           SetToD3DContextAllShaderTypes( ID3D11DeviceContext * context, ID3D11ShaderResourceView * buffer, int32 slot );
        static void                           AssertSetToD3DContextAllShaderTypes( ID3D11DeviceContext * context, ID3D11ShaderResourceView * buffer, int32 slot );

        static void                           SetToD3DContextCS( ID3D11DeviceContext * context, ID3D11UnorderedAccessView * buffer, int32 slot, uint32 initialCount );
        static void                           AssertSetToD3DContextCS( ID3D11DeviceContext * context, ID3D11UnorderedAccessView * buffer, int32 slot );
    };



    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Inline definitions
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // vaDirectXTools11
    template<typename OutType>
    inline OutType * vaDirectXTools11::QueryResourceInterface( ID3D11Resource * d3dResource, REFIID riid )
    {
        OutType * ret = NULL;
        if( SUCCEEDED( d3dResource->QueryInterface( riid, (void**)&ret ) ) )
        {
            return ret;
        }
        else
        {
            return NULL;
        }
    }
    //
    inline void vaDirectXTools11::SetToD3DContextAllShaderTypes( ID3D11DeviceContext * context, ID3D11Buffer * buffer, int32 slot )
    {
        VA_ASSERT( slot >= 0, L"vaDirectXTools11::SetToD3DContextAllShaderTypes : If you've left the DefaultSlot template parameter at default then you have to specify a valid shader slot!" );

        context->PSSetConstantBuffers( (uint32)slot, 1, &buffer );
        context->CSSetConstantBuffers( (uint32)slot, 1, &buffer );
        context->VSSetConstantBuffers( (uint32)slot, 1, &buffer );
        context->DSSetConstantBuffers( (uint32)slot, 1, &buffer );
        context->GSSetConstantBuffers( (uint32)slot, 1, &buffer );
        context->HSSetConstantBuffers( (uint32)slot, 1, &buffer );
    }
    //
    inline void vaDirectXTools11::AssertSetToD3DContextAllShaderTypes( ID3D11DeviceContext * context, ID3D11Buffer * buffer, int32 slot )
    {
#ifdef _DEBUG
        ID3D11Buffer * _buffer = NULL;
        context->PSGetConstantBuffers( (uint32)slot, 1, &_buffer ); assert( buffer == _buffer ); SAFE_RELEASE( _buffer );
        context->CSGetConstantBuffers( (uint32)slot, 1, &_buffer ); assert( buffer == _buffer ); SAFE_RELEASE( _buffer );
        context->VSGetConstantBuffers( (uint32)slot, 1, &_buffer ); assert( buffer == _buffer ); SAFE_RELEASE( _buffer );
        context->DSGetConstantBuffers( (uint32)slot, 1, &_buffer ); assert( buffer == _buffer ); SAFE_RELEASE( _buffer );
        context->GSGetConstantBuffers( (uint32)slot, 1, &_buffer ); assert( buffer == _buffer ); SAFE_RELEASE( _buffer );
        context->HSGetConstantBuffers( (uint32)slot, 1, &_buffer ); assert( buffer == _buffer ); SAFE_RELEASE( _buffer );
#else
        context; buffer; slot;
#endif
    }
    //
    inline void vaDirectXTools11::SetToD3DContextAllShaderTypes( ID3D11DeviceContext * context, ID3D11SamplerState * sampler, int32 slot )
    {
        VA_ASSERT( slot >= 0, L"vaDirectXTools11::SetToD3DContextAllShaderTypes : If you've left the DefaultSlot template parameter at default then you have to specify a valid shader slot!" );

        context->PSSetSamplers( (uint32)slot, 1, &sampler );
        context->CSSetSamplers( (uint32)slot, 1, &sampler );
        context->VSSetSamplers( (uint32)slot, 1, &sampler );
        context->DSSetSamplers( (uint32)slot, 1, &sampler );
        context->GSSetSamplers( (uint32)slot, 1, &sampler );
        context->HSSetSamplers( (uint32)slot, 1, &sampler );
    }
    //
    inline void vaDirectXTools11::SetToD3DContextAllShaderTypes( ID3D11DeviceContext * context, ID3D11SamplerState ** samplers, int32 slot, int32 count )
    {
        VA_ASSERT( slot >= 0, L"vaDirectXTools11::SetToD3DContextAllShaderTypes : If you've left the DefaultSlot template parameter at default then you have to specify a valid shader slot!" );

        context->PSSetSamplers( (uint32)slot, count, samplers );
        context->CSSetSamplers( (uint32)slot, count, samplers );
        context->VSSetSamplers( (uint32)slot, count, samplers );
        context->DSSetSamplers( (uint32)slot, count, samplers );
        context->GSSetSamplers( (uint32)slot, count, samplers );
        context->HSSetSamplers( (uint32)slot, count, samplers );
    }
    //
    inline void vaDirectXTools11::AssertSetToD3DContextAllShaderTypes( ID3D11DeviceContext * context, ID3D11SamplerState * sampler, int32 slot )
    {
#ifdef _DEBUG
        ID3D11SamplerState * _sampler = NULL;
        context->PSGetSamplers( (uint32)slot, 1, &_sampler ); assert( sampler == _sampler ); SAFE_RELEASE( _sampler );
        context->CSGetSamplers( (uint32)slot, 1, &_sampler ); assert( sampler == _sampler ); SAFE_RELEASE( _sampler );
        context->VSGetSamplers( (uint32)slot, 1, &_sampler ); assert( sampler == _sampler ); SAFE_RELEASE( _sampler );
        context->DSGetSamplers( (uint32)slot, 1, &_sampler ); assert( sampler == _sampler ); SAFE_RELEASE( _sampler );
        context->GSGetSamplers( (uint32)slot, 1, &_sampler ); assert( sampler == _sampler ); SAFE_RELEASE( _sampler );
        context->HSGetSamplers( (uint32)slot, 1, &_sampler ); assert( sampler == _sampler ); SAFE_RELEASE( _sampler );
#else
        context; sampler; slot;
#endif
    }
    //
    inline void vaDirectXTools11::AssertSetToD3DContextAllShaderTypes( ID3D11DeviceContext * context, ID3D11SamplerState ** samplers, int32 slot, int32 count )
    {
#ifdef _DEBUG
        ID3D11SamplerState * _samplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
        memset( _samplers, 0, sizeof( _samplers ) );
        context->PSGetSamplers( (uint32)slot, count, _samplers ); assert( memcmp( samplers, _samplers, count * sizeof( ID3D11SamplerState * ) ) == 0 ); SAFE_RELEASE_ARRAY( _samplers );
        context->CSGetSamplers( (uint32)slot, count, _samplers ); assert( memcmp( samplers, _samplers, count * sizeof( ID3D11SamplerState * ) ) == 0 ); SAFE_RELEASE_ARRAY( _samplers );
        context->VSGetSamplers( (uint32)slot, count, _samplers ); assert( memcmp( samplers, _samplers, count * sizeof( ID3D11SamplerState * ) ) == 0 ); SAFE_RELEASE_ARRAY( _samplers );
        context->DSGetSamplers( (uint32)slot, count, _samplers ); assert( memcmp( samplers, _samplers, count * sizeof( ID3D11SamplerState * ) ) == 0 ); SAFE_RELEASE_ARRAY( _samplers );
        context->GSGetSamplers( (uint32)slot, count, _samplers ); assert( memcmp( samplers, _samplers, count * sizeof( ID3D11SamplerState * ) ) == 0 ); SAFE_RELEASE_ARRAY( _samplers );
        context->HSGetSamplers( (uint32)slot, count, _samplers ); assert( memcmp( samplers, _samplers, count * sizeof( ID3D11SamplerState * ) ) == 0 ); SAFE_RELEASE_ARRAY( _samplers );
#else
        context; samplers; slot; count;
#endif
    }
    //
    inline void vaDirectXTools11::SetToD3DContextAllShaderTypes( ID3D11DeviceContext * context, ID3D11ShaderResourceView * srv, int32 slot )
    {
        VA_ASSERT( slot >= 0, L"vaDirectXTools11::SetToD3DContextAllShaderTypes : If you've left the DefaultSlot template parameter at default then you have to specify a valid shader slot!" );

        context->PSSetShaderResources( (uint32)slot, 1, &srv );
        context->CSSetShaderResources( (uint32)slot, 1, &srv );
        context->VSSetShaderResources( (uint32)slot, 1, &srv );
        context->DSSetShaderResources( (uint32)slot, 1, &srv );
        context->GSSetShaderResources( (uint32)slot, 1, &srv );
        context->HSSetShaderResources( (uint32)slot, 1, &srv );
    }
    //
    inline void vaDirectXTools11::AssertSetToD3DContextAllShaderTypes( ID3D11DeviceContext * context, ID3D11ShaderResourceView * srv, int32 slot )
    {
#ifdef _DEBUG
        ID3D11ShaderResourceView * _srv = NULL;
        context->PSGetShaderResources( (uint32)slot, 1, &_srv ); assert( srv == _srv ); SAFE_RELEASE( _srv );
        context->CSGetShaderResources( (uint32)slot, 1, &_srv ); assert( srv == _srv ); SAFE_RELEASE( _srv );
        context->VSGetShaderResources( (uint32)slot, 1, &_srv ); assert( srv == _srv ); SAFE_RELEASE( _srv );
        context->DSGetShaderResources( (uint32)slot, 1, &_srv ); assert( srv == _srv ); SAFE_RELEASE( _srv );
        context->GSGetShaderResources( (uint32)slot, 1, &_srv ); assert( srv == _srv ); SAFE_RELEASE( _srv );
        context->HSGetShaderResources( (uint32)slot, 1, &_srv ); assert( srv == _srv ); SAFE_RELEASE( _srv );
#else
        context; srv; slot;
#endif
    }
    //
    inline void vaDirectXTools11::SetToD3DContextCS( ID3D11DeviceContext * context, ID3D11UnorderedAccessView * uav, int32 slot, uint32 initialCount )
    {
        VA_ASSERT( slot >= 0, L"vaDirectXTools11::SetToD3DContextAllShaderTypes : If you've left the DefaultSlot template parameter at default then you have to specify a valid shader slot!" );

        context->CSSetUnorderedAccessViews( (uint32)slot, 1, &uav, &initialCount );
    }
    //
    inline void vaDirectXTools11::AssertSetToD3DContextCS( ID3D11DeviceContext * context, ID3D11UnorderedAccessView * uav, int32 slot )
    {
#ifdef _DEBUG
        ID3D11UnorderedAccessView * _uav = NULL;
        context->CSGetUnorderedAccessViews( (uint32)slot, 1, &_uav ); assert( uav == _uav ); SAFE_RELEASE( _uav );
#else
        context; uav; slot;
#endif
    }

    class vaDirectXTools12 final
    {
        //enum TextureLoadFlags
        //{
        //   TLF_NoFlags                      = 0,
        //   //TLF_InputI
        //};

    public:
        // Helper DirectX functions
  //      static ID3D11Texture1D *              CreateTexture1D(    DXGI_FORMAT format, UINT width, D3D11_SUBRESOURCE_DATA * initialData = NULL, 
  //                                                                UINT arraySize = 1, UINT mipLevels = 0, UINT bindFlags = D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE usage = D3D11_USAGE_DEFAULT,
  //                                                                UINT cpuaccessFlags = 0, UINT miscFlags = 0 );
  //      static ID3D11Texture2D *              CreateTexture2D(    DXGI_FORMAT format, UINT width, UINT height, D3D11_SUBRESOURCE_DATA * initialData = NULL, 
  //                                                                UINT arraySize = 1, UINT mipLevels = 0, UINT bindFlags = D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE usage = D3D11_USAGE_DEFAULT,
  //                                                                UINT cpuaccessFlags = 0, UINT sampleCount = 1, UINT sampleQuality = 0, UINT miscFlags = 0 );
  //      static ID3D11Texture3D *              CreateTexture3D(    DXGI_FORMAT format, UINT width, UINT height, UINT depth, D3D11_SUBRESOURCE_DATA * initialData = NULL, UINT mipLevels = 0,
  //                                                                UINT bindFlags = D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE usage = D3D11_USAGE_DEFAULT, UINT cpuaccessFlags = 0, UINT miscFlags = 0 );

        static bool FillShaderResourceViewDesc( D3D12_SHADER_RESOURCE_VIEW_DESC & outDesc, ID3D12Resource * resource, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN, int mipSliceMin = 0, int mipSliceCount = -1, int arraySliceMin = 0, int arraySliceCount = -1, bool isCubemap = false );
        static bool FillDepthStencilViewDesc( D3D12_DEPTH_STENCIL_VIEW_DESC & outDesc, ID3D12Resource * resource, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN, int mipSliceMin = 0, int arraySliceMin = 0, int arraySliceCount = -1 );
        static bool FillRenderTargetViewDesc( D3D12_RENDER_TARGET_VIEW_DESC & outDesc, ID3D12Resource * resource, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN, int mipSliceMin = 0, int arraySliceMin = 0, int arraySliceCount = -1 );
        static bool FillUnorderedAccessViewDesc( D3D12_UNORDERED_ACCESS_VIEW_DESC & outDesc, ID3D12Resource * resource, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN, int mipSliceMin = 0, int arraySliceMin = 0, int arraySliceCount = -1 );

        static void FillSamplerStatePointClamp( D3D12_STATIC_SAMPLER_DESC & outDesc );
        static void FillSamplerStatePointWrap( D3D12_STATIC_SAMPLER_DESC & outDesc );
        static void FillSamplerStateLinearClamp( D3D12_STATIC_SAMPLER_DESC & outDesc );
        static void FillSamplerStateLinearWrap( D3D12_STATIC_SAMPLER_DESC & outDesc );
        static void FillSamplerStateAnisotropicClamp( D3D12_STATIC_SAMPLER_DESC & outDesc );
        static void FillSamplerStateAnisotropicWrap( D3D12_STATIC_SAMPLER_DESC & outDesc );
        static void FillSamplerStateShadowCmp( D3D12_STATIC_SAMPLER_DESC & outDesc );

        // VERY simple - expand when needed :)
        static void FillBlendState( D3D12_BLEND_DESC & outDesc, vaBlendMode blendMode );

        // isDDS==true path will not allocate memory in outData and outSubresources will point into dataBuffer so make sure to keep it alive
        static bool LoadTexture( ID3D12Device * device, void * dataBuffer, uint64 dataBufferSize, bool isDDS, vaTextureLoadFlags loadFlags, vaResourceBindSupportFlags bindFlags, ID3D12Resource *& outResource, std::vector<D3D12_SUBRESOURCE_DATA> & outSubresources, std::unique_ptr<byte[]> & outData, bool & outIsCubemap ); // ,  uint64 * outCRC
        
        // both isDDS paths will allocate memory in outData and outSubresources will point into it so make sure to keep it alive
        static bool LoadTexture( ID3D12Device * device, const wchar_t * filePath, bool isDDS, vaTextureLoadFlags loadFlags, vaResourceBindSupportFlags bindFlags, ID3D12Resource *& outResource, std::vector<D3D12_SUBRESOURCE_DATA> & outSubresources, std::unique_ptr<byte[]> & outData, bool & outIsCubemap ); 
    };

    // yep, quite horrible
    using Microsoft::WRL::ComPtr;

#if 0
    //! A template COM smart pointer. I don't remember why I'm not using ComPtr which would probably be a better idea. TODO I guess?
    template<typename I>
    class vaCOMSmartPtr
    {
    public:
        //! Constructs an empty smart pointer.
        vaCOMSmartPtr( ) : m_p( 0 ) {}

        //! Assumes ownership of the given instance, if non-null.
        /*! \param	p	A pointer to an existing COM instance, or 0 to create an
                    empty COM smart pointer.
                    \note The smart pointer will assume ownership of the given instance.
                    It will \b not AddRef the contents, but it will Release the object
                    as it goes out of scope.
                    */
        vaCOMSmartPtr( I* p ) : m_p( p ) {}

        //! Releases the contained instance.
        ~vaCOMSmartPtr( )
        {
            SafeRelease( );
        }

        //! Copy-construction.
        vaCOMSmartPtr( vaCOMSmartPtr<I> const& ptr ) : m_p( ptr.m_p )
        {
            if( m_p )
                m_p->AddRef( );
        }

        //! Assignment.
        vaCOMSmartPtr<I>& operator=( vaCOMSmartPtr<I> const& ptr )
        {
            vaCOMSmartPtr<I> copy( ptr );
            Swap( copy );
            return *this;
        }

        //! Releases a contained instance, if present.
        /*! \note You should never need to call this function unless you wish to
           take control a Release an instance before the smart pointer goes out
           of scope.
           */
        void SafeRelease( )
        {
            if( m_p )
                m_p->Release( );
            m_p = 0;
        }

        //! First release existing ptr if any and then explicitly get the address of the pointer.
        I** ReleaseAndGetAddressOf( )
        {
            SafeRelease( );
            return &m_p;
        }

        //! Explicitly gets the address of the pointer.
        /*! \note This function should not be called on a smart pointer with
           non-zero contents. This is to avoid memory leaks by blatting over the
           contents without calling Release. Hence the complaint to std::cerr.
           */
        I** AddressOf( )
        {
            if( m_p != nullptr )
            {
                assert( false );
                // std::cerr << __FUNCTION__
                //     << ": non-zero contents - possible memory leak" << std::endl;
            }
            return &m_p;
        }

        //! Gets the address of the pointer using the address of operator.
        /*! \note This function should not be called on a smart pointer with
           non-zero contents. This is to avoid memory leaks by blatting over the
           contents without calling Release. Hence the complaint to std::cerr.
           */
        I** operator&( )
        {
            if( m_p != nullptr )
            {
                assert( false );
                // std::cerr << __FUNCTION__
                //     << ": non-zero contents - possible memory leak" << std::endl;
            }
            return &m_p;
        }

        //! Gets the encapsulated pointer.
        I* Get( ) const { return m_p; }

        //! Gets the encapsulated pointer.
        I* operator->( ) const { return m_p; }

        //! Swaps the encapsulated pointer with that of the argument.
        void Swap( vaCOMSmartPtr<I>& ptr )
        {
            I* p = m_p;
            m_p = ptr.m_p;
            ptr.m_p = p;
        }

        //! Gets the encapsulated pointer resets the smart pointer with without releasing the pointer.
        I* Detach( )
        {
            I * ret = m_p;
            m_p = nullptr;
            return ret;
        }

        // query for U interface
        HRESULT CopyFrom( IUnknown * inPtr )
        {
            I* outPtr = nullptr;
            HRESULT hr = inPtr->QueryInterface( __uuidof( I ), reinterpret_cast<void**>( &outPtr ) );
            if( SUCCEEDED( hr ) )
            {
                SafeRelease( );
                m_p = outPtr;
            }
            return hr;
        }

        template<typename U>
        HRESULT CopyFrom( const vaCOMSmartPtr<U> & inPtr )
        {
            return CopyFrom( inPtr.Get( ) );
        }

        //! Returns true if non-empty.
        operator bool( ) const { return m_p != 0; }

    private:
        //! The encapsulated instance.
        I* m_p;
    };
#endif

    class vaVertexInputLayoutsDX11
    {
    public:
        class CD3D11_INPUT_ELEMENT_DESC : public D3D11_INPUT_ELEMENT_DESC
        {
        public:
            CD3D11_INPUT_ELEMENT_DESC( LPCSTR semanticName, UINT semanticIndex, DXGI_FORMAT format, UINT inputSlot, UINT alignedByteOffset, D3D11_INPUT_CLASSIFICATION inputSlotClass, UINT instanceDataStepRate )
            {
                SemanticName = semanticName;
                SemanticIndex = semanticIndex;
                Format = format;
                InputSlot = inputSlot;
                AlignedByteOffset = alignedByteOffset;
                InputSlotClass = inputSlotClass;
                InstanceDataStepRate = instanceDataStepRate;
            }
        };

    public:
        //static std::vector<D3D11_INPUT_ELEMENT_DESC> PositionColorNormalTangentTexcoord1Vertex_WInsteadOfColor_Decl( )
        //{
        //    std::vector<D3D11_INPUT_ELEMENT_DESC> inputElements;
        //
        //    inputElements.push_back( CD3D11_INPUT_ELEMENT_DESC( "SV_Position", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 ) );
        //    inputElements.push_back( CD3D11_INPUT_ELEMENT_DESC( "NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 ) );
        //    inputElements.push_back( CD3D11_INPUT_ELEMENT_DESC( "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 ) );
        //    inputElements.push_back( CD3D11_INPUT_ELEMENT_DESC( "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 ) );
        //
        //    return inputElements;
        //}

        static std::vector<D3D11_INPUT_ELEMENT_DESC> PositionColorNormalTangentTexcoord1VertexDecl( )
        {
            std::vector<D3D11_INPUT_ELEMENT_DESC> inputElements;

            inputElements.push_back( CD3D11_INPUT_ELEMENT_DESC( "SV_Position", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 ) );
            inputElements.push_back( CD3D11_INPUT_ELEMENT_DESC( "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 ) );
            inputElements.push_back( CD3D11_INPUT_ELEMENT_DESC( "NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 ) );
            inputElements.push_back( CD3D11_INPUT_ELEMENT_DESC( "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 ) );
            inputElements.push_back( CD3D11_INPUT_ELEMENT_DESC( "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 ) );

            return inputElements;
        }
    };

    inline DXGI_FORMAT DXGIFormatFromVA( vaResourceFormat format )       
    { 
        return (DXGI_FORMAT)format;
    }

    inline vaResourceFormat VAFormatFromDXGI( DXGI_FORMAT format )
    {
        return (vaResourceFormat)format;
    }

    inline vaResourceBindSupportFlags BindFlagsVAFromDX11( UINT bindFlags )
    {
        vaResourceBindSupportFlags ret = vaResourceBindSupportFlags::None;
        if( ( bindFlags & D3D11_BIND_VERTEX_BUFFER ) != 0 )
            ret |= vaResourceBindSupportFlags::VertexBuffer;
        if( ( bindFlags & D3D11_BIND_INDEX_BUFFER ) != 0 )
            ret |= vaResourceBindSupportFlags::IndexBuffer;
        if( ( bindFlags & D3D11_BIND_CONSTANT_BUFFER ) != 0 )
            ret |= vaResourceBindSupportFlags::ConstantBuffer;
        if( ( bindFlags & D3D11_BIND_SHADER_RESOURCE ) != 0 )
            ret |= vaResourceBindSupportFlags::ShaderResource;
        if( ( bindFlags & D3D11_BIND_RENDER_TARGET ) != 0 )
            ret |= vaResourceBindSupportFlags::RenderTarget;
        if( ( bindFlags & D3D11_BIND_DEPTH_STENCIL ) != 0 )
            ret |= vaResourceBindSupportFlags::DepthStencil;
        if( ( bindFlags & D3D11_BIND_UNORDERED_ACCESS ) != 0 )
            ret |= vaResourceBindSupportFlags::UnorderedAccess;
        return ret;
    }

    inline UINT BindFlagsDX11FromVA( vaResourceBindSupportFlags bindFlags )
    {
        UINT ret = 0;
        if( ( bindFlags & vaResourceBindSupportFlags::VertexBuffer ) != 0 )
            ret |= D3D11_BIND_VERTEX_BUFFER;
        if( ( bindFlags & vaResourceBindSupportFlags::IndexBuffer ) != 0 )
            ret |= D3D11_BIND_INDEX_BUFFER;
        if( ( bindFlags & vaResourceBindSupportFlags::ConstantBuffer ) != 0 )
            ret |= D3D11_BIND_CONSTANT_BUFFER;
        if( ( bindFlags & vaResourceBindSupportFlags::ShaderResource ) != 0 )
            ret |= D3D11_BIND_SHADER_RESOURCE;
        if( ( bindFlags & vaResourceBindSupportFlags::RenderTarget ) != 0 )
            ret |= D3D11_BIND_RENDER_TARGET;
        if( ( bindFlags & vaResourceBindSupportFlags::DepthStencil ) != 0 )
            ret |= D3D11_BIND_DEPTH_STENCIL;
        if( ( bindFlags & vaResourceBindSupportFlags::UnorderedAccess ) != 0 )
            ret |= D3D11_BIND_UNORDERED_ACCESS;
        return ret;
    }

    inline vaResourceBindSupportFlags BindFlagsVAFromDX12( D3D12_RESOURCE_FLAGS resFlags )
    {
        vaResourceBindSupportFlags ret = vaResourceBindSupportFlags::ShaderResource;
        if( ( resFlags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE ) != 0 )
            ret &= ~vaResourceBindSupportFlags::ShaderResource;
        if( ( resFlags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET ) != 0 )
            ret |= vaResourceBindSupportFlags::RenderTarget;
        if( ( resFlags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL ) != 0 )
            ret |= vaResourceBindSupportFlags::DepthStencil;
        if( ( resFlags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS ) != 0 )
            ret |= vaResourceBindSupportFlags::UnorderedAccess;
        return ret;
    }

    inline D3D12_RESOURCE_FLAGS ResourceFlagsDX12FromVA( vaResourceBindSupportFlags bindFlags )
    {
        D3D12_RESOURCE_FLAGS ret = D3D12_RESOURCE_FLAG_NONE;
        if( ( bindFlags & vaResourceBindSupportFlags::VertexBuffer ) != 0 )
            ret |= D3D12_RESOURCE_FLAG_NONE;
        if( ( bindFlags & vaResourceBindSupportFlags::IndexBuffer ) != 0 )
            ret |= D3D12_RESOURCE_FLAG_NONE;
        if( ( bindFlags & vaResourceBindSupportFlags::ConstantBuffer ) != 0 )
            ret |= D3D12_RESOURCE_FLAG_NONE;
        if( ( bindFlags & vaResourceBindSupportFlags::ShaderResource ) != 0 )
            ret |= D3D12_RESOURCE_FLAG_NONE;
        if( ( bindFlags & vaResourceBindSupportFlags::RenderTarget ) != 0 )
            ret |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if( ( bindFlags & vaResourceBindSupportFlags::DepthStencil ) != 0 )
        {
            ret |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
            if( ( bindFlags & (vaResourceBindSupportFlags::ShaderResource | vaResourceBindSupportFlags::ConstantBuffer | vaResourceBindSupportFlags::UnorderedAccess | vaResourceBindSupportFlags::RenderTarget) ) == 0 ) // not sure about vertex/index but why would anyone want same buffer bound as depth and vert/ind buff? 
                ret |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
        }
        if( ( bindFlags & vaResourceBindSupportFlags::UnorderedAccess ) != 0 )
            ret |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        return ret;
    }

    inline UINT TexFlags11DXFromVA( vaTextureFlags texFlags )
    {
        UINT ret = 0;
        if( ( texFlags & vaTextureFlags::Cubemap ) != 0 )
            ret |= D3D11_RESOURCE_MISC_TEXTURECUBE;
        return ret;
    }

    inline D3D12_HEAP_TYPE HeapTypeDX12FromAccessFlags( vaResourceAccessFlags accessFlags )
    {
        if( accessFlags == vaResourceAccessFlags::Default )         return D3D12_HEAP_TYPE_DEFAULT; 
        if( ( accessFlags & vaResourceAccessFlags::CPUWrite) != 0 ) return D3D12_HEAP_TYPE_UPLOAD;  
        if( ( accessFlags & vaResourceAccessFlags::CPURead ) != 0 ) return D3D12_HEAP_TYPE_READBACK;
        assert( false ); return (D3D12_HEAP_TYPE)0;
    }

    ID3D11BlendState * DX11BlendModeFromVABlendMode( vaBlendMode blendMode );

    // generic bindable resource base class
    class vaShaderResourceDX11 : public vaShaderResource
    {
    public:
        virtual ~vaShaderResourceDX11( ) {};

        virtual ID3D11Buffer *                      GetBuffer( ) const = 0;
        virtual ID3D11UnorderedAccessView *         GetUAV( ) const = 0;
        virtual ID3D11ShaderResourceView *          GetSRV( ) const = 0;
    };
    
    class vaRenderDeviceDX12;
    class vaRenderDeviceContextDX12;

    // Resource state transition manager - initial implementation is naive but extendable while the interface & usage should remain the same
    //  * Intended to be inherited from - if you need to use it as a standalone variable that's fine, either change const/dest & methods to public or inherit and expose.
    //  * Must manually attach and detach
    class vaResourceStateTransitionHelperDX12
    {
    private:
        ComPtr<ID3D12Resource>              m_rsthResource;
        D3D12_RESOURCE_STATES               m_rsthCurrent = D3D12_RESOURCE_STATE_COMMON;

        // this currently only contains those different from m_rsthCurrent
        // alternative would be to track all subresources - this would use storage even when no subresources are transitioned independently but could be cleaner?
        vector< pair< uint32, D3D12_RESOURCE_STATES > >
                                            m_rsthSubResStates;

    public:
        vaResourceStateTransitionHelperDX12( ) { }
        ~vaResourceStateTransitionHelperDX12( ) { assert( m_rsthResource == nullptr ); }    // since this works as an 'attachment' then make sure it's been detached correctly

        void                                            RSTHAttach( ID3D12Resource * resource, D3D12_RESOURCE_STATES current )              { assert( m_rsthResource == nullptr ); m_rsthResource = resource; m_rsthCurrent = current; }
        void                                            RSTHDetach( ID3D12Resource * resource )                                             { resource; assert( m_rsthResource != nullptr && m_rsthResource.Get( ) == resource ); m_rsthResource.Reset( ); m_rsthCurrent = D3D12_RESOURCE_STATE_COMMON; m_rsthSubResStates.clear(); }
        const ComPtr<ID3D12Resource> &                  RSTHGetResource( ) const                                                            { return m_rsthResource; }
        D3D12_RESOURCE_STATES                           RSTHGetCurrentState( ) const { assert( m_rsthSubResStates.size() == 0 ); return m_rsthCurrent; } // if this asserts, call RSTHTransitionSubResUnroll before

        void                                            RSTHTransition( vaRenderDeviceContextDX12 & context, D3D12_RESOURCE_STATES target, uint32 subResIndex = -1 );
        void                                            RSTHTransitionSubResUnroll( vaRenderDeviceContextDX12 & context );
        void                                            RSTHAdoptResourceState( vaRenderDeviceContextDX12 & context, D3D12_RESOURCE_STATES target, uint32 subResIndex = -1 );
        
    private:
        void                                            RSTHTransitionSubRes( vaRenderDeviceContextDX12 & context, D3D12_RESOURCE_STATES target, uint32 subResIndex  );
    };

    // Resource view helpers - unlike DX11 views (ID3D11ShaderResourceView) these do not hold a reference to the actual resource
    // but they can get safely re-initialized with a different resource at runtime by doing Destroy->Create. Old descriptor is
    // kept alive until the frame is done.
    class vaResourceViewDX12
    {
    protected:
        vaRenderDeviceDX12 &                m_device;
        const D3D12_DESCRIPTOR_HEAP_TYPE    m_type = (D3D12_DESCRIPTOR_HEAP_TYPE)-1;
        int                                 m_heapIndex = -1;
        D3D12_CPU_DESCRIPTOR_HANDLE         m_CPUHandle = { 0 };
        D3D12_GPU_DESCRIPTOR_HANDLE         m_GPUHandle = { 0 };

    protected:
        vaResourceViewDX12( vaRenderDeviceDX12 & device, D3D12_DESCRIPTOR_HEAP_TYPE type ) : m_device( device ), m_type( type ) { }
        void                                Allocate( );

    public:
        ~vaResourceViewDX12( );

        bool                                IsCreated( ) const      { return m_heapIndex != -1; }
        void                                SafeRelease( );

        const D3D12_CPU_DESCRIPTOR_HANDLE & GetCPUHandle( ) const   { return m_CPUHandle; }
        //const D3D12_GPU_DESCRIPTOR_HANDLE & GetGPUHandle( ) const   { return m_GPUHandle; }
    };

    class vaConstantBufferViewDX12 : public vaResourceViewDX12
    {
        // this is left in for convenience & debugging purposes
        D3D12_CONSTANT_BUFFER_VIEW_DESC     m_desc = { };

    public:
        vaConstantBufferViewDX12( vaRenderDeviceDX12 & device ) : vaResourceViewDX12( device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ) { }

    public:
        const D3D12_CONSTANT_BUFFER_VIEW_DESC & GetDesc( ) const                            { return m_desc; }
        void                                Create( const D3D12_CONSTANT_BUFFER_VIEW_DESC & desc );
        void                                CreateNull( );
    };

    class vaShaderResourceViewDX12 : public vaResourceViewDX12
    {
        // this is left in for convenience & debugging purposes
        D3D12_SHADER_RESOURCE_VIEW_DESC     m_desc = { };

    public:
        vaShaderResourceViewDX12( vaRenderDeviceDX12 & device ) : vaResourceViewDX12( device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ) { }

    public:
        const D3D12_SHADER_RESOURCE_VIEW_DESC & GetDesc( ) const                            { return m_desc; }
        void                                Create( ID3D12Resource * resource, const D3D12_SHADER_RESOURCE_VIEW_DESC & desc );
        void                                CreateNull( );
    };

    class vaUnorderedAccessViewDX12 : public vaResourceViewDX12
    {
        // this is left in for convenience & debugging purposes
        D3D12_UNORDERED_ACCESS_VIEW_DESC    m_desc = { };

    public:
        vaUnorderedAccessViewDX12( vaRenderDeviceDX12 & device ) : vaResourceViewDX12( device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ) { }

    public:
        const D3D12_UNORDERED_ACCESS_VIEW_DESC & GetDesc( ) const                            { return m_desc; }
        void                                Create( ID3D12Resource *resource, ID3D12Resource * counterResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC & desc );
        void                                CreateNull( );
    };

    class vaRenderTargetViewDX12 : public vaResourceViewDX12
    {
        // this is left in for convenience & debugging purposes
        D3D12_RENDER_TARGET_VIEW_DESC       m_desc = { };

    public:
        vaRenderTargetViewDX12( vaRenderDeviceDX12 & device ) : vaResourceViewDX12( device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV ) { }

        vaRenderTargetViewDX12( ) = delete;
        vaRenderTargetViewDX12( const vaRenderTargetViewDX12& ) = delete;
        vaRenderTargetViewDX12& operator =( const vaRenderTargetViewDX12& ) = delete;

    public:
        const D3D12_RENDER_TARGET_VIEW_DESC & GetDesc( ) const                            { return m_desc; }
        void                                Create( ID3D12Resource *resource, const D3D12_RENDER_TARGET_VIEW_DESC & desc );
        void                                CreateNull( );
    };

    class vaDepthStencilViewDX12 : public vaResourceViewDX12
    {
        // this is left in for convenience & debugging purposes
        D3D12_DEPTH_STENCIL_VIEW_DESC       m_desc = { };

    public:
        vaDepthStencilViewDX12( vaRenderDeviceDX12 & device ) : vaResourceViewDX12( device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV ) { }

    public:
        const D3D12_DEPTH_STENCIL_VIEW_DESC & GetDesc( ) const                            { return m_desc; }
        void                                Create( ID3D12Resource *resource, const D3D12_DEPTH_STENCIL_VIEW_DESC & desc );
        void                                CreateNull( );
    };

    class vaSamplerViewDX12 : public vaResourceViewDX12
    {
        // this is left in for convenience & debugging purposes
        D3D12_SAMPLER_DESC                  m_desc = { };

    public:
        vaSamplerViewDX12( vaRenderDeviceDX12 & device ) : vaResourceViewDX12( device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ) { }

    public:
        const D3D12_SAMPLER_DESC &          GetDesc( ) const                            { return m_desc; }
        void                                Create( const D3D12_SAMPLER_DESC & desc );
        void                                CreateNull( );
    };


    // generic bindable resource base class
    class vaShaderResourceDX12 : public vaShaderResource
    {
    public:
        virtual ~vaShaderResourceDX12( ) {};

        virtual const vaConstantBufferViewDX12 *    GetCBV( )   const                                                                       = 0;
        virtual const vaUnorderedAccessViewDX12 *   GetUAV( )   const                                                                       = 0;
        virtual const vaShaderResourceViewDX12 *    GetSRV( )   const                                                                       = 0;

        virtual void                                TransitionResource( vaRenderDeviceContextDX12 & context, D3D12_RESOURCE_STATES target ) = 0;
        virtual void                                AdoptResourceState( vaRenderDeviceContextDX12 & context, D3D12_RESOURCE_STATES target ) = 0;   // if something external does a transition we can update our internal tracking
    };

    // Used to request cached D3D12_GRAPHICS_PIPELINE_STATE_DESC from the vaRenderDeviceContextDX12 (baseline implementation is in vaRenderDeviceDX12)
    struct vaGraphicsPSODescDX12
    {
        // root signature inherited from device context 
        // ID3D12RootSignature *pRootSignature;


        // 'extracted' shader data here - will keep the blobs and layouts alive as long as they're needed even if the shader gets deleted or recompiled 
        // (probably not needed as this shouldn't happen during rendering, but just to be on the safe side)
        // (shaders will have an unique identifier - this can persist between app restarts since they're cached already)
        ComPtr<ID3DBlob>                    VSBlob;
        int64                               VSUniqueContentsID = -1;
        shared_ptr< vector< D3D12_INPUT_ELEMENT_DESC > > 
                                            VSInputLayout;
        //
        ComPtr<ID3DBlob>                    PSBlob;
        int64                               PSUniqueContentsID = -1;
        //
        ComPtr<ID3DBlob>                    DSBlob;
        int64                               DSUniqueContentsID = -1;
        //
        ComPtr<ID3DBlob>                    HSBlob;
        int64                               HSUniqueContentsID = -1;
        //
        ComPtr<ID3DBlob>                    GSBlob;
        int64                               GSUniqueContentsID = -1;

        // stream output not yet implemented and probably will never be
        // D3D12_STREAM_OUTPUT_DESC StreamOutput;

        // blend states oversimplified with just vaBlendMode - to be upgraded when needed (full info is in D3D12_BLEND_DESC BlendState)
        vaBlendMode                         BlendMode               = vaBlendMode::Opaque;
            
        // blend sample mask not yet implemented
        // UINT SampleMask;

        // simplified rasterizer desc (expand when needed)
        // D3D12_RASTERIZER_DESC
        vaFillMode                          FillMode                = vaFillMode::Solid;
        vaFaceCull                          CullMode                = vaFaceCull::Back;
        bool                                FrontCounterClockwise   = false;
        bool                                MultisampleEnable       = true;  // only enabled if currently set render target
        //bool                                ScissorEnable           = true;

        // simplified depth-stencil (expand when needed)
        bool                                DepthEnable             = false;
        bool                                DepthWriteEnable        = false;
        vaComparisonFunc                    DepthFunc               = vaComparisonFunc::Always;

        // InputLayout picked up and versioned from vertex shader
        // D3D12_INPUT_LAYOUT_DESC InputLayout;

        // not implemented
        // D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue;

        // topology
        vaPrimitiveTopology                 Topology                = vaPrimitiveTopology::TriangleList;

        // 
        uint32                              NumRenderTargets;       // set by current context
        vaResourceFormat                    RTVFormats[8];          // set by current context
        vaResourceFormat                    DSVFormat;              // set by current context
        uint32                              SampleDescCount;        // set by currently set render target
        // uint32                              SampleDescQuality;   // would set by currently set render target, if it was implemented

        // not implemented
        // UINT NodeMask;

        // should probably be enabled for WARP once I get WARP working
        // D3D12_PIPELINE_STATE_FLAGS Flags;

        void                                FillGraphicsPipelineStateDesc( D3D12_GRAPHICS_PIPELINE_STATE_DESC & outDesc, ID3D12RootSignature * pRootSignature ) const;

        void                                FillKey( vaMemoryStream & outStream ) const;
    };

    // Used for caching
    class vaGraphicsPSODX12
    {
        friend class vaRenderDeviceDX12;
        vaGraphicsPSODescDX12 const         m_desc;
        ComPtr<ID3D12PipelineState>         m_pso;
        int64                               m_lastUsedFrame     = -1;       // last used frame index (unused PSOs will get removed from the cache)
        bool                                m_safelyReleased    = false;    // should only be deleted through FindOrCreateGraphicsPipelineState!!!

    public:
        vaGraphicsPSODX12( const vaGraphicsPSODescDX12 & desc ) : m_desc(desc) { }
        ~vaGraphicsPSODX12( ) { assert( m_safelyReleased ); }

        vaGraphicsPSODX12( const vaGraphicsPSODX12 & ) = delete;
        vaGraphicsPSODX12& operator =( const vaGraphicsPSODX12& ) = delete;

        const vaGraphicsPSODescDX12 &       GetDesc() const                             { return m_desc; }
        const ComPtr<ID3D12PipelineState> & GetPSO() const                              { return m_pso; }

        int64                               GetLastUsedFrame( ) const                   { return m_lastUsedFrame; }
        void                                SetLastUsedFrame( int64 lastUsedFrame )     { m_lastUsedFrame = lastUsedFrame; }

        HRESULT                             CreatePSO( vaRenderDeviceDX12 & device, ID3D12RootSignature * rootSignature );
    };

    // Used to request cached D3D12_COMPUTE_PIPELINE_STATE_DESC from the vaRenderDeviceContextDX12 (baseline implementation is in vaRenderDeviceDX12)
    struct vaComputePSODescDX12
    {
        // root signature inherited from device context 
        // ID3D12RootSignature *pRootSignature;

        // 'extracted' shader data here - will keep the blobs and layouts alive as long as they're needed even if the shader gets deleted or recompiled 
        // (probably not needed as this shouldn't happen during rendering, but just to be on the safe side)
        // (shaders will have an unique identifier - this can persist between app restarts since they're cached already)
        ComPtr<ID3DBlob>                    CSBlob;
        int64                               CSUniqueContentsID;

        // not implemented
        // UINT NodeMask;

        // should probably be enabled for WARP once I get WARP working
        // D3D12_PIPELINE_STATE_FLAGS Flags;

        void                                FillComputePipelineStateDesc( D3D12_COMPUTE_PIPELINE_STATE_DESC & outDesc, ID3D12RootSignature * pRootSignature ) const;

        void                                FillKey( vaMemoryStream & outStream ) const;
    };

    // Used for caching
    class vaComputePSODX12
    {
        friend class vaRenderDeviceDX12;
        vaComputePSODescDX12 const          m_desc;
        ComPtr<ID3D12PipelineState>         m_pso;
        int64                               m_lastUsedFrame     = -1;       // last used frame index (unused PSOs will get removed from the cache)
        bool                                m_safelyReleased    = false;    // should only be deleted through FindOrCreateComputePipelineState!!!

    public:
        vaComputePSODX12( const vaComputePSODescDX12 & desc ) : m_desc( desc ) { }
        ~vaComputePSODX12( ) { assert( m_safelyReleased ); }

        vaComputePSODX12( const vaComputePSODX12 & ) = delete;
        vaComputePSODX12& operator =( const vaComputePSODX12& ) = delete;

        const vaComputePSODescDX12 &        GetDesc() const                             { return m_desc; }
        const ComPtr<ID3D12PipelineState> & GetPSO() const                              { return m_pso; }

        int64                               GetLastUsedFrame( ) const                   { return m_lastUsedFrame; }
        void                                SetLastUsedFrame( int64 lastUsedFrame )     { m_lastUsedFrame = lastUsedFrame; }

        HRESULT                             CreatePSO( vaRenderDeviceDX12 & device, ID3D12RootSignature * rootSignature );
    };
        
    inline vaShaderResourceDX12 &  AsDX12( vaShaderResource & resource )   { return *resource.SafeCast<vaShaderResourceDX12*>(); }
    inline vaShaderResourceDX12 *  AsDX12( vaShaderResource * resource )   { return resource->SafeCast<vaShaderResourceDX12*>(); }
}
