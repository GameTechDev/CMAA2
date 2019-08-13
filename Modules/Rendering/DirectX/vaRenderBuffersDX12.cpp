///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019, filip.strugar@gmail.com
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

#include "vaRenderBuffersDX12.h"

#include "Rendering/DirectX/vaRenderDeviceContextDX12.h"

#pragma warning ( disable : 4238 )  //  warning C4238: nonstandard extension used: class rvalue used as lvalue

using namespace VertexAsylum;


vaConstantBufferDX12::DetachableUploadBuffer::DetachableUploadBuffer( vaRenderDeviceDX12 & device, ComPtr<ID3D12Resource> resource, uint32 sizeInBytes, uint32 actualSizeInBytes ) : Device( device ), CBV( device ), Resource( resource ), SizeInBytes( sizeInBytes ), ActualSizeInBytes( actualSizeInBytes )
{  
    //RSTHAttach( m_resource.Get(), D3D12_RESOURCE_STATE_GENERIC_READ );
    assert( resource != nullptr );
    assert( actualSizeInBytes >= sizeInBytes );
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = { Resource->GetGPUVirtualAddress(), actualSizeInBytes };    
    CBV.Create( cbvDesc ); 
    HRESULT hr;
    CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
    if( SUCCEEDED( hr = Resource->Map( 0, &readRange, reinterpret_cast<void**>(&MappedData) ) ) )
    {
    }
    else
    {
        assert( false );
        MappedData = nullptr;
    }
   
}
vaConstantBufferDX12::DetachableUploadBuffer::~DetachableUploadBuffer( )
{
    if( MappedData != nullptr )
    {
        Resource->Unmap(0, nullptr);
        MappedData = nullptr;
    }
    if( DestroyImmediate )
        Resource.Reset();
    else
        Device.SafeReleaseAfterCurrentGPUFrameDone( Resource, false );
    CBV.SafeRelease();
}

vaConstantBufferDX12::vaConstantBufferDX12( const vaRenderingModuleParams & params ) : vaConstantBuffer( params ), m_GPUConstantBufferCBV( AsDX12(params.RenderDevice) )
{
}

vaConstantBufferDX12::~vaConstantBufferDX12( )
{
    assert( vaThreading::IsMainThread( ) );
    if( m_uploadConstantBuffer != nullptr ) 
    {
        delete m_uploadConstantBuffer;
        m_uploadConstantBuffer = nullptr;
    }
    if( m_GPUConstantBuffer != nullptr )
    {
        AsDX12(GetRenderDevice()).SafeReleaseAfterCurrentGPUFrameDone( m_GPUConstantBuffer, false );
        m_GPUConstantBufferCBV.SafeRelease();
    }
    for( DetachableUploadBuffer * buffs : m_unusedBuffersPool )
    {
        buffs->DestroyImmediate = true;
        delete buffs;
    }
    m_unusedBuffersPool.clear();
}

const vaConstantBufferViewDX12 * vaConstantBufferDX12::GetCBV( ) const 
{ 
    if( m_dynamicUpload )
    {
        if( m_uploadConstantBuffer != nullptr ) 
            return m_uploadConstantBuffer->CBV.IsCreated()?(&m_uploadConstantBuffer->CBV):(nullptr); 
    }
    else
    {
        if( m_GPUConstantBuffer != nullptr )
            return m_GPUConstantBufferCBV.IsCreated()?(&m_GPUConstantBufferCBV):(nullptr);
    }
    return nullptr; 
}

void vaConstantBufferDX12::AllocateNextUploadBuffer( )
{
    assert( m_uploadConstantBuffer == nullptr );
    while( m_unusedBuffersPool.size( ) > 0 )
    {
        auto back = m_unusedBuffersPool.back();
        m_unusedBuffersPool.pop_back();
        if( back->ActualSizeInBytes == m_actualSizeInBytes )
        {
            m_uploadConstantBuffer = back;
            break;
        }
        else
        {
            assert( false ); // why different size? this bit will be ok but check it out anyway
            delete back;
        }
    }

    if( m_uploadConstantBuffer == nullptr )
    {
        ComPtr<ID3D12Resource> resource;
        HRESULT hr;
        V( AsDX12(GetRenderDevice()).GetPlatformDevice()->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer( m_actualSizeInBytes ),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&resource)) );
        resource->SetName( L"vaConstantBufferDX12_upload" );

        m_uploadConstantBuffer = new DetachableUploadBuffer( AsDX12(GetRenderDevice()), resource, m_dataSize, m_actualSizeInBytes );
    }
}

