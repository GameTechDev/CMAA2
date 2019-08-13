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

#include "vaRenderDeviceContextDX12.h"
#include "vaTextureDX12.h"

#include "vaRenderBuffersDX12.h"

#include "Rendering/vaTextureHelpers.h"

// #include "Rendering/DirectX/vaRenderGlobalsDX11.h"

// #include "vaResourceFormatsDX11.h"

using namespace VertexAsylum;

namespace 
{
    struct CommonSimpleVertex
    {
        float   Position[4];
        float   UV[2];

        CommonSimpleVertex( ) {};
        CommonSimpleVertex( float px, float py, float pz, float pw, float uvx, float uvy ) { Position[0] = px; Position[1] = py; Position[2] = pz; Position[3] = pw; UV[0] = uvx; UV[1] = uvy; }
    };
}

// // used to make Gather using UV slightly off the border (so we get the 0,0 1,0 0,1 1,1 even if there's a minor calc error, without adding the half pixel offset)
// static const float  c_minorUVOffset = 0.00006f;  // less than 0.5/8192

vaRenderDeviceContextDX12::vaRenderDeviceContextDX12( const vaRenderingModuleParams & params )
    : vaRenderDeviceContext( params )
{ 
    //m_fullscreenVB              = nullptr;
    //m_fullscreenVBDynamic       = nullptr;
}

vaRenderDeviceContextDX12::~vaRenderDeviceContextDX12( )
{ 
    Destroy();
}

void vaRenderDeviceContextDX12::Initialize( )
{
    HRESULT hr;

    auto & d3d12Device = AsDX12( GetRenderDevice() ).GetPlatformDevice();

    // Create command allocator for each frame.
    for( uint i = 0; i < vaRenderDeviceDX12::c_BackbufferCount; i++ )
    {
        V( d3d12Device->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[i]) ) );
        
        wstring name = vaStringTools::Format( L"MainDeviceContextAllocator%d", i );
        V( m_commandAllocators[i]->SetName( name.c_str() ) );
    }

    uint32 currentFrame = AsDX12( GetRenderDevice() ).GetCurrentBackBufferIndex( ) ;

    // Create the command list.
    V( d3d12Device->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[currentFrame].Get(), nullptr, IID_PPV_ARGS(&m_commandList)) );
    V( m_commandList->SetName( L"MainDeviceContext" ) );

    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    V( m_commandList->Close() );
}

void vaRenderDeviceContextDX12::Destroy( )
{
    m_commandList.Reset();

    for( uint i = 0; i < vaRenderDeviceDX12::c_BackbufferCount; i++ )
        m_commandAllocators[i].Reset();
}

void vaRenderDeviceContextDX12::BindDefaultStates( )
{
    assert( m_commandListReady );
    AsDX12( GetRenderDevice() ).BindDefaultDescriptorHeaps( m_commandList.Get() );

    m_commandList->SetGraphicsRootSignature( AsDX12( GetRenderDevice() ).GetDefaultGraphicsRootSignature()  );
    m_commandList->SetComputeRootSignature( AsDX12( GetRenderDevice() ).GetDefaultComputeRootSignature() );

    UpdateViewport( );
    UpdateRenderTargetsDepthStencilUAVs( );

    // some other default states
    FLOAT defBlendFactor[4] = { 1, 1, 1, 1 };
    m_commandList->OMSetBlendFactor( defBlendFactor ); // If you pass NULL, the runtime uses or stores a blend factor equal to { 1, 1, 1, 1 }.
    m_commandList->OMSetStencilRef( 0 );
    m_commandListCurrentTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    m_commandList->IASetPrimitiveTopology( m_commandListCurrentTopology );
}

void vaRenderDeviceContextDX12::ResetAndInitializeCommandList( int currentFrame )
{
    assert( m_itemsStarted == vaRenderTypeFlags::None );
    HRESULT hr;
    V( m_commandList->Reset( m_commandAllocators[currentFrame].Get(), nullptr ) );

    m_commandListReady = true;

    BindDefaultStates();
}

void vaRenderDeviceContextDX12::BeginFrame( )
{
    assert( vaThreading::IsMainThread() );
    assert( m_itemsStarted == vaRenderTypeFlags::None );
    assert( !m_commandListReady );
    uint32 currentFrame = AsDX12( GetRenderDevice() ).GetCurrentBackBufferIndex( ) ;

    HRESULT hr;

    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    V( m_commandAllocators[currentFrame]->Reset() );

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ResetAndInitializeCommandList( currentFrame );
}

void vaRenderDeviceContextDX12::EndFrame( )
{
    assert( vaThreading::IsMainThread() );
    assert( m_commandListReady );
    assert( m_itemsStarted == vaRenderTypeFlags::None );
    
    uint32 currentFrame = AsDX12( GetRenderDevice() ).GetCurrentBackBufferIndex( ) ; currentFrame;

    HRESULT hr;
    V( m_commandList->Close() );

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    AsDX12(GetRenderDevice()).GetCommandQueue()->ExecuteCommandLists( _countof(ppCommandLists), ppCommandLists );

    m_commandListReady = false;
}

void vaRenderDeviceContextDX12::ExecuteCommandList( )
{
    assert( vaThreading::IsMainThread() );
    assert( m_itemsStarted == vaRenderTypeFlags::None );

    HRESULT hr;
    uint32 currentFrame = AsDX12( GetRenderDevice() ).GetCurrentBackBufferIndex( ) ;

    V( m_commandList->Close() );

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    AsDX12(GetRenderDevice()).GetCommandQueue()->ExecuteCommandLists( _countof(ppCommandLists), ppCommandLists );

    ResetAndInitializeCommandList( currentFrame );

    m_itemsSubmittedAfterLastExecute = 0;
}

void vaRenderDeviceContextDX12::Flush( )
{
    ExecuteCommandList();
}

vaRenderDeviceContext * vaRenderDeviceContextDX12::Create( vaRenderDevice & device, int someParametersGoHereMaybe )
{
    assert( someParametersGoHereMaybe == 42 ); someParametersGoHereMaybe;

    vaRenderDeviceContext * context = VA_RENDERING_MODULE_CREATE( vaRenderDeviceContext, device );

    vaSaferStaticCast<vaRenderDeviceContextDX12*>( context )->Initialize( );

    return context;
}

