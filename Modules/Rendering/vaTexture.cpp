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

#include "vaTexture.h"

#include "vaRenderDevice.h"
#include "vaRenderDeviceContext.h"

using namespace VertexAsylum;

vaResourceFormat             vaTexture::s_nextCreateFastClearFormat         = vaResourceFormat::Unknown;
vaVector4                    vaTexture::s_nextCreateFastClearColorValue     = vaVector4(0,0,0,0);
float                        vaTexture::s_nextCreateFastClearDepthValue     = 0.0f;
uint8                        vaTexture::s_nextCreateFastClearStencilValue   = 0;

vaTexture::vaTexture( const vaRenderingModuleParams & params ) : 
    vaRenderingModule( params ), vaAssetResource( vaSaferStaticCast< const vaTextureConstructorParams &, const vaRenderingModuleParams &>( params ).UID )
{ 
    m_bindSupportFlags      = vaResourceBindSupportFlags::None;
    m_resourceFormat        = vaResourceFormat::Unknown;
    m_srvFormat             = vaResourceFormat::Unknown;
    m_rtvFormat             = vaResourceFormat::Unknown;
    m_dsvFormat             = vaResourceFormat::Unknown;
    m_uavFormat             = vaResourceFormat::Unknown;

    m_accessFlags           = vaResourceAccessFlags::Default;
    m_type                  = vaTextureType::Unknown;
    m_contentsType          = vaTextureContentsType::GenericColor;
    
    m_sizeX                 = 0;
    m_sizeY                 = 0;
    m_sizeZ                 = 0;
    m_sampleCount           = 0;
    m_mipLevels             = 0;

    m_viewedSliceSizeX      = 0;
    m_viewedSliceSizeY      = 0;
    m_viewedSliceSizeZ      = 0;

    m_viewedMipSlice        = 0;
    m_viewedArraySlice      = 0;
    m_viewedMipSliceCount   = -1;
    m_viewedArraySliceCount = -1;

    m_isMapped              = false;

    m_overrideView          = nullptr;
//    assert( vaRenderingCore::IsInitialized( ) ); 
}

void vaTexture::Initialize( vaResourceBindSupportFlags binds, vaResourceAccessFlags accessFlags, vaResourceFormat resourceFormat, vaResourceFormat srvFormat, vaResourceFormat rtvFormat, vaResourceFormat dsvFormat, vaResourceFormat uavFormat, vaTextureFlags flags, int viewedMipSliceMin, int viewedMipSliceCount, int viewedArraySliceMin, int viewedArraySliceCount, vaTextureContentsType contentsType )
{
    m_bindSupportFlags      = binds;
    m_accessFlags           = accessFlags;
    m_resourceFormat        = resourceFormat;
    m_srvFormat             = srvFormat;
    m_rtvFormat             = rtvFormat;
    m_dsvFormat             = dsvFormat;
    m_uavFormat             = uavFormat;
    m_flags                 = flags;
    m_viewedMipSlice        = viewedMipSliceMin;
    m_viewedMipSliceCount   = viewedMipSliceCount;
    m_viewedArraySlice      = viewedArraySliceMin;
    m_viewedArraySliceCount = viewedArraySliceCount;
    m_contentsType          = contentsType;

    // no point having format if no bind support - bind flag maybe forgotten?
    if( (m_srvFormat != vaResourceFormat::Unknown) && (m_srvFormat != vaResourceFormat::Automatic) )
    {
        assert( ( m_bindSupportFlags & vaResourceBindSupportFlags::ShaderResource ) != 0 );
    }
    if( ( m_rtvFormat != vaResourceFormat::Unknown ) && (m_rtvFormat != vaResourceFormat::Automatic) )
    {
        assert( ( m_bindSupportFlags & vaResourceBindSupportFlags::RenderTarget ) != 0 );
    }
    if( ( m_dsvFormat != vaResourceFormat::Unknown ) && (m_dsvFormat != vaResourceFormat::Automatic) )
    {
        assert( ( m_bindSupportFlags & vaResourceBindSupportFlags::DepthStencil ) != 0 );
    }
    if( ( m_uavFormat != vaResourceFormat::Unknown ) && (m_uavFormat != vaResourceFormat::Automatic) )
    {
        assert( ( m_bindSupportFlags & vaResourceBindSupportFlags::UnorderedAccess ) != 0 );
    }
}

