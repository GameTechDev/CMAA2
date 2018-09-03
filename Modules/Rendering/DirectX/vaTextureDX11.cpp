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

#include "vaTextureDX11.h"

#include "vaRenderingToolsDX11.h"

#include "Rendering/DirectX/vaRenderDeviceContextDX11.h"

#include "IntegratedExternals/DirectXTex/DirectXTex/DirectXTex.h"

#include "Core/System/vaFileTools.h"

#include "Core/Misc/vaProfiler.h"

using namespace VertexAsylum;

static D3D11_USAGE DX11UsageFromVAAccessFlags( vaTextureAccessFlags flags )
{
    if( ( ( flags & vaTextureAccessFlags::CPURead ) != 0 ) && ( ( flags & vaTextureAccessFlags::CPUWrite ) != 0 ) )
    {
        // wrong combination - both read and write at the same time not supported by VA
        assert( false );
        return D3D11_USAGE_DEFAULT;
    }
    if( ( flags & vaTextureAccessFlags::CPURead ) != 0 )
        return D3D11_USAGE_STAGING;
    else if( ( flags & vaTextureAccessFlags::CPUWrite ) != 0 )
        return D3D11_USAGE_DYNAMIC;
    else
        return D3D11_USAGE_DEFAULT;
}

#pragma warning( suppress : 4505 ) //  unreferenced local function has been removed
static vaTextureAccessFlags CPUAccessFlagsVAFromDX( UINT accessFlags )
{
    vaTextureAccessFlags ret = vaTextureAccessFlags::None;
    if( ( accessFlags & D3D11_CPU_ACCESS_READ ) != 0 )
        ret |= vaTextureAccessFlags::CPURead;
    if( ( accessFlags & D3D11_CPU_ACCESS_WRITE ) != 0 )
        ret |= vaTextureAccessFlags::CPUWrite;
    return ret;
}

static UINT CPUAccessFlagsDXFromVA( vaTextureAccessFlags accessFlags )
{
    UINT ret = 0;
    if( ( accessFlags & vaTextureAccessFlags::CPURead ) != 0 )
        ret |= D3D11_CPU_ACCESS_READ;
    if( ( accessFlags & vaTextureAccessFlags::CPUWrite ) != 0 )
        ret |= D3D11_CPU_ACCESS_WRITE;
    return ret;
}

static D3D11_MAP MapTypeDXFromVA( vaResourceMapType mapType )
{
    switch( mapType )
    {
    case VertexAsylum::vaResourceMapType::Read:              return D3D11_MAP_READ;
    case VertexAsylum::vaResourceMapType::Write:             return D3D11_MAP_WRITE;
    case VertexAsylum::vaResourceMapType::ReadWrite:         return D3D11_MAP_READ_WRITE;
    case VertexAsylum::vaResourceMapType::WriteDiscard:      return D3D11_MAP_WRITE_DISCARD;
    case VertexAsylum::vaResourceMapType::WriteNoOverwrite:  return D3D11_MAP_WRITE_NO_OVERWRITE;
    default:
        assert( false );
        return (D3D11_MAP)0;
        break;
    }
}

shared_ptr<vaTexture> vaTextureDX11::CreateWrap( vaRenderDevice & renderDevice, ID3D11Resource * resource, vaResourceFormat srvFormat, vaResourceFormat rtvFormat, vaResourceFormat dsvFormat, vaResourceFormat uavFormat, vaTextureContentsType contentsType )
{
    D3D11_RESOURCE_DIMENSION dim;
    resource->GetType( &dim );

    UINT bindFlags       = 0;

    vaResourceFormat resourceFormat = vaResourceFormat::Automatic;

    // get bind flags and format - needed at this point
    switch( dim )
    {
        case D3D11_RESOURCE_DIMENSION_BUFFER:
        {
            ID3D11Buffer * buffer = vaDirectXTools::QueryResourceInterface<ID3D11Buffer>( resource, IID_ID3D11Buffer );        
            assert( buffer != NULL ); if( buffer == NULL ) return NULL;

            D3D11_BUFFER_DESC desc; buffer->GetDesc( &desc ); SAFE_RELEASE( buffer );
            bindFlags = desc.BindFlags;
        }  break;
        case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
        {
            ID3D11Texture1D * tex = vaDirectXTools::QueryResourceInterface<ID3D11Texture1D>( resource, IID_ID3D11Texture1D );
            assert( tex != NULL );  if( tex == NULL ) return NULL;

            D3D11_TEXTURE1D_DESC desc; tex->GetDesc( &desc ); SAFE_RELEASE( tex );
            bindFlags       = desc.BindFlags;
            resourceFormat  = (vaResourceFormat)desc.Format;
        }  break;
        case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
        {
            ID3D11Texture2D * tex = vaDirectXTools::QueryResourceInterface<ID3D11Texture2D>( resource, IID_ID3D11Texture2D );
            assert( tex != NULL );  if( tex == NULL ) return NULL;

            D3D11_TEXTURE2D_DESC desc; tex->GetDesc( &desc ); SAFE_RELEASE( tex );
            bindFlags       = desc.BindFlags;
            resourceFormat  = (vaResourceFormat)desc.Format;
        }  break;
        case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
        {
            ID3D11Texture3D * tex = vaDirectXTools::QueryResourceInterface<ID3D11Texture3D>( resource, IID_ID3D11Texture3D );
            assert( tex != NULL );  if( tex == NULL ) return NULL;

            D3D11_TEXTURE3D_DESC desc; tex->GetDesc( &desc ); SAFE_RELEASE( tex );
            bindFlags       = desc.BindFlags;
            resourceFormat  = (vaResourceFormat)desc.Format;
        } break;
        default:
        {
            assert( false ); return NULL;
        }  break;
    }

    vaTextureFlags flags = vaTextureFlags::None; // this should be deduced from something probably, but not needed at the moment

    shared_ptr<vaTexture> newTexture = VA_RENDERING_MODULE_CREATE_SHARED( vaTexture, vaTextureConstructorParams( renderDevice, vaCore::GUIDCreate( ) ) );
    newTexture->Initialize( BindFlagsVAFromDX(bindFlags), resourceFormat, srvFormat, rtvFormat, dsvFormat, uavFormat, flags, 0, -1, 0, -1, contentsType );

    vaTextureDX11 * newDX11Texture = newTexture->SafeCast<vaTextureDX11*>();

    resource->AddRef();
    newDX11Texture->SetResource( resource );
    newDX11Texture->ProcessResource( false );

    return newTexture;
}

/*
vaTexture * vaTexture::CreateCubeArrayView( vaTexture & texture )
{
    if( ( texture.GetFlags( ) & vaTextureFlags::Cubemap ) == 0 )
    {
        // this only works on cubemaps
        assert( false );
        return nullptr;
    }

    vaTextureDX11 * origDX11Texture = texture.SafeCast<vaTextureDX11*>( );

    ID3D11Resource * resource = origDX11Texture->GetResource( );

    if( resource == NULL )
    {
        assert( false );
        return nullptr;
    }

    vaResourceBindSupportFlags bindFlags = vaResourceBindSupportFlags::ShaderResource;
    vaResourceFormat srvFormat = texture.GetSRVFormat();

    // Can't request additional binding flags that were not supported in the original texture
    vaResourceBindSupportFlags origFlags = origDX11Texture->GetBindSupportFlags( );
    assert( ( ( ~origFlags ) & bindFlags ) == 0 );
    origFlags; // unreferenced in Release

    vaTextureConstructorParams params( vaCore::GUIDCreate( ) );
    vaTexture * newTexture = VA_RENDERING_MODULE_CREATE( vaTexture, &params );
    newTexture->Initialize( bindFlags, texture.GetResourceFormat( ), srvFormat, vaResourceFormat::Unknown, vaResourceFormat::Unknown, vaResourceFormat::Unknown, vaTextureFlags::None, 0, -1, 0, -1, texture.GetContentsType( ) );

    // it is debatable whether this is needed since DX resources have reference counting and will stay alive, but it might be useful for DX12 or other API implementations so I'll leave it in (actually, I removed it later :P)
    // newTexture->m_viewedOriginal = texture;
    newTexture->m_isView = true;

    vaTextureDX11 * newDX11Texture = vaSaferStaticCast<vaTextureDX11*>( newTexture );
    resource->AddRef( );
    newDX11Texture->SetResource( resource );
    newDX11Texture->m_flags = vaTextureFlags::Cubemap | vaTextureFlags::CubemapButArraySRV;
    newDX11Texture->ProcessResource( true, true );
    return newTexture;
}
*/


vaTextureDX11::vaTextureDX11( const vaRenderingModuleParams & params ) : vaTexture( params )
{ 
    m_resource      = nullptr;
    m_buffer        = nullptr;
    m_texture1D     = nullptr;
    m_texture2D     = nullptr;
    m_texture3D     = nullptr;
    m_srv           = nullptr;
    m_rtv           = nullptr;
    m_dsv           = nullptr;
    m_uav           = nullptr;
}