void vaConstantBufferDX12::SafeReleaseUploadBuffer( DetachableUploadBuffer * uploadBuffer )
{
    weak_ptr<vaConstantBufferDX12*> thisBuffer = m_createdThis;

    AsDX12( GetRenderDevice() ).ExecuteAfterCurrentGPUFrameDone(
    [thisBuffer, dataSize = m_dataSize, uploadBuffer]( vaRenderDeviceDX12 & device )
        {
            device;
            shared_ptr<vaConstantBufferDX12*> theBuffer = thisBuffer.lock();
            if( theBuffer != nullptr )
            {
                (*theBuffer)->m_unusedBuffersPool.push_back( uploadBuffer );
            }
            else
            {
                uploadBuffer->DestroyImmediate = true;
                delete uploadBuffer;
            }
        } );
}

void vaConstantBufferDX12::Create( int bufferSize, const void * initialData, bool dynamicUpload )
{
    //std::unique_lock<mutex> mutexLock(m_mutex);

    DestroyInternal(/*false*/);

    m_createdThis = std::make_shared<vaConstantBufferDX12*>( this );

    assert( m_uploadConstantBuffer == nullptr );
    assert( m_GPUConstantBuffer == nullptr );

    assert( bufferSize > 0 );
    if( bufferSize <= 0 )
        return;
    
    m_dataSize      = bufferSize;
    m_dynamicUpload = dynamicUpload;

    const uint32 alignUpToBytes = 256;
    m_actualSizeInBytes = ((m_dataSize-1) / alignUpToBytes + 1) * alignUpToBytes;

    if( m_dynamicUpload || initialData != nullptr )
        AllocateNextUploadBuffer();

    if( !m_dynamicUpload )
    {
        assert( m_GPUConstantBuffer == nullptr );
        HRESULT hr;
        V( AsDX12(GetRenderDevice()).GetPlatformDevice()->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer( m_actualSizeInBytes ),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_GPUConstantBuffer)) );
        m_GPUConstantBuffer->SetName( L"vaConstantBufferDX12_default" );

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = { m_GPUConstantBuffer->GetGPUVirtualAddress(), m_actualSizeInBytes };
        assert( !m_GPUConstantBufferCBV.IsCreated() );
        m_GPUConstantBufferCBV.Create( cbvDesc ); 
    }

    if( initialData != nullptr && m_uploadConstantBuffer != nullptr )
    {
        assert( m_uploadConstantBuffer->MappedData != nullptr );
        if( m_uploadConstantBuffer->MappedData != nullptr )
            memcpy( m_uploadConstantBuffer->MappedData, initialData, bufferSize );

        if( !m_dynamicUpload )
        {
            weak_ptr<vaConstantBufferDX12*> thisBufferWeak = m_createdThis;
            auto updateProc = [thisBufferWeak, uploadBuffer = m_uploadConstantBuffer]( vaRenderDeviceDX12 & device )
            {
                shared_ptr<vaConstantBufferDX12*> thisBuffer = thisBufferWeak.lock();
                if( thisBuffer != nullptr )
                {
                    assert( (*thisBuffer)->m_actualSizeInBytes == uploadBuffer->ActualSizeInBytes );
                            
                    auto d3d12Context = AsDX12( device.GetMainContext() );

                    d3d12Context->GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition( (*thisBuffer)->m_GPUConstantBuffer.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST ) );
                    d3d12Context->GetCommandList()->CopyResource( (*thisBuffer)->m_GPUConstantBuffer.Get(), uploadBuffer->Resource.Get() );
                    d3d12Context->GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition( (*thisBuffer)->m_GPUConstantBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ ) );

                    (*thisBuffer)->SafeReleaseUploadBuffer( uploadBuffer );
                }
                else
                    delete uploadBuffer;
            } ;

            // this happens mostly for things that are part of the device initialization, before there's MainContext - so defer those, no big deal
            if( !GetRenderDevice().IsRenderThread() || GetRenderDevice().GetMainContext( ) == nullptr || !GetRenderDevice().IsFrameStarted() )
            {
                AsDX12(GetRenderDevice()).ExecuteAtBeginFrame( updateProc );
            }
            else
            {
                updateProc( AsDX12(GetRenderDevice() ) );
            }
            m_uploadConstantBuffer = nullptr;
        }
    }
}