shared_ptr<vaTexture> vaTexture::CreateFromImageFile( vaRenderDevice & device, const wstring & storagePath, vaTextureLoadFlags loadFlags, vaResourceBindSupportFlags binds, vaTextureContentsType contentsType )
{
    shared_ptr<vaTexture> texture = VA_RENDERING_MODULE_CREATE_SHARED( vaTexture, vaTextureConstructorParams( device, vaCore::GUIDCreate( ) ) );
    texture->Initialize( binds, vaResourceAccessFlags::Default, vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaTextureFlags::None, 0, -1, 0, -1, vaTextureContentsType::GenericColor );

    if( texture->Import( storagePath, loadFlags, binds, contentsType ) )
    {
        return texture;
    }
    else
    {
        assert( false );
        return nullptr;
    }
}

shared_ptr<vaTexture> vaTexture::CreateFromImageBuffer( vaRenderDevice & device, void * buffer, uint64 bufferSize, vaTextureLoadFlags loadFlags, vaResourceBindSupportFlags binds, vaTextureContentsType contentsType )
{
    shared_ptr<vaTexture> texture = VA_RENDERING_MODULE_CREATE_SHARED( vaTexture, vaTextureConstructorParams( device, vaCore::GUIDCreate( ) ) );
    texture->Initialize( binds, vaResourceAccessFlags::Default, vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaTextureFlags::None, 0, -1, 0, -1, vaTextureContentsType::GenericColor );

    if( texture->Import( buffer, bufferSize, loadFlags, binds, contentsType ) )
    {
        return texture;
    }
    else
    {
        assert( false );
        return nullptr;
    }
}

void vaTexture::CreateMirrorIfNeeded( vaTexture & original, shared_ptr<vaTexture> & mirror )
{
    if( original.IsView() )
        { assert( false ); return; }    // not implemented
    if( original.GetType() == vaTextureType::Buffer )
        { assert( false ); return; }    // not implemented
    else if( original.GetType( ) == vaTextureType::Texture1D )
        { assert( false ); return; }    // not implemented
    else if( original.GetType( ) == vaTextureType::Texture3D )
        { assert( false ); return; }    // not implemented
    else
    {
        if( mirror == nullptr || 
            original.GetResourceFormat() != mirror->GetResourceFormat() || original.GetSRVFormat() != mirror->GetSRVFormat() || original.GetDSVFormat() != mirror->GetDSVFormat()
            || original.GetRTVFormat() != mirror->GetRTVFormat() || original.GetUAVFormat() != mirror->GetUAVFormat() 
            || original.GetSizeX() != mirror->GetSizeX() || original.GetSizeY() != mirror->GetSizeY() || original.GetSizeZ() != mirror->GetSizeZ() 
            || original.GetArrayCount() != mirror->GetArrayCount() || original.GetSampleCount() != mirror->GetSampleCount() || original.GetMipLevels() != mirror->GetMipLevels()  
            || original.GetAccessFlags() != mirror->GetAccessFlags() || original.GetContentsType() != mirror->GetContentsType() || original.GetBindSupportFlags() != mirror->GetBindSupportFlags() 
            || original.GetFlags() != mirror->GetFlags() )
            mirror = shared_ptr<vaTexture>( Create2D( original.GetRenderDevice(), original.GetResourceFormat(), original.GetSizeX(), original.GetSizeY(), original.GetMipLevels(), original.GetArrayCount(), original.GetSampleCount(),
                original.GetBindSupportFlags(), original.GetAccessFlags(), original.GetSRVFormat(), original.GetRTVFormat(), original.GetDSVFormat(), original.GetUAVFormat(), original.GetFlags(), original.GetContentsType(), nullptr, 0 ) );
    }
}

shared_ptr<vaTexture> vaTexture::Create1D( vaRenderDevice & device, vaResourceFormat format, int width, int mipLevels, int arraySize, vaResourceBindSupportFlags bindFlags, vaResourceAccessFlags accessFlags, vaResourceFormat srvFormat, vaResourceFormat rtvFormat, vaResourceFormat dsvFormat, vaResourceFormat uavFormat, vaTextureFlags flags, vaTextureContentsType contentsType, void * initialData )
{
    shared_ptr<vaTexture> texture = VA_RENDERING_MODULE_CREATE_SHARED( vaTexture, vaTextureConstructorParams( device, vaCore::GUIDCreate( ) ) );
    if( !texture->InternalCreate1D( format, width, mipLevels, arraySize, bindFlags, accessFlags, srvFormat, rtvFormat, dsvFormat, uavFormat, flags, contentsType, initialData ) )
        { assert( false ); return nullptr; }
    return texture;
}