vaTextureDX11::~vaTextureDX11( )
{ 
    Destroy( );
}

void vaTextureDX11::Destroy( )
{
    assert( !IsMapped() );

    SAFE_RELEASE( m_resource );
    SAFE_RELEASE( m_buffer );
    SAFE_RELEASE( m_texture1D );
    SAFE_RELEASE( m_texture2D );
    SAFE_RELEASE( m_texture3D );
    SAFE_RELEASE( m_srv );
    SAFE_RELEASE( m_rtv );
    SAFE_RELEASE( m_dsv );
    SAFE_RELEASE( m_uav );
}

bool vaTextureDX11::Import( void * buffer, uint64 bufferSize, vaTextureLoadFlags loadFlags, vaResourceBindSupportFlags binds, vaTextureContentsType contentsType )
{
    if( bufferSize <= 4 )
    {
        assert( false ); // ?
        return false;
    }

    Destroy( );

    bool dontAutogenerateMIPs = ( loadFlags & vaTextureLoadFlags::AutogenerateMIPsIfMissing ) == 0;
    if( !dontAutogenerateMIPs )
        binds |= vaResourceBindSupportFlags::RenderTarget;   // have to add render target support for when auto generating MIPs

    m_contentsType = contentsType;

    uint32_t dwMagicNumber = *(const uint32_t*)(buffer);

    const uint32_t DDS_MAGIC = 0x20534444; // "DDS "
    bool isDDS = dwMagicNumber == DDS_MAGIC;

    if( isDDS )
        m_resource = vaDirectXTools::LoadTextureDDS( GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice(), buffer, bufferSize, loadFlags, binds );
    else
        m_resource = vaDirectXTools::LoadTextureWIC( GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice(), buffer, bufferSize, loadFlags, binds );

    if( m_resource == NULL )
    {
        VA_WARN( L"vaTextureDX11::Import - unable to load texture from a buffer!" );

        return false;
    }

    m_bindSupportFlags = binds;

    ProcessResource( );

    return true;
}

bool vaTextureDX11::Import( const wstring & storageFilePath, vaTextureLoadFlags loadFlags, vaResourceBindSupportFlags binds, vaTextureContentsType contentsType )
{
    Destroy( );

    bool dontAutogenerateMIPs = (loadFlags & vaTextureLoadFlags::AutogenerateMIPsIfMissing) == 0;
    if( !dontAutogenerateMIPs )
        binds |= vaResourceBindSupportFlags::RenderTarget;   // have to add render target support for when auto generating MIPs

    std::shared_ptr<vaMemoryStream> fileContents;

    wstring usedPath;

    wstring outDir, outName, outExt;
    vaFileTools::SplitPath( storageFilePath, &outDir, &outName, &outExt );
    outExt = vaStringTools::ToLower( outExt );

    bool isDDS = outExt == L".dds";

    m_contentsType = contentsType;

    // try asset paths
    if( fileContents.get( ) == NULL ) 
    {
        usedPath = vaFileTools::FindLocalFile( storageFilePath );
        //fileContents = vaFileTools::LoadFileToMemoryStream( usedPath.c_str() );
    }

    // found? try load and return!
    if( vaFileTools::FileExists( usedPath ) )
    //if( fileContents.get( ) != NULL )
    {
        if( isDDS )
            m_resource = vaDirectXTools::LoadTextureDDS( GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice(), usedPath.c_str(), loadFlags, binds );
        else
            m_resource = vaDirectXTools::LoadTextureWIC( GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice(), usedPath.c_str(), loadFlags, binds );
    }

    if( m_resource == NULL )
    { 
        // not found? try embedded
        vaFileTools::EmbeddedFileData embeddedFile = vaFileTools::EmbeddedFilesFind( ( L"textures:\\" + storageFilePath ).c_str( ) );
        if( embeddedFile.HasContents( ) )
        {
            if( isDDS )
                m_resource = vaDirectXTools::LoadTextureDDS( GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice(), embeddedFile.MemStream->GetBuffer( ), embeddedFile.MemStream->GetLength( ), loadFlags, binds );
            else
                m_resource = vaDirectXTools::LoadTextureWIC( GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice(), embeddedFile.MemStream->GetBuffer( ), embeddedFile.MemStream->GetLength( ), loadFlags, binds );
        }
    }

    if( m_resource == NULL )
    {
        VA_WARN( L"vaTextureDX11::Import - unable to find or load '%s' texture file!", storageFilePath.c_str( ) );

        return false;
    }

    m_bindSupportFlags = binds;

    ProcessResource( );    

    return true;
}

