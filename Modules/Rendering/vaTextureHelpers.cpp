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

#include "vaTextureHelpers.h"

using namespace VertexAsylum;

void vaTexturePool::FillDesc( ItemDesc & desc, const shared_ptr< vaTexture > texture )
{
    desc.Flags              = texture->GetFlags();
    desc.AccessFlags        = texture->GetAccessFlags();
    desc.Type               = texture->GetType();
    desc.BindSupportFlags   = texture->GetBindSupportFlags();
    desc.ResourceFormat     = texture->GetResourceFormat();
    desc.SRVFormat          = texture->GetSRVFormat();
    desc.RTVFormat          = texture->GetRTVFormat();
    desc.DSVFormat          = texture->GetDSVFormat();
    desc.UAVFormat          = texture->GetUAVFormat();
    desc.SizeX              = texture->GetSizeX();
    desc.SizeY              = texture->GetSizeY();
    desc.SizeZ              = texture->GetSizeZ();
    desc.SampleCount        = texture->GetSampleCount();
    desc.MIPLevels          = texture->GetMipLevels();
}

shared_ptr< vaTexture > vaTexturePool::FindOrCreate2D( vaRenderDevice & device, vaResourceFormat format, int width, int height, int mipLevels, int arraySize, int sampleCount, vaResourceBindSupportFlags bindFlags, vaTextureAccessFlags accessFlags, void * initialData, int initialDataPitch, vaResourceFormat srvFormat, vaResourceFormat rtvFormat, vaResourceFormat dsvFormat, vaResourceFormat uavFormat, vaTextureFlags flags )
{
    std::unique_lock<std::mutex> lock( m_mutex );

    ItemDesc desc;
    desc.Type = vaTextureType::Texture2D;
    if( (arraySize > 1 ) && (sampleCount == 1) ) desc.Type = vaTextureType::Texture2DArray;
    if( (arraySize == 1) && (sampleCount > 1 ) ) desc.Type = vaTextureType::Texture2DMS;
    if( (arraySize > 1 ) && (sampleCount > 1 ) ) desc.Type = vaTextureType::Texture2DMSArray;

    if( (srvFormat == vaResourceFormat::Automatic) && ((bindFlags & vaResourceBindSupportFlags::ShaderResource) != 0) )
        srvFormat = format;
    if( (rtvFormat == vaResourceFormat::Automatic) && ((bindFlags & vaResourceBindSupportFlags::RenderTarget) != 0) )
        rtvFormat = format;
    if( (dsvFormat == vaResourceFormat::Automatic) && ((bindFlags & vaResourceBindSupportFlags::DepthStencil) != 0) )
        dsvFormat = format;
    if( (uavFormat == vaResourceFormat::Automatic) && ((bindFlags & vaResourceBindSupportFlags::UnorderedAccess) != 0) )
        uavFormat = format;

    desc.Device             = &device;
    desc.Flags              = flags;
    desc.AccessFlags        = accessFlags;
    desc.BindSupportFlags   = bindFlags;
    desc.ResourceFormat     = format;
    desc.SRVFormat          = srvFormat;
    desc.RTVFormat          = rtvFormat;
    desc.DSVFormat          = dsvFormat;
    desc.UAVFormat          = uavFormat;
    desc.SizeX              = width;
    desc.SizeY              = height;
    desc.SizeZ              = arraySize;
    desc.SampleCount        = sampleCount;
    desc.MIPLevels          = mipLevels;

    shared_ptr< vaTexture > retTexture;

    auto it = m_items.find( desc );
    if( it != m_items.end( ) )
    {
        retTexture = it->second;
        m_items.erase( it );
    }
    else
    {
        retTexture = vaTexture::Create2D( device, format, width, height, mipLevels, arraySize, sampleCount, bindFlags, accessFlags, srvFormat, rtvFormat, dsvFormat, uavFormat, flags, vaTextureContentsType::GenericColor, initialData, initialDataPitch );

#ifdef _DEBUG
        ItemDesc testDesc;
        FillDesc( testDesc, retTexture );
        if( memcmp( &testDesc, &desc, sizeof(desc) ) )
        {
            // there's a mismatch in desc creation above and actual texture desc : you need to find and correct it; if it's platform/API dependent then add a platform dependent implementation
            assert( false );
        }
#endif
    }

    return retTexture;
}