void vaRenderDeviceContextDX12::UpdateViewport( )
{
    // nothing to set the viewport to but it will be set on ResetAndInitializeCommandList so we can skip for now
    if( !m_commandListReady )
        return;

    const vaViewport & vavp = GetViewport();

    D3D12_VIEWPORT viewport;
    viewport.TopLeftX   = (float)vavp.X;
    viewport.TopLeftY   = (float)vavp.Y;
    viewport.Width      = (float)vavp.Width;
    viewport.Height     = (float)vavp.Height;
    viewport.MinDepth   = vavp.MinDepth;
    viewport.MaxDepth   = vavp.MaxDepth;

    m_commandList->RSSetViewports(1, &viewport);

    vaRecti scissorRect;
    bool scissorRectEnabled;
    GetScissorRect( scissorRect, scissorRectEnabled );
    if( scissorRectEnabled )
    {
        D3D12_RECT rect;
        rect.left   = scissorRect.left;
        rect.top    = scissorRect.top;
        rect.right  = scissorRect.right;
        rect.bottom = scissorRect.bottom;
        m_commandList->RSSetScissorRects(1, &rect);
    }
    else
    {
        // set the scissor to viewport size, for rasterizer states that have it enabled
        D3D12_RECT rect;
        rect.left   = vavp.X;
        rect.top    = vavp.Y;
        rect.right  = vavp.Width + vavp.X;
        rect.bottom = vavp.Height + vavp.Y;
        m_commandList->RSSetScissorRects(1, &rect);
    }
}

void vaRenderDeviceContextDX12::UpdateRenderTargetsDepthStencilUAVs( )
{
    // nothing to set the render targets to but it will be set on ResetAndInitializeCommandList so we can skip for now
    if( !m_commandListReady )
        return;

    D3D12_CPU_DESCRIPTOR_HANDLE     RTVs[ c_maxRTs ];
    //ID3D11UnorderedAccessView * UAVs[ c_maxUAVs];
    D3D12_CPU_DESCRIPTOR_HANDLE     DSV = { 0 };
    int numRTVs = 0;
    for( int i = 0; i < c_maxRTs; i++ )  
    {
        RTVs[i] = {0};
        if( m_outputsState.RenderTargets[i] != nullptr )
        {
            const vaRenderTargetViewDX12 * rtv = AsDX12( *m_outputsState.RenderTargets[i] ).GetRTV( );
            if( rtv != nullptr && rtv->IsCreated() )
            {
                AsDX12( *m_outputsState.RenderTargets[i] ).TransitionResource( *this, D3D12_RESOURCE_STATE_RENDER_TARGET );
                RTVs[i] = rtv->GetCPUHandle();
                numRTVs = i+1;
            }
            else
                { assert( false ); }    // error, texture has no rtv but set as a render target
        }
    }
    
    // collected in ExecuteItem( vaGraphicsItem ... )
    // for( size_t i = 0; i < c_maxUAVs; i++ )
    // {
        //UAVs[i] = ( m_outputsState.UAVs[i] != nullptr ) ? ( vaSaferStaticCast<vaTextureDX11*>( m_outputsState.UAVs[i].get( ) )->GetUAV( ) ) : ( nullptr );
        //assert( m_outputsState.UAVs[i] == nullptr ); // not implemented - should be implemented in a different place maybe? not sure
    // }

    D3D12_CPU_DESCRIPTOR_HANDLE * pDSV = nullptr;

    if( m_outputsState.DepthStencil != nullptr )
    {
            const vaDepthStencilViewDX12 * dsv = AsDX12( *m_outputsState.DepthStencil ).GetDSV( );
            if( dsv != nullptr && dsv->IsCreated( ) )
            {
                AsDX12( *m_outputsState.DepthStencil ).TransitionResource( *this, D3D12_RESOURCE_STATE_DEPTH_WRITE );
                DSV = dsv->GetCPUHandle();
                pDSV = &DSV;
            }
            else
                { assert( false ); }    // error, texture has no dsv but set as a render target

    }

    m_commandList->OMSetRenderTargets( numRTVs, RTVs, FALSE, pDSV );
    
    // m_deviceContext->OMSetRenderTargetsAndUnorderedAccessViews( m_outputsState.RenderTargetCount, (m_outputsState.RenderTargetCount>0)?(RTVs):(nullptr), DSV, m_outputsState.UAVsStartSlot, m_outputsState.UAVCount, UAVs, m_outputsState.UAVInitialCounts );
}

void vaRenderDeviceContextDX12::BeginItems( vaRenderTypeFlags typeFlags )
{
    assert( vaThreading::IsMainThread() );
    assert( m_itemsStarted == vaRenderTypeFlags::None );
    assert( m_lastSceneDrawContext == nullptr );
    m_itemsStarted = typeFlags;

#if 0 // maybe ifdef debug? this way at least we have a chance of getting a warning from the debug runtime if a mismatch view is set
    const vaConstantBufferViewDX12 & nullCBV        = AsDX12( GetRenderDevice() ).GetNullCBV        ();
    const vaShaderResourceViewDX12 & nullSRV        = AsDX12( GetRenderDevice() ).GetNullSRV        ();
    const vaUnorderedAccessViewDX12& nullUAV        = AsDX12( GetRenderDevice() ).GetNullUAV        ();
    const vaSamplerViewDX12        & nullSamplerView= AsDX12( GetRenderDevice() ).GetNullSamplerView();
    const auto & ranges                             = AsDX12( GetRenderDevice() ).GetDefaultRootParamIndexRanges();

    if( (typeFlags & vaRenderTypeFlags::Graphics) != 0 )
    {
        for( int i = 0; i < ranges.SRVCount; i++ )
            m_commandList->SetGraphicsRootDescriptorTable( ranges.SRVBase+i, nullSRV.GetGPUHandle() );
        for( int i = 0; i < ranges.CBVCount; i++ )
            m_commandList->SetGraphicsRootDescriptorTable( ranges.CBVBase+i, nullCBV.GetGPUHandle() );
        for( int i = 0; i < ranges.UAVCount; i++ )
            m_commandList->SetGraphicsRootDescriptorTable( ranges.UAVBase+i, nullUAV.GetGPUHandle() );
        for( int i = 0; i < ranges.SamplerCount; i++ )
            m_commandList->SetGraphicsRootDescriptorTable( ranges.SamplerBase+i, nullSamplerView.GetGPUHandle() );
    }

    if( (typeFlags & vaRenderTypeFlags::Compute) != 0 )
    {
        for( int i = 0; i < ranges.SRVCount; i++ )
            m_commandList->SetComputeRootDescriptorTable( ranges.SRVBase+i, nullSRV.GetGPUHandle() );
        for( int i = 0; i < ranges.CBVCount; i++ )
            m_commandList->SetComputeRootDescriptorTable( ranges.CBVBase+i, nullCBV.GetGPUHandle() );
        for( int i = 0; i < ranges.UAVCount; i++ )
            m_commandList->SetComputeRootDescriptorTable( ranges.UAVBase+i, nullUAV.GetGPUHandle() );
        for( int i = 0; i < ranges.SamplerCount; i++ )
            m_commandList->SetComputeRootDescriptorTable( ranges.SamplerBase+i, nullSamplerView.GetGPUHandle() );
    }
#endif
}