void vaTextureDX11::InternalUpdateFromRenderingCounterpart( bool notAllBindViewsNeeded, bool dontResetFlags )
{
    vaTextureDX11 * dx11Texture = vaSaferStaticCast<vaTextureDX11*>( this );

    D3D11_RESOURCE_DIMENSION dim;
    dx11Texture->GetResource()->GetType( &dim );

    D3D11_USAGE Usage           = D3D11_USAGE_DEFAULT;
    UINT        BindFlags       = 0;
    UINT        CPUAccessFlags  = 0;
    UINT        MiscFlags       = 0;

    if( !dontResetFlags )
        m_flags     = vaTextureFlags::None;
    m_type      = vaTextureType::Unknown; 

    switch( dim )
    {
        case D3D11_RESOURCE_DIMENSION_UNKNOWN:
        {
            assert( false );   
         }  break;
        case D3D11_RESOURCE_DIMENSION_BUFFER:
        {
            assert( false ); // never debugged - just go through it and make sure it's true

            dx11Texture->m_buffer = vaDirectXTools::QueryResourceInterface<ID3D11Buffer>( dx11Texture->GetResource(), IID_ID3D11Buffer );        
            assert( dx11Texture->m_buffer != NULL );
            if( dx11Texture->m_buffer == NULL )
            {
                dx11Texture->Destroy();
                return;
            }
            m_type = vaTextureType::Buffer;

            D3D11_BUFFER_DESC desc;
            dx11Texture->m_buffer->GetDesc( &desc );
            
            m_sizeX             = desc.ByteWidth;
            m_sizeY             = desc.StructureByteStride;

            Usage               = desc.Usage;
            BindFlags           = desc.BindFlags;
            CPUAccessFlags      = desc.CPUAccessFlags;
            MiscFlags           = desc.MiscFlags;

        }  break;
        case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
        {
            dx11Texture->m_texture1D = vaDirectXTools::QueryResourceInterface<ID3D11Texture1D>( dx11Texture->GetResource( ), IID_ID3D11Texture1D );
            assert( dx11Texture->m_texture1D != NULL );
            if( dx11Texture->m_texture1D == NULL )
            {
                dx11Texture->Destroy( );
                return;
            }

            D3D11_TEXTURE1D_DESC desc;
            dx11Texture->m_texture1D->GetDesc( &desc );

            m_sizeX             = desc.Width;
            m_mipLevels         = desc.MipLevels;
            m_sizeY             = desc.ArraySize;
            m_sizeZ             = 1;
            if( desc.ArraySize > 1 )
                m_type = vaTextureType::Texture1DArray;
            else
                m_type = vaTextureType::Texture1D;
            if( m_resourceFormat != vaResourceFormat::Automatic )
            { assert( m_resourceFormat == (vaResourceFormat)desc.Format ); }
            m_resourceFormat = (vaResourceFormat)desc.Format;
            m_sampleCount       = 1;

            Usage               = desc.Usage;
            BindFlags           = desc.BindFlags;
            CPUAccessFlags      = desc.CPUAccessFlags;
            MiscFlags           = desc.MiscFlags;

            // Support for mapping/unmapping (not all possible permutations supported yet)
            if( (m_sizeY == 1) && (m_sizeZ == 1) && (m_sampleCount == 1) && ((CPUAccessFlags & (D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ) ) != 0) )
            {
                int bytesPerPixel = vaResourceFormatHelpers::GetPixelSizeInBytes( m_resourceFormat );
                assert( bytesPerPixel != 0 );

                if( bytesPerPixel > 0 )
                {
                    m_mappedData.resize( m_mipLevels );

                    int sizeX = m_sizeX;
                    int sizeY = m_sizeY;
                    for( int i = 0; i < m_mipLevels; i++ )
                    {
                        m_mappedData[i].SizeX          = sizeX;
                        m_mappedData[i].SizeY          = sizeY;
                        m_mappedData[i].BytesPerPixel  = bytesPerPixel;
                        m_mappedData[i].RowPitch       = 0;
                        m_mappedData[i].SizeInBytes    = 0;
                        m_mappedData[i].DepthPitch     = 0;
                        m_mappedData[i].Buffer         = nullptr;
        
                        // shouldn't ever be zero - too many mips? that should've been handled before!
                        if( i != ( m_mipLevels - 1 ) )
                        {
                            assert( ( sizeX % 2 ) == 0 );
                            assert( ( sizeY % 2 ) == 0 );
                            sizeX = sizeX / 2;
                            sizeY = sizeY / 2;
                        }
                    }
                }
            }


        }  break;
        case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
        {
            dx11Texture->m_texture2D = vaDirectXTools::QueryResourceInterface<ID3D11Texture2D>( dx11Texture->GetResource( ), IID_ID3D11Texture2D );
            assert( dx11Texture->m_texture2D != NULL );
            if( dx11Texture->m_texture2D == NULL )
            {
                dx11Texture->Destroy( );
                return;
            }

            D3D11_TEXTURE2D_DESC desc;
            dx11Texture->m_texture2D->GetDesc( &desc );

            m_sizeX             = desc.Width;
            m_sizeY             = desc.Height;
            m_mipLevels         = desc.MipLevels;
            m_sizeZ             = desc.ArraySize;
            if( m_resourceFormat != vaResourceFormat::Automatic )
            { assert( m_resourceFormat == (vaResourceFormat)desc.Format ); }
            m_resourceFormat = (vaResourceFormat)desc.Format;
            m_sampleCount       = desc.SampleDesc.Count;
            //                  = desc.SampleDesc.Quality;

            if( desc.ArraySize > 1 )
            {
                if( desc.SampleDesc.Count > 1 )
                    m_type = vaTextureType::Texture2DMSArray;
                else
                    m_type = vaTextureType::Texture2DArray;
            }
            else
            {
                if( desc.SampleDesc.Count > 1 )
                    m_type = vaTextureType::Texture2DMS;
                else
                    m_type = vaTextureType::Texture2D;
            }

            Usage               = desc.Usage;
            BindFlags           = desc.BindFlags;
            CPUAccessFlags      = desc.CPUAccessFlags;
            MiscFlags           = desc.MiscFlags;

            // Support for mapping/unmapping (not all possible permutations supported yet)
            if( (m_sizeZ == 1) && (m_sampleCount == 1) && ((CPUAccessFlags & (D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ) ) != 0) )
            {
                int bytesPerPixel = vaResourceFormatHelpers::GetPixelSizeInBytes( m_resourceFormat );
                assert( bytesPerPixel != 0 );

                if( bytesPerPixel > 0 )
                {
                    m_mappedData.resize( m_mipLevels );

                    int sizeX = m_sizeX;
                    int sizeY = m_sizeY;
                    for( int i = 0; i < m_mipLevels; i++ )
                    {
                        m_mappedData[i].SizeX           = sizeX;
                        m_mappedData[i].SizeY           = sizeY;
                        m_mappedData[i].BytesPerPixel   = bytesPerPixel;
                        m_mappedData[i].RowPitch       = 0;
                        m_mappedData[i].SizeInBytes    = 0;
                        m_mappedData[i].DepthPitch     = 0;
                        m_mappedData[i].Buffer         = nullptr;
        
                        // shouldn't ever be zero - too many mips? that should've been handled before!
                        if( i != ( m_mipLevels - 1 ) )
                        {
                            assert( ( sizeX % 2 ) == 0 );
                            assert( ( sizeY % 2 ) == 0 );
                            sizeX = sizeX / 2;
                            sizeY = sizeY / 2;
                        }
                    }
                }
            }

        }  break;
        case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
        {
            dx11Texture->m_texture3D = vaDirectXTools::QueryResourceInterface<ID3D11Texture3D>( dx11Texture->GetResource( ), IID_ID3D11Texture3D );
            assert( dx11Texture->m_texture3D != NULL );
            if( dx11Texture->m_texture3D == NULL )
            {
                dx11Texture->Destroy( );
                return;
            }

            D3D11_TEXTURE3D_DESC desc;
            dx11Texture->m_texture3D->GetDesc( &desc );

            m_sizeX             = desc.Width;
            m_sizeY             = desc.Height;
            m_sizeZ             = desc.Depth;
            m_mipLevels         = desc.MipLevels;
            if( m_resourceFormat != vaResourceFormat::Automatic )
            { assert( m_resourceFormat == (vaResourceFormat)desc.Format ); }
            m_resourceFormat = (vaResourceFormat)desc.Format;
            m_sampleCount       = 1;
            m_type              = vaTextureType::Texture3D;

            Usage               = desc.Usage;
            BindFlags           = desc.BindFlags;
            CPUAccessFlags      = desc.CPUAccessFlags;
            MiscFlags           = desc.MiscFlags;
        }  break;
        default:
        {
            m_type = vaTextureType::Unknown; assert( false );
        }  break;
    }

    // -1 means all above min
    if( m_viewedMipSliceCount == -1 )
        m_viewedMipSliceCount = GetMipLevels() - m_viewedMipSlice;
    if( m_viewedArraySliceCount == -1 )
        m_viewedArraySliceCount = GetArrayCount() - m_viewedArraySlice;

    assert( m_viewedMipSlice >= 0 && m_viewedMipSlice < GetMipLevels() );
    assert( (m_viewedMipSlice+m_viewedMipSliceCount) > 0 && (m_viewedMipSlice+m_viewedMipSliceCount) <= GetMipLevels() );
    assert( m_viewedArraySlice >= 0 && m_viewedArraySlice < GetArrayCount( ) );
    assert( ( m_viewedArraySlice + m_viewedArraySliceCount ) > 0 && ( m_viewedArraySlice + m_viewedArraySliceCount ) <= GetArrayCount( ) );

    // this is to support views (vaTexture::CreateView) into specific array items or mips - it's initialized for regular textures as well to maintain full interoperability
    m_viewedSliceSizeX = m_sizeX;
    m_viewedSliceSizeY = m_sizeY;
    m_viewedSliceSizeZ = m_sizeZ;
    for( int i = 0; i < m_viewedMipSlice; i++ )
    {
        // mipmap generation seems to compute mip dimensions by round down (dim >> 1) which means potential loss of data, so that's why we assert
        // (these asserts were temporarily disabled - need to be put back in at some point when the rest of the codebase is fixed to comply)
        // assert( (m_viewedSliceSizeX % 2) == 0 || m_viewedSliceSizeX == 1 ); 
        // assert( (m_viewedSliceSizeY % 2) == 0 || m_viewedSliceSizeY == 1 ); 
        // if( m_type == vaTextureType::Texture3D )
        //     assert( (m_viewedSliceSizeZ % 2) == 0 || m_viewedSliceSizeZ == 1 );
        m_viewedSliceSizeX = (m_viewedSliceSizeX) / 2;
        m_viewedSliceSizeY = (m_viewedSliceSizeY) / 2;
        m_viewedSliceSizeZ = (m_viewedSliceSizeZ) / 2;
    }
    m_viewedSliceSizeX = vaMath::Max( m_viewedSliceSizeX, 1 );
    m_viewedSliceSizeY = vaMath::Max( m_viewedSliceSizeY, 1 );
    m_viewedSliceSizeZ = vaMath::Max( m_viewedSliceSizeZ, 1 );

    Usage;

    if( (MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) != 0 )
        m_flags |= vaTextureFlags::Cubemap;

    m_accessFlags = vaTextureAccessFlags::None;
    if( (CPUAccessFlags & D3D11_CPU_ACCESS_WRITE ) != 0 )
        m_accessFlags = m_accessFlags | vaTextureAccessFlags::CPUWrite;
    if( ( CPUAccessFlags & D3D11_CPU_ACCESS_READ ) != 0 )
        m_accessFlags = m_accessFlags | vaTextureAccessFlags::CPURead;

    // make sure bind flags were set up correctly
    if( !notAllBindViewsNeeded )
    {
        if( (BindFlags & D3D11_BIND_VERTEX_BUFFER       ) != 0 )
            assert( ( m_bindSupportFlags & vaResourceBindSupportFlags::VertexBuffer ) != 0 );
        if( (BindFlags & D3D11_BIND_INDEX_BUFFER        ) != 0 ) 
            assert( ( m_bindSupportFlags & vaResourceBindSupportFlags::IndexBuffer ) != 0 );
        if( (BindFlags & D3D11_BIND_CONSTANT_BUFFER     ) != 0 ) 
            assert( ( m_bindSupportFlags & vaResourceBindSupportFlags::ConstantBuffer ) != 0 );
        if( (BindFlags & D3D11_BIND_SHADER_RESOURCE     ) != 0 ) 
            assert( ( m_bindSupportFlags & vaResourceBindSupportFlags::ShaderResource ) != 0 );
        if( (BindFlags & D3D11_BIND_RENDER_TARGET       ) != 0 ) 
            assert( ( m_bindSupportFlags & vaResourceBindSupportFlags::RenderTarget ) != 0 );
        if( (BindFlags & D3D11_BIND_DEPTH_STENCIL       ) != 0 ) 
            assert( ( m_bindSupportFlags & vaResourceBindSupportFlags::DepthStencil ) != 0 );
        if( (BindFlags & D3D11_BIND_UNORDERED_ACCESS    ) != 0 ) 
            assert( ( m_bindSupportFlags & vaResourceBindSupportFlags::UnorderedAccess ) != 0 );
    }

    MiscFlags;


}

void vaTextureDX11::SetResource( ID3D11Resource * resource )
{
    Destroy();
    m_resource = resource;
}

