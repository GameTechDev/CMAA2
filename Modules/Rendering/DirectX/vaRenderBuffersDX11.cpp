///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017, filip.strugar@gmail.com
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

#include "vaRenderBuffersDX11.h"

#include "vaRenderingToolsDX11.h"

#include "Rendering/DirectX/vaRenderDeviceContextDX11.h"


using namespace VertexAsylum;

vaConstantBufferDX11::vaConstantBufferDX11( const vaRenderingModuleParams & params ) : vaConstantBuffer( params )
{
    m_buffer = nullptr;
}

vaConstantBufferDX11::~vaConstantBufferDX11( )
{
    SAFE_RELEASE( m_buffer );
    m_dataSize = 0;
}

static ID3D11Buffer * CreateBuffer( ID3D11Device * device, uint32 sizeInBytes, uint32 bindFlags, D3D11_USAGE usage, uint32 cpuAccessFlags, uint32 miscFlags, uint32 structByteStride, const void * initializeData )
{
    D3D11_BUFFER_DESC desc;
    desc.ByteWidth = sizeInBytes;
    desc.Usage = usage;
    desc.BindFlags = bindFlags;
    desc.CPUAccessFlags = cpuAccessFlags;
    desc.MiscFlags = miscFlags;
    desc.StructureByteStride = structByteStride;
    HRESULT hr;
    ID3D11Buffer * buffer = NULL;
    if( initializeData == NULL )
    {
        V( device->CreateBuffer( &desc, NULL, &buffer ) );
    }
    else
    {
        D3D11_SUBRESOURCE_DATA initSubresData;
        initSubresData.pSysMem = initializeData;
        initSubresData.SysMemPitch = sizeInBytes;
        initSubresData.SysMemSlicePitch = sizeInBytes;
        V( device->CreateBuffer( &desc, &initSubresData, &buffer ) );
    }
    if( FAILED( hr ) )
        return NULL;
    // vaDirectXCore::NameObject( buffer, "vaDirectXTools::CreateBuffer" );
    return buffer;
}

void vaConstantBufferDX11::Create( int bufferSize, const void * initialData )
{
    Destroy( );
    if( bufferSize > 0 )
    {
        m_buffer = CreateBuffer( GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice(), bufferSize, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DEFAULT, 0, 0, 0, initialData );
        m_dataSize = bufferSize;
    }
}

void vaConstantBufferDX11::Destroy( )
{
    SAFE_RELEASE( m_buffer );
    m_dataSize = 0;
}

void vaConstantBufferDX11::Update( ID3D11DeviceContext * dx11Context, const void * data, uint32 dataSize )
{
    assert( dataSize <= m_dataSize );

    dx11Context->UpdateSubresource( m_buffer, 0, NULL, data, (UINT)dataSize, 0 );
}

void vaConstantBufferDX11::Update( vaRenderDeviceContext & apiContext, const void * data, uint32 dataSize )
{
    Update( apiContext.SafeCast<vaRenderDeviceContextDX11*>( )->GetDXContext(), data, dataSize );
}

void vaConstantBufferDX11::SetToAPISlot( vaRenderDeviceContext & apiContext, int slot, bool assertOnOverwrite )
{
    ID3D11DeviceContext * dx11Context = apiContext.SafeCast<vaRenderDeviceContextDX11*>( )->GetDXContext();

    if( assertOnOverwrite )
    {
        vaDirectXTools::AssertSetToD3DContextAllShaderTypes( dx11Context, ( ID3D11Buffer* )nullptr, slot );
    }
    vaDirectXTools::SetToD3DContextAllShaderTypes( dx11Context, ( ID3D11Buffer* )GetBuffer(), slot );
}

void vaConstantBufferDX11::UnsetFromAPISlot( vaRenderDeviceContext & apiContext, int slot, bool assertOnNotSet )
{
    ID3D11DeviceContext * dx11Context = apiContext.SafeCast<vaRenderDeviceContextDX11*>( )->GetDXContext();

    if( assertOnNotSet )
    {
        vaDirectXTools::AssertSetToD3DContextAllShaderTypes( dx11Context, ( ID3D11Buffer* )GetBuffer(), slot );
    }
   vaDirectXTools::SetToD3DContextAllShaderTypes( dx11Context, ( ID3D11Buffer* )nullptr, slot );
}

//////////////////////////////////////////////////////////////////////////

vaIndexBufferDX11::vaIndexBufferDX11( const vaRenderingModuleParams & params ) : vaIndexBuffer( params )
{
    m_buffer = nullptr;
}

vaIndexBufferDX11::~vaIndexBufferDX11( )
{
    SAFE_RELEASE( m_buffer );
    m_indexCount = 0;
    m_dataSize = 0;
}

