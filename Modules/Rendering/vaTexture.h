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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// vaTexture can be a static texture used as an asset, loaded from storage or created procedurally, or a dynamic 
// GPU-only texture for use as a render target or similar. 
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Core/vaCoreIncludes.h"
#include "Core/Misc/vaResourceFormats.h"

#include "vaRendering.h"

namespace VertexAsylum
{
    enum class vaTextureAccessFlags : uint32
    {
        None                        = 0,
        CPURead                     = ( 1 << 0 ),
        CPUWrite                    = ( 1 << 1 )
    };

    enum class vaTextureType : int32
    {
        Unknown	                    = 0,
        Buffer	                    = 1,
        Texture1D	                = 2,
        Texture1DArray              = 3,
        Texture2D	                = 4,
        Texture2DArray	            = 5,
        Texture2DMS                 = 6,
        Texture2DMSArray            = 7,
        Texture3D	                = 8
    };

    enum class vaTextureLoadFlags : uint32
    {
        Default                     = 0,
        PresumeDataIsSRGB           = (1 << 0),
        PresumeDataIsLinear         = (1 << 1),
        AutogenerateMIPsIfMissing   = (1 << 2),
    };

    enum class vaTextureFlags : uint32
    {
        None                        = 0,
        Cubemap                     = ( 1 << 0 ),

        CubemapButArraySRV          = ( 1 << 16 ),  // this is only if you wish the SRV to be of Array (and not Cube or Cube Array) type
    };

    enum class vaTextureContentsType
    {
        GenericColor                = 0,
        GenericLinear               = 1,
        NormalsXYZ_UNORM            = 2,    // unpacked by doing normalOut = normalize( normalIn.xyz * 2.0 - 1.0 );
        NormalsXY_UNORM             = 3,    // unpacked by doing normalOut.xy = (normalIn.xy * 2.0 - 1.0); normalOut.z = sqrt( 1 - normalIn.x*normalIn.x - normalIn.y*normalIn.y );
        NormalsWY_UNORM             = 4,    // unpacked by doing normalOut.xy = (normalIn.wy * 2.0 - 1.0); normalOut.z = sqrt( 1 - normalIn.x*normalIn.x - normalIn.y*normalIn.y ); (this is for DXT5_NM - see UnpackNormalDXT5_NM)
        SingleChannelLinearMask     = 5,
        DepthBuffer                 = 6,
        LinearDepth                 = 7,
        NormalsXY_LAEA_ENCODED      = 8,    // Lambert Azimuthal Equal-Area projection, see vaShared.hlsl for encode/decode
    };

    BITFLAG_ENUM_CLASS_HELPER( vaTextureAccessFlags );
    BITFLAG_ENUM_CLASS_HELPER( vaTextureLoadFlags );
    BITFLAG_ENUM_CLASS_HELPER( vaTextureFlags );


    struct vaTextureConstructorParams : vaRenderingModuleParams
    {
        const vaGUID &          UID;
        explicit vaTextureConstructorParams( vaRenderDevice & device, const vaGUID & uid = vaCore::GUIDCreate( ) ) : vaRenderingModuleParams(device), UID( uid ) { }
    };

    struct vaTextureSubresource
    {
        byte *                              Buffer          = nullptr;
        int                                 BytesPerPixel   = 0;
        int64                               SizeInBytes     = 0;
        int                                 RowPitch        = 0;            // in bytes
        int                                 DepthPitch      = 0;            // in bytes
        int                                 SizeX           = 0;            // width
        int                                 SizeY           = 0;            // height

    public:
        vaTextureSubresource( )             { Buffer = nullptr; Reset(); }
        ~vaTextureSubresource( )            { delete[] Buffer; }
    protected:
        void Reset( )                       { if( Buffer != nullptr ) delete[] Buffer; Buffer = nullptr; BytesPerPixel = 0; SizeInBytes = 0; RowPitch = 0; DepthPitch = 0; }

    public:
        //template<typename T>
        //T &                                 PixelAt( int x )        { assert( RowPitch == Size; ); assert( sizeof(T) == BytesPerPixel ); return *(T*)&Buffer[x * BytesPerPixel]; }

        template<typename T>
        inline T &                          PixelAt( int x, int y )         { assert( DepthPitch == 0 ); assert( sizeof(T) == BytesPerPixel ); assert( (x >= 0) && (x < SizeX) ); assert( (y >= 0) && (y < SizeY) ); return *(T*)&Buffer[x * BytesPerPixel + y * RowPitch]; }