void vaConstantBufferDX12::Update( vaRenderDeviceContext & renderContext, const void * data, uint32 dataSize )
{
    assert( &renderContext.GetRenderDevice() == &GetRenderDevice() );
    
    // this is actually safe to do but not useful since there's no sync with RT contexts
    assert( GetRenderDevice().IsRenderThread() );
    
    assert( m_dataSize == dataSize );

    if( m_dynamicUpload )
    {
        assert( m_uploadConstantBuffer != nullptr );
        if( m_uploadConstantBuffer != nullptr ) 
        {
            SafeReleaseUploadBuffer( m_uploadConstantBuffer );
            m_uploadConstantBuffer = nullptr;
        }
    }

    AllocateNextUploadBuffer();

    assert( m_uploadConstantBuffer->MappedData != nullptr );
    if( m_uploadConstantBuffer->MappedData != nullptr )
        memcpy( m_uploadConstantBuffer->MappedData, data, dataSize );

    if( !m_dynamicUpload )
    {
        auto * d3d12Context = AsDX12( &renderContext );

        d3d12Context->GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition( m_GPUConstantBuffer.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST ) );
        d3d12Context->GetCommandList()->CopyResource( m_GPUConstantBuffer.Get(), m_uploadConstantBuffer->Resource.Get() );
        d3d12Context->GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition( m_GPUConstantBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ ) );
        SafeReleaseUploadBuffer( m_uploadConstantBuffer );
        m_uploadConstantBuffer = nullptr;
    }
}

void vaConstantBufferDX12::DestroyInternal( /*bool lockMutex*/ )
{
    //std::unique_lock<mutex> mutexLock(m_mutex, std::defer_lock ); if( lockMutex ) mutexLock.lock();

    m_createdThis = nullptr;

    if( m_uploadConstantBuffer != nullptr ) 
    {
        SafeReleaseUploadBuffer( m_uploadConstantBuffer );
        m_uploadConstantBuffer = nullptr;
    }
    if( m_GPUConstantBuffer != nullptr )
    {
        AsDX12(GetRenderDevice()).SafeReleaseAfterCurrentGPUFrameDone( m_GPUConstantBuffer, false );
        m_GPUConstantBufferCBV.SafeRelease();
    }

    m_dataSize = 0;
    m_actualSizeInBytes = 0;
    m_dynamicUpload = false;
    for( DetachableUploadBuffer * buffs : m_unusedBuffersPool )
    {
        buffs->DestroyImmediate = true;
        delete buffs;
    }
    m_unusedBuffersPool.clear();
}

void vaConstantBufferDX12::Destroy( )
{
    DestroyInternal( /*true */);
}

vaVertIndBufferDX12::DetachableBuffer::DetachableBuffer( vaRenderDeviceDX12 & device, ComPtr<ID3D12Resource> resource, uint32 sizeInBytes, uint32 actualSizeInBytes ) : Device( device ), Resource( resource ), SizeInBytes( sizeInBytes )
{  
    //RSTHAttach( m_resource.Get(), D3D12_RESOURCE_STATE_GENERIC_READ );
    actualSizeInBytes;
    assert( resource != nullptr );
    assert( actualSizeInBytes >= sizeInBytes );
}
vaVertIndBufferDX12::DetachableBuffer::~DetachableBuffer( )
{
    //RSTHDetach( m_resource.Get() );
    Device.SafeReleaseAfterCurrentGPUFrameDone( Resource, false );
}