void vaIndexBufferDX11::Create( int indexCount, const void * initialData )
{
    Destroy( );

    if( indexCount > 0 )
    {
        m_buffer        = CreateBuffer( GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice(), indexCount * 4, D3D11_BIND_INDEX_BUFFER, D3D11_USAGE_DEFAULT, 0, 0, 0, initialData );
        m_indexCount    = indexCount;
        m_dataSize      = indexCount * 4;
    }
}

void vaIndexBufferDX11::Destroy( )
{
    SAFE_RELEASE( m_buffer );
    m_indexCount    = 0;
    m_dataSize      = 0;
}

void vaIndexBufferDX11::Update( vaRenderDeviceContext & apiContext, const void * data, uint32  dataSize )
{
    ID3D11DeviceContext * dx11Context = apiContext.SafeCast<vaRenderDeviceContextDX11*>( )->GetDXContext();

    assert( dataSize <= m_dataSize );

    dx11Context->UpdateSubresource( m_buffer, 0, NULL, data, (UINT)dataSize, 0 );
}

//////////////////////////////////////////////////////////////////////////

vaVertexBufferDX11::vaVertexBufferDX11( const vaRenderingModuleParams & params ) : vaVertexBuffer( params )
{
    m_buffer = nullptr;
}

vaVertexBufferDX11::~vaVertexBufferDX11( )
{
    SAFE_RELEASE( m_buffer );
    m_vertexCount = 0;
    m_vertexSize = 0;
    m_dataSize = 0;
    m_dynamic = 0;
    assert( !IsMapped() );
}

void vaVertexBufferDX11::Create( int vertexCount, int vertexSize, const void * initialData, bool dynamic )
{
    Destroy();

    int bufferSize = vertexSize * vertexCount;

    if( vertexCount > 0 )
    {
        assert( vertexSize > 0 );

        D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
        uint32 cpuAccessFlags = 0;
        if( dynamic )
        {
            usage = D3D11_USAGE_DYNAMIC;
            cpuAccessFlags = D3D11_CPU_ACCESS_WRITE;
        }

        m_buffer        = CreateBuffer( GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice(), bufferSize, D3D11_BIND_VERTEX_BUFFER, usage, cpuAccessFlags, 0, 0, initialData );
        m_vertexCount   = vertexCount;
        m_vertexSize    = vertexSize;
        m_dataSize      = bufferSize;
        m_dynamic       = dynamic;
    }
}

void vaVertexBufferDX11::Destroy( )
{
    SAFE_RELEASE( m_buffer );

    m_vertexCount   = 0;
    m_vertexSize    = 0;
    m_dataSize      = 0;
    m_dynamic       = 0;
}

void vaVertexBufferDX11::Update( vaRenderDeviceContext & apiContext, const void * data, uint32 dataSize )
{
    ID3D11DeviceContext * dx11Context = apiContext.SafeCast<vaRenderDeviceContextDX11*>( )->GetDXContext();

    assert( dataSize <= m_dataSize );

    D3D11_BOX dstBox;
    dstBox.left = 0;
    dstBox.right = dataSize;            //  Coordinates are in bytes for buffers and in texels for textures. (https://msdn.microsoft.com/en-us/library/windows/desktop/ff476486(v=vs.85).aspx)
    dstBox.top = 0;
    dstBox.bottom = 1;
    dstBox.front = 0;
    dstBox.back = 1;

    dx11Context->UpdateSubresource( m_buffer, 0, &dstBox, data, 0, 0 );
}

bool vaVertexBufferDX11::TryMap( vaRenderDeviceContext & apiContext, vaResourceMapType mapType, bool doNotWait )
{
    ID3D11DeviceContext * dx11Context = apiContext.SafeCast<vaRenderDeviceContextDX11*>( )->GetDXContext();

    if( IsMapped() || !m_dynamic )
        { assert( false ); return false; }


    D3D11_MAP d3d11MapType = D3D11_MAP_WRITE;
    switch( mapType )
    {
    case( vaResourceMapType::Write ):           d3d11MapType = D3D11_MAP_WRITE;                 break;
    case( vaResourceMapType::WriteDiscard ):    d3d11MapType = D3D11_MAP_WRITE_DISCARD;         break;
    case( vaResourceMapType::WriteNoOverwrite ):d3d11MapType = D3D11_MAP_WRITE_NO_OVERWRITE;    break;
    default: { assert( false ); return false; } 
    }

    D3D11_MAPPED_SUBRESOURCE mappedSubresource;
    HRESULT hr = dx11Context->Map( m_buffer, 0, d3d11MapType, (doNotWait)?(D3D11_MAP_FLAG_DO_NOT_WAIT):(0), &mappedSubresource );
    if( !SUCCEEDED( hr ) )
        return false;

    m_mappedData = mappedSubresource.pData;
    
    return true;
}