void vaTextureDX11::ProcessResource( bool notAllBindViewsNeeded, bool dontResetFlags )
{
    InternalUpdateFromRenderingCounterpart( notAllBindViewsNeeded, dontResetFlags );

    if( ( GetBindSupportFlags( ) & vaResourceBindSupportFlags::VertexBuffer ) != 0 )
    {
        assert( false ); // not implemented yet
    }

    if( ( GetBindSupportFlags( ) & vaResourceBindSupportFlags::IndexBuffer ) != 0 )
    {
        assert( false ); // not implemented yet
    }

    if( ( GetBindSupportFlags( ) & vaResourceBindSupportFlags::ConstantBuffer ) != 0 )
    {
        assert( false ); // not implemented yet
    }

    if( ( GetBindSupportFlags( ) & vaResourceBindSupportFlags::ConstantBuffer ) != 0 )
    {
        assert( false ); // not implemented yet
    }

    if( ( ( GetBindSupportFlags( ) & vaResourceBindSupportFlags::ShaderResource ) != 0 ) && (GetSRVFormat() != vaResourceFormat::Unknown) )
    {
        // not the cleanest way to do this - should probably get updated and also assert on _TYPELESS
        if( GetSRVFormat() == vaResourceFormat::Automatic )
        {
            m_srvFormat = m_resourceFormat;
        }
        
        if( ((m_flags & vaTextureFlags::Cubemap) != 0) && ((m_flags & vaTextureFlags::CubemapButArraySRV) == 0) )
        {
            m_srv = vaDirectXTools::CreateShaderResourceViewCubemap( m_resource, DXGIFormatFromVA(GetSRVFormat( )), m_viewedMipSlice, m_viewedMipSliceCount, m_viewedArraySlice, m_viewedArraySliceCount );
        }
        else
        {
            m_srv = vaDirectXTools::CreateShaderResourceView( m_resource, DXGIFormatFromVA(GetSRVFormat()), m_viewedMipSlice, m_viewedMipSliceCount, m_viewedArraySlice, m_viewedArraySliceCount );
        }
    }

    if( ( ( GetBindSupportFlags( ) & vaResourceBindSupportFlags::RenderTarget ) != 0 ) && (GetRTVFormat() != vaResourceFormat::Unknown) )
    {
        // not the cleanest way to do this - should probably get updated and also assert on _TYPELESS
        if( GetRTVFormat( ) == vaResourceFormat::Automatic )
        {
            m_rtvFormat = m_resourceFormat;
        }
        m_rtv = vaDirectXTools::CreateRenderTargetView( m_resource, DXGIFormatFromVA(GetRTVFormat()), m_viewedMipSlice, m_viewedArraySlice, m_viewedArraySliceCount );
    }

    if( ( ( GetBindSupportFlags( ) & vaResourceBindSupportFlags::DepthStencil ) != 0 ) && (GetDSVFormat() != vaResourceFormat::Unknown) )
    {
        // not the cleanest way to do this - should probably get updated and also assert on _TYPELESS
        if( GetDSVFormat( ) == vaResourceFormat::Automatic )
        {
            m_dsvFormat = m_resourceFormat;
        }
        m_dsv = vaDirectXTools::CreateDepthStencilView( m_resource, DXGIFormatFromVA(GetDSVFormat()), m_viewedMipSlice, m_viewedArraySlice, m_viewedArraySliceCount );
    }

    if( ( ( GetBindSupportFlags( ) & vaResourceBindSupportFlags::UnorderedAccess ) != 0 ) && (GetUAVFormat() != vaResourceFormat::Unknown) )
    {
        // not the cleanest way to do this - should probably get updated and also assert on _TYPELESS
        if( GetUAVFormat( ) == vaResourceFormat::Automatic )
        {
            m_uavFormat = m_resourceFormat;
        }
        m_uav = vaDirectXTools::CreateUnorderedAccessView( m_resource, DXGIFormatFromVA(GetUAVFormat()), m_viewedMipSlice, m_viewedArraySlice, m_viewedArraySliceCount );
    }
}

void vaTextureDX11::ClearRTV( vaRenderDeviceContext & context, const vaVector4 & clearValue )
{
    assert( m_rtv != nullptr ); if( m_rtv == nullptr ) return;
    ID3D11DeviceContext * dx11Context = vaSaferStaticCast< vaRenderDeviceContextDX11 * >( &context )->GetDXContext( );
    dx11Context->ClearRenderTargetView( m_rtv, &clearValue.x );
}

void vaTextureDX11::ClearUAV( vaRenderDeviceContext & context, const vaVector4ui & clearValue )
{
    assert( m_uav != nullptr ); if( m_uav == nullptr ) return;
    ID3D11DeviceContext * dx11Context = vaSaferStaticCast< vaRenderDeviceContextDX11 * >( &context )->GetDXContext( );
    dx11Context->ClearUnorderedAccessViewUint( m_uav, &clearValue.x );
}

void vaTextureDX11::ClearUAV( vaRenderDeviceContext & context, const vaVector4 & clearValue )
{
    assert( m_uav != nullptr ); if( m_uav == nullptr ) return;
    ID3D11DeviceContext * dx11Context = vaSaferStaticCast< vaRenderDeviceContextDX11 * >( &context )->GetDXContext( );
    dx11Context->ClearUnorderedAccessViewFloat( m_uav, &clearValue.x );
}

void vaTextureDX11::ClearDSV( vaRenderDeviceContext & context, bool clearDepth, float depthValue, bool clearStencil, uint8 stencilValue )
{
    assert( m_dsv != nullptr ); if( m_dsv == nullptr ) return;
    ID3D11DeviceContext * dx11Context = vaSaferStaticCast< vaRenderDeviceContextDX11 * >( &context )->GetDXContext( );
    UINT clearFlags = 0;
    if( clearDepth )    clearFlags |= D3D11_CLEAR_DEPTH;
    if( clearStencil )  clearFlags |= D3D11_CLEAR_STENCIL;
    dx11Context->ClearDepthStencilView( m_dsv, clearFlags, depthValue, stencilValue );
}

void vaTextureDX11::CopyFrom( vaRenderDeviceContext & context, const shared_ptr<vaTexture> & srcTexture )
{
    ID3D11DeviceContext * dx11Context = vaSaferStaticCast< vaRenderDeviceContextDX11 * >( &context )->GetDXContext( );

    ID3D11Resource * dstResourceDX11 = SafeCast<vaTextureDX11*>( )->GetResource();
    ID3D11Resource * srcResourceDX11 = srcTexture->SafeCast<vaTextureDX11*>( )->GetResource();

    if( srcTexture->GetSampleCount( ) > 1 && GetSampleCount( ) == 1 )
    {
        //m_resolvedSrcRadiance->SafeCast<vaTextureDX11*>( )->GetResource(), 0, srcRadiance->SafeCast<vaTextureDX11*>( )->GetResource(), 0, (DXGI_FORMAT)srcRadiance->GetSRVFormat() );
        dx11Context->ResolveSubresource( dstResourceDX11, 0, srcResourceDX11, 0, DXGIFormatFromVA(GetSRVFormat()) );
    }
    else
    {
        dx11Context->CopyResource( dstResourceDX11, srcResourceDX11 );
    }
}

bool vaTextureDX11::SaveAPACK( vaStream & outStream )
{
    // assert( m_viewedOriginal == nullptr );
    // if( m_viewedOriginal != nullptr )
    //     return false;

    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<int32>( c_fileVersion ) );

    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<vaTextureFlags            > ( m_flags ) );

    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<vaTextureAccessFlags      >( m_accessFlags      ) );
    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<vaTextureType             >( m_type             ) );
    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<vaResourceBindSupportFlags >( m_bindSupportFlags ) );
    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<vaTextureContentsType     >( m_contentsType     ) );
    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<vaResourceFormat           >( m_resourceFormat   ) );
    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<vaResourceFormat           >( m_srvFormat        ) );
    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<vaResourceFormat           >( m_rtvFormat        ) );
    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<vaResourceFormat           >( m_dsvFormat        ) );
    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<vaResourceFormat           >( m_uavFormat        ) );
    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<int                       >( m_sizeX            ) );
    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<int                       >( m_sizeY            ) );
    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<int                       >( m_sizeZ            ) );
    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<int                       >( m_sampleCount      ) );
    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<int                       >( m_mipLevels        ) );

    int64 posOfSize = outStream.GetPosition( );
    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<int64>( 0 ) );

    VERIFY_TRUE_RETURN_ON_FALSE( vaDirectXTools::SaveDDSTexture( GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice(), outStream, m_resource ) );

    int64 calculatedSize = outStream.GetPosition( ) - posOfSize;
    outStream.Seek( posOfSize );
    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<int64>( calculatedSize - 8 ) );
    outStream.Seek( posOfSize + calculatedSize );

    return true;
}