void vaTexturePool::Release( const shared_ptr< vaTexture > texture )
{
    std::unique_lock<std::mutex> lock( m_mutex );

    assert( texture != nullptr );
    if( texture == nullptr )
        return;

    ItemDesc desc;
    FillDesc( desc, texture );

    if( m_items.size( ) > m_maxPooledTextureCount )
    {
        //assert( false );
        // need to track memory use and start dropping 'oldest' (or just random one?) when over the limit
//        return;
        m_items.erase( m_items.begin() );
    }

    m_items.insert( std::make_pair( desc, texture ) );
}

void vaTexturePool::ClearAll( )
{ 
    m_items.clear(); 
}

vaTextureCPU2GPU::vaTextureCPU2GPU( const vaRenderingModuleParams & params ) : vaRenderingModule( params )
{
    m_uploadRequested = false;
}

vaTextureCPU2GPU::~vaTextureCPU2GPU( )
{

}

bool vaTextureCPU2GPU::SetTexture( const shared_ptr< vaTexture > & gpuTexture )
{
    VA_VERIFY_RETURN_IF_FALSE( gpuTexture != nullptr, "input parameter null" );

    assert( m_GPUTexture == nullptr );

    //VA_VERIFY_RETURN_IF_FALSE( gpuTexture->GetAccessFlags() == vaTextureAccessFlags::CPUWrite, "input parameter wrong" );
    m_GPUTexture = gpuTexture;

    m_CPUData.clear();
    m_CPUData.resize( gpuTexture->GetMipLevels() );

    m_CPUDataBytesPerPixel = vaResourceFormatHelpers::GetPixelSizeInBytes( gpuTexture->GetResourceFormat() );
    if( m_CPUDataBytesPerPixel == 0 )
    {
        // unsupported (yet)
        assert( false );
        m_CPUData.clear();
        return false;
    }
    if( gpuTexture->GetType() != vaTextureType::Texture2D )
    {
        // unsupported (yet)
        return false;
    }

    int sizeX = gpuTexture->GetSizeX();
    int sizeY = gpuTexture->GetSizeY();
    for( int i = 0; i < gpuTexture->GetMipLevels(); i++ )
    {
        m_CPUData[i].SizeX          = sizeX;
        m_CPUData[i].SizeY          = sizeY;
        m_CPUData[i].BytesPerPixel  = m_CPUDataBytesPerPixel;
        m_CPUData[i].RowPitch       = sizeX * m_CPUDataBytesPerPixel;
        m_CPUData[i].SizeInBytes    = (int64)sizeY * (int64)m_CPUData[i].RowPitch;
        m_CPUData[i].DepthPitch     = 0;
        m_CPUData[i].Buffer         = new byte[ m_CPUData[i].SizeInBytes ];

        if( i < gpuTexture->GetMipLevels()-1 )
        {
            // must be divisible by 2; DirectX will just drop the last bit but depending on your filter it means data loss so we don't allow it here (for now)
            assert( ( sizeX % 2 ) == 0 );
            assert( ( sizeY % 2 ) == 0 );
            sizeX = sizeX / 2;
            sizeY = sizeY / 2;
        }
    }

    m_uploadRequested = false;

    return true;
}

shared_ptr< vaTextureCPU2GPU > vaTextureCPU2GPU::CreateFromExisting( const shared_ptr< vaTexture > & gpuTexture )
{
    // using UpdateSubresource now, so this isn't needed. Yeah, it's messy, and if it's different on other APIs, make this function virtual and handle it separately
    // if( gpuTexture->GetAccessFlags( ) != vaTextureAccessFlags::CPUWrite )
    // {
    //     assert( false );
    //     return nullptr;
    // }

    shared_ptr< vaTextureCPU2GPU > ret = VA_RENDERING_MODULE_CREATE_SHARED( vaTextureCPU2GPU, gpuTexture->GetRenderDevice() );

    if( ret->SetTexture( gpuTexture ) )
        return ret;
    else
        return nullptr;
}