        template<typename T>
        inline const T &                    PixelAt( int x, int y ) const   { assert( DepthPitch == Size ); assert( sizeof(T) == BytesPerPixel ); assert( (x >= 0) && (x < SizeX) ); assert( (y >= 0) && (y < SizeY) ); return *(T*)&Buffer[x * BytesPerPixel + y * RowPitch]; }
    };

    class vaTexture : public vaAssetResource, public vaRenderingModule, public vaShaderResource
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    private:
        vaTextureFlags                      m_flags;

        vaTextureAccessFlags                m_accessFlags;
        vaTextureType                       m_type;
        vaResourceBindSupportFlags           m_bindSupportFlags;
        vaTextureContentsType               m_contentsType;

        vaResourceFormat                     m_resourceFormat;
        vaResourceFormat                     m_srvFormat;
        vaResourceFormat                     m_rtvFormat;
        vaResourceFormat                     m_dsvFormat;
        vaResourceFormat                     m_uavFormat;

        int                                 m_sizeX;            // serves as desc.ByteWidth for Buffer
        int                                 m_sizeY;            // doubles as ArraySize in 1D texture
        int                                 m_sizeZ;            // doubles as ArraySize in 2D texture
        int                                 m_sampleCount;
        int                                 m_mipLevels;

        int                                 m_viewedMipSlice;           // if m_viewedOriginal is nullptr, this will always be 0
        int                                 m_viewedMipSliceCount;      // if m_viewedOriginal is nullptr, this will always be 0
        int                                 m_viewedArraySlice;         // if m_viewedOriginal is nullptr, this will always be 0
        int                                 m_viewedArraySliceCount;    // if m_viewedOriginal is nullptr, this will always be 0
        //shared_ptr< vaTexture >             m_viewedOriginal;
        bool                                m_isView;
        int                                 m_viewedSliceSizeX;
        int                                 m_viewedSliceSizeY;
        int                                 m_viewedSliceSizeZ;

        vector< vaTextureSubresource >      m_mappedData;
        bool                                m_isMapped;

        // Used to temporarily override this texture with another only for rendering-from purposes; effectively only overrides SRV - all other getters/setters and RTV/UAV/DSV accesses remain unchanged
        // (useful for debugging, UI and similar)
        shared_ptr< vaTexture >             m_overrideView;
    
    protected:
        static const int                    c_fileVersion           = 2;

    protected:
        friend class vaTextureDX11;         // ugly but has to be here for every API implementation - not sure how to easily handle this otherwise
                                            vaTexture( const vaRenderingModuleParams & params  );
                                            void                                Initialize( vaResourceBindSupportFlags binds, vaResourceFormat resourceFormat = vaResourceFormat::Automatic, vaResourceFormat srvFormat = vaResourceFormat::Automatic, vaResourceFormat rtvFormat = vaResourceFormat::Automatic, vaResourceFormat dsvFormat = vaResourceFormat::Automatic, vaResourceFormat uavFormat = vaResourceFormat::Automatic, vaTextureFlags flags = vaTextureFlags::None, int viewedMipSliceMin = 0, int viewedMipSliceCount = -1, int viewedArraySliceMin = 0, int viewedArraySliceCount = -1, vaTextureContentsType contentsType = vaTextureContentsType::GenericColor );
    
    public:
        virtual                             ~vaTexture( )                                                   { assert( !m_isMapped ); }

    public:
        // all of these need a non-static (protected) virtual counterparts so the whole vaRenderingModule API specialization works...
        static shared_ptr<vaTexture>        CreateFromImageFile( vaRenderDevice & device, const wstring & storagePath, vaTextureLoadFlags loadFlags = vaTextureLoadFlags::Default, vaResourceBindSupportFlags binds = vaResourceBindSupportFlags::ShaderResource, vaTextureContentsType contentsType = vaTextureContentsType::GenericColor );
        static shared_ptr<vaTexture>        CreateFromImageFile( vaRenderDevice & device, const string & storagePath, vaTextureLoadFlags loadFlags = vaTextureLoadFlags::Default, vaResourceBindSupportFlags binds = vaResourceBindSupportFlags::ShaderResource, vaTextureContentsType contentsType = vaTextureContentsType::GenericColor )       { return CreateFromImageFile( device, vaStringTools::SimpleWiden(storagePath), loadFlags, binds, contentsType ); }
        static shared_ptr<vaTexture>        CreateFromImageData( vaRenderDevice & device, void * buffer, uint64 bufferSize, vaTextureLoadFlags loadFlags = vaTextureLoadFlags::Default, vaResourceBindSupportFlags binds = vaResourceBindSupportFlags::ShaderResource, vaTextureContentsType contentsType = vaTextureContentsType::GenericColor );
        