/*
void vaRenderDeviceContextDX12::SetRootDescriptorTable( UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor )
{
    assert( m_itemsStarted != vaRenderTypeFlags::None );
    if( (m_itemsStarted & vaRenderTypeFlags::Graphics) != 0 )
        m_commandList->SetGraphicsRootDescriptorTable( RootParameterIndex, BaseDescriptor );
    if( (m_itemsStarted & vaRenderTypeFlags::Compute) != 0 )
        m_commandList->SetComputeRootDescriptorTable( RootParameterIndex, BaseDescriptor );
}
*/

vaDrawResultFlags vaRenderDeviceContextDX12::SetSceneGlobals( vaSceneDrawContext * sceneDrawContext )
{
    assert( vaThreading::IsMainThread() );
    sceneDrawContext;
    assert( AsDX12(&sceneDrawContext->RenderDeviceContext) == this );
    assert( m_itemsStarted != vaRenderTypeFlags::None );

    assert( sceneDrawContext != nullptr );
    if( sceneDrawContext != nullptr )
    {
        //////////////////////////////////////////////////////////////////////////
        // set descriptor tables and prepare for copying
        vaRenderDeviceDX12::TransientGPUDescriptorHeap * gpuHeapSRVCBVUAV = AsDX12( GetRenderDevice() ).GetTransientDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        int descHeapBaseIndexSRVCBVUAV;
        if( !gpuHeapSRVCBVUAV->Allocate( vaRenderDeviceDX12::ExtendedRootSignatureIndexRanges::SRVCBVUAVTotalCount, descHeapBaseIndexSRVCBVUAV ) )
            { assert( false ); return vaDrawResultFlags::UnspecifiedError; }

        // vaRenderDeviceDX12::TransientGPUDescriptorHeap * gpuHeapSampler = AsDX12( GetRenderDevice() ).GetTransientDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        // int descHeapBaseIndexSampler;
        // if( !gpuHeapSampler->Allocate( vaRenderDeviceDX12::ExtendedRootSignatureIndexRanges::SamplerCount, descHeapBaseIndexSampler ) )
        //     { assert( false ); return vaDrawResultFlags::UnspecifiedError; }
        //
        m_commandList->SetGraphicsRootDescriptorTable( vaRenderDeviceDX12::ExtendedRootSignatureIndexRanges::RootParameterIndexSRVCBVUAV, gpuHeapSRVCBVUAV->ComputeGPUHandle(descHeapBaseIndexSRVCBVUAV) );
        m_commandList->SetComputeRootDescriptorTable( vaRenderDeviceDX12::ExtendedRootSignatureIndexRanges::RootParameterIndexSRVCBVUAV, gpuHeapSRVCBVUAV->ComputeGPUHandle(descHeapBaseIndexSRVCBVUAV) );
        //m_commandList->SetGraphicsRootDescriptorTable( vaRenderDeviceDX12::ExtendedRootSignatureIndexRanges::RootParameterIndexSampler, gpuHeapSampler->ComputeGPUHandle(descHeapBaseIndexSampler) );
        //m_commandList->SetComputeRootDescriptorTable( vaRenderDeviceDX12::ExtendedRootSignatureIndexRanges::RootParameterIndexSampler, gpuHeapSampler->ComputeGPUHandle(descHeapBaseIndexSampler) );
        //
        int descHeapSRVOffset = descHeapBaseIndexSRVCBVUAV + vaRenderDeviceDX12::ExtendedRootSignatureIndexRanges::SRVBase - EXTENDED_SRV_SLOT_BASE;
        int descHeapCBVOffset = descHeapBaseIndexSRVCBVUAV + vaRenderDeviceDX12::ExtendedRootSignatureIndexRanges::CBVBase - EXTENDED_CBV_SLOT_BASE;
        // int descHeapUAVOffset = descHeapBaseIndexSRVCBVUAV + vaRenderDeviceDX12::ExtendedRootSignatureIndexRanges::CBVBase - EXTENDED_UAV_SLOT_BASE; descHeapUAVOffset;
        //////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
    #define SET_UNUSED_DESC_TO_NULL
#endif

#ifdef SET_UNUSED_DESC_TO_NULL
        const vaConstantBufferViewDX12 & nullCBV        = AsDX12( GetRenderDevice() ).GetNullCBV        ();
        const vaShaderResourceViewDX12 & nullSRV        = AsDX12( GetRenderDevice() ).GetNullSRV        ();
        const vaUnorderedAccessViewDX12& nullUAV        = AsDX12( GetRenderDevice() ).GetNullUAV        (); nullUAV;
        const vaSamplerViewDX12        & nullSamplerView= AsDX12( GetRenderDevice() ).GetNullSamplerView(); nullSamplerView;
#endif

        auto & d3d12Device = AsDX12( GetRenderDevice() ).GetPlatformDevice( );

        if( sceneDrawContext->Lighting != nullptr )
        {
            vaLighting & lighting = *sceneDrawContext->Lighting;
            lighting.UpdateShaderConstants( *sceneDrawContext );

            // many lines of code to set a srv/cbv? maybe refactor this...
            {
                const shared_ptr<vaConstantBuffer> & lightingConsts = lighting.GetConstantsBuffer( );
                const vaConstantBufferViewDX12 * lightingCBV        = (lightingConsts == nullptr)    ? ( nullptr ) : (AsDX12( *lightingConsts ).GetCBV() );
#ifdef SET_UNUSED_DESC_TO_NULL
                if( lightingCBV == nullptr )
                    lightingCBV = &nullCBV;
#endif
                if( lightingCBV != nullptr )
                {
                    d3d12Device->CopyDescriptorsSimple( 1, gpuHeapSRVCBVUAV->ComputeCPUHandle(descHeapCBVOffset + LIGHTINGGLOBAL_CONSTANTSBUFFERSLOT ), 
                        lightingCBV->GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
                }
            }
            {
                shared_ptr<vaTexture> envmapTexture         = lighting.GetEnvmapTexture( );
                if( envmapTexture == nullptr )
                    envmapTexture = GetRenderDevice().GetTextureTools().GetCommonTexture(vaTextureTools::CommonTextureName::Black1x1Cube);
                const vaShaderResourceViewDX12 * envmapSRV          = (envmapTexture == nullptr )    ? ( nullptr ) : (AsDX12( *envmapTexture ).GetSRV() );
#ifdef SET_UNUSED_DESC_TO_NULL
                if( envmapSRV == nullptr )
                    envmapSRV = &nullSRV;
#endif
                if( envmapSRV != nullptr )
                {
                    if( envmapTexture != nullptr )
                        AsDX12( *envmapTexture ).TransitionResource( *this, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE );
                    d3d12Device->CopyDescriptorsSimple( 1, gpuHeapSRVCBVUAV->ComputeCPUHandle(descHeapSRVOffset + SHADERGLOBAL_LIGHTING_ENVMAP_TEXTURESLOT ), 
                        envmapSRV->GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
                }
            }
            {

                const shared_ptr<vaTexture> & shadowCubesTexture    = lighting.GetShadowCubeArrayTexture( );
                const vaShaderResourceViewDX12 * shadowCubesSRV     = (shadowCubesTexture== nullptr) ? ( nullptr ) : (AsDX12( *shadowCubesTexture ).GetSRV() );
#ifdef SET_UNUSED_DESC_TO_NULL
                if( shadowCubesSRV == nullptr )
                    shadowCubesSRV = &nullSRV;
#endif
                if( shadowCubesSRV != nullptr )
                {
                    if( shadowCubesTexture != nullptr )
                        AsDX12( *shadowCubesTexture ).TransitionResource( *this, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE );
                    d3d12Device->CopyDescriptorsSimple( 1, gpuHeapSRVCBVUAV->ComputeCPUHandle(descHeapSRVOffset + SHADERGLOBAL_LIGHTING_CUBE_SHADOW_TEXTURESLOT ), 
                        shadowCubesSRV->GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
                }
            }
        }

        // globals
        {
            GetRenderDevice().GetRenderGlobals().UpdateShaderConstants( *sceneDrawContext );
            const vaConstantBufferViewDX12 * globalConstantsView = (GetRenderDevice().GetRenderGlobals().GetConstantsBuffer() == nullptr) ? (nullptr) : (AsDX12( *GetRenderDevice().GetRenderGlobals().GetConstantsBuffer() ).GetCBV());
#ifdef SET_UNUSED_DESC_TO_NULL
                if( globalConstantsView == nullptr )
                    globalConstantsView = &nullCBV;
#endif
            if( globalConstantsView != nullptr )
            {
                // res.TransitionResource( *this, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE );
                d3d12Device->CopyDescriptorsSimple( 1, gpuHeapSRVCBVUAV->ComputeCPUHandle(descHeapCBVOffset + SHADERGLOBAL_CONSTANTSBUFFERSLOT ), 
                    globalConstantsView->GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
            }        
        }
    }
    return vaDrawResultFlags::None;
}

void vaRenderDeviceContextDX12::UnsetSceneGlobals( vaSceneDrawContext * sceneDrawContext )
{
    assert( vaThreading::IsMainThread() );
    sceneDrawContext;

    /*
    // this is mostly for debugging purposes - make sure there's nothing leftover if we haven't specifically requested it so it's deterministic
    const vaConstantBufferViewDX12 & nullCBV        = AsDX12( GetRenderDevice() ).GetNullCBV        ();
    const vaShaderResourceViewDX12 & nullSRV        = AsDX12( GetRenderDevice() ).GetNullSRV        ();
    //const vaUnorderedAccessViewDX12& nullUAV        = AsDX12( GetRenderDevice() ).GetNullUAV        ();
    //const vaSamplerViewDX12        & nullSamplerView= AsDX12( GetRenderDevice() ).GetNullSamplerView();
    const auto & ranges = AsDX12( GetRenderDevice() ).GetDefaultRootParamIndexRanges();
    SetRootDescriptorTable( ranges.SRVBase+SHADERGLOBAL_LIGHTING_ENVMAP_TEXTURESLOT, nullSRV.GetGPUHandle() );
    SetRootDescriptorTable( ranges.SRVBase+SHADERGLOBAL_LIGHTING_CUBE_SHADOW_TEXTURESLOT, nullSRV.GetGPUHandle() );
    SetRootDescriptorTable( ranges.CBVBase+LIGHTINGGLOBAL_CONSTANTSBUFFERSLOT, nullCBV.GetGPUHandle() );
    SetRootDescriptorTable( ranges.CBVBase+SHADERGLOBAL_CONSTANTSBUFFERSLOT, nullCBV.GetGPUHandle() );
    */
}

void vaRenderDeviceContextDX12::EndItems( )
{
    assert( vaThreading::IsMainThread() );
    assert( m_itemsStarted != vaRenderTypeFlags::None );

    UpdateSceneDrawContext( nullptr );

    m_itemsStarted = vaRenderTypeFlags::None;
    assert( m_commandListReady );

    if( m_itemsSubmittedAfterLastExecute > c_flushAfterItemCount )
        Flush();
}

void vaRenderDeviceContextDX12::UpdateSceneDrawContext( vaSceneDrawContext * sceneDrawContext )
{
    assert( vaThreading::IsMainThread() );
    sceneDrawContext;
    
    if( m_lastSceneDrawContext != sceneDrawContext )
    {
        if( sceneDrawContext == nullptr )
            UnsetSceneGlobals( m_lastSceneDrawContext );
        else
            SetSceneGlobals( sceneDrawContext );
        m_lastSceneDrawContext = sceneDrawContext;
    }
}

vaDrawResultFlags vaRenderDeviceContextDX12::ExecuteItem( const vaGraphicsItem & renderItem )
{
    assert( vaThreading::IsMainThread() );
    if( renderItem.SceneDrawContext != nullptr )
    { assert( AsDX12(&renderItem.SceneDrawContext->RenderDeviceContext) == this ); }

    // ExecuteTask can only be called in between BeginTasks and EndTasks - call ExecuteSingleItem 
    assert( (m_itemsStarted & vaRenderTypeFlags::Graphics) != 0 );
    if( (m_itemsStarted & vaRenderTypeFlags::Graphics) == 0 )
        return vaDrawResultFlags::UnspecifiedError;
    
    // must have a vertex shader at least
    if( renderItem.VertexShader == nullptr || renderItem.VertexShader->IsEmpty() )
        { assert( false ); return vaDrawResultFlags::UnspecifiedError; }

    vaGraphicsPSODescDX12 psoDesc;

    vaShader::State shState;
    if( (shState = AsDX12(*renderItem.VertexShader).GetShader( psoDesc.VSBlob, psoDesc.VSInputLayout, psoDesc.VSUniqueContentsID ) ) != vaShader::State::Cooked )
    {
        assert( shState != vaShader::State::Empty ); // trying to render with empty compute shader & this happened between here and the check few lines above? this is VERY weird and possibly a bug
        return (shState == vaShader::State::Uncooked)?(vaDrawResultFlags::ShadersStillCompiling):(vaDrawResultFlags::UnspecifiedError);
    }

    // vaShader::State::Empty and vaShader::State::Cooked are both ok but we must abort for uncooked!
    if( renderItem.PixelShader != nullptr && AsDX12(*renderItem.PixelShader).GetShader( psoDesc.PSBlob, psoDesc.PSUniqueContentsID ) == vaShader::State::Uncooked )
        return vaDrawResultFlags::ShadersStillCompiling;
    if( renderItem.GeometryShader != nullptr && AsDX12(*renderItem.GeometryShader).GetShader( psoDesc.GSBlob, psoDesc.GSUniqueContentsID ) == vaShader::State::Uncooked )
        return vaDrawResultFlags::ShadersStillCompiling;
    if( renderItem.HullShader != nullptr && AsDX12(*renderItem.HullShader).GetShader( psoDesc.HSBlob, psoDesc.HSUniqueContentsID ) == vaShader::State::Uncooked )
        return vaDrawResultFlags::ShadersStillCompiling;
    if( renderItem.DomainShader != nullptr && AsDX12(*renderItem.DomainShader).GetShader( psoDesc.DSBlob, psoDesc.DSUniqueContentsID ) == vaShader::State::Uncooked )
        return vaDrawResultFlags::ShadersStillCompiling;
    
    UpdateSceneDrawContext( renderItem.SceneDrawContext );

    //////////////////////////////////////////////////////////////////////////
    // set descriptor tables and prepare for copying
    vaRenderDeviceDX12::TransientGPUDescriptorHeap * gpuHeapSRVCBVUAV = AsDX12( GetRenderDevice() ).GetTransientDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    int descHeapBaseIndexSRVCBVUAV;
    if( !gpuHeapSRVCBVUAV->Allocate( vaRenderDeviceDX12::DefaultRootSignatureIndexRanges::SRVCBVUAVTotalCount, descHeapBaseIndexSRVCBVUAV ) )
        { assert( false ); return vaDrawResultFlags::UnspecifiedError; }

    // vaRenderDeviceDX12::TransientGPUDescriptorHeap * gpuHeapSampler = AsDX12( GetRenderDevice() ).GetTransientDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    // int descHeapBaseIndexSampler;
    // if( !gpuHeapSampler->Allocate( vaRenderDeviceDX12::DefaultRootSignatureIndexRanges::SamplerCount, descHeapBaseIndexSampler ) )
    //     { assert( false ); return vaDrawResultFlags::UnspecifiedError; }
    //
    m_commandList->SetGraphicsRootDescriptorTable( vaRenderDeviceDX12::DefaultRootSignatureIndexRanges::RootParameterIndexSRVCBVUAV, gpuHeapSRVCBVUAV->ComputeGPUHandle(descHeapBaseIndexSRVCBVUAV) );
    //m_commandList->SetGraphicsRootDescriptorTable( vaRenderDeviceDX12::DefaultRootSignatureIndexRanges::RootParameterIndexSampler, gpuHeapSampler->ComputeGPUHandle(descHeapBaseIndexSampler) );
    //////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
    #define SET_UNUSED_DESC_TO_NULL
#endif

#ifdef SET_UNUSED_DESC_TO_NULL
    const vaConstantBufferViewDX12 & nullCBV        = AsDX12( GetRenderDevice() ).GetNullCBV        ();
    const vaShaderResourceViewDX12 & nullSRV        = AsDX12( GetRenderDevice() ).GetNullSRV        ();
    const vaUnorderedAccessViewDX12& nullUAV        = AsDX12( GetRenderDevice() ).GetNullUAV        ();
    const vaSamplerViewDX12        & nullSamplerView= AsDX12( GetRenderDevice() ).GetNullSamplerView(); nullSamplerView;
#endif

    auto & d3d12Device = AsDX12( GetRenderDevice() ).GetPlatformDevice( );

    for( int i = 0; i < _countof( renderItem.ShaderResourceViews ); i++ )
    {
        if( renderItem.ShaderResourceViews[i] == nullptr )
            continue;

        vaShaderResourceDX12 & res = AsDX12(*renderItem.ShaderResourceViews[i]);
        const vaShaderResourceViewDX12 * srv = res.GetSRV();
        if( srv != nullptr )
        {
            res.TransitionResource( *this, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE );
            d3d12Device->CopyDescriptorsSimple( 1, gpuHeapSRVCBVUAV->ComputeCPUHandle(descHeapBaseIndexSRVCBVUAV + vaRenderDeviceDX12::DefaultRootSignatureIndexRanges::SRVBase + i ), srv->GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
        }
#ifdef SET_UNUSED_DESC_TO_NULL
        else
            d3d12Device->CopyDescriptorsSimple( 1, gpuHeapSRVCBVUAV->ComputeCPUHandle(descHeapBaseIndexSRVCBVUAV + vaRenderDeviceDX12::DefaultRootSignatureIndexRanges::SRVBase + i ), nullSRV.GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
#endif
    }

    for( int i = 0; i < _countof( renderItem.ConstantBuffers ); i++ )
    {
        if( renderItem.ConstantBuffers[i] == nullptr )
            continue;

        vaShaderResourceDX12 & res = AsDX12(*renderItem.ConstantBuffers[i]);
        const vaConstantBufferViewDX12 * cbv = res.GetCBV();
        if( cbv != nullptr )
        {
            // res.TransitionResource( *this, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE );
            d3d12Device->CopyDescriptorsSimple( 1, gpuHeapSRVCBVUAV->ComputeCPUHandle(descHeapBaseIndexSRVCBVUAV + vaRenderDeviceDX12::DefaultRootSignatureIndexRanges::CBVBase + i ), cbv->GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
        }
#ifdef SET_UNUSED_DESC_TO_NULL
        else
            d3d12Device->CopyDescriptorsSimple( 1, gpuHeapSRVCBVUAV->ComputeCPUHandle(descHeapBaseIndexSRVCBVUAV + vaRenderDeviceDX12::DefaultRootSignatureIndexRanges::CBVBase + i ), nullCBV.GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
#endif
    }
    assert( _countof( m_outputsState.UAVs ) >= m_outputsState.UAVsStartSlot + m_outputsState.UAVCount );
    for( int i = m_outputsState.UAVsStartSlot; i < (int)( m_outputsState.UAVsStartSlot + m_outputsState.UAVCount ); i++ )
    {
        if( m_outputsState.UAVs[i-m_outputsState.UAVsStartSlot] == nullptr )
            continue;

        vaShaderResourceDX12 & res = AsDX12(*m_outputsState.UAVs[i-m_outputsState.UAVsStartSlot]);
        const vaUnorderedAccessViewDX12 * uav = res.GetUAV();
        if( uav != nullptr )
        {
            res.TransitionResource( *this, D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
            d3d12Device->CopyDescriptorsSimple( 1, gpuHeapSRVCBVUAV->ComputeCPUHandle(descHeapBaseIndexSRVCBVUAV + vaRenderDeviceDX12::DefaultRootSignatureIndexRanges::UAVBase + i ), uav->GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
        }
#ifdef SET_UNUSED_DESC_TO_NULL
        else
            d3d12Device->CopyDescriptorsSimple( 1, gpuHeapSRVCBVUAV->ComputeCPUHandle(descHeapBaseIndexSRVCBVUAV + vaRenderDeviceDX12::DefaultRootSignatureIndexRanges::UAVBase + i ), nullUAV.GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
#endif
        assert( m_outputsState.UAVInitialCounts[i-m_outputsState.UAVsStartSlot] == -1 ); // UAV counters not supported (and no plans on supporting them)
    }

    // transition render target resources here too
    for( int i = 0; i < c_maxRTs; i++ )  
    {
        if( m_outputsState.RenderTargets[i] != nullptr )
        {
            const vaRenderTargetViewDX12 * rtv = AsDX12( *m_outputsState.RenderTargets[i] ).GetRTV( );
            if( rtv != nullptr && rtv->IsCreated() )
                AsDX12( *m_outputsState.RenderTargets[i] ).TransitionResource( *this, D3D12_RESOURCE_STATE_RENDER_TARGET );
        }
    }
    if( m_outputsState.DepthStencil != nullptr )
    {
        const vaDepthStencilViewDX12 * dsv = AsDX12( *m_outputsState.DepthStencil ).GetDSV( );
        if( dsv != nullptr && dsv->IsCreated( ) )
            AsDX12( *m_outputsState.DepthStencil ).TransitionResource( *this, D3D12_RESOURCE_STATE_DEPTH_WRITE );
    }

    psoDesc.BlendMode               = renderItem.BlendMode;
    psoDesc.FillMode                = renderItem.FillMode;
    psoDesc.CullMode                = renderItem.CullMode;
    psoDesc.FrontCounterClockwise   = renderItem.FrontCounterClockwise;
    psoDesc.DepthEnable             = renderItem.DepthEnable;
    psoDesc.DepthWriteEnable        = renderItem.DepthWriteEnable;
    psoDesc.DepthFunc               = renderItem.DepthFunc;
    psoDesc.Topology                = renderItem.Topology;

    int sampleCount = 1;
    if( m_outputsState.RenderTargets[0] != nullptr )
        sampleCount = m_outputsState.RenderTargets[0]->GetSampleCount( );
    else if( m_outputsState.DepthStencil != nullptr )
        sampleCount = m_outputsState.DepthStencil->GetSampleCount( );
    // else { assert( false ); }   // no render targets? no depth either? hmm?

    psoDesc.SampleDescCount         = sampleCount;
    psoDesc.MultisampleEnable       = sampleCount > 1;
    psoDesc.NumRenderTargets        = m_outputsState.RenderTargetCount;
    for( int i = 0; i < _countof(psoDesc.RTVFormats); i++ )
        psoDesc.RTVFormats[i]       = ( m_outputsState.RenderTargets[i] != nullptr )?(m_outputsState.RenderTargets[i]->GetRTVFormat()):(vaResourceFormat::Unknown);
    psoDesc.DSVFormat               = ( m_outputsState.DepthStencil != nullptr )?(m_outputsState.DepthStencil->GetDSVFormat()):(vaResourceFormat::Unknown);

    // TOPOLOGY
    D3D_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    switch( renderItem.Topology )
    {   case vaPrimitiveTopology::PointList:        topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;        break;
        case vaPrimitiveTopology::LineList:         topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;         break;
        case vaPrimitiveTopology::TriangleList:     topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;     break;
        case vaPrimitiveTopology::TriangleStrip:    topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;    break;
        default: assert( false ); break;
    }
    if( topology != m_commandListCurrentTopology )
    {
        m_commandList->IASetPrimitiveTopology( topology );
        m_commandListCurrentTopology = topology;
    }
    
    if( renderItem.IndexBuffer != nullptr )
    {
        D3D12_INDEX_BUFFER_VIEW bufferView = AsDX12(*renderItem.IndexBuffer).GetResourceView();
        m_commandList->IASetIndexBuffer( &bufferView );
    }
    else
        m_commandList->IASetIndexBuffer( nullptr );
    
    if( renderItem.VertexBuffer != nullptr )
    {
        D3D12_VERTEX_BUFFER_VIEW bufferView = AsDX12(*renderItem.VertexBuffer).GetResourceView();
        m_commandList->IASetVertexBuffers( 0, 1, &bufferView );
    }
    else
        m_commandList->IASetVertexBuffers( 0, 0, nullptr );

    //// these are the defaults set in BindDefaultStates - change this code if they need changing per-draw-call
    //FLOAT defBlendFactor[4] = { 1, 1, 1, 1 };
    //m_commandList->OMSetBlendFactor( defBlendFactor ); // If you pass NULL, the runtime uses or stores a blend factor equal to { 1, 1, 1, 1 }.
    //m_commandList->OMSetStencilRef( 0 );

    shared_ptr<vaGraphicsPSODX12> pso = AsDX12(GetRenderDevice()).FindOrCreateGraphicsPipelineState( psoDesc );
    m_commandList->SetPipelineState( pso->GetPSO().Get() );
   
    bool continueWithDraw = true;
    if( renderItem.PreDrawHook != nullptr )
        continueWithDraw = renderItem.PreDrawHook( renderItem, *this );

    if( continueWithDraw )
    {
        switch( renderItem.DrawType )
        {
        case( vaGraphicsItem::DrawType::DrawSimple ): 
            m_commandList->DrawInstanced( renderItem.DrawSimpleParams.VertexCount, 1, renderItem.DrawSimpleParams.StartVertexLocation, 0 );
            break;
        case( vaGraphicsItem::DrawType::DrawIndexed ): 
            m_commandList->DrawIndexedInstanced( renderItem.DrawIndexedParams.IndexCount, 1, renderItem.DrawIndexedParams.StartIndexLocation, renderItem.DrawIndexedParams.BaseVertexLocation, 0 );
            break;
        default:
            assert( false );
            break;
        }

        m_itemsSubmittedAfterLastExecute++;
    }

    if( renderItem.PostDrawHook != nullptr )
        renderItem.PostDrawHook( renderItem, *this );

    AsDX12(GetRenderDevice()).ReleasePipelineState( pso );
    //// for caching - not really needed for now
    //// m_lastRenderItem = renderItem;
    return vaDrawResultFlags::None;
}

vaDrawResultFlags vaRenderDeviceContextDX12::ExecuteItem( const vaComputeItem & computeItem )
{
    assert( vaThreading::IsMainThread() );
    // ExecuteTask can only be called in between BeginTasks and EndTasks - call ExecuteSingleItem 
    assert( (m_itemsStarted & vaRenderTypeFlags::Compute) != 0 );
    if( (m_itemsStarted & vaRenderTypeFlags::Compute) == 0 )
        return vaDrawResultFlags::UnspecifiedError;

    // must have compute shader at least
    if( computeItem.ComputeShader == nullptr || computeItem.ComputeShader->IsEmpty() )
        { assert( false ); return vaDrawResultFlags::UnspecifiedError; }

    vaComputePSODescDX12 psoDesc;

    vaShader::State shState;
    if( ( shState = AsDX12(*computeItem.ComputeShader).GetShader( psoDesc.CSBlob, psoDesc.CSUniqueContentsID ) ) != vaShader::State::Cooked )
    {
        assert( shState != vaShader::State::Empty ); // trying to render with empty compute shader & this happened between here and the check few lines above? this is VERY weird and possibly a bug
        return (shState == vaShader::State::Uncooked)?(vaDrawResultFlags::ShadersStillCompiling):(vaDrawResultFlags::UnspecifiedError);
    }

    UpdateSceneDrawContext( computeItem.SceneDrawContext );

    //////////////////////////////////////////////////////////////////////////
    // set descriptor tables and prepare for copying
    vaRenderDeviceDX12::TransientGPUDescriptorHeap * gpuHeapSRVCBVUAV = AsDX12( GetRenderDevice() ).GetTransientDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    int descHeapBaseIndexSRVCBVUAV;
    if( !gpuHeapSRVCBVUAV->Allocate( vaRenderDeviceDX12::DefaultRootSignatureIndexRanges::SRVCBVUAVTotalCount, descHeapBaseIndexSRVCBVUAV ) )
        { assert( false ); return vaDrawResultFlags::UnspecifiedError; }

    // vaRenderDeviceDX12::TransientGPUDescriptorHeap * gpuHeapSampler = AsDX12( GetRenderDevice() ).GetTransientDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    // int descHeapBaseIndexSampler;
    // if( !gpuHeapSampler->Allocate( vaRenderDeviceDX12::DefaultRootSignatureIndexRanges::SamplerCount, descHeapBaseIndexSampler ) )
    //     { assert( false ); return vaDrawResultFlags::UnspecifiedError; }
    //
    m_commandList->SetComputeRootDescriptorTable( vaRenderDeviceDX12::DefaultRootSignatureIndexRanges::RootParameterIndexSRVCBVUAV, gpuHeapSRVCBVUAV->ComputeGPUHandle(descHeapBaseIndexSRVCBVUAV) );
    // m_commandList->SetComputeRootDescriptorTable( vaRenderDeviceDX12::DefaultRootSignatureIndexRanges::RootParameterIndexSampler, gpuHeapSampler->ComputeGPUHandle(descHeapBaseIndexSampler) );
    //////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
    #define SET_UNUSED_DESC_TO_NULL
#endif

#ifdef SET_UNUSED_DESC_TO_NULL
    const vaConstantBufferViewDX12 & nullCBV        = AsDX12( GetRenderDevice() ).GetNullCBV        ();
    const vaShaderResourceViewDX12 & nullSRV        = AsDX12( GetRenderDevice() ).GetNullSRV        ();
    const vaUnorderedAccessViewDX12& nullUAV        = AsDX12( GetRenderDevice() ).GetNullUAV        ();
    const vaSamplerViewDX12        & nullSamplerView= AsDX12( GetRenderDevice() ).GetNullSamplerView(); nullSamplerView;
#endif

    auto & d3d12Device = AsDX12( GetRenderDevice() ).GetPlatformDevice( );

    // CONSTANT BUFFERS, SRVs, UAVs
    for( int i = 0; i < _countof( computeItem.ShaderResourceViews ); i++ )
    {
        if( computeItem.ShaderResourceViews[i] == nullptr )
            continue;

        vaShaderResourceDX12 & res = AsDX12(*computeItem.ShaderResourceViews[i]);
        const vaShaderResourceViewDX12 * srv = res.GetSRV();
        if( srv != nullptr )
        {
            res.TransitionResource( *this, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE );
            d3d12Device->CopyDescriptorsSimple( 1, gpuHeapSRVCBVUAV->ComputeCPUHandle(descHeapBaseIndexSRVCBVUAV + vaRenderDeviceDX12::DefaultRootSignatureIndexRanges::SRVBase + i ), srv->GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
        }
#ifdef SET_UNUSED_DESC_TO_NULL
        else
            d3d12Device->CopyDescriptorsSimple( 1, gpuHeapSRVCBVUAV->ComputeCPUHandle(descHeapBaseIndexSRVCBVUAV + vaRenderDeviceDX12::DefaultRootSignatureIndexRanges::SRVBase + i ), nullSRV.GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
#endif
    }
    for( int i = 0; i < _countof( computeItem.ConstantBuffers ); i++ )
    {
        if( computeItem.ConstantBuffers[i] == nullptr )
            continue;

        vaShaderResourceDX12 & res = AsDX12(*computeItem.ConstantBuffers[i]);
        const vaConstantBufferViewDX12 * cbv = res.GetCBV();
        if( cbv != nullptr )
        {
            // res.GetRSTH()->RSTHTransition( *this, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE );
            d3d12Device->CopyDescriptorsSimple( 1, gpuHeapSRVCBVUAV->ComputeCPUHandle(descHeapBaseIndexSRVCBVUAV + vaRenderDeviceDX12::DefaultRootSignatureIndexRanges::CBVBase + i ), cbv->GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
        }
#ifdef SET_UNUSED_DESC_TO_NULL
        else
            d3d12Device->CopyDescriptorsSimple( 1, gpuHeapSRVCBVUAV->ComputeCPUHandle(descHeapBaseIndexSRVCBVUAV + vaRenderDeviceDX12::DefaultRootSignatureIndexRanges::CBVBase + i ), nullCBV.GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
#endif
    }
    for( int i = 0; i < _countof( computeItem.UnorderedAccessViews ); i++ )
    {
        if( computeItem.UnorderedAccessViews[i] == nullptr )
            continue;

        vaShaderResourceDX12 & res = AsDX12(*computeItem.UnorderedAccessViews[i]);
        const vaUnorderedAccessViewDX12 * uav = res.GetUAV();
        if( uav != nullptr )
        {
            res.TransitionResource( *this, D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
            d3d12Device->CopyDescriptorsSimple( 1, gpuHeapSRVCBVUAV->ComputeCPUHandle(descHeapBaseIndexSRVCBVUAV + vaRenderDeviceDX12::DefaultRootSignatureIndexRanges::UAVBase + i ), uav->GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
        }
#ifdef SET_UNUSED_DESC_TO_NULL
        else
            d3d12Device->CopyDescriptorsSimple( 1, gpuHeapSRVCBVUAV->ComputeCPUHandle(descHeapBaseIndexSRVCBVUAV + vaRenderDeviceDX12::DefaultRootSignatureIndexRanges::UAVBase + i ), nullUAV.GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
#endif
        assert( m_outputsState.UAVInitialCounts[i-m_outputsState.UAVsStartSlot] == -1 ); // UAV counters not supported (and no plans on supporting them)
    }

    shared_ptr<vaComputePSODX12> pso = AsDX12(GetRenderDevice()).FindOrCreateComputePipelineState( psoDesc );
    m_commandList->SetPipelineState( pso->GetPSO().Get() );

    bool continueWithDraw = true;
    if( computeItem.PreComputeHook != nullptr )
    {
        assert( false ); // well this was never tested so... step through, think of any implications - messing up caching of states or something? how to verify that? etc.
        continueWithDraw = computeItem.PreComputeHook( computeItem, *this );
    }

    if( continueWithDraw )
    {
        switch( computeItem.ComputeType )
        {
        case( vaComputeItem::Dispatch ): 
            m_commandList->Dispatch( computeItem.DispatchParams.ThreadGroupCountX, computeItem.DispatchParams.ThreadGroupCountY, computeItem.DispatchParams.ThreadGroupCountZ );
            break;
        case( vaComputeItem::DispatchIndirect ): 
            // assert( computeItem.DispatchIndirectParams.BufferForArgs != nullptr );
            assert( false ); // not yet implemented
            // see: https://docs.microsoft.com/en-us/windows/desktop/direct3d12/indirect-drawing and https://docs.microsoft.com/en-us/windows/desktop/api/d3d12/nf-d3d12-id3d12graphicscommandlist-executeindirect
            // m_deviceContext->DispatchIndirect( computeItem.DispatchIndirectParams.BufferForArgs->SafeCast<vaShaderResourceDX11*>()->GetBuffer(), computeItem.DispatchIndirectParams.AlignedOffsetForArgs );
            break;
        default:
            assert( false );
            break;
        }
        m_itemsSubmittedAfterLastExecute++;
    }
    
    AsDX12(GetRenderDevice()).ReleasePipelineState( pso );

    // // API-SPECIFIC MODIFIER CALLBACKS
    // std::function<bool( RenderItem & )> PreDrawHook;

    // for caching - not really needed for now
    // m_lastRenderItem = renderItem;
    return vaDrawResultFlags::None;
}

bool vaRenderDeviceContextDX12::IsMainContext( ) const
{
    return AsDX12( GetRenderDevice() ).GetMainContext() == this;
}


void RegisterDeviceContextDX12( )
{
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX12, vaRenderDeviceContext, vaRenderDeviceContextDX12 );
}


