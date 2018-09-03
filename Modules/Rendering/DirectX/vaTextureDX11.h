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

#pragma once

#include "Core/vaCoreIncludes.h"

#include "Rendering/DirectX/vaDirectXIncludes.h"
#include "Rendering/DirectX/vaDirectXTools.h"

#include "Rendering/vaRenderingIncludes.h"

#include "Rendering/DirectX/vaResourceFormatsDX11.h"


namespace VertexAsylum
{

    class vaTextureDX11 : public vaTexture, public vaShaderResourceSRVDX11
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    private:
        ID3D11Resource *                    m_resource;
        ID3D11Buffer *                      m_buffer;
        ID3D11Texture1D *                   m_texture1D;
        ID3D11Texture2D *                   m_texture2D;
        ID3D11Texture3D *                   m_texture3D;
        ID3D11ShaderResourceView *          m_srv;
        ID3D11RenderTargetView *            m_rtv;
        ID3D11DepthStencilView *            m_dsv;
        ID3D11UnorderedAccessView *         m_uav;

    protected:
        friend class vaTexture;
        explicit                            vaTextureDX11( const vaRenderingModuleParams & params );
        virtual                             ~vaTextureDX11( )   ;

        virtual bool                        Import( const wstring & storageFilePath, vaTextureLoadFlags loadFlags, vaResourceBindSupportFlags binds, vaTextureContentsType contentsType = vaTextureContentsType::GenericColor ) override;
        virtual bool                        Import( void * buffer, uint64 bufferSize, vaTextureLoadFlags loadFlags = vaTextureLoadFlags::Default, vaResourceBindSupportFlags binds = vaResourceBindSupportFlags::ShaderResource, vaTextureContentsType contentsType = vaTextureContentsType::GenericColor ) override;
        virtual void                        Destroy( ) override;

        virtual shared_ptr<vaTexture>       CreateView( vaResourceBindSupportFlags bindFlags, vaResourceFormat srvFormat, vaResourceFormat rtvFormat, vaResourceFormat dsvFormat, vaResourceFormat uavFormat, vaTextureFlags flags, int viewedMipSliceMin, int viewedMipSliceCount, int viewedArraySliceMin, int viewedArraySliceCount );

        virtual bool                        InternalCreate1D( vaResourceFormat format, int width, int mipLevels, int arraySize, vaResourceBindSupportFlags bindFlags, vaTextureAccessFlags accessFlags, vaResourceFormat srvFormat, vaResourceFormat rtvFormat, vaResourceFormat dsvFormat, vaResourceFormat uavFormat, vaTextureFlags flags, vaTextureContentsType contentsType, void * initialData ) override;
        virtual bool                        InternalCreate2D( vaResourceFormat format, int width, int height, int mipLevels, int arraySize, int sampleCount, vaResourceBindSupportFlags bindFlags, vaTextureAccessFlags accessFlags, vaResourceFormat srvFormat, vaResourceFormat rtvFormat, vaResourceFormat dsvFormat, vaResourceFormat uavFormat, vaTextureFlags flags, vaTextureContentsType contentsType, void * initialData, int initialDataPitch ) override;
        virtual bool                        InternalCreate3D( vaResourceFormat format, int width, int height, int depth, int mipLevels, vaResourceBindSupportFlags bindFlags, vaTextureAccessFlags accessFlags, vaResourceFormat srvFormat, vaResourceFormat rtvFormat, vaResourceFormat dsvFormat, vaResourceFormat uavFormat, vaTextureFlags flags, vaTextureContentsType contentsType, void * initialData, int initialDataPitch, int initialDataSlicePitch ) override;

    public:
        ID3D11Resource *                    GetResource( ) const        { return m_resource; }
        virtual ID3D11Buffer *              GetBuffer( ) const override { return nullptr; }
        ID3D11Texture1D *                   GetTexture1D( ) const       { return m_texture1D; }
        ID3D11Texture2D *                   GetTexture2D( ) const       { return m_texture2D; }
        ID3D11Texture3D *                   GetTexture3D( ) const       { return m_texture3D; }
        virtual ID3D11ShaderResourceView *  GetSRV( ) const override    { return (m_overrideView==nullptr)?(m_srv):(m_overrideView->SafeCast<vaTextureDX11*>()->GetSRV()); }
        ID3D11RenderTargetView *            GetRTV( ) const             { assert(m_overrideView==nullptr); return m_rtv; }
        ID3D11DepthStencilView *            GetDSV( ) const             { assert(m_overrideView==nullptr); return m_dsv; }
        virtual ID3D11UnorderedAccessView * GetUAV( ) const override    { assert(m_overrideView==nullptr); return m_uav; }