        // 1D textures (regular, array)
        static shared_ptr<vaTexture>        Create1D( vaRenderDevice & device, vaResourceFormat format, int width, int mipLevels, int arraySize, vaResourceBindSupportFlags bindFlags, vaTextureAccessFlags accessFlags = vaTextureAccessFlags::None, vaResourceFormat srvFormat = vaResourceFormat::Automatic, vaResourceFormat rtvFormat = vaResourceFormat::Automatic, vaResourceFormat dsvFormat = vaResourceFormat::Automatic, vaResourceFormat uavFormat = vaResourceFormat::Automatic, vaTextureFlags flags = vaTextureFlags::None, vaTextureContentsType contentsType = vaTextureContentsType::GenericColor, void * initialData = nullptr );
        // 2D textures (regular, array, ms, msarray, cubemap, cubemap array)
        static shared_ptr<vaTexture>        Create2D( vaRenderDevice & device, vaResourceFormat format, int width, int height, int mipLevels, int arraySize, int sampleCount, vaResourceBindSupportFlags bindFlags, vaTextureAccessFlags accessFlags = vaTextureAccessFlags::None, vaResourceFormat srvFormat = vaResourceFormat::Automatic, vaResourceFormat rtvFormat = vaResourceFormat::Automatic, vaResourceFormat dsvFormat = vaResourceFormat::Automatic, vaResourceFormat uavFormat = vaResourceFormat::Automatic, vaTextureFlags flags = vaTextureFlags::None, vaTextureContentsType contentsType = vaTextureContentsType::GenericColor, void * initialData = nullptr, int initialDataPitch = 0 );
        // 3D textures
        static shared_ptr<vaTexture>        Create3D( vaRenderDevice & device, vaResourceFormat format, int width, int height, int depth, int mipLevels, vaResourceBindSupportFlags bindFlags, vaTextureAccessFlags accessFlags = vaTextureAccessFlags::None, vaResourceFormat srvFormat = vaResourceFormat::Automatic, vaResourceFormat rtvFormat = vaResourceFormat::Automatic, vaResourceFormat dsvFormat = vaResourceFormat::Automatic, vaResourceFormat uavFormat = vaResourceFormat::Automatic, vaTextureFlags flags = vaTextureFlags::None, vaTextureContentsType contentsType = vaTextureContentsType::GenericColor, void * initialData = nullptr, int initialDataPitch = 0, int initialDataSlicePitch = 0 );

        static void                         CreateMirrorIfNeeded( vaTexture & original, shared_ptr<vaTexture> & mirror );
        
        // // just an array view into the cubemap - should be mixed into createview, no need for a separate function
        // static shared_ptr<vaTexture>        CreateCubeArrayView( vaTexture & texture );

        static shared_ptr<vaTexture>        CreateView( vaTexture & texture, vaResourceBindSupportFlags bindFlags, vaResourceFormat srvFormat = vaResourceFormat::Automatic, vaResourceFormat rtvFormat = vaResourceFormat::Automatic, vaResourceFormat dsvFormat = vaResourceFormat::Automatic, vaResourceFormat uavFormat = vaResourceFormat::Automatic, vaTextureFlags flags = vaTextureFlags::None, int viewedMipSliceMin = 0, int viewedMipSliceCount = -1, int viewedArraySliceMin = 0, int viewedArraySliceCount = -1 );

    public:
        vaTextureType                       GetType( ) const                                                { return m_type; }
        virtual vaResourceBindSupportFlags  GetBindSupportFlags( ) const override                           { return m_bindSupportFlags; }
        vaTextureFlags                      GetFlags( ) const                                               { return m_flags; }
        vaTextureAccessFlags                GetAccessFlags( ) const                                         { return m_accessFlags; }

        vaTextureContentsType               GetContentsType( ) const                                        { return m_contentsType; }
        void                                SetContentsType( vaTextureContentsType contentsType )           { m_contentsType = contentsType; }