shared_ptr< vaTextureCPU2GPU> vaTextureCPU2GPU::Create1D( vaRenderDevice & device, vaResourceFormat format, int width, int mipLevels, int arraySize, vaResourceBindSupportFlags gpuBindFlags, void * initialData, vaResourceFormat gpuSRVFormat, vaResourceFormat gpuRTVFormat, vaResourceFormat gpuDSVFormat, vaResourceFormat gpuUAVFormat, vaTextureFlags flags )
{
    if( ( gpuBindFlags & ( vaResourceBindSupportFlags::RenderTarget | vaResourceBindSupportFlags::DepthStencil | vaResourceBindSupportFlags::UnorderedAccess ) ) != 0 )
    {
        VA_ERROR( "Unsupported bind flags" );
        return nullptr;
    }

    device; format; width; mipLevels; arraySize; gpuBindFlags; initialData; gpuSRVFormat; gpuRTVFormat; gpuDSVFormat; gpuUAVFormat; flags;

    assert( false ); // not yet implemented
    return nullptr;
}

shared_ptr< vaTextureCPU2GPU> vaTextureCPU2GPU::Create2D( vaRenderDevice & device, vaResourceFormat format, int width, int height, int mipLevels, int arraySize, int sampleCount, vaResourceBindSupportFlags gpuBindFlags, void * initialData, int initialDataPitch, vaResourceFormat gpuSRVFormat, vaResourceFormat gpuRTVFormat, vaResourceFormat gpuDSVFormat, vaResourceFormat gpuUAVFormat, vaTextureFlags flags )
{
    if( ( gpuBindFlags & ( vaResourceBindSupportFlags::RenderTarget | vaResourceBindSupportFlags::DepthStencil | vaResourceBindSupportFlags::UnorderedAccess ) ) != 0 )
    {
        VA_ERROR( "Unsupported bind flags" );
        return nullptr;
    }
    assert( arraySize == 1 );
    assert( sampleCount == 1 );

    shared_ptr< vaTexture > gpuTexture( vaTexture::Create2D( device, format, width, height, mipLevels, arraySize, sampleCount, gpuBindFlags, vaTextureAccessFlags::None/*vaTextureAccessFlags::CPUWrite*/, gpuSRVFormat, gpuRTVFormat, gpuDSVFormat, gpuUAVFormat, flags, vaTextureContentsType::GenericColor, initialData, initialDataPitch ) );
    
    return CreateFromExisting( gpuTexture );
}