void vaVertexBufferDX11::Unmap( vaRenderDeviceContext & apiContext )
{
    ID3D11DeviceContext * dx11Context = apiContext.SafeCast<vaRenderDeviceContextDX11*>( )->GetDXContext();

    assert( IsMapped() );
    if( !IsMapped() )
        return;

    m_mappedData = nullptr;
    dx11Context->Unmap( m_buffer, 0 );
}

//////////////////////////////////////////////////////////////////////////

vaStructuredBufferDX11::vaStructuredBufferDX11( const vaRenderingModuleParams & params ) : vaStructuredBuffer( params )
{
    m_buffer    = nullptr;
    m_bufferUAV = nullptr;
    m_bufferSRV = nullptr;
}

vaStructuredBufferDX11::~vaStructuredBufferDX11( )
{
    SAFE_RELEASE( m_buffer );
    SAFE_RELEASE( m_bufferUAV );
    SAFE_RELEASE( m_bufferSRV );
    m_structureByteSize = 0;
    m_elementCount      = 0;
    m_dataSize          = 0;
}

bool vaStructuredBufferDX11::Create( int elementCount, int structureByteSize, bool hasCounter, const void * initialData )
{
    Destroy();

    if( elementCount > 0 )
    {
        assert( structureByteSize > 0 );

        m_structureByteSize = structureByteSize;
        m_elementCount      = elementCount;
        m_dataSize          = m_structureByteSize * m_elementCount;

        HRESULT hr;
        ID3D11Device * device = GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice();

        CD3D11_BUFFER_DESC cdesc( m_elementCount * m_structureByteSize, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, D3D11_USAGE_DEFAULT, 0, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, m_structureByteSize );

        if( initialData == nullptr )
        {
            V( device->CreateBuffer( &cdesc, nullptr, &m_buffer ) );
            if( FAILED(hr) ) return false;
        }
        else
        {
            D3D11_SUBRESOURCE_DATA subRes;
            subRes.pSysMem = initialData;
            subRes.SysMemPitch = m_elementCount * m_structureByteSize;
            subRes.SysMemSlicePitch = m_elementCount * m_structureByteSize;
            V( device->CreateBuffer( &cdesc, &subRes, &m_buffer ) );
            if( FAILED(hr) ) return false;
        }

        if( m_buffer != nullptr )
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
            ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
            SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
            SRVDesc.Buffer.FirstElement = 0;
            SRVDesc.Buffer.NumElements = cdesc.ByteWidth / cdesc.StructureByteStride;
            V( device->CreateShaderResourceView( m_buffer, &SRVDesc, &m_bufferSRV ) );
            if( FAILED( hr ) )
            {
                SAFE_RELEASE( m_buffer );
                return false;
            }
        }

        if( m_buffer != nullptr )
        {
            D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc;
            UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
            UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
            UAVDesc.Buffer.FirstElement = 0;
            UAVDesc.Buffer.NumElements = cdesc.ByteWidth / cdesc.StructureByteStride;
            UAVDesc.Buffer.Flags = (hasCounter)?(D3D11_BUFFER_UAV_FLAG_COUNTER):(0);
            V( device->CreateUnorderedAccessView( m_buffer, &UAVDesc, &m_bufferUAV ) );
            if( FAILED( hr ) )
            {
                SAFE_RELEASE( m_buffer );
                SAFE_RELEASE( m_bufferSRV );
                return false;
            }
        }
    }
    return true;
}

void vaStructuredBufferDX11::Destroy( )
{
    SAFE_RELEASE( m_buffer );
    SAFE_RELEASE( m_bufferUAV );
    SAFE_RELEASE( m_bufferSRV );
    m_structureByteSize = 0;
    m_elementCount      = 0;
    m_dataSize          = 0;
}

void vaStructuredBufferDX11::Update( vaRenderDeviceContext & apiContext, const void * data, uint32 dataSize )
{
    ID3D11DeviceContext * dx11Context = apiContext.SafeCast<vaRenderDeviceContextDX11*>( )->GetDXContext();

    assert( dataSize <= m_dataSize );

    D3D11_BOX dstBox;
    dstBox.left = 0;
    dstBox.right = dataSize;        //  Coordinates are in bytes for buffers and in texels for textures. (https://msdn.microsoft.com/en-us/library/windows/desktop/ff476486(v=vs.85).aspx)
    dstBox.top = 0;
    dstBox.bottom = 1;
    dstBox.front = 0;
    dstBox.back = 1;

    dx11Context->UpdateSubresource( m_buffer, 0, &dstBox, data, 0, 0 );
}