        vaResourceFormat                     GetResourceFormat( ) const                                      { return m_resourceFormat; }
        vaResourceFormat                     GetSRVFormat( ) const                                           { return m_srvFormat;      }
        vaResourceFormat                     GetDSVFormat( ) const                                           { return m_dsvFormat;      }
        vaResourceFormat                     GetRTVFormat( ) const                                           { return m_rtvFormat;      }
        vaResourceFormat                     GetUAVFormat( ) const                                           { return m_uavFormat;      }

        int                                 GetSizeX( )             const                                   { return m_sizeX;       }     // serves as desc.ByteWidth for Buffer
        int                                 GetSizeY( )             const                                   { return m_sizeY;       }     // doubles as ArraySize in 1D texture
        int                                 GetSizeZ( )             const                                   { return m_sizeZ;       }     // doubles as ArraySize in 2D texture
        int                                 GetSampleCount( )       const                                   { return m_sampleCount; }
        int                                 GetMipLevels( )         const                                   { return m_mipLevels;   }
        int                                 GetArrayCount( )        const                                   { if( GetType() == vaTextureType::Texture1DArray ) return GetSizeY(); if( GetType() == vaTextureType::Texture2DArray || GetType() == vaTextureType::Texture2DMSArray ) return GetSizeZ(); return 1; }

        // if view, return viewed subset, otherwise return the above (viewing the whole resource)
        int                                 GetViewedMipSlice( )        const                               { return (IsView())?(m_viewedMipSlice       ):(0); }
        int                                 GetViewedMipSliceCount( )   const                               { return (IsView())?(m_viewedMipSliceCount  ):(GetMipLevels()); }
        int                                 GetViewedArraySlice( )  const                                   { return (IsView())?(m_viewedArraySlice     ):(0); }
        int                                 GetViewedArraySliceCount( )  const                              { return (IsView())?(m_viewedArraySliceCount):(GetArrayCount()); }
        int                                 GetViewedSliceSizeX( )  const                                   { return (IsView())?(m_viewedSliceSizeX     ):(GetSizeX()); }
        int                                 GetViewedSliceSizeY( )  const                                   { return (IsView())?(m_viewedSliceSizeY     ):(GetSizeY()); }
        int                                 GetViewedSliceSizeZ( )  const                                   { return (IsView())?(m_viewedSliceSizeZ     ):(GetSizeZ()); }

        bool                                IsCubemap( ) const                                              { return (m_flags & vaTextureFlags::Cubemap) != 0; }

        bool                                IsView( ) const                                                 { return m_isView; } // m_viewedOriginal != nullptr; }

        bool                                IsMapped( ) const                                               { return m_isMapped; }
        vector<vaTextureSubresource> &      GetMappedData( )                                                { assert(m_isMapped); return m_mappedData; }

        // Temporarily override this texture with another only for rendering-from purposes; effectively only overrides SRV - all other getters/setters and RTV/UAV/DSV accesses remain unchanged
        // (useful for debugging, UI and similar)
        void                                SetOverrideView( const shared_ptr<vaTexture> & overrideView )   { m_overrideView = overrideView; }
        const shared_ptr<vaTexture> &       GetOverrideView( )                                              { return m_overrideView; }

        virtual bool                        TryMap( vaRenderDeviceContext & apiContext, vaResourceMapType mapType, bool doNotWait = false )                             = 0;
        virtual void                        Unmap( vaRenderDeviceContext & apiContext )                                                                                 = 0;

        // all these should maybe go to API context itself to match DX
        virtual void                        ClearRTV( vaRenderDeviceContext & apiContext, const vaVector4 & clearValue )                                                = 0;
        virtual void                        ClearUAV( vaRenderDeviceContext & apiContext, const vaVector4ui & clearValue )                                              = 0;
        virtual void                        ClearUAV( vaRenderDeviceContext & apiContext, const vaVector4 & clearValue )                                                = 0;
        virtual void                        ClearDSV( vaRenderDeviceContext & apiContext, bool clearDepth, float depthValue, bool clearStencil, uint8 stencilValue )    = 0;
        virtual void                        CopyFrom( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & srcTexture )                                    = 0;

        // moving to stateless, this is going away
        // // Set as SRVs
        // virtual void                        SetToAPISlotSRV( vaRenderDeviceContext & apiContext, int slot, bool assertOnOverwrite = true )                              = 0;
        // virtual void                        UnsetFromAPISlotSRV( vaRenderDeviceContext & apiContext, int slot, bool assertOnNotSet = true )                             = 0;