vaVertIndBufferDX12::vaVertIndBufferDX12( vaRenderDeviceDX12 & device )              
    : m_device( device ) 
{ 
}
vaVertIndBufferDX12::~vaVertIndBufferDX12( )
{
    assert( vaThreading::IsMainThread( ) );
    if( m_buffer != nullptr ) 
    {
        delete m_buffer;
        m_buffer = nullptr;
    }
    for( DetachableBuffer * buffs : m_unusedBuffersPool )
        delete buffs;
    m_unusedBuffersPool.clear();
}

void vaVertIndBufferDX12::Create( int elementCount, int elementSize, const void * initialData, bool dynamicUpload )
{
    assert( !IsMapped() );

    // std::unique_lock<mutex> mutexLock(m_mutex);

    // this could be made to work but at the moment it's untested and there's not much point to it anyway, right?
    if( m_buffer != nullptr )
        { assert( dynamicUpload == m_dynamicUpload ); }

    DestroyInternal( false );

    m_createdThis = std::make_shared<vaVertIndBufferDX12*>( this );

    assert( elementSize > 0 );
    if( elementCount <= 0 || elementSize <= 0 )
        return;

    m_elementCount  = elementCount;
    m_elementSize   = elementSize;
    m_dataSize      = (uint32)elementCount * elementSize;
    m_dynamicUpload = dynamicUpload;

    uint32 actualBufferSize = m_dataSize;// vaMath::Max( (uint32)256, m_dataSize );

    if( dynamicUpload )
    {
        while( m_unusedBuffersPool.size( ) > 0 )
        {
            auto back = m_unusedBuffersPool.back();
            m_unusedBuffersPool.pop_back();
            if( back->SizeInBytes == m_dataSize )
            {
                m_buffer = back;
                break;
            }
            else
            {
                assert( false ); // why different size? maybe ok but check it out anyway
                delete back;
            }
        }

        if( m_buffer == nullptr )
        {
            ComPtr<ID3D12Resource> resource;
            HRESULT hr;
            V( m_device.GetPlatformDevice()->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                D3D12_HEAP_FLAG_NONE,
                &CD3DX12_RESOURCE_DESC::Buffer(actualBufferSize),
                D3D12_RESOURCE_STATE_GENERIC_READ,      // TODO - resource state tracking
                nullptr,
                IID_PPV_ARGS(&resource)) );
            resource->SetName( L"vaVertIndBufferDX12_upload" );

            m_buffer = new DetachableBuffer( m_device, resource, m_dataSize, actualBufferSize );
        }

        if( initialData != nullptr )
        {
            HRESULT hr;
            UINT8* pDataBegin;
            CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
            V( m_buffer->Resource->Map(0, &readRange, reinterpret_cast<void**>(&pDataBegin)) );
            memcpy(pDataBegin, initialData, m_dataSize);
            m_buffer->Resource->Unmap(0, nullptr);
        }
    }
    else
    {
        for( DetachableBuffer * buffs : m_unusedBuffersPool )
            delete buffs;
        m_unusedBuffersPool.clear();

        ComPtr<ID3D12Resource> resource;
        HRESULT hr;
        V( m_device.GetPlatformDevice()->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(actualBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,      // TODO - resource state tracking
            nullptr,
            IID_PPV_ARGS(&resource)) );
        resource->SetName( L"vaVertIndBufferDX12" );

        m_buffer = new DetachableBuffer( m_device, resource, m_dataSize, actualBufferSize );

        if( initialData != nullptr )
        {
            // this happens mostly for things that are part of the device initialization, before there's MainContext - so defer those, no big deal
            if( !m_device.IsRenderThread() || m_device.GetMainContext( ) == nullptr || !m_device.IsFrameStarted() )
            {
                byte * dataCopy = new byte[m_dataSize]; 
                memcpy( dataCopy, initialData, m_dataSize );

                weak_ptr<vaVertIndBufferDX12*> thisBuffer = m_createdThis;

                m_device.ExecuteAtBeginFrame(
                [thisBuffer, dataCopy, dataSize = m_dataSize]( vaRenderDeviceDX12 & device )
                    {
                        shared_ptr<vaVertIndBufferDX12*> theBuffer = thisBuffer.lock();
                        if( theBuffer != nullptr && (*theBuffer)->IsCreated() )
                            (*theBuffer)->Update( *device.GetMainContext(), dataCopy, dataSize );
                        delete[] dataCopy;
                    } );

            }
            else
            {
                // mutexLock.unlock();

                // Do the copy now
                Update( *m_device.GetMainContext(), initialData, m_dataSize );
            }
        }
    }
}

