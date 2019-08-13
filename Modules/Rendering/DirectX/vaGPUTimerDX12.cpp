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

#include "Rendering/DirectX/vaGPUTimerDX12.h"

#include "Rendering/DirectX/vaRenderDeviceContextDX12.h"
#include "Rendering/DirectX/vaRenderBuffersDX12.h"

#include <algorithm>

#ifdef VA_USE_PIX3
#define USE_PIX
#endif

#ifdef USE_PIX
#include <pix3.h>
#endif

// not included or enabled by default
// #define USE_RGP

#ifdef USE_RGP
#include <AMDDxExt\AmdPix3.h>
#endif


using namespace VertexAsylum;

vaGPUTimerManagerDX12::vaGPUTimerManagerDX12( vaRenderDeviceDX12 & device ) : m_device( device )
{
    for( int i = 0; i < _countof(m_activeTimers); i++ )
        m_activeTimers[i] = nullptr;

    D3D12_QUERY_HEAP_DESC queryHeapDesc;
    queryHeapDesc.Count     = c_maxTimerCount*2;
    queryHeapDesc.NodeMask  = 0;
    queryHeapDesc.Type      = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    device.GetPlatformDevice()->CreateQueryHeap( &queryHeapDesc, IID_PPV_ARGS(&m_queryHeap) );
    m_queryHeap->SetName( L"vaGPUTimerManagerDX12_QueryHeap" );
    
    for( int i = 0; i < _countof(m_queryReadbackBuffers); i++ )
    {
        m_queryReadbackBuffers[i] = shared_ptr<vaBufferDX12>( new vaBufferDX12( device, sizeof(uint64)*c_maxTimerCount*2, vaResourceAccessFlags::CPURead | vaResourceAccessFlags::CPUReadManuallySynced ) );
        m_queryReadbackBuffersFilled[i] = false;
    }
}

vaGPUTimerManagerDX12::~vaGPUTimerManagerDX12( )
{
    assert( m_freed.size() == m_allocatedCount );
    for( int i = 0; i < _countof( m_activeTimers ); i++ )
        assert( m_activeTimers[i] == nullptr );
}

int  vaGPUTimerManagerDX12::AllocateIndex( vaGPUTimerDX12 * owner )
{
    assert( m_device.IsRenderThread() );

    int ret;

    // do we have one already freed? return that one
    if( m_freed.size( ) > 0 )
    {
        ret = m_freed.back( );
        m_freed.pop_back( );
    }
    else // or allocate the new one
    {
        if( m_allocatedCount >= c_maxTimerCount )
        {
            // ran out of timer space - blah
            assert( false );
            return -1;
        }
        ret = m_allocatedCount++;
    }
    assert( m_activeTimers[ret] == nullptr );
    m_activeTimers[ret] = owner;
    return ret;
}

void vaGPUTimerManagerDX12::ReleaseIndex( int index, vaGPUTimerDX12 * owner )
{
    assert( vaGPUTimerManagerDX12::GetInstancePtr() != nullptr );
 
    assert( m_device.IsRenderThread( ) );

    assert( m_activeTimers[index] == owner ); owner;
    m_activeTimers[index] = nullptr;

    // if last then just drop it - we could defrag here too but we have to find them in m_freed too and that's too much work :)
    if( index == (m_allocatedCount-1) )
        m_allocatedCount--;
    else
        m_freed.push_back( index );
}

void vaGPUTimerManagerDX12::ResolveQueryDataAndUpdateTimers( )
{
    assert( vaGPUTimerManagerDX12::GetInstancePtr() != nullptr );

    bool dataOk = false;
    if( m_lastBackbufferIndex != -1 )
    {
        if( m_queryReadbackBuffersFilled[m_lastBackbufferIndex] )
        {
            assert( m_allocatedCount <= c_maxTimerCount );

            // get the oldest (it's previous if vaRenderDevice::c_BackbufferCount == 2) data first - guaranteed to be ready for a while already but doing it here for convenience
            if( m_queryReadbackBuffers[m_lastBackbufferIndex]->Map( AsDX12(*m_device.GetMainContext()), vaResourceMapType::Read ) )
            {
                memcpy( m_lastCopiedReadbackBuffer, m_queryReadbackBuffers[m_lastBackbufferIndex]->GetMappedData( ), sizeof(uint64)*m_allocatedCount*2 );
                m_queryReadbackBuffers[m_lastBackbufferIndex]->Unmap( AsDX12(*m_device.GetMainContext()) );
                dataOk = true;
            }
            else
            {
                VA_WARN( "Unable to map query resolve data" );
            }
        }

        // Copy to the current buffer
        AsDX12(*m_device.GetMainContext()).GetCommandList()->ResolveQueryData( m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, m_allocatedCount*2, m_queryReadbackBuffers[m_lastBackbufferIndex]->GetResource().Get( ), 0 );
        m_queryReadbackBuffersFilled[m_lastBackbufferIndex] = true;
    }

    // Move on to the next buffer
    m_lastBackbufferIndex = m_device.GetCurrentBackBufferIndex();

    if( dataOk )
    {
        // ok, sort out data now - convert into time
        UINT64 frequency; HRESULT hr;
        V( m_device.GetCommandQueue( )->GetTimestampFrequency( &frequency ) );

        for( int i = 0; i < m_allocatedCount; i++ )
        {
            if( m_activeTimers[i] == nullptr )
                continue;

            m_activeTimers[i]->Update( m_lastCopiedReadbackBuffer[i*2], m_lastCopiedReadbackBuffer[i*2+1], frequency );
        }
    }
}