bool vaTextureDX11::LoadAPACK( vaStream & inStream )
{
    Destroy( );

    int32 fileVersion = 0;
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int32>( fileVersion ) );
    if( fileVersion != c_fileVersion )
    {
        VA_LOG( L"vaRenderMaterial::Load(): unsupported file version" );
        return false;
    }

    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<vaTextureFlags            >( m_flags ) );
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<vaTextureAccessFlags      >( m_accessFlags ) );
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<vaTextureType             >( m_type ) );
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<vaResourceBindSupportFlags >( m_bindSupportFlags ) );
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<vaTextureContentsType     >( m_contentsType ) );
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<vaResourceFormat           >( m_resourceFormat ) );
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<vaResourceFormat           >( m_srvFormat ) );
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<vaResourceFormat           >( m_rtvFormat ) );
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<vaResourceFormat           >( m_dsvFormat ) );
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<vaResourceFormat           >( m_uavFormat ) );
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int                       >( m_sizeX ) );
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int                       >( m_sizeY ) );
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int                       >( m_sizeZ ) );
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int                       >( m_sampleCount ) );
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int                       >( m_mipLevels ) );

    int64 textureDataSize;
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int64                     >( textureDataSize ) );

    // direct reading from the stream not implemented yet
    byte * buffer = new byte[ textureDataSize ];
    if( !inStream.Read( buffer, textureDataSize ) )
    {
        assert( false );
        delete[] buffer;
        return false;
    }

    // used for creating mip-maps; stripping it here but in theory should be stripped on save in case we really do want to use it as a RT at any point (not sure why we would though)
    const bool stripRenderTargetFlag = true;
    if( stripRenderTargetFlag )
        m_bindSupportFlags &= ~vaResourceBindSupportFlags::RenderTarget;

    m_resource = vaDirectXTools::LoadTextureDDS( GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice(), buffer, textureDataSize, vaTextureLoadFlags::Default, m_bindSupportFlags );
    delete[] buffer;

    if( m_resource == NULL )
    {
        VA_WARN( L"vaTextureDX11::Load - error processing file!" );
        assert( false );

        return false;
    }

    ProcessResource( );

    return true;
}

bool vaTextureDX11::SerializeUnpacked( vaXMLSerializer & serializer, const wstring & assetFolder )
{
    int32 fileVersion = c_fileVersion;
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "FileVersion", fileVersion ) );

    VERIFY_TRUE_RETURN_ON_FALSE( fileVersion == c_fileVersion );

    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "flags",            (int32&)m_flags             ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "accessFlags",      (int32&)m_accessFlags       ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "type",             (int32&)m_type              ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "bindSupportFlags", (int32&)m_bindSupportFlags  ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "contentsType",     (int32&)m_contentsType      ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "resourceFormat",   (int32&)m_resourceFormat    ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "srvFormat",        (int32&)m_srvFormat         ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "rtvFormat",        (int32&)m_rtvFormat         ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "dsvFormat",        (int32&)m_dsvFormat         ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "uavFormat",        (int32&)m_uavFormat         ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "sizeX",            (int32&)m_sizeX             ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "sizeY",            (int32&)m_sizeY             ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "sizeZ",            (int32&)m_sizeZ             ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "sampleCount",      (int32&)m_sampleCount       ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "mipLevels",        (int32&)m_mipLevels         ) );

    vaFileStream textureFile;

    if( serializer.IsWriting( ) )
    {
        if( !textureFile.Open( assetFolder + L"/Texture.dds", FileCreationMode::Create, FileAccessMode::ReadWrite ) )
        {
            VA_LOG_ERROR( L"vaTextureDX11::SerializeUnpacked - Unable to open '%s'", (assetFolder + L"/Texture.dds").c_str() );
            return false;
        }
        VERIFY_TRUE_RETURN_ON_FALSE( vaDirectXTools::SaveDDSTexture( GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice(), textureFile, m_resource ) );
        textureFile.Close();
    }
    else if( serializer.IsReading( ) )
    {
        // if( !textureFile.Open( assetFolder + L"/Texture.dds", FileCreationMode::Open ) )
        // {
        //     VA_LOG_ERROR( L"vaTextureDX11::SerializeUnpacked - Unable to open '%s'", (assetFolder + L"/Texture.dds").c_str() );
        //     return false;
        // }
        // 
        // int64 length = textureFile.GetLength();
        // byte * tmpBuffer = new byte[length];
        // 
        // if(  )
        // 
        // VERIFY_TRUE_RETURN_ON_FALSE( vaDirectXTools::LoadTextureDDS( textureFile, m_resource ) );
        // textureFile.Close();

        // used for creating mip-maps; stripping it here but in theory should be stripped on save in case we really do want to use it as a RT at any point (not sure why we would though)
        const bool stripRenderTargetFlag = true;
        if( stripRenderTargetFlag )
            m_bindSupportFlags &= ~vaResourceBindSupportFlags::RenderTarget;

        m_resource = vaDirectXTools::LoadTextureDDS( GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice(), (assetFolder + L"/Texture.dds").c_str(), vaTextureLoadFlags::Default, m_bindSupportFlags, false );

        if( m_resource == NULL )
        {
            VA_WARN( L"vaTextureDX11::SerializeUnpacked - error processing file!" );
            assert( false );

            return false;
        }
        ProcessResource( );
    }
    else { assert( false ); return false; }
    return true;
}


bool vaTextureDX11::TryMap( vaRenderDeviceContext & apiContext, vaResourceMapType mapType, bool doNotWait )
{
    ID3D11DeviceContext * dx11Context = apiContext.SafeCast<vaRenderDeviceContextDX11*>( )->GetDXContext();

    assert( !m_isMapped );
    if( m_isMapped )
        return false;

    // map not supported on this resource - either the flags are wrong or the support has not yet been added; check InternalUpdateFromRenderingCounterpart for adding support
    assert( m_mappedData.size() > 0 );
    if( m_mappedData.size() == 0 )
        return false;

    for( int i = 0; i < m_mappedData.size(); i++ )
    {
        D3D11_MAPPED_SUBRESOURCE mappedSubresource; memset( &mappedSubresource, 0, sizeof(mappedSubresource) );

        D3D11_MAP dx11MapType = MapTypeDXFromVA( mapType );

        VA_SCOPE_CPU_TIMER( vaTextureDX11TryMap );
        HRESULT hr = dx11Context->Map( m_resource, i, dx11MapType, (doNotWait)?(D3D11_MAP_FLAG_DO_NOT_WAIT):(0), &mappedSubresource );
        if( FAILED( hr ) )
        {
            if( doNotWait && (hr == DXGI_ERROR_WAS_STILL_DRAWING) )
            {
            }
            else
            {
                assert( false );
            }
            // roll back all maps made so far
            for( int j = i-1; j >= 0; j-- )
            {
                dx11Context->Unmap( m_resource, j );
                m_mappedData[j].Buffer      = nullptr;
                m_mappedData[j].SizeInBytes = 0;
                m_mappedData[j].RowPitch    = 0;
                m_mappedData[j].DepthPitch  = 0;
            }
            return false;
        }
        else
        {
            m_mappedData[i].Buffer          = (byte*)mappedSubresource.pData;
            m_mappedData[i].RowPitch        = mappedSubresource.RowPitch;
            m_mappedData[i].DepthPitch      = 0; //mappedSubresource.DepthPitch;
            m_mappedData[i].SizeInBytes     = m_mappedData[i].SizeY * m_mappedData[i].RowPitch;
            //assert( cpuSubresource->DepthPitch == 0 );
            //assert( m_mappedData[i].DepthPitch == m_mappedData[i].SizeInBytes );
        }
    }
    m_isMapped = true;
    return true;
}

void vaTextureDX11::Unmap( vaRenderDeviceContext & apiContext )
{
    ID3D11DeviceContext * dx11Context = apiContext.SafeCast<vaRenderDeviceContextDX11*>( )->GetDXContext();

    assert( m_isMapped );
    if( !m_isMapped )
        return;

    for( int i = 0; i < m_mappedData.size(); i++ )
    {
        auto & cpuSubresource = m_mappedData[ i ];
        assert( cpuSubresource.Buffer != nullptr );    // not fatal, checked below but still unexpected, indicating a bug
        cpuSubresource; // unreferenced in Release
        dx11Context->Unmap( m_resource, i );
        m_mappedData[i].Buffer      = nullptr;
        m_mappedData[i].SizeInBytes = 0;
        m_mappedData[i].RowPitch    = 0;
        m_mappedData[i].DepthPitch  = 0;
    }
    m_isMapped = false;
}

void vaTextureDX11::ResolveSubresource( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & dstResource, uint dstSubresource, uint srcSubresource, vaResourceFormat format )
{
    ID3D11DeviceContext * dx11Context = apiContext.SafeCast<vaRenderDeviceContextDX11*>( )->GetDXContext( );

    if( format == vaResourceFormat::Automatic )
        format = GetResourceFormat();

    dx11Context->ResolveSubresource( dstResource->SafeCast<vaTextureDX11*>( )->GetResource(), dstSubresource, GetResource(), srcSubresource, DXGIFormatFromVA( format ) );
}