void vaVertIndBufferDX12::Destroy( )
{
    DestroyInternal( true );
}

void vaVertIndBufferDX12::DestroyInternal( bool lockMutex )
{
    lockMutex;
    // std::unique_lock<mutex> mutexLock(m_mutex, std::defer_lock ); if( lockMutex ) mutexLock.lock();

    m_createdThis = nullptr;

    assert( m_mappedData == nullptr ); //assert( !IsMapped() );
    if( m_buffer == nullptr ) 
        return;
    
    if( m_dynamicUpload )
    {
        weak_ptr<vaVertIndBufferDX12*> thisVIBuffer = m_smartThis;

        m_device.ExecuteAfterCurrentGPUFrameDone(
        [thisVIBuffer, dataSize = m_dataSize, buffer = m_buffer]( vaRenderDeviceDX12 & device )
            {
                device;
                shared_ptr<vaVertIndBufferDX12*> theVIBuffer = thisVIBuffer.lock();
                if( theVIBuffer != nullptr )
                {
                    (*theVIBuffer)->m_unusedBuffersPool.push_back( buffer );
                }
                else
                {
                    delete buffer;
                }
            } );
    }
    else
    {
        delete m_buffer;
    }
    m_mappedData    = nullptr;
    m_elementCount  = 0;
    m_elementSize   = 0;
    m_dataSize      = 0;
    m_buffer        = nullptr;
    m_dataSize      = 0;
}

void vaVertIndBufferDX12::Update( vaRenderDeviceContext & renderContext, const void * data, uint32 dataSize )
{
    assert( m_device.IsRenderThread() ); // for now...

    assert( IsCreated() );
    assert( !IsMapped() );

    assert( dataSize == m_dataSize );   // must match, otherwise not supported
    if( dataSize != m_dataSize )
        return;

    // not the most optimal path but it works
    if( m_dynamicUpload )
    {
        Map( renderContext, vaResourceMapType::WriteDiscard );
        memcpy( GetMappedData(), data, dataSize );
        Unmap( renderContext );
    }
    else
    {
        HRESULT hr;

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize( m_buffer->Resource.Get(), 0, 1 );
        assert( uploadBufferSize == m_dataSize );


        // Note: ComPtr's are CPU objects but this resource needs to stay in scope until the command list that references it has finished executing on the GPU.
        // We will use vaRenderDeviceDX12::SafeReleaseAfterCurrentGPUFrameDone method to ensure the resource is not prematurely destroyed.
        ComPtr<ID3D12Resource> bufferUploadHeap;

        // Create the GPU upload buffer.
        V( m_device.GetPlatformDevice()->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&bufferUploadHeap)));
        bufferUploadHeap->SetName( L"vaVertIndBufferDX12_upload" );

        D3D12_SUBRESOURCE_DATA subResData = {};
        subResData.pData = data;
        subResData.RowPitch = dataSize;
        subResData.SlicePitch = dataSize;

        AsDX12( renderContext ).GetCommandList( )->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition( m_buffer->Resource.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST ) );

        UpdateSubresources( AsDX12( renderContext ).GetCommandList().Get(), m_buffer->Resource.Get(), bufferUploadHeap.Get(), 0, 0, 1, &subResData );

        AsDX12( renderContext ).GetCommandList( )->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition( m_buffer->Resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ ) );

        m_device.SafeReleaseAfterCurrentGPUFrameDone(bufferUploadHeap);
    }
}