shared_ptr<vaTexture> vaTexture::Create2D( vaRenderDevice & device, vaResourceFormat format, int width, int height, int mipLevels, int arraySize, int sampleCount, vaResourceBindSupportFlags bindFlags, vaResourceAccessFlags accessFlags, vaResourceFormat srvFormat, vaResourceFormat rtvFormat, vaResourceFormat dsvFormat, vaResourceFormat uavFormat, vaTextureFlags flags, vaTextureContentsType contentsType, void * initialData, int initialDataRowPitch )
{
    shared_ptr<vaTexture> texture = VA_RENDERING_MODULE_CREATE_SHARED( vaTexture, vaTextureConstructorParams( device, vaCore::GUIDCreate( ) ) );
    if( !texture->InternalCreate2D( format, width, height, mipLevels, arraySize, sampleCount, bindFlags, accessFlags, srvFormat, rtvFormat, dsvFormat, uavFormat, flags, contentsType, initialData, initialDataRowPitch ) )
        { assert( false ); return nullptr; }
    return texture;
}

shared_ptr<vaTexture> vaTexture::Create3D( vaRenderDevice & device, vaResourceFormat format, int width, int height, int depth, int mipLevels, vaResourceBindSupportFlags bindFlags, vaResourceAccessFlags accessFlags, vaResourceFormat srvFormat, vaResourceFormat rtvFormat, vaResourceFormat dsvFormat, vaResourceFormat uavFormat, vaTextureFlags flags, vaTextureContentsType contentsType, void * initialData, int initialDataRowPitch, int initialDataSlicePitch )
{
    shared_ptr<vaTexture> texture = VA_RENDERING_MODULE_CREATE_SHARED( vaTexture, vaTextureConstructorParams( device, vaCore::GUIDCreate( ) ) );
    if( !texture->InternalCreate3D( format, width, height, depth, mipLevels, bindFlags, accessFlags, srvFormat, rtvFormat, dsvFormat, uavFormat, flags, contentsType, initialData, initialDataRowPitch, initialDataSlicePitch ) )
        { assert( false ); return nullptr; }
    return texture;
}

shared_ptr<vaTexture> vaTexture::CreateView( const shared_ptr<vaTexture> & texture, vaResourceBindSupportFlags bindFlags, vaResourceFormat srvFormat, vaResourceFormat rtvFormat, vaResourceFormat dsvFormat, vaResourceFormat uavFormat, vaTextureFlags flags, int viewedMipSliceMin, int viewedMipSliceCount, int viewedArraySliceMin, int viewedArraySliceCount )
{
    return texture->CreateViewInternal( texture, bindFlags, srvFormat, rtvFormat, dsvFormat, uavFormat, flags, viewedMipSliceMin, viewedMipSliceCount, viewedArraySliceMin, viewedArraySliceCount );
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

shared_ptr<vaTexture> vaTexture::CreateLowerResFromMIPs( vaRenderDeviceContext & renderContext, int numberOfMIPsToDrop, bool neverGoBelow4x4 )
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
                VA_LOG( "vaTexture::CreateLowerResFromMIPs - stopping before required numberOfMipsToDrop due to reaching min size of 4" );
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

        shared_ptr<vaTexture> tempThisPtr( this, [](vaTexture*){}); // ooo my gods, hope no one ever sees it. (adding asserts and checks to make sure nothing ever references it out of this scope)
        
        if( newTex != nullptr )
        {
            for( int i = 0; i < m_mipLevels - numberOfMIPsToDrop; i++ )
            {
                shared_ptr<vaTexture> mipRTV = vaTexture::CreateView( newTex, vaResourceBindSupportFlags::RenderTarget, vaResourceFormat::Unknown, rtvFormat, vaResourceFormat::Unknown, vaResourceFormat::Unknown, vaTextureFlags::None, i, 1 );
                shared_ptr<vaTexture> mipSRV = vaTexture::CreateView( tempThisPtr, vaResourceBindSupportFlags::ShaderResource, vaResourceFormat::Automatic, vaResourceFormat::Unknown, vaResourceFormat::Unknown, vaResourceFormat::Unknown, vaTextureFlags::None, i + numberOfMIPsToDrop, 1 );

                renderContext.CopySRVToRTV( mipRTV, mipSRV );
                assert( mipSRV.use_count( ) == 1 ); // see tempThisPtr kludge above!
            }
            return newTex;
        }
        assert( tempThisPtr.use_count( ) == 1 ); // see tempThisPtr kludge above!
    }

    assert( false );
    VA_ERROR( "Path not yet (fully) implemented, or a bug was encountered" );
    return nullptr;
}

void vaTexture::SetNextCreateFastClearRTV( vaResourceFormat format, vaVector4 clearColor )
{
    s_nextCreateFastClearFormat = format;
    s_nextCreateFastClearColorValue = clearColor;
}

void vaTexture::SetNextCreateFastClearDSV( vaResourceFormat format, float clearDepth, uint8 clearStencil )
{
    s_nextCreateFastClearFormat = format;
    s_nextCreateFastClearDepthValue = clearDepth;
    s_nextCreateFastClearStencilValue = clearStencil;

}

