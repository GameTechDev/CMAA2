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

#include "vaTexture.h"

using namespace VertexAsylum;

vaTexture::vaTexture( const vaRenderingModuleParams & params ) : 
    vaRenderingModule( params ), vaAssetResource( vaSaferStaticCast< const vaTextureConstructorParams &, const vaRenderingModuleParams &>( params ).UID )
{ 
    m_bindSupportFlags      = vaResourceBindSupportFlags::None;
    m_resourceFormat        = vaResourceFormat::Unknown;
    m_srvFormat             = vaResourceFormat::Unknown;
    m_rtvFormat             = vaResourceFormat::Unknown;
    m_dsvFormat             = vaResourceFormat::Unknown;
    m_uavFormat             = vaResourceFormat::Unknown;

    m_accessFlags           = vaTextureAccessFlags::None;
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

    m_isView                = false;

//    assert( vaRenderingCore::IsInitialized( ) ); 
}

void vaTexture::Initialize( vaResourceBindSupportFlags binds, vaResourceFormat resourceFormat, vaResourceFormat srvFormat, vaResourceFormat rtvFormat, vaResourceFormat dsvFormat, vaResourceFormat uavFormat, vaTextureFlags flags, int viewedMipSliceMin, int viewedMipSliceCount, int viewedArraySliceMin, int viewedArraySliceCount, vaTextureContentsType contentsType )
{
    m_bindSupportFlags      = binds;
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
    texture->Initialize( binds );

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

shared_ptr<vaTexture> vaTexture::CreateFromImageData( vaRenderDevice & device, void * buffer, uint64 bufferSize, vaTextureLoadFlags loadFlags, vaResourceBindSupportFlags binds, vaTextureContentsType contentsType )
{
    shared_ptr<vaTexture> texture = VA_RENDERING_MODULE_CREATE_SHARED( vaTexture, vaTextureConstructorParams( device, vaCore::GUIDCreate( ) ) );
    texture->Initialize( binds );

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
    else if( original.GetType( ) == vaTextureType::Texture1D || original.GetType( ) == vaTextureType::Texture1DArray )
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

shared_ptr<vaTexture> vaTexture::Create1D( vaRenderDevice & device, vaResourceFormat format, int width, int mipLevels, int arraySize, vaResourceBindSupportFlags bindFlags, vaTextureAccessFlags accessFlags, vaResourceFormat srvFormat, vaResourceFormat rtvFormat, vaResourceFormat dsvFormat, vaResourceFormat uavFormat, vaTextureFlags flags, vaTextureContentsType contentsType, void * initialData )
{
    shared_ptr<vaTexture> texture = VA_RENDERING_MODULE_CREATE_SHARED( vaTexture, vaTextureConstructorParams( device, vaCore::GUIDCreate( ) ) );
    if( !texture->InternalCreate1D( format, width, mipLevels, arraySize, bindFlags, accessFlags, srvFormat, rtvFormat, dsvFormat, uavFormat, flags, contentsType, initialData ) )
        { assert( false ); return nullptr; }
    return texture;
}

shared_ptr<vaTexture> vaTexture::Create2D( vaRenderDevice & device, vaResourceFormat format, int width, int height, int mipLevels, int arraySize, int sampleCount, vaResourceBindSupportFlags bindFlags, vaTextureAccessFlags accessFlags, vaResourceFormat srvFormat, vaResourceFormat rtvFormat, vaResourceFormat dsvFormat, vaResourceFormat uavFormat, vaTextureFlags flags, vaTextureContentsType contentsType, void * initialData, int initialDataPitch )
{
    shared_ptr<vaTexture> texture = VA_RENDERING_MODULE_CREATE_SHARED( vaTexture, vaTextureConstructorParams( device, vaCore::GUIDCreate( ) ) );
    if( !texture->InternalCreate2D( format, width, height, mipLevels, arraySize, sampleCount, bindFlags, accessFlags, srvFormat, rtvFormat, dsvFormat, uavFormat, flags, contentsType, initialData, initialDataPitch ) )
        { assert( false ); return nullptr; }
    return texture;
}

shared_ptr<vaTexture> vaTexture::Create3D( vaRenderDevice & device, vaResourceFormat format, int width, int height, int depth, int mipLevels, vaResourceBindSupportFlags bindFlags, vaTextureAccessFlags accessFlags, vaResourceFormat srvFormat, vaResourceFormat rtvFormat, vaResourceFormat dsvFormat, vaResourceFormat uavFormat, vaTextureFlags flags, vaTextureContentsType contentsType, void * initialData, int initialDataPitch, int initialDataSlicePitch )
{
    shared_ptr<vaTexture> texture = VA_RENDERING_MODULE_CREATE_SHARED( vaTexture, vaTextureConstructorParams( device, vaCore::GUIDCreate( ) ) );
    if( !texture->InternalCreate3D( format, width, height, depth, mipLevels, bindFlags, accessFlags, srvFormat, rtvFormat, dsvFormat, uavFormat, flags, contentsType, initialData, initialDataPitch, initialDataSlicePitch ) )
        { assert( false ); return nullptr; }
    return texture;
}

shared_ptr<vaTexture> vaTexture::CreateView( vaTexture & texture, vaResourceBindSupportFlags bindFlags, vaResourceFormat srvFormat, vaResourceFormat rtvFormat, vaResourceFormat dsvFormat, vaResourceFormat uavFormat, vaTextureFlags flags, int viewedMipSliceMin, int viewedMipSliceCount, int viewedArraySliceMin, int viewedArraySliceCount )
{
    return texture.CreateView( bindFlags, srvFormat, rtvFormat, dsvFormat, uavFormat, flags, viewedMipSliceMin, viewedMipSliceCount, viewedArraySliceMin, viewedArraySliceCount );
}