shared_ptr< vaTextureCPU2GPU> vaTextureCPU2GPU::Create3D( vaRenderDevice & device, vaResourceFormat format, int width, int height, int depth, int mipLevels, vaResourceBindSupportFlags gpuBindFlags, void * initialData, int initialDataPitch, int initialDataSlicePitch, vaResourceFormat gpuSRVFormat, vaResourceFormat gpuRTVFormat, vaResourceFormat gpuDSVFormat, vaResourceFormat gpuUAVFormat, vaTextureFlags flags )
{
    if( ( gpuBindFlags & ( vaResourceBindSupportFlags::RenderTarget | vaResourceBindSupportFlags::DepthStencil | vaResourceBindSupportFlags::UnorderedAccess ) ) != 0 )
    {
        VA_ERROR( "Unsupported bind flags" );
        return nullptr;
    }

    device; format; width; height; depth; mipLevels; gpuBindFlags; initialData; initialDataPitch; initialDataSlicePitch; gpuSRVFormat; gpuRTVFormat; gpuDSVFormat; gpuUAVFormat; flags;

    assert( false ); // not yet implemented
    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

vaTextureGPU2CPU::vaTextureGPU2CPU( const vaRenderingModuleParams & params ) : vaRenderingModule( params )
{
    m_downloadRequested             = false; 
    m_downloadFinished              = false;
    m_directMapped                  = false;
    m_directMappedMarkForUnmap      = false;
    m_ticksFromLastDownloadRequest  = 0;
    m_copyToMapDelayTicks           = 0;
}

vaTextureGPU2CPU::~vaTextureGPU2CPU( )
{
    assert( !m_directMapped );
}

bool vaTextureGPU2CPU::SetTexture( bool directMapMode, const shared_ptr< vaTexture > & gpuTexture )
{
    VA_VERIFY_RETURN_IF_FALSE( gpuTexture != nullptr, "input parameter null" );

    assert( m_CPUTexture == nullptr );
    assert( m_GPUTexture == nullptr );

    m_directMapMode = directMapMode;

    //VA_VERIFY_RETURN_IF_FALSE( gpuTexture->GetAccessFlags() == vaTextureAccessFlags::CPUWrite, "input parameter wrong" );
    m_GPUTexture = gpuTexture;

    m_CPUData.clear();
    m_CPUData.resize( gpuTexture->GetMipLevels() );
    m_CPUDataDownloaded.resize( gpuTexture->GetMipLevels() );

    for( int i = 0; i < m_CPUDataDownloaded.size(); i++ ) m_CPUDataDownloaded[i] = false;

    m_CPUDataBytesPerPixel = vaResourceFormatHelpers::GetPixelSizeInBytes( gpuTexture->GetResourceFormat() );
    if( m_CPUDataBytesPerPixel == 0 )
    {
        // unsupported (yet)
        assert( false );
        m_CPUData.clear();
        return false;
    }
    if( gpuTexture->GetType() != vaTextureType::Texture2D )
    {
        // unsupported (yet)
        return false;
    }

    int sizeX = gpuTexture->GetSizeX();
    int sizeY = gpuTexture->GetSizeY();
    for( int i = 0; i < gpuTexture->GetMipLevels(); i++ )
    {
        m_CPUData[i].SizeX          = sizeX;
        m_CPUData[i].SizeY          = sizeY;
        m_CPUData[i].BytesPerPixel  = m_CPUDataBytesPerPixel;

        if( !m_directMapMode )
        {
            m_CPUData[i].RowPitch       = sizeX * m_CPUDataBytesPerPixel;
            m_CPUData[i].SizeInBytes    = (int64)sizeY * (int64)m_CPUData[i].RowPitch;
            m_CPUData[i].DepthPitch     = 0;
            m_CPUData[i].Buffer         = new byte[ m_CPUData[i].SizeInBytes ];
        }
        else
        {
            m_CPUData[i].RowPitch       = 0;
            m_CPUData[i].SizeInBytes    = 0;
            m_CPUData[i].DepthPitch     = 0;
            m_CPUData[i].Buffer         = nullptr;
        }
        
        if( i < gpuTexture->GetMipLevels()-1 )
        {
            // must be divisible by 2; DirectX will just drop the last bit but depending on your filter it means data loss so we don't allow it here (for now)
            assert( ( sizeX % 2 ) == 0 );
            assert( ( sizeY % 2 ) == 0 );
            sizeX = sizeX / 2;
            sizeY = sizeY / 2;
        }
    }

    if( directMapMode && gpuTexture->GetBindSupportFlags() == vaResourceBindSupportFlags::None )
        m_CPUTexture = m_GPUTexture;
    else
        m_CPUTexture =  vaTexture::Create2D( GetRenderDevice(), gpuTexture->GetResourceFormat(), gpuTexture->GetSizeX(), gpuTexture->GetSizeY(), gpuTexture->GetMipLevels(), gpuTexture->GetSizeZ(), gpuTexture->GetSampleCount(), vaResourceBindSupportFlags::None, vaTextureAccessFlags::CPURead, vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaResourceFormat::Automatic, gpuTexture->GetFlags(), vaTextureContentsType::GenericColor, nullptr, 0 );

    m_directMappedMarkForUnmap = false;

    return true;
}

void vaTextureGPU2CPU::RequestDownload( )
{ 
    if( m_directMapMode )
    {
        if( m_directMapped )
        {
            assert( false );
            return;
        }
        if( m_directMappedMarkForUnmap )
        {
            assert( false );
            return;
        }
    }

    assert(!m_downloadRequested); 
    m_downloadRequested = true; 
    m_downloadFinished = false; 
    m_ticksFromLastDownloadRequest = 0; 
    for( int i = 0; i < m_CPUDataDownloaded.size(); i++ ) 
        m_CPUDataDownloaded[i] = false; 
}

void vaTextureGPU2CPU::MarkForUnmap( )
{
    // ReleaseDirectMapped only works in DirectMapMode
    if( !m_directMapMode )
    {
        assert( false );
        return;
    }
    if( !m_directMapped )
    {
        assert( false );
        return;
    }
    m_directMappedMarkForUnmap = true;
}

shared_ptr< vaTextureGPU2CPU > vaTextureGPU2CPU::CreateFromExisting( bool directMapMode, const shared_ptr< vaTexture > & gpuTexture )
{
    // using UpdateSubresource now, so this isn't needed. Yeah, it's messy, and if it's different on other APIs, make this function virtual and handle it separately
    // if( gpuTexture->GetAccessFlags( ) != vaTextureAccessFlags::CPUWrite )
    // {
    //     assert( false );
    //     return nullptr;
    // }

    shared_ptr< vaTextureGPU2CPU > ret = VA_RENDERING_MODULE_CREATE_SHARED( vaTextureGPU2CPU, gpuTexture->GetRenderDevice() );

    if( ret->SetTexture( directMapMode, gpuTexture ) )
        return ret;
    else
        return nullptr;
}

shared_ptr< vaTextureGPU2CPU > vaTextureGPU2CPU::Create1D( vaRenderDevice & device, bool directMapMode, vaResourceFormat format, int width, int mipLevels, int arraySize, vaResourceBindSupportFlags gpuBindFlags, void * initialData, vaResourceFormat gpuSRVFormat, vaResourceFormat gpuRTVFormat, vaResourceFormat gpuDSVFormat, vaResourceFormat gpuUAVFormat, vaTextureFlags flags )
{
//    if( ( gpuBindFlags & ( vaResourceBindSupportFlags::RenderTarget | vaResourceBindSupportFlags::DepthStencil | vaResourceBindSupportFlags::UnorderedAccess ) ) != 0 )
//    {
//        VA_ERROR( "Unsupported bind flags" );
//        return nullptr;
//    }
    device; directMapMode; format; width; mipLevels; arraySize; gpuBindFlags; initialData; gpuSRVFormat; gpuRTVFormat; gpuDSVFormat; gpuUAVFormat; flags;

    assert( false ); // not yet implemented
    return nullptr;
}

shared_ptr< vaTextureGPU2CPU > vaTextureGPU2CPU::Create2D( vaRenderDevice & device, bool directMapMode, vaResourceFormat format, int width, int height, int mipLevels, int arraySize, int sampleCount, vaResourceBindSupportFlags gpuBindFlags, void * initialData, int initialDataPitch, vaResourceFormat gpuSRVFormat, vaResourceFormat gpuRTVFormat, vaResourceFormat gpuDSVFormat, vaResourceFormat gpuUAVFormat, vaTextureFlags flags )
{
//    if( ( gpuBindFlags & ( vaResourceBindSupportFlags::RenderTarget | vaResourceBindSupportFlags::DepthStencil | vaResourceBindSupportFlags::UnorderedAccess ) ) != 0 )
//    {
//        VA_ERROR( "Unsupported bind flags" );
//        return nullptr;
//    }
    assert( arraySize == 1 );
    assert( sampleCount == 1 );

    vaTextureAccessFlags accessFlags = vaTextureAccessFlags::None;

    if( directMapMode && gpuBindFlags == vaResourceBindSupportFlags::None )
    {
        accessFlags = vaTextureAccessFlags::CPURead;
    }

    shared_ptr< vaTexture > gpuTexture( vaTexture::Create2D( device, format, width, height, mipLevels, arraySize, sampleCount, gpuBindFlags, accessFlags, gpuSRVFormat, gpuRTVFormat, gpuDSVFormat, gpuUAVFormat, flags, vaTextureContentsType::GenericColor, initialData, initialDataPitch ) );
    
    return CreateFromExisting( directMapMode, gpuTexture );
}

shared_ptr< vaTextureGPU2CPU > vaTextureGPU2CPU::Create3D( vaRenderDevice & device, bool directMapMode, vaResourceFormat format, int width, int height, int depth, int mipLevels, vaResourceBindSupportFlags gpuBindFlags, void * initialData, int initialDataPitch, int initialDataSlicePitch, vaResourceFormat gpuSRVFormat, vaResourceFormat gpuRTVFormat, vaResourceFormat gpuDSVFormat, vaResourceFormat gpuUAVFormat, vaTextureFlags flags )
{
//    if( ( gpuBindFlags & ( vaResourceBindSupportFlags::RenderTarget | vaResourceBindSupportFlags::DepthStencil | vaResourceBindSupportFlags::UnorderedAccess ) ) != 0 )
//    {
//        VA_ERROR( "Unsupported bind flags" );
//        return nullptr;
//    }

    device; directMapMode; format; width; height; depth; mipLevels; gpuBindFlags; initialData; initialDataPitch; initialDataSlicePitch; gpuSRVFormat; gpuRTVFormat; gpuDSVFormat; gpuUAVFormat; flags;

    assert( false ); // not yet implemented
    return nullptr;
}

vaTextureTools::vaTextureTools( vaRenderDevice & device ) //: vaRenderingModule( vaRenderingModuleParams(device) )
    :
    m_UIDrawShaderConstants( device ),
    m_UIDrawTexture2DPS( device ),
    m_UIDrawTexture2DArrayPS( device ),
    m_UIDrawTextureCubePS( device )
{
    {
        uint32 initialData = 0x00000000;
        m_textures[(int)CommonTextureName::Black1x1] = vaTexture::Create2D( device, vaResourceFormat::R8G8B8A8_UNORM, 1, 1, 1, 1, 1, vaResourceBindSupportFlags::ShaderResource, vaTextureAccessFlags::None, vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaTextureFlags::None, vaTextureContentsType::GenericColor, &initialData, sizeof( initialData ) );
    }
    {
        uint32 initialData = 0xFFFFFFFF;
        m_textures[(int)CommonTextureName::White1x1] = vaTexture::Create2D( device, vaResourceFormat::R8G8B8A8_UNORM, 1, 1, 1, 1, 1, vaResourceBindSupportFlags::ShaderResource, vaTextureAccessFlags::None, vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaTextureFlags::None, vaTextureContentsType::GenericColor, &initialData, sizeof( initialData ) );
    }

    {
        uint32 initialData[16*16];
        for( int y = 0; y < 16; y++ )
            for( int x = 0; x < 16; x++ )
            {
                initialData[ y * 16 + x ] = (((x+y)%2) == 0)?(0xFFFFFFFF):(0x00000000);
            }
        m_textures[(int)CommonTextureName::Checkerboard16x16] = vaTexture::Create2D( device, vaResourceFormat::R8G8B8A8_UNORM, 16, 16, 1, 1, 1, vaResourceBindSupportFlags::ShaderResource, vaTextureAccessFlags::None, vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaTextureFlags::None, vaTextureContentsType::GenericColor, initialData, 16*sizeof( uint32 ) );
    }


    m_UIDrawTexture2DPS->CreateShaderFromFile( "vaHelperTools.hlsl", "ps_5_0", "UIDrawTexture2DPS" );
    m_UIDrawTexture2DArrayPS->CreateShaderFromFile( "vaHelperTools.hlsl", "ps_5_0", "UIDrawTexture2DArrayPS" );
    m_UIDrawTextureCubePS->CreateShaderFromFile( "vaHelperTools.hlsl", "ps_5_0", "UIDrawTextureCubePS" );
}

shared_ptr< vaTexture > vaTextureTools::GetCommonTexture( CommonTextureName textureName )
{
    int index = (int)textureName;
    if( index < 0 || index >= _countof(m_textures) )
        return nullptr;
    return m_textures[index];
}

void vaTextureTools::UIDrawImages( vaRenderDeviceContext & apiContext )
{
    if( m_UIDrawItems.size( ) == 0 )
        return;


    // everything is ok, this path was just never debug-tested so do it now :)
    assert( false ); 

    vaRenderItem renderItem;

    apiContext.BeginItems();

    apiContext.FillFullscreenPassRenderItem(renderItem);

    renderItem.ConstantBuffers[TEXTURE_UI_DRAW_TOOL_BUFFERSLOT] = m_UIDrawShaderConstants;

    // m_UIDrawShaderConstants.SetToAPISlot( apiContext, TEXTURE_UI_DRAW_TOOL_BUFFERSLOT );

    for( int i = (int)m_UIDrawItems.size( )-1; i >= 0; i-- )
    {
        UITextureState & item = m_UIDrawItems[i];

        shared_ptr<vaTexture> texture = item.Texture.lock();
        if( texture == nullptr )
            continue;

        UITextureDrawShaderConstants consts; memset( &consts, 0, sizeof( consts ) );
        consts.ClipRect             = item.ClipRectangle;
        consts.DestinationRect      = item.Rectangle;
        consts.Alpha                = item.Alpha;
        consts.TextureArrayIndex    = item.ArrayIndex;
        consts.ContentsType         = (int)texture->GetContentsType();

        m_UIDrawShaderConstants.Update( apiContext, consts );

        // texture->SetToAPISlotSRV( apiContext, TEXTURE_UI_DRAW_TOOL_TEXTURE_SLOT0 );
        renderItem.ShaderResourceViews[TEXTURE_UI_DRAW_TOOL_TEXTURE_SLOT0] = texture;
        renderItem.BlendMode = vaBlendMode::AlphaBlend;

        if( texture->GetType() == vaTextureType::Texture2D )
            //apiContext.FullscreenPassDraw( *m_UIDrawTexture2DPS, vaBlendMode::AlphaBlend );
            renderItem.PixelShader = m_UIDrawTexture2DPS;
        else if( texture->GetType() == vaTextureType::Texture2DArray )
        {
            if( ((texture->GetFlags() & vaTextureFlags::Cubemap) != 0) && ((texture->GetFlags() & vaTextureFlags::CubemapButArraySRV) == 0) )
                //apiContext.FullscreenPassDraw( *m_UIDrawTextureCubePS, vaBlendMode::AlphaBlend );
                renderItem.PixelShader = m_UIDrawTextureCubePS;
            else
                //apiContext.FullscreenPassDraw( *m_UIDrawTexture2DArrayPS, vaBlendMode::AlphaBlend );
                renderItem.PixelShader = m_UIDrawTexture2DArrayPS;
        }
        else
        {
            // not yet implemented
            assert( false );
        }

        apiContext.ExecuteItem( renderItem );

        //texture->UnsetFromAPISlotSRV( apiContext, TEXTURE_UI_DRAW_TOOL_TEXTURE_SLOT0 );

        if( item.InUse )
            item.InUse = false;
        else
            m_UIDrawItems.erase( m_UIDrawItems.begin() + i );
    }

    apiContext.EndItems();
    
    //m_UIDrawShaderConstants.UnsetFromAPISlot( apiContext, TEXTURE_UI_DRAW_TOOL_BUFFERSLOT );

    //apiContext.UnsetStandardSamplers();
}

void vaTextureTools::UIDrawImGuiInfo( const shared_ptr<vaTexture> & texture )
{
    ImGui::PushID( &(*texture) );

    // look up previous frame draws for persistence
    int stateIndexFound = -1;
    for( int i = 0; i < m_UIDrawItems.size(); i++ )
    {
        if( m_UIDrawItems[i].Texture.lock() == texture )
        {
            stateIndexFound = i;
            assert( !m_UIDrawItems[i].InUse );  // multiple UI elements using the same item - time to upgrade 'bool InUse' to 'uint64 userID' or similar for multi-item support
            break;
        }
    }
    if( stateIndexFound == -1 )
    {
        UITextureState newItem;
        newItem.Texture         = texture;
        newItem.Alpha           = 1.0f;
        newItem.ArrayIndex      = 0;
        newItem.FullscreenPopUp = false;
        m_UIDrawItems.push_back( newItem );
        stateIndexFound = (int)m_UIDrawItems.size()-1;
    }
    UITextureState & uiState = m_UIDrawItems[stateIndexFound];
    uiState.InUse = true;

    if( ImGui::Button( "Texture View", ImVec2( ImGui::GetContentRegionAvailWidth( ), 0 ) ) )
    {
        uiState.FullscreenPopUp = !uiState.FullscreenPopUp;
        if( uiState.FullscreenPopUp )
            ImGui::OpenPopup( "Texture View" );
    }

    float columnSize = ImGui::GetContentRegionAvailWidth( ) * 2.0f / 4.0f;

    ImGui::SetNextWindowSize( ImVec2( ImGui::GetIO( ).DisplaySize.x * 0.8f, ImGui::GetIO( ).DisplaySize.y * 0.8f ), ImGuiCond_Always );
    bool popupOpen = ImGui::BeginPopupModal( "Texture View", &uiState.FullscreenPopUp );

    ImGui::Text( "Dimensions:" );
    ImGui::SameLine( columnSize );
    ImGui::Text( "%d x %d x %d, %d MIPs", texture->GetSizeX( ), texture->GetSizeY( ), texture->GetSizeZ( ), texture->GetMipLevels( ) );

    ImGui::Text( "Format (res/SRV):" );
    ImGui::SameLine( columnSize );
    ImGui::Text( "%s", vaResourceFormatHelpers::EnumToString( texture->GetResourceFormat( ) ).c_str( ), vaResourceFormatHelpers::EnumToString( texture->GetSRVFormat( ) ).c_str( ) );

    string contentsTypeInfo;
    switch( texture->GetContentsType( ) )
    {
    case vaTextureContentsType::GenericColor:           contentsTypeInfo = "GenericColor";              break;
    case vaTextureContentsType::GenericLinear:          contentsTypeInfo = "GenericLinear";             break;
    case vaTextureContentsType::NormalsXYZ_UNORM:       contentsTypeInfo = "NormalsXYZ_UNORM";          break;
    case vaTextureContentsType::NormalsXY_UNORM:        contentsTypeInfo = "NormalsXY_UNORM";           break;
    case vaTextureContentsType::NormalsWY_UNORM:        contentsTypeInfo = "NormalsWY_UNORM";           break;
    case vaTextureContentsType::SingleChannelLinearMask:contentsTypeInfo = "SingleChannelLinearMask";   break;
    case vaTextureContentsType::DepthBuffer:            contentsTypeInfo = "DepthBuffer";               break;
    case vaTextureContentsType::LinearDepth:            contentsTypeInfo = "LinearDepth";               break;
    default:                                            contentsTypeInfo = "Unknown";                   break;
    }
    ImGui::Text( "Contents type:" );
    ImGui::SameLine( columnSize );
    ImGui::Text( contentsTypeInfo.c_str( ) );
    if( texture->IsView( ) )
    {
        ImGui::Text( "View, MIP" );
        ImGui::SameLine( columnSize );
        ImGui::Text( "from %d, count %d", texture->GetViewedMipSlice(), texture->GetViewedMipSliceCount() );
        ImGui::Text( "View, array" );
        ImGui::SameLine( columnSize );
        ImGui::Text( "from %d, count %d", texture->GetViewedArraySlice(), texture->GetViewedArraySliceCount() );
    }

    // display texture here
    //if( enqueueImageDraw )
    {
        auto availSize = ImGui::GetContentRegionAvail( );
        availSize.y -= 40; // space for array index and mip controls
        float texAspect = (float)texture->GetSizeX() / (float)texture->GetSizeY();
        if( texAspect > availSize.x / availSize.y )
        {
            uiState.Rectangle.z = availSize.x;
            uiState.Rectangle.w = availSize.x / texAspect;
        }
        else
        {
            uiState.Rectangle.z = availSize.y / texAspect;
            uiState.Rectangle.w = availSize.y;
        }


        bool clicked = ImGui::Selectable( "##dummy", false, 0, ImVec2(uiState.Rectangle.z, uiState.Rectangle.w) );
        clicked;

        uiState.ClipRectangle.x = ImGui::GetCurrentWindowRead( )->ClipRect.Min.x;
        uiState.ClipRectangle.y = ImGui::GetCurrentWindowRead( )->ClipRect.Min.y;
        uiState.ClipRectangle.z = ImGui::GetCurrentWindowRead( )->ClipRect.Max.x - ImGui::GetCurrentWindowRead( )->ClipRect.Min.x;
        uiState.ClipRectangle.w = ImGui::GetCurrentWindowRead( )->ClipRect.Max.y - ImGui::GetCurrentWindowRead( )->ClipRect.Min.y;
        
        uiState.Rectangle.x = ImGui::GetItemRectMin().x;
        uiState.Rectangle.y = ImGui::GetItemRectMin().y;
        uiState.Rectangle.z = ImGui::GetItemRectSize( ).x;
        uiState.Rectangle.w = ImGui::GetItemRectSize( ).y;
        if( texture->GetSizeZ( ) > 1 )
            ImGui::InputInt( "Array index", &uiState.ArrayIndex, 1 );
        uiState.ArrayIndex = vaMath::Clamp( uiState.ArrayIndex, 0, texture->GetViewedArraySliceCount( )-1 );
        ImGui::Text( "(viewing specific MIP not yet implemented)" );
    }

    if( popupOpen )
    {
        if( !uiState.FullscreenPopUp )
            ImGui::CloseCurrentPopup( );

        ImGui::EndPopup();
    }
    uiState.FullscreenPopUp = ImGui::IsPopupOpen("Texture View");

    ImGui::PopID();
}
