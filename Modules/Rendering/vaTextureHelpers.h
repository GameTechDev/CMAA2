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

#include "vaTexture.h"

#include "Rendering/vaRenderBuffers.h"
#include "Rendering/vaShader.h"

#include "Rendering/Shaders/vaSharedTypes_HelperTools.h"

namespace VertexAsylum
{

    //////////////////////////////////////////////////////////////////////////
    // intended to provide reuse of textures but not very flexible yet
    //////////////////////////////////////////////////////////////////////////
    class vaTexturePool : public vaSingletonBase<vaTexturePool>
    {
    public:
        int                                     m_maxPooledTextureCount;
        int                                     m_maxMemoryUse;    

        struct ItemDesc
        {
            vaRenderDevice *                    Device;
            vaTextureFlags                      Flags;
            vaResourceAccessFlags               AccessFlags;
            vaTextureType                       Type;
            vaResourceBindSupportFlags          BindSupportFlags;
            vaResourceFormat                    ResourceFormat;
            vaResourceFormat                    SRVFormat;
            vaResourceFormat                    RTVFormat;
            vaResourceFormat                    DSVFormat;
            vaResourceFormat                    UAVFormat;
            int                                 SizeX;
            int                                 SizeY;
            int                                 SizeZ;
            int                                 SampleCount;
            int                                 MIPLevels;
        };

        struct ItemComparer
        {
            bool operator()( const ItemDesc & Left, const ItemDesc & Right ) const
            {
                // comparison logic goes here
                return memcmp( &Left, &Right, sizeof( Right ) ) < 0;
            }
        };

        std::multimap< ItemDesc, shared_ptr<vaTexture>, ItemComparer >
                                                m_items;

        std::mutex                              m_mutex;

    public:
        vaTexturePool( ) : m_maxPooledTextureCount( 64 ), m_maxMemoryUse( 64 * 1024 * 1024 ) { }
        virtual ~vaTexturePool( )               { }

    protected:
        void                                    FillDesc( ItemDesc & desc, const shared_ptr< vaTexture > texture );

    public:
        shared_ptr< vaTexture >                 FindOrCreate2D( vaRenderDevice & device, vaResourceFormat format, int width, int height, int mipLevels, int arraySize, int sampleCount, vaResourceBindSupportFlags bindFlags, vaResourceAccessFlags accessFlags = vaResourceAccessFlags::Default, void * initialData = NULL, int initialDataRowPitch = 0, vaResourceFormat srvFormat = vaResourceFormat::Automatic, vaResourceFormat rtvFormat = vaResourceFormat::Automatic, vaResourceFormat dsvFormat = vaResourceFormat::Automatic, vaResourceFormat uavFormat = vaResourceFormat::Automatic, vaTextureFlags flags = vaTextureFlags::None );
        
        // releases a texture into the pool so it can be returned by FindOrCreateXX
        void                                    Release( const shared_ptr< vaTexture > texture );

        void                                    ClearAll( );
    };


    // used for automatically updating CPU -> GPU textures in an API agnostic way; there is no handling for regions, arrays or anything clever, but there is support for MIP levels
    // to trigger upload, set "Modified" flag on vaTextureMappedSubresource before a call to TickGPU
    class vaTextureCPU2GPU : public vaRenderingModule
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    protected:
        vector<vaTextureMappedSubresource>            m_CPUData;
        int                                     m_CPUDataBytesPerPixel      = 0;

        shared_ptr< vaTexture >                 m_GPUTexture;
        shared_ptr< vaTexture >                 m_CPUTexture;

        bool                                    m_uploadRequested           = false;
        // TODO: implement direct map mode

    protected:
        vaTextureCPU2GPU( const vaRenderingModuleParams & params );
    
    public:
        virtual ~vaTextureCPU2GPU( );

    protected:
        bool                                    SetTexture( const shared_ptr< vaTexture > & gpuTexture );

    public:
        vaTextureMappedSubresource *                  GetSubresource( int mipIndex = 0, int arrayIndex = 0)   { assert( arrayIndex == 0 ); arrayIndex; return &m_CPUData[mipIndex]; }

        const shared_ptr< vaTexture > &         GetGPUTexture( ) const                                  { return m_GPUTexture; }

        void                                    RequestUpload( )                                        { m_uploadRequested = true; }

        // if CPU texture dirty, this will update to the GPU
        virtual void                            TickGPU( vaRenderDeviceContext & context )              = 0;

        vaResourceFormat                         GetResourceFormat( ) const                              { if( m_GPUTexture == nullptr ) return vaResourceFormat::Unknown; } 
        int                                     GetSizeX( )          const                              { if( m_GPUTexture == nullptr ) return 0; return m_GPUTexture->GetSizeX( )         ; } 
        int                                     GetSizeY( )          const                              { if( m_GPUTexture == nullptr ) return 0; return m_GPUTexture->GetSizeY( )         ; } 
        int                                     GetSizeZ( )          const                              { if( m_GPUTexture == nullptr ) return 0; return m_GPUTexture->GetSizeZ( )         ; } 
        int                                     GetSampleCount( )    const                              { if( m_GPUTexture == nullptr ) return 0; return m_GPUTexture->GetSampleCount( )   ; } 
        int                                     GetMipLevels( )      const                              { if( m_GPUTexture == nullptr ) return 0; return m_GPUTexture->GetMipLevels( )     ; } 


