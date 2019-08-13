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

#pragma once

#include "Core/vaCoreIncludes.h"

#include "Rendering/DirectX/vaRenderDeviceDX12.h"
#include "Rendering/DirectX/vaDirectXIncludes.h"
#include "Rendering/DirectX/vaDirectXTools.h"
#include "Rendering/DirectX/vaShaderDX12.h"

#include "Rendering/vaRenderingIncludes.h"


namespace VertexAsylum
{
    // In DX12 case vaRenderDeviceContext encapsulates ID3D12CommandAllocator, ID3D12GraphicsCommandList, viewports and render target stuff
    // There's one "main" context for use from the main thread (vaThreading::IsMainThread) that is also used for buffer copies/updates,
    // UI and similar. 
    // How exactly I'll handle multithreading I'm not sure but there will be a context per thread. This context will probably not have
    // a functional RenderOutputsState but 'inherit' it from the main render device context. 
    class vaRenderDeviceContextDX12 : public vaRenderDeviceContext
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    public:

    private:
        ComPtr<ID3D12CommandAllocator>  m_commandAllocators[vaRenderDevice::c_BackbufferCount];
        ComPtr<ID3D12GraphicsCommandList> m_commandList;

        bool                            m_commandListReady                  = false;
    
        vaRenderTypeFlags               m_itemsStarted                      = vaRenderTypeFlags::None;
        vaSceneDrawContext *            m_lastSceneDrawContext              = nullptr;

        // applies to both render and compute items
        const int                       c_flushAfterItemCount               = 128;
        int                             m_itemsSubmittedAfterLastExecute    = 0;

        D3D_PRIMITIVE_TOPOLOGY          m_commandListCurrentTopology        = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    protected:
        explicit                        vaRenderDeviceContextDX12( const vaRenderingModuleParams & params );
        virtual                         ~vaRenderDeviceContextDX12( );

    public:
        static vaRenderDeviceContext *  Create( vaRenderDevice & device, int someParametersGoHereMaybe );

    public:
        virtual void                    BeginItems( vaRenderTypeFlags typeFlags ) override;
        virtual vaDrawResultFlags       ExecuteItem( const vaGraphicsItem & renderItem ) override;
        virtual vaDrawResultFlags       ExecuteItem( const vaComputeItem & renderItem ) override;
        virtual void                    EndItems( ) override;
        virtual vaRenderTypeFlags       GetSupportFlags( ) const override                                   { return vaRenderTypeFlags::Graphics | vaRenderTypeFlags::Compute; }
        virtual vaRenderTypeFlags       GetStartedFlags( ) const override                                   { return m_itemsStarted; }

        bool                            IsMainContext( ) const;

        const ComPtr<ID3D12GraphicsCommandList> & 
                                        GetCommandList( ) const         { return m_commandList; }

        // Executes the command list on the main queue. Cannot be called between BeginItems/EndItems
        void                            Flush( );

        // This binds descriptor heaps, root signatures, viewports, scissor rects and render targets; useful if any external code messes with them
        void                            BindDefaultStates( );

    private:
        void                            Initialize( );
        void                            Destroy( );

        void                            UpdateSceneDrawContext( vaSceneDrawContext * sceneDrawContext );

        void                            ResetAndInitializeCommandList( int currentFrame );

        // mostly for vaRenderGlobals and vaLighting states
        vaDrawResultFlags               SetSceneGlobals( vaSceneDrawContext * sceneDrawContext );
        void                            UnsetSceneGlobals( vaSceneDrawContext * sceneDrawContext );

    protected:
        friend class vaRenderDeviceDX12;
        void                            BeginFrame( );
        void                            EndFrame( );
        void                            ExecuteCommandList( );

    private:
        // vaRenderDeviceContext
        virtual void                    UpdateViewport( );
        virtual void                    UpdateRenderTargetsDepthStencilUAVs( );
    };

    inline vaRenderDeviceContextDX12 &  AsDX12( vaRenderDeviceContext & device )   { return *device.SafeCast<vaRenderDeviceContextDX12*>(); }
    inline vaRenderDeviceContextDX12 *  AsDX12( vaRenderDeviceContext * device )   { return device->SafeCast<vaRenderDeviceContextDX12*>(); }
}