void vaTextureDX11::SetToAPISlotSRV( vaRenderDeviceContext & apiContext, int slot, bool assertOnOverwrite )
{
    ID3D11DeviceContext * dx11Context = apiContext.SafeCast<vaRenderDeviceContextDX11*>( )->GetDXContext();

    if( assertOnOverwrite )
    {
        vaDirectXTools::AssertSetToD3DContextAllShaderTypes( dx11Context, ( ID3D11ShaderResourceView* )nullptr, slot );
    }
    vaDirectXTools::SetToD3DContextAllShaderTypes( dx11Context, ( ID3D11ShaderResourceView* )GetSRV(), slot );
}

void vaTextureDX11::UnsetFromAPISlotSRV( vaRenderDeviceContext & apiContext, int slot, bool assertOnNotSet )
{
    ID3D11DeviceContext * dx11Context = apiContext.SafeCast<vaRenderDeviceContextDX11*>( )->GetDXContext();

    if( assertOnNotSet )
    {
        vaDirectXTools::AssertSetToD3DContextAllShaderTypes( dx11Context, ( ID3D11ShaderResourceView* )GetSRV(), slot );
    }
   vaDirectXTools::SetToD3DContextAllShaderTypes( dx11Context, ( ID3D11ShaderResourceView* )nullptr, slot );
}

void vaTextureDX11::SetToAPISlotUAV_CS( vaRenderDeviceContext & apiContext, int slot, uint32 initialCount, bool assertOnOverwrite )
{
    ID3D11DeviceContext * dx11Context = apiContext.SafeCast<vaRenderDeviceContextDX11*>( )->GetDXContext();

    if( assertOnOverwrite )
    {
        vaDirectXTools::AssertSetToD3DContextCS( dx11Context, ( ID3D11UnorderedAccessView* )nullptr, slot );
    }
    vaDirectXTools::SetToD3DContextCS( dx11Context, ( ID3D11UnorderedAccessView* )GetUAV(), slot, initialCount );
}

void vaTextureDX11::UnsetFromAPISlotUAV_CS( vaRenderDeviceContext & apiContext, int slot, bool assertOnNotSet )
{
    ID3D11DeviceContext * dx11Context = apiContext.SafeCast<vaRenderDeviceContextDX11*>( )->GetDXContext();

    if( assertOnNotSet )
    {
        vaDirectXTools::AssertSetToD3DContextCS( dx11Context, ( ID3D11UnorderedAccessView* )GetUAV(), slot );
    }
    vaDirectXTools::SetToD3DContextCS( dx11Context, ( ID3D11UnorderedAccessView* )nullptr, slot, (uint32)-1 );
}

void vaTextureDX11::UpdateSubresource( vaRenderDeviceContext & apiContext, int dstSubresourceIndex, const vaBoxi & dstBox, void * srcData, int srcDataRowPitch, int srcDataDepthPitch )
{
    ID3D11DeviceContext * dx11Context = apiContext.SafeCast<vaRenderDeviceContextDX11*>( )->GetDXContext( );

    D3D11_BOX d3d11box = { (UINT)dstBox.left, (UINT)dstBox.top, (UINT)dstBox.front, (UINT)dstBox.right, (UINT)dstBox.bottom, (UINT)dstBox.back };
    dx11Context->UpdateSubresource( m_resource, (UINT)dstSubresourceIndex, &d3d11box, srcData, srcDataRowPitch, srcDataDepthPitch );
}

static vaResourceFormat ConvertBCFormatToUncompressedCounterpart( vaResourceFormat format )
{
    switch( format )
    {
    case( vaResourceFormat::BC4_UNORM ):
        format = vaResourceFormat::R8_UNORM; break;
    case( vaResourceFormat::BC5_UNORM ):
        format = vaResourceFormat::R8G8_UNORM; break;
    case( vaResourceFormat::BC6H_UF16 ):
        format = vaResourceFormat::R16G16B16A16_FLOAT; break;
    case( vaResourceFormat::BC1_UNORM_SRGB ):
    case( vaResourceFormat::BC7_UNORM_SRGB ):
        format = vaResourceFormat::R8G8B8A8_UNORM_SRGB; break;
    default: break;
    }
    return format;
}

shared_ptr<vaTexture> vaTextureDX11::CreateLowerResFromMIPs( vaRenderDeviceContext & apiContext, int numberOfMIPsToDrop, bool neverGoBelow4x4 )
{
    if( numberOfMIPsToDrop <= 0 || numberOfMIPsToDrop >= m_mipLevels )
    {
        assert( false );
        VA_ERROR( "numberOfMIPsToDrop must be > 0 and less than the number of MIP levels (%d)", m_mipLevels );
        return nullptr;
    }


    int newTexSizeX = m_sizeX;
    int newTexSizeY = m_sizeY;
    int newTexSizeZ = m_sizeZ;

    for( int i = 0; i < numberOfMIPsToDrop; i++ )
    {
        if( neverGoBelow4x4 )
        {
            if( newTexSizeX == 4 || newTexSizeY == 4 || newTexSizeZ == 4 )
            {
                numberOfMIPsToDrop = i;
                VA_LOG( "vaTextureDX11::CreateLowerResFromMIPs - stopping before required numberOfMipsToDrop due to reaching min size of 4" );
                break;
            }
        }

        // mipmap generation seems to compute mip dimensions by round down (dim >> 1) which means loss of data, so that's why we assert
        assert( ( newTexSizeX % 2 ) == 0 );
        if( m_type == vaTextureType::Texture2D || m_type == vaTextureType::Texture3D )
            assert( ( newTexSizeY % 2 ) == 0 );
        if( m_type == vaTextureType::Texture3D )
            assert( ( newTexSizeZ % 2 ) == 0 );
        newTexSizeX = ( newTexSizeX ) / 2;
        newTexSizeY = ( newTexSizeY ) / 2;
        newTexSizeZ = ( newTexSizeZ ) / 2;
    }

    if( m_type == vaTextureType::Texture2D )
    {
        assert( m_sampleCount == 1 );

        vaResourceFormat newResFormat = m_resourceFormat;
        vaResourceFormat srvFormat = vaResourceFormat::Automatic;
        vaResourceFormat rtvFormat = vaResourceFormat::Automatic;
        vaResourceFormat dsvFormat = vaResourceFormat::Automatic;
        vaResourceFormat uavFormat = vaResourceFormat::Automatic;

        if( ( m_bindSupportFlags & vaResourceBindSupportFlags::ShaderResource ) != 0 )
            srvFormat = m_srvFormat;
        if( ( m_bindSupportFlags & vaResourceBindSupportFlags::RenderTarget ) != 0 )
            rtvFormat = m_rtvFormat;
        if( ( m_bindSupportFlags & vaResourceBindSupportFlags::DepthStencil ) != 0 )
            dsvFormat = m_dsvFormat;
        if( ( m_bindSupportFlags & vaResourceBindSupportFlags::UnorderedAccess ) != 0 )
            uavFormat = m_uavFormat;

        // Handle compressed textures by uncompressing them - not all done, needs more work
        newResFormat = ConvertBCFormatToUncompressedCounterpart( newResFormat );
        srvFormat = ConvertBCFormatToUncompressedCounterpart( srvFormat );
        uavFormat = ConvertBCFormatToUncompressedCounterpart( uavFormat );
        rtvFormat = ConvertBCFormatToUncompressedCounterpart( rtvFormat );

        // add RT so we can render into it as a way of setting mips...
        m_bindSupportFlags |= vaResourceBindSupportFlags::RenderTarget;

        shared_ptr<vaTexture> newTex = Create2D( GetRenderDevice(), newResFormat, newTexSizeX, newTexSizeY, m_mipLevels-numberOfMIPsToDrop, m_sizeZ, 1, m_bindSupportFlags, m_accessFlags, 
                                        srvFormat, rtvFormat, dsvFormat, uavFormat, m_flags, m_contentsType );

        if( newTex != nullptr )
        {
            for( int i = 0; i < m_mipLevels - numberOfMIPsToDrop; i++ )
            {
                shared_ptr<vaTexture> mipRTV = vaTexture::CreateView( *newTex, vaResourceBindSupportFlags::RenderTarget, vaResourceFormat::Unknown, rtvFormat, vaResourceFormat::Unknown, vaResourceFormat::Unknown, vaTextureFlags::None, i, 1 );
                shared_ptr<vaTexture> mipSRV = vaTexture::CreateView( *this, vaResourceBindSupportFlags::ShaderResource, vaResourceFormat::Automatic, vaResourceFormat::Unknown, vaResourceFormat::Unknown, vaResourceFormat::Unknown, vaTextureFlags::None, i + numberOfMIPsToDrop, 1 );

                apiContext.CopySRVToRTV( mipRTV, mipSRV );
            }
            return newTex;
        }
    }

    assert( false );
    VA_ERROR( "Path not yet (fully) implemented, or a bug was encountered" );
    return nullptr;
}