    public:
        static shared_ptr< vaTextureCPU2GPU>    CreateFromExisting( const shared_ptr< vaTexture > & gpuTexture );
        static shared_ptr< vaTextureCPU2GPU>    Create1D( vaRenderDevice & device, vaResourceFormat format, int width, int mipLevels, int arraySize, vaResourceBindSupportFlags gpuBindFlags, void * initialData = NULL, vaResourceFormat gpuSRVFormat = vaResourceFormat::Automatic, vaResourceFormat gpuRTVFormat = vaResourceFormat::Automatic, vaResourceFormat gpuDSVFormat = vaResourceFormat::Automatic, vaResourceFormat gpuUAVFormat = vaResourceFormat::Automatic, vaTextureFlags flags = vaTextureFlags::None );
        static shared_ptr< vaTextureCPU2GPU>    Create2D( vaRenderDevice & device, vaResourceFormat format, int width, int height, int mipLevels, int arraySize, int sampleCount, vaResourceBindSupportFlags gpuBindFlags, void * initialData = NULL, int initialDataRowPitch = 0, vaResourceFormat gpuSRVFormat = vaResourceFormat::Automatic, vaResourceFormat gpuRTVFormat = vaResourceFormat::Automatic, vaResourceFormat gpuDSVFormat = vaResourceFormat::Automatic, vaResourceFormat gpuUAVFormat = vaResourceFormat::Automatic, vaTextureFlags flags = vaTextureFlags::None );
        static shared_ptr< vaTextureCPU2GPU>    Create3D( vaRenderDevice & device, vaResourceFormat format, int width, int height, int depth, int mipLevels, vaResourceBindSupportFlags gpuBindFlags, void * initialData = NULL, int initialDataRowPitch = 0, int initialDataSlicePitch = 0, vaResourceFormat gpuSRVFormat = vaResourceFormat::Automatic, vaResourceFormat gpuRTVFormat = vaResourceFormat::Automatic, vaResourceFormat gpuDSVFormat = vaResourceFormat::Automatic, vaResourceFormat gpuUAVFormat = vaResourceFormat::Automatic, vaTextureFlags flags = vaTextureFlags::None );

    };


    // used for automatically downloading GPU -> CPU texture in an API agnostic way; there is no handling for regions, arrays or anything clever, but there is support for MIP levels
    // to trigger download, call RequestDownload before a call to TickGPU; once the data is 'downloaded' (might take multiple frames / TickGPU), the IsDownloadFinished will return true
    class vaTextureGPU2CPU : public vaRenderingModule
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    protected:
        bool                                    m_directMapMode                 = false;                    // does not create backup storage and copy memory but rather Map-s and keeps it available until Unmap is called; avoids memcopy but more tedious to work with
        bool                                    m_directMapped                  = false;
        bool                                    m_directMappedMarkForUnmap      = false;

        vector<vaTextureMappedSubresource>            m_CPUData;
        int                                     m_CPUDataBytesPerPixel          = 0;

        shared_ptr< vaTexture >                 m_CPUTexture;
        shared_ptr< vaTexture >                 m_GPUTexture;

        bool                                    m_downloadRequested             = false;
        int                                     m_ticksFromLastDownloadRequest  = 0;                         // practically, age of m_CPUData
        bool                                    m_downloadFinished              = false;
        vector<bool>                            m_CPUDataDownloaded;

        int                                     m_copyToMapDelayTicks           = 0;                        // don't try to map right after CopyResource, delay by x ticks

    protected:
        vaTextureGPU2CPU( const vaRenderingModuleParams & params );
    
    public:
        virtual ~vaTextureGPU2CPU( );

    protected:
        bool                                    SetTexture( bool directMapMode, const shared_ptr< vaTexture > & gpuTexture );

    public:
        vaTextureMappedSubresource *                  GetSubresource( int mipIndex = 0, int arrayIndex = 0)   { assert( arrayIndex == 0 ); arrayIndex; return &m_CPUData[mipIndex]; }

        const shared_ptr< vaTexture > &         GetGPUTexture( ) const                                  { return m_GPUTexture; }

        // if CPU texture dirty, this will update to the GPU
        virtual void                            TickGPU( vaRenderDeviceContext & context )              = 0;
        virtual void                            Unmap( vaRenderDeviceContext & context )                { context; } //= 0;

        vaResourceFormat                         GetResourceFormat( ) const                              { if( m_GPUTexture == nullptr ) return vaResourceFormat::Unknown; return m_GPUTexture->GetResourceFormat( ); } 
        int                                     GetSizeX( )          const                              { if( m_GPUTexture == nullptr ) return 0; return m_GPUTexture->GetSizeX( )         ; } 
        int                                     GetSizeY( )          const                              { if( m_GPUTexture == nullptr ) return 0; return m_GPUTexture->GetSizeY( )         ; } 
        int                                     GetSizeZ( )          const                              { if( m_GPUTexture == nullptr ) return 0; return m_GPUTexture->GetSizeZ( )         ; } 
        int                                     GetSampleCount( )    const                              { if( m_GPUTexture == nullptr ) return 0; return m_GPUTexture->GetSampleCount( )   ; } 
        int                                     GetMipLevels( )      const                              { if( m_GPUTexture == nullptr ) return 0; return m_GPUTexture->GetMipLevels( )     ; } 

        void                                    RequestDownload( );
        bool                                    IsDownloadRequested( ) const                            { return m_downloadRequested; }
        bool                                    IsDownloadFinished( ) const                             { return m_downloadFinished; }
        bool                                    IsMapped( ) const                                       { return m_directMapped; }              // only for direct mapping mode
        void                                    MarkForUnmap( );

        int                                     GetCopyToMapDelayTicks() const                          { return m_copyToMapDelayTicks; }
        void                                    SetCopyToMapDelayTicks( int copyToMapDelayTicks )       { m_copyToMapDelayTicks = copyToMapDelayTicks; assert( m_copyToMapDelayTicks >= 0 && m_copyToMapDelayTicks < 10 ); }

    public:
        static shared_ptr< vaTextureGPU2CPU >   CreateFromExisting( bool directMapMode, const shared_ptr< vaTexture > & gpuTexture );
        static shared_ptr< vaTextureGPU2CPU >   Create1D( vaRenderDevice & device, bool directMapMode, vaResourceFormat format, int width, int mipLevels, int arraySize, vaResourceBindSupportFlags gpuBindFlags, void * initialData = NULL, vaResourceFormat gpuSRVFormat = vaResourceFormat::Automatic, vaResourceFormat gpuRTVFormat = vaResourceFormat::Automatic, vaResourceFormat gpuDSVFormat = vaResourceFormat::Automatic, vaResourceFormat gpuUAVFormat = vaResourceFormat::Automatic, vaTextureFlags flags = vaTextureFlags::None );
        static shared_ptr< vaTextureGPU2CPU >   Create2D( vaRenderDevice & device, bool directMapMode, vaResourceFormat format, int width, int height, int mipLevels, int arraySize, int sampleCount, vaResourceBindSupportFlags gpuBindFlags, void * initialData = NULL, int initialDataRowPitch = 0, vaResourceFormat gpuSRVFormat = vaResourceFormat::Automatic, vaResourceFormat gpuRTVFormat = vaResourceFormat::Automatic, vaResourceFormat gpuDSVFormat = vaResourceFormat::Automatic, vaResourceFormat gpuUAVFormat = vaResourceFormat::Automatic, vaTextureFlags flags = vaTextureFlags::None );
        static shared_ptr< vaTextureGPU2CPU >   Create3D( vaRenderDevice & device, bool directMapMode, vaResourceFormat format, int width, int height, int depth, int mipLevels, vaResourceBindSupportFlags gpuBindFlags, void * initialData = NULL, int initialDataRowPitch = 0, int initialDataSlicePitch = 0, vaResourceFormat gpuSRVFormat = vaResourceFormat::Automatic, vaResourceFormat gpuRTVFormat = vaResourceFormat::Automatic, vaResourceFormat gpuDSVFormat = vaResourceFormat::Automatic, vaResourceFormat gpuUAVFormat = vaResourceFormat::Automatic, vaTextureFlags flags = vaTextureFlags::None );

    };

    //////////////////////////////////////////////////////////////////////////
    // Provides some common textures, utility functions, etc.
    //////////////////////////////////////////////////////////////////////////
    class vaTextureTools// : public vaSingletonBase<vaTextureTools>
    {
    public:
        enum class CommonTextureName
        {
            Black1x1,
            White1x1,
            Checkerboard16x16,
            Black1x1Cube,

            MaxValue
        };

        struct UITextureState
        {
            weak_ptr<vaTexture>         Texture;
            vaVector4                   ClipRectangle;  // x, y, width, height
            vaVector4                   Rectangle;      // x, y, width, height
            float                       Alpha;
            int                         ArrayIndex;

            bool                        FullscreenPopUp;

            bool                        InUse;
        };

    protected:
        shared_ptr< vaTexture >                 m_textures[(int)CommonTextureName::MaxValue];

        vector<UITextureState>                  m_UIDrawItems;

        vaTypedConstantBufferWrapper< UITextureDrawShaderConstants >
                                                m_UIDrawShaderConstants;

        vaAutoRMI<vaPixelShader>                m_UIDrawTexture2DPS;
        vaAutoRMI<vaPixelShader>                m_UIDrawTexture2DArrayPS;
        vaAutoRMI<vaPixelShader>                m_UIDrawTextureCubePS;

    public:
        vaTextureTools( vaRenderDevice & device );
        virtual ~vaTextureTools( ) { }

    public:

        shared_ptr< vaTexture >                 GetCommonTexture( CommonTextureName textureName );
        
        void                                    UIDrawImages( vaRenderDeviceContext & renderContext );

        void                                    UIDrawImGuiInfo( const shared_ptr<vaTexture> & texture );

    };

}