bool vaVertIndBufferDX12::Map( vaRenderDeviceContext & renderContext, vaResourceMapType mapType )
{
    assert( m_device.IsRenderThread() ); // for now...

    if( !IsCreated() || IsMapped() || !m_dynamicUpload )
        { assert( false ); return false; }

    if( mapType != vaResourceMapType::WriteDiscard && mapType != vaResourceMapType::WriteNoOverwrite )
    {
        assert( false ); // only map types supported on this buffer
        return false;
    }

    HRESULT hr;
    hr;
    renderContext;

    if( mapType == vaResourceMapType::WriteDiscard )
    {
        int elementCount = m_elementCount;
        int elementSize = m_elementSize;
        Create( elementCount, elementSize, nullptr, true ); // will destroy and re-create
    }

    // Copy the triangle data to the vertex buffer.
    CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
    if( SUCCEEDED( m_buffer->Resource->Map( 0, &readRange, reinterpret_cast<void**>( &m_mappedData ) ) ) )
    {
        return true;
    }
    else
    {
        m_mappedData = nullptr;
        return false;
    }
}

void vaVertIndBufferDX12::Unmap( vaRenderDeviceContext & renderContext )
{
    assert( m_device.IsRenderThread() ); // for now...

    assert( IsMapped() );
    if( !IsMapped() )
        return;
    
    renderContext;

    m_mappedData = nullptr;
    m_buffer->Resource->Unmap(0, nullptr);
}

vaIndexBufferDX12::vaIndexBufferDX12( const vaRenderingModuleParams & params ) : vaIndexBuffer( params ), m_buffer( AsDX12(params.RenderDevice) ) { }

vaVertexBufferDX12::vaVertexBufferDX12( const vaRenderingModuleParams & params ) : vaVertexBuffer( params ), m_buffer( AsDX12(params.RenderDevice) ) { }


vaBufferDX12::vaBufferDX12( vaRenderDeviceDX12 & device, uint64 size, vaResourceAccessFlags accessFlags ) : m_device( device ), m_dataSize( size )
{
    assert( accessFlags == (vaResourceAccessFlags::CPURead | vaResourceAccessFlags::CPUReadManuallySynced) ); // only this supported because only this was needed so far

    D3D12_HEAP_TYPE heapType = HeapTypeDX12FromAccessFlags( accessFlags );
    D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;

    D3D12_RESOURCE_DESC BufferDesc;
    BufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    BufferDesc.Alignment = 0;
    BufferDesc.Width = size;
    BufferDesc.Height = 1;
    BufferDesc.DepthOrArraySize = 1;
    BufferDesc.MipLevels = 1;
    BufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    BufferDesc.SampleDesc.Count = 1;
    BufferDesc.SampleDesc.Quality = 0;
    BufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    BufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    CD3DX12_HEAP_PROPERTIES heapProps(heapType);

    HRESULT hr;
    V( m_device.GetPlatformDevice()->CreateCommittedResource( &heapProps, heapFlags, &BufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(& m_resource ) ) );
    m_resource->SetName( L"vaBufferDX12" );
}

vaBufferDX12::~vaBufferDX12( ) 
{ 
    assert( !IsMapped() );
}

bool vaBufferDX12::Map( vaRenderDeviceContext & renderContext, vaResourceMapType mapType )
{
    assert( m_device.IsRenderThread( ) ); // for now...

    if( m_resource == nullptr || IsMapped( ) )
    {
        assert( false ); return false;
    }

    // only read supported now
    if( mapType != vaResourceMapType::Read )
    {
        assert( false ); // only map types supported on this buffer
        return false;
    }

    HRESULT hr;
    hr;
    renderContext;

    CD3DX12_RANGE readRange( 0, m_dataSize );
    if( SUCCEEDED( m_resource->Map( 0, &readRange, reinterpret_cast<void**>( &m_mappedData ) ) ) )
    {
        return true;
    }
    else
    {
        m_mappedData = nullptr;
        return false;
    }
}

void vaBufferDX12::Unmap( vaRenderDeviceContext & renderContext )
{
    assert( m_device.IsRenderThread( ) ); // for now...

    assert( IsMapped( ) );
    if( !IsMapped( ) )
        return;

    renderContext;

    m_mappedData = nullptr;
    m_resource->Unmap( 0, nullptr );
}


void RegisterBuffersDX12( )
{
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX12, vaConstantBuffer, vaConstantBufferDX12 );
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX12, vaIndexBuffer, vaIndexBufferDX12 );
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX12, vaVertexBuffer, vaVertexBufferDX12 );
    // VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX12, vaStructuredBuffer, vaStructuredBufferDX12 );
}