    public:
        // Given an existing D3D11 resource, make a vaTexture around it!
        static shared_ptr<vaTexture>        CreateWrap( vaRenderDevice & renderDevice, ID3D11Resource * resource, vaResourceFormat srvFormat = vaResourceFormat::Automatic, vaResourceFormat rtvFormat = vaResourceFormat::Automatic, vaResourceFormat dsvFormat = vaResourceFormat::Automatic, vaResourceFormat uavFormat = vaResourceFormat::Automatic, vaTextureContentsType contentsType = vaTextureContentsType::GenericColor );

    private:
        void                                SetResource( ID3D11Resource * resource );
        void                                ProcessResource( bool notAllBindViewsNeeded = false, bool dontResetFlags = false );
        void                                InternalUpdateFromRenderingCounterpart( bool notAllBindViewsNeeded, bool dontResetFlags );

    protected:
        // vaTexture
        virtual bool                        TryMap( vaRenderDeviceContext & apiContext, vaResourceMapType mapType, bool doNotWait );
        virtual void                        Unmap( vaRenderDeviceContext & apiContext );

        virtual void                        ClearRTV( vaRenderDeviceContext & apiContext, const vaVector4 & clearValue );
        virtual void                        ClearUAV( vaRenderDeviceContext & apiContext, const vaVector4ui & clearValue );
        virtual void                        ClearUAV( vaRenderDeviceContext & apiContext, const vaVector4 & clearValue );
        virtual void                        ClearDSV( vaRenderDeviceContext & apiContext, bool clearDepth, float depthValue, bool clearStencil, uint8 stencilValue );
        virtual void                        CopyFrom( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & srcTexture );

        virtual void                        UpdateSubresource( vaRenderDeviceContext & apiContext, int dstSubresourceIndex, const vaBoxi & dstBox, void * srcData, int srcDataRowPitch, int srcDataDepthPitch = 0 );
        virtual void                        ResolveSubresource( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & dstResource, uint dstSubresource, uint srcSubresource, vaResourceFormat format ) override;

        virtual bool                        LoadAPACK( vaStream & inStream ) override;
        virtual bool                        SaveAPACK( vaStream & outStream ) override;
        virtual bool                        SerializeUnpacked( vaXMLSerializer & serializer, const wstring & assetFolder ) override;

        virtual shared_ptr<vaTexture>       TryCompress( ) override;

        virtual shared_ptr<vaTexture>       CreateLowerResFromMIPs( vaRenderDeviceContext & apiContext, int numberOfMIPsToDrop, bool neverGoBelow4x4 = true ) override;

    public:
        // Set as SRVs
        virtual void                        SetToAPISlotSRV( vaRenderDeviceContext & apiContext, int slot, bool assertOnOverwrite = true );
        virtual void                        UnsetFromAPISlotSRV( vaRenderDeviceContext & apiContext, int slot, bool assertOnNotSet = true );

        // Set as UAVs
        virtual void                        SetToAPISlotUAV_CS( vaRenderDeviceContext & apiContext, int slot, uint32 initialCount = (uint32)-1, bool assertOnOverwrite = true );
        virtual void                        UnsetFromAPISlotUAV_CS( vaRenderDeviceContext & apiContext, int slot, bool assertOnNotSet = true );
    };

    inline DXGI_FORMAT DXGIFormatFromVA( vaResourceFormat format )       
    { 
        return (DXGI_FORMAT)format;
    }

    inline vaResourceFormat VAFormatFromDXGI( DXGI_FORMAT format )
    {
        return (vaResourceFormat)format;
    }

    inline vaResourceBindSupportFlags BindFlagsVAFromDX( UINT bindFlags )
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

    inline UINT BindFlagsDXFromVA( vaResourceBindSupportFlags bindFlags )
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

    inline UINT TexFlagsDXFromVA( vaTextureFlags texFlags )
    {
        UINT ret = 0;
        if( ( texFlags & vaTextureFlags::Cubemap ) != 0 )
            ret |= D3D11_RESOURCE_MISC_TEXTURECUBE;
        return ret;
    }
}