shared_ptr<vaTexture> vaTextureDX11::TryCompress( )
{
    // Already compressed?
    if( (m_resourceFormat >= vaResourceFormat::BC1_TYPELESS && m_resourceFormat <= vaResourceFormat::BC5_SNORM)
        || (m_resourceFormat >= vaResourceFormat::BC6H_TYPELESS && m_resourceFormat <= vaResourceFormat::BC7_UNORM_SRGB) )
        return nullptr;

    DXGI_FORMAT             destinationFormat;
    vaTextureContentsType   destinationContentsType = m_contentsType;
    DWORD                   compressFlags       = DirectX::TEX_COMPRESS_DEFAULT; 

#ifdef _OPENMP
    compressFlags |= DirectX::TEX_COMPRESS_PARALLEL;
#endif

    // normals
    if( ( m_contentsType == vaTextureContentsType::NormalsXYZ_UNORM ) || ( m_contentsType == vaTextureContentsType::NormalsXY_UNORM ) )
    {
        if( ( m_resourceFormat == vaResourceFormat::R8G8_UNORM ) || ( m_resourceFormat == vaResourceFormat::R8G8B8A8_UNORM ) || ( m_resourceFormat == vaResourceFormat::B8G8R8A8_UNORM ) || ( m_resourceFormat == vaResourceFormat::B8G8R8X8_UNORM ) )
        {
            destinationFormat = DXGI_FORMAT_BC5_UNORM;
            destinationContentsType = vaTextureContentsType::NormalsXY_UNORM;
        }
        else
        {
            // format might be ok just not tested - try out and see
            assert( false );
            return nullptr;
        }
    }
    else if( m_contentsType == vaTextureContentsType::GenericColor )
    {
        if( ( m_resourceFormat == vaResourceFormat::R8G8B8A8_UNORM_SRGB ) || ( m_resourceFormat == vaResourceFormat::B8G8R8A8_UNORM_SRGB ) )
        {
            destinationFormat = DXGI_FORMAT_BC7_UNORM_SRGB;
        }
        else
        {
            // format might be ok just not tested - try out and see
            assert( false );
            return nullptr;
        }
    } 
    else if( m_contentsType == vaTextureContentsType::GenericLinear )
    {
        assert( false );
        if( ( m_resourceFormat == vaResourceFormat::R8G8B8A8_UNORM ) || ( m_resourceFormat == vaResourceFormat::B8G8R8A8_UNORM ) )
        {
            destinationFormat = DXGI_FORMAT_BC7_UNORM;
        }
        else
        {
            // format might be ok just not tested - try out and see
            assert( false );
            return nullptr;
        }
    }
    else if( m_contentsType == vaTextureContentsType::SingleChannelLinearMask )
    {
        if( ( m_resourceFormat == vaResourceFormat::R8G8B8A8_UNORM ) || ( m_resourceFormat == vaResourceFormat::B8G8R8A8_UNORM ) || ( m_resourceFormat == vaResourceFormat::R8_UNORM ) )
        {
            destinationFormat = DXGI_FORMAT_BC4_UNORM;
        }
        else
        {
            // format might be ok just not tested - try out and see
            assert( false );
            return nullptr;
        }
    }
    else
    {
        // not supported yet!
        assert( false );
        return nullptr;
    }


    DirectX::TexMetadata info;
    DirectX::ScratchImage scratchImage;
    DirectX::ScratchImage compressedScratchImage;

    HRESULT hr;
    {
        vaMemoryStream memoryStream( (int64)0, 1024 * 1024 );
        vaDirectXTools::SaveDDSTexture( GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice(), memoryStream, m_resource );
        hr = DirectX::LoadFromDDSMemory( memoryStream.GetBuffer(), memoryStream.GetLength(), DirectX::DDS_FLAGS_NONE, &info, scratchImage );
        assert( SUCCEEDED( hr ) );
        if( !SUCCEEDED( hr ) )
        {
            return nullptr;
        }
    }




    hr = DirectX::Compress( scratchImage.GetImages( ), scratchImage.GetImageCount( ), info, destinationFormat, compressFlags, DirectX::TEX_THRESHOLD_DEFAULT, compressedScratchImage );
    assert( SUCCEEDED( hr ) );
    if( !SUCCEEDED( hr ) )
    {
        return nullptr;
    }
    
    DirectX::Blob blob;
    hr = DirectX::SaveToDDSMemory( compressedScratchImage.GetImages( ), compressedScratchImage.GetImageCount( ), compressedScratchImage.GetMetadata( ), DirectX::DDS_FLAGS_NONE, blob );
    assert( SUCCEEDED( hr ) );
    if( !SUCCEEDED( hr ) )
    {
        return nullptr;
    }

    return vaTexture::CreateFromImageData( GetRenderDevice(), blob.GetBufferPointer(), blob.GetBufferSize(), vaTextureLoadFlags::Default, GetBindSupportFlags(), destinationContentsType );
}

bool vaTextureDX11::InternalCreate1D( vaResourceFormat format, int width, int mipLevels, int arraySize, vaResourceBindSupportFlags bindFlags, vaTextureAccessFlags accessFlags, vaResourceFormat srvFormat, vaResourceFormat rtvFormat, vaResourceFormat dsvFormat, vaResourceFormat uavFormat, vaTextureFlags flags, vaTextureContentsType contentsType, void * initialData )
{
    Initialize( bindFlags, format, srvFormat, rtvFormat, dsvFormat, uavFormat, flags, 0, -1, 0, -1, contentsType );

    D3D11_SUBRESOURCE_DATA dxInitDataObj;
    D3D11_SUBRESOURCE_DATA * dxInitDataPtr = NULL;
    if( initialData != NULL )
    {
        dxInitDataObj.pSysMem = initialData;
        dxInitDataObj.SysMemPitch = 0;
        dxInitDataObj.SysMemSlicePitch = 0;
        dxInitDataPtr = &dxInitDataObj;
    }

    if( (accessFlags & vaTextureAccessFlags::CPURead) != 0 )
    {
        // Cannot have any shader binds if texture has CPU read access flag
        assert( bindFlags == vaResourceBindSupportFlags::None );
        if( bindFlags != vaResourceBindSupportFlags::None )
            return NULL;
    }

    // do we need an option to make this any other usage? staging will be needed likely
    D3D11_USAGE dx11Usage = DX11UsageFromVAAccessFlags( accessFlags );
    UINT dx11BindFlags = BindFlagsDXFromVA( bindFlags );
    UINT dx11MiscFlags = TexFlagsDXFromVA( flags );
    if( ( dx11Usage == D3D11_USAGE_DYNAMIC ) && ( bindFlags == 0 ) )
    {
        assert( false ); // codepath not tested
        dx11Usage = D3D11_USAGE_STAGING; // is this intended? I think it is
    }

    ID3D11Texture1D * resource = nullptr;
    CD3D11_TEXTURE1D_DESC desc( DXGIFormatFromVA(format), width, arraySize, mipLevels, dx11BindFlags, dx11Usage, CPUAccessFlagsDXFromVA( accessFlags ), dx11MiscFlags  );
    HRESULT hr = GetRenderDevice().SafeCast<vaRenderDeviceDX11*>()->GetPlatformDevice()->CreateTexture1D( &desc, dxInitDataPtr, &resource );
    assert( SUCCEEDED(hr) );

    //ID3D11Resource * resource = vaDirectXTools::CreateTexture1D( DXGIFormatFromVA(format), width, dxInitDataPtr, arraySize, mipLevels, dx11BindFlags, dx11Usage, CPUAccessFlagsDXFromVA( accessFlags ), dx11MiscFlags );

    if( SUCCEEDED(hr) && resource != NULL )
    {
        SetResource( resource );
        ProcessResource( false );
        return true;
    }
    else
    {
        assert( false );
        return false;
    }
}

