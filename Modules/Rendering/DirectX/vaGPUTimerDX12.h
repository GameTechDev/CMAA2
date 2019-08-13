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

#include "Rendering/vaGPUTimer.h"

#include "Rendering/DirectX/vaDirectXIncludes.h"
#include "Rendering/DirectX/vaDirectXTools.h"

#include <vector>

namespace VertexAsylum
{
    class vaGPUTimerDX12;
    class vaBufferDX12;

    class vaGPUTimerManagerDX12 : public vaSingletonBase< vaGPUTimerManagerDX12 >
    {
        friend class vaRenderDeviceDX12;
        friend class vaGPUTimerDX12;

        vaRenderDeviceDX12 &                        m_device;
        static const int                            c_maxTimerCount     = 4096;
        vaGPUTimerDX12 *                            m_activeTimers[c_maxTimerCount];
        int                                         m_allocatedCount    = 0;
        vector<int>                                 m_freed;
        
        ComPtr<ID3D12QueryHeap>                     m_queryHeap;
        shared_ptr<vaBufferDX12>                    m_queryReadbackBuffers[vaRenderDevice::c_BackbufferCount];
        bool                                        m_queryReadbackBuffersFilled[vaRenderDevice::c_BackbufferCount];
        uint64                                      m_lastCopiedReadbackBuffer[c_maxTimerCount*2];

        int                                         m_lastBackbufferIndex   = -1;
        //int64                                       m_lastFrameResolved     = -1;

        vaGPUTimerManagerDX12( vaRenderDeviceDX12 & device );
        ~vaGPUTimerManagerDX12( );

        int                                         AllocateIndex( vaGPUTimerDX12 * owner );
        void                                        ReleaseIndex( int index, vaGPUTimerDX12 * owner );

        const ComPtr<ID3D12QueryHeap> &             GetHeap( ) const            { return m_queryHeap; }
        int                                         GetAllocatedCount( ) const  { return m_allocatedCount; }

        void                                        ResolveQueryDataAndUpdateTimers( );

        // Generic PIX3 calls
        void                                        PIXBeginEvent( const wstring & name );
        void                                        PIXEndEvent( );
    };


    class vaGPUTimerDX12 : public vaGPUTimer
    {
    protected:
        friend class vaGPUTimerManagerDX12;

        // used as a query heap index!
        int                                         m_instanceIndex = -1;

        double                                      m_lastTimeBegin = 0;
        double                                      m_lastTimeStop  = 0;

        vaRenderDeviceContext *                     m_startRenderDeviceContext = nullptr;

    public:
        vaGPUTimerDX12( const vaRenderingModuleParams & params );
        virtual                                     ~vaGPUTimerDX12( );

    private:
        void                                        Update( uint64 beginTick, uint64 endTick, uint64 frequency );

    public:

        // vaGPUTimer implementation
        virtual void                                Start( vaRenderDeviceContext * renderDeviceContext );
        virtual void                                Stop( vaRenderDeviceContext * renderDeviceContext );

        virtual double                              GetLastTime( ) const            { return std::max( 0.0, (double)(m_lastTimeStop - m_lastTimeBegin) ); }
    };


}