void vaGPUTimerManagerDX12::PIXBeginEvent( const wstring & name )
{
    name;
#ifdef USE_RGP
//    RgpPushMarker( AsDX12( *renderDeviceContext ).GetCommandList( ).Get(), vaStringTools::SimpleWiden( m_name ).c_str( ) );
#endif
#ifdef USE_PIX
    ::PIXBeginEvent( PIX_COLOR_INDEX(2), name.c_str( ) );
#endif
}

void vaGPUTimerManagerDX12::PIXEndEvent( )
{
#ifdef USE_RGP
//    RgpPopMarker( AsDX12( *renderDeviceContext ).GetCommandList( ).Get() );
#endif
#ifdef USE_PIX
    ::PIXEndEvent( );
#endif
}

vaGPUTimerDX12::vaGPUTimerDX12( const vaRenderingModuleParams & params ) : vaGPUTimer( vaSaferStaticCast< const vaGPUTimerParams &, const vaRenderingModuleParams &>( params ) )
{
    params; // unreferenced

    m_instanceIndex = vaGPUTimerManagerDX12::GetInstance().AllocateIndex( this );
}

vaGPUTimerDX12::~vaGPUTimerDX12( )
{
    if( m_instanceIndex != -1 && vaGPUTimerManagerDX12::GetInstancePtr() != nullptr )
        vaGPUTimerManagerDX12::GetInstance( ).ReleaseIndex( m_instanceIndex, this );
}

void vaGPUTimerDX12::Update( uint64 beginTick, uint64 endTick, uint64 frequency )
{
    if( beginTick >= endTick )
    {
        m_lastTimeBegin     = 0;
        m_lastTimeStop      = 0;
        return;
    }

    m_lastTimeBegin     = (double)(beginTick)   / (double)frequency;
    m_lastTimeStop      = (double)(endTick)     / (double)frequency;
}

void vaGPUTimerDX12::Start( vaRenderDeviceContext * renderDeviceContext )
{
    assert( m_startRenderDeviceContext == nullptr );
    m_startRenderDeviceContext = renderDeviceContext;
    assert( m_instanceIndex != -1 );
    if( m_instanceIndex == -1 )
        return;

     if( m_name != "" )
     {
#ifdef USE_RGP
         RgpPushMarker( AsDX12( *renderDeviceContext ).GetCommandList( ).Get(), vaStringTools::SimpleWiden( m_name ).c_str( ) );
#endif
#ifdef USE_PIX
         ::PIXBeginEvent( AsDX12( *renderDeviceContext ).GetCommandList( ).Get(), PIX_COLOR_INDEX(1), m_name.c_str( ) );
 #endif
     }

    
    AsDX12( *renderDeviceContext ).GetCommandList( )->EndQuery( vaGPUTimerManagerDX12::GetInstance().GetHeap().Get( ), D3D12_QUERY_TYPE_TIMESTAMP, m_instanceIndex*2+0 );
}

void vaGPUTimerDX12::Stop( vaRenderDeviceContext * renderDeviceContext )
{
    assert( m_startRenderDeviceContext == renderDeviceContext );
    m_startRenderDeviceContext = nullptr;
    assert( m_instanceIndex != -1 );
    if( m_instanceIndex == -1 )
        return;

    AsDX12( *renderDeviceContext ).GetCommandList( )->EndQuery( vaGPUTimerManagerDX12::GetInstance().GetHeap().Get( ), D3D12_QUERY_TYPE_TIMESTAMP, m_instanceIndex*2+1 );

     if( m_name != "" )
     {
#ifdef USE_RGP
         RgpPopMarker( AsDX12( *renderDeviceContext ).GetCommandList( ).Get() );
#endif
#ifdef USE_PIX
         ::PIXEndEvent( AsDX12( *renderDeviceContext ).GetCommandList( ).Get() );
#endif
     }
}