bool vaTextureDX11::InternalCreate2D( vaResourceFormat format, int width, int height, int mipLevels, int arraySize, int sampleCount, vaResourceBindSupportFlags bindFlags, vaTextureAccessFlags accessFlags, vaResourceFormat srvFormat, vaResourceFormat rtvFormat, vaResourceFormat dsvFormat, vaResourceFormat uavFormat, vaTextureFlags flags, vaTextureContentsType contentsType, void * initialData, int initialDataPitch )
{
    Initialize( bindFlags, format, srvFormat, rtvFormat, dsvFormat, uavFormat, flags, 0, -1, 0, -1, contentsType );

    D3D11_SUBRESOURCE_DATA dxInitDataObj;
    D3D11_SUBRESOURCE_DATA * dxInitDataPtr = NULL;
    if( initialData != NULL )
    {
        dxInitDataObj.pSysMem           = initialData;
        dxInitDataObj.SysMemPitch       = initialDataPitch;
        dxInitDataObj.SysMemSlicePitch  = 0;
        dxInitDataPtr = &dxInitDataObj;
    }

    // do we need an option to make this any other usage? staging will be needed likely
    D3D11_USAGE dx11Usage = DX11UsageFromVAAccessFlags( accessFlags );
    UINT dx11BindFlags = BindFlagsDXFromVA( bindFlags );
    UINT dx11MiscFlags = TexFlagsDXFromVA( flags );
    if( ( dx11Usage == D3D11_USAGE_DYNAMIC ) && ( bindFlags == 0 ) )
    {
        dx11Usage = D3D11_USAGE_STAGING; // is this intended? I think it is
    }

    UINT sampleQuality = 0; //(sampleCount>1)?((UINT)D3D11_STANDARD_MULTISAMPLE_PATTERN):(0);

    ID3D11Texture2D * resource = nullptr;
    CD3D11_TEXTURE2D_DESC desc( DXGIFormatFromVA(format), width, height, arraySize, mipLevels, dx11BindFlags, dx11Usage, CPUAccessFlagsDXFromVA(accessFlags), sampleCount, sampleQuality, dx11MiscFlags );
    HRESULT hr = GetRenderDevice().SafeCast<vaRenderDeviceDX11*>()->GetPlatformDevice()->CreateTexture2D( &desc, dxInitDataPtr, &resource );
    assert( SUCCEEDED(hr) );

    //ID3D11Resource * resource = vaDirectXTools::CreateTexture2D( DXGIFormatFromVA(format), width, height, dxInitDataPtr, arraySize, mipLevels, dx11BindFlags, dx11Usage, CPUAccessFlagsDXFromVA(accessFlags), sampleCount, sampleQuality, dx11MiscFlags );
    
    if( SUCCEEDED(hr) && resource != NULL )
    {
        SetResource( resource );
        ProcessResource( false );
        return true;
    }
    else
    {
        assert( false );
        return false;
    }
}

bool vaTextureDX11::InternalCreate3D( vaResourceFormat format, int width, int height, int depth, int mipLevels, vaResourceBindSupportFlags bindFlags, vaTextureAccessFlags accessFlags, vaResourceFormat srvFormat, vaResourceFormat rtvFormat, vaResourceFormat dsvFormat, vaResourceFormat uavFormat, vaTextureFlags flags, vaTextureContentsType contentsType, void * initialData, int initialDataPitch, int initialDataSlicePitch )
{
    Initialize( bindFlags, format, srvFormat, rtvFormat, dsvFormat, uavFormat, flags, 0, -1, 0, -1, contentsType );

    D3D11_SUBRESOURCE_DATA dxInitDataObj;
    D3D11_SUBRESOURCE_DATA * dxInitDataPtr = NULL;
    if( initialData != NULL )
    {
        dxInitDataObj.pSysMem = initialData;
        dxInitDataObj.SysMemPitch = initialDataPitch;
        dxInitDataObj.SysMemSlicePitch = initialDataSlicePitch;
        dxInitDataPtr = &dxInitDataObj;
    }

    // do we need an option to make this any other usage? staging will be needed likely
    D3D11_USAGE dx11Usage = DX11UsageFromVAAccessFlags( accessFlags );
    UINT dx11BindFlags = BindFlagsDXFromVA( bindFlags );
    UINT dx11MiscFlags = TexFlagsDXFromVA( flags );
    if( ( dx11Usage == D3D11_USAGE_DYNAMIC ) && ( bindFlags == 0 ) )
    {
        assert( false ); // codepath not tested
        dx11Usage = D3D11_USAGE_STAGING; // is this intended? I think it is
    }

    ID3D11Texture3D * resource = nullptr;
    CD3D11_TEXTURE3D_DESC desc( DXGIFormatFromVA(format), width, height, depth, mipLevels, dx11BindFlags, dx11Usage, CPUAccessFlagsDXFromVA( accessFlags ), dx11MiscFlags );
    HRESULT hr = GetRenderDevice().SafeCast<vaRenderDeviceDX11*>()->GetPlatformDevice()->CreateTexture3D( &desc, dxInitDataPtr, &resource );
    assert( SUCCEEDED(hr) );

    // ID3D11Resource * resource = vaDirectXTools::CreateTexture3D( DXGIFormatFromVA(format), width, height, depth, dxInitDataPtr, mipLevels, dx11BindFlags, dx11Usage, CPUAccessFlagsDXFromVA( accessFlags ), dx11MiscFlags );

    if( SUCCEEDED(hr) && resource != NULL )
    {
        SetResource( resource );
        ProcessResource( false );
        return true;
    }
    else
    {
        assert( false );
        return false;
    }
}

shared_ptr<vaTexture> vaTextureDX11::CreateView( vaResourceBindSupportFlags bindFlags, vaResourceFormat srvFormat, vaResourceFormat rtvFormat, vaResourceFormat dsvFormat, vaResourceFormat uavFormat, vaTextureFlags flags, int viewedMipSliceMin, int viewedMipSliceCount, int viewedArraySliceMin, int viewedArraySliceCount )
{
    vaTextureDX11 * origDX11Texture = this->SafeCast<vaTextureDX11*>( );

    // -1 means all above min
    if( viewedMipSliceCount == -1 )
        viewedMipSliceCount = this->GetMipLevels() - viewedMipSliceMin;
    if( viewedArraySliceCount == -1 )
        viewedArraySliceCount = this->GetArrayCount() - viewedArraySliceMin;
    assert( viewedMipSliceCount > 0 );
    assert( viewedArraySliceCount > 0 );

    assert( viewedMipSliceMin >= 0 && viewedMipSliceMin < this->GetMipLevels() );
    assert( (viewedMipSliceMin+viewedMipSliceCount) > 0 && (viewedMipSliceMin+viewedMipSliceCount) <= this->GetMipLevels() );
    assert( viewedArraySliceMin >= 0 && viewedArraySliceMin < this->GetArrayCount( ) );
    assert( ( viewedArraySliceMin + viewedArraySliceCount ) > 0 && ( viewedArraySliceMin + viewedArraySliceCount ) <= this->GetArrayCount( ) );

    ID3D11Resource * resource = origDX11Texture->GetResource( );

    if( resource == NULL )
    {
        assert( false ); 
        return NULL;
    }

    // Can't request additional binding flags that were not supported in the original texture
    vaResourceBindSupportFlags origFlags = origDX11Texture->GetBindSupportFlags();
    assert( ((~origFlags) & bindFlags) == 0 );
    origFlags; // unreferenced in Release

    shared_ptr<vaTexture> newTexture = VA_RENDERING_MODULE_CREATE_SHARED( vaTexture, vaTextureConstructorParams( GetRenderDevice(), vaCore::GUIDCreate( ) ) );
    newTexture->Initialize( bindFlags, this->GetResourceFormat(), srvFormat, rtvFormat, dsvFormat, uavFormat, this->GetFlags(), viewedMipSliceMin, viewedMipSliceCount, viewedArraySliceMin, viewedArraySliceCount, this->GetContentsType() );
    
    // it is debatable whether this is needed since DX resources have reference counting and will stay alive, but it might be useful for DX12 or other API implementations so I'll leave it in (actually, I removed it later :P)
    // newTexture->m_viewedOriginal = texture;
    newTexture->m_isView = true;

    vaTextureDX11 * newDX11Texture = newTexture->SafeCast<vaTextureDX11*>( );
    resource->AddRef( );
    newDX11Texture->SetResource( resource );
    newDX11Texture->m_flags = flags;            // override flags (currently only used for cubemaps)
    newDX11Texture->ProcessResource( true, true );

    // since we've used nonAllBindViewsNeeded==true in ProcessResource above, we have to manually check if the binds requested for this specific view were correctly created
    if( ( bindFlags & vaResourceBindSupportFlags::ShaderResource ) != 0 )
        { assert( srvFormat != vaResourceFormat::Unknown ); assert( newDX11Texture->m_srv != nullptr ); }
    else
        { assert( srvFormat == vaResourceFormat::Unknown || srvFormat == vaResourceFormat::Automatic ); assert( newDX11Texture->m_srv == nullptr ); }
    if( ( bindFlags & vaResourceBindSupportFlags::RenderTarget ) != 0 )
        { assert( rtvFormat != vaResourceFormat::Unknown ); assert( newDX11Texture->m_rtv != nullptr ); }
    else
        { assert( rtvFormat == vaResourceFormat::Unknown || rtvFormat == vaResourceFormat::Automatic ); assert( newDX11Texture->m_rtv == nullptr ); }
    if( ( bindFlags & vaResourceBindSupportFlags::DepthStencil ) != 0 )
        { assert( dsvFormat != vaResourceFormat::Unknown ); assert( newDX11Texture->m_dsv != nullptr ); }
    else
        { assert( dsvFormat == vaResourceFormat::Unknown || dsvFormat == vaResourceFormat::Automatic ); assert( newDX11Texture->m_dsv == nullptr ); }
    if( ( bindFlags & vaResourceBindSupportFlags::UnorderedAccess ) != 0 )
        { assert( uavFormat != vaResourceFormat::Unknown ); assert( newDX11Texture->m_uav != nullptr ); }
    else
        { assert( uavFormat == vaResourceFormat::Unknown || uavFormat == vaResourceFormat::Automatic ); assert( newDX11Texture->m_uav == nullptr ); }

    return newTexture;
}