        // moving to stateless, this is going away
        // // Set as UAVs
        // virtual void                        SetToAPISlotUAV_CS( vaRenderDeviceContext & apiContext, int slot, uint32 initialCount = -1, bool assertOnOverwrite = true ) = 0;
        // virtual void                        UnsetFromAPISlotUAV_CS( vaRenderDeviceContext & apiContext, int slot, bool assertOnNotSet = true )                          = 0;

        // all these should maybe go to API context itself to match DX
        virtual void                        UpdateSubresource( vaRenderDeviceContext & apiContext, int dstSubresourceIndex, const vaBoxi & dstBox, void * srcData, int srcDataRowPitch, int srcDataDepthPitch = 0 ) = 0;
        virtual void                        ResolveSubresource( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & dstResource, uint dstSubresource, uint srcSubresource, vaResourceFormat format = vaResourceFormat::Automatic ) = 0;

        // Will try to create a BC5-6-7 compressed copy of the texture to the best of its abilities or if it can't then return nullptr; very rudimentary at the moment, future upgrades required.
        virtual shared_ptr<vaTexture>       TryCompress( )                                                                                                              = 0;

        virtual shared_ptr<vaTexture>       CreateLowerResFromMIPs( vaRenderDeviceContext & apiContext, int numberOfMIPsToDrop, bool neverGoBelow4x4 = true )           = 0;

    protected:
        virtual bool                        Import( const wstring & storageFilePath, vaTextureLoadFlags loadFlags, vaResourceBindSupportFlags binds, vaTextureContentsType contentsType = vaTextureContentsType::GenericColor ) = 0;
        virtual bool                        Import( void * buffer, uint64 bufferSize, vaTextureLoadFlags loadFlags = vaTextureLoadFlags::Default, vaResourceBindSupportFlags binds = vaResourceBindSupportFlags::ShaderResource, vaTextureContentsType contentsType = vaTextureContentsType::GenericColor ) = 0;
        virtual void                        Destroy( ) = 0;

        virtual shared_ptr<vaTexture>       CreateView( vaResourceBindSupportFlags bindFlags, vaResourceFormat srvFormat, vaResourceFormat rtvFormat, vaResourceFormat dsvFormat, vaResourceFormat uavFormat, vaTextureFlags flags, int viewedMipSliceMin, int viewedMipSliceCount, int viewedArraySliceMin, int viewedArraySliceCount ) = 0;

        virtual bool                        InternalCreate1D( vaResourceFormat format, int width, int mipLevels, int arraySize, vaResourceBindSupportFlags bindFlags, vaTextureAccessFlags accessFlags, vaResourceFormat srvFormat, vaResourceFormat rtvFormat, vaResourceFormat dsvFormat, vaResourceFormat uavFormat, vaTextureFlags flags, vaTextureContentsType contentsType, void * initialData ) = 0;
        virtual bool                        InternalCreate2D( vaResourceFormat format, int width, int height, int mipLevels, int arraySize, int sampleCount, vaResourceBindSupportFlags bindFlags, vaTextureAccessFlags accessFlags, vaResourceFormat srvFormat, vaResourceFormat rtvFormat, vaResourceFormat dsvFormat, vaResourceFormat uavFormat, vaTextureFlags flags, vaTextureContentsType contentsType, void * initialData, int initialDataPitch ) = 0;
        virtual bool                        InternalCreate3D( vaResourceFormat format, int width, int height, int depth, int mipLevels, vaResourceBindSupportFlags bindFlags, vaTextureAccessFlags accessFlags, vaResourceFormat srvFormat, vaResourceFormat rtvFormat, vaResourceFormat dsvFormat, vaResourceFormat uavFormat, vaTextureFlags flags, vaTextureContentsType contentsType, void * initialData, int initialDataPitch, int initialDataSlicePitch ) = 0;

    public:
        void                                ClearRTV( vaRenderDeviceContext & apiContext, float clearValue[] )                      { ClearRTV( apiContext, vaVector4(clearValue) ); }
        void                                ClearUAV( vaRenderDeviceContext & apiContext, uint32 clearValue[] )                     { ClearUAV( apiContext, vaVector4ui(clearValue[0], clearValue[1], clearValue[2], clearValue[3]) ); }
        void                                ClearUAV( vaRenderDeviceContext & apiContext, float clearValue[] )                      { ClearUAV( apiContext, vaVector4(clearValue) ); }

    };


}