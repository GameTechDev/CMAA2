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

#pragma once

#include "Core/vaCoreIncludes.h"

#include "Rendering/DirectX/vaDirectXIncludes.h"
#include "Rendering/DirectX/vaDirectXTools.h"

#include "Rendering/vaRenderingIncludes.h"


namespace VertexAsylum
{
    class vaConstantBufferDX11 : public vaConstantBuffer, public virtual vaShaderResourceDX11
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    private:
        ID3D11Buffer *                      m_buffer    = nullptr;

    protected:
        friend class vaConstantBuffer;
        explicit                            vaConstantBufferDX11( const vaRenderingModuleParams & params );
        virtual                             ~vaConstantBufferDX11( );

        virtual void                        Update( vaRenderDeviceContext & renderContext, const void * data, uint32 dataSize ) override;
        virtual void                        Create( int bufferSize, const void * initialData, bool dynamicUpload ) override;
        virtual void                        Destroy( ) override;

    public:
        // this needs to go out probably
        void                                SetToAPISlot( vaRenderDeviceContext & renderContext, int slot, bool assertOnOverwrite = true );
        void                                UnsetFromAPISlot( vaRenderDeviceContext & renderContext, int slot, bool assertOnNotSet = true );

    public:
        void                                Update( ID3D11DeviceContext * dx11Context, const void * data, uint32 dataSize );

    public:
        virtual vaResourceBindSupportFlags  GetBindSupportFlags( ) const override                           { return vaResourceBindSupportFlags::ConstantBuffer; }
        virtual ID3D11Buffer *              GetBuffer( ) const override { return m_buffer; }
        virtual ID3D11UnorderedAccessView * GetUAV( ) const override    { return nullptr; }
        virtual ID3D11ShaderResourceView *  GetSRV( ) const override    { return nullptr; }
    };


    class vaIndexBufferDX11 : public vaIndexBuffer, public virtual vaShaderResourceDX11
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    private:
        ID3D11Buffer *                      m_buffer    = nullptr;

    protected:
        friend class vaConstantBuffer;
        explicit                            vaIndexBufferDX11( const vaRenderingModuleParams & params );
        virtual                             ~vaIndexBufferDX11( );

        virtual void                        Update( vaRenderDeviceContext & renderContext, const void * data, uint32 dataSize ) override;
        virtual void                        Create( int indexCount, const void * initialData ) override;
        virtual void                        Destroy( ) override;
        virtual bool                        IsCreated( ) const override     { return m_buffer != nullptr; }

    public:
        virtual vaResourceBindSupportFlags  GetBindSupportFlags( ) const override                           { return vaResourceBindSupportFlags::IndexBuffer; }
        virtual ID3D11Buffer *              GetBuffer( ) const override { return m_buffer; }
        virtual ID3D11UnorderedAccessView * GetUAV( ) const override    { return nullptr; }
        virtual ID3D11ShaderResourceView *  GetSRV( ) const override    { return nullptr; }
    };


    class vaVertexBufferDX11 : public vaVertexBuffer, public virtual vaShaderResourceDX11
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    private:
        ID3D11Buffer *                      m_buffer    = nullptr;

    protected:
        friend class vaConstantBuffer;
        explicit                            vaVertexBufferDX11( const vaRenderingModuleParams & params );
        virtual                             ~vaVertexBufferDX11( );

        virtual void                        Update( vaRenderDeviceContext & renderContext, const void * data, uint32 dataSize ) override;
        virtual void                        Create( int vertexCount, int vertexSize, const void * initialData, bool dynamicUpload ) override;
        virtual void                        Destroy( ) override;
        virtual bool                        IsCreated( ) const override     { return m_buffer != nullptr; }

        virtual bool                        Map( vaRenderDeviceContext & renderContext, vaResourceMapType mapType );
        virtual void                        Unmap( vaRenderDeviceContext & renderContext );

    public:
        virtual vaResourceBindSupportFlags  GetBindSupportFlags( ) const override                           { return vaResourceBindSupportFlags::VertexBuffer; }
        virtual ID3D11Buffer *              GetBuffer( ) const override { return m_buffer; }
        virtual ID3D11UnorderedAccessView * GetUAV( ) const override    { return nullptr; }
        virtual ID3D11ShaderResourceView *  GetSRV( ) const override    { return nullptr; }

    };
    
    class vaStructuredBufferDX11 : public vaStructuredBuffer, public virtual vaShaderResourceDX11
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    private:
        ID3D11Buffer *                      m_buffer    = nullptr;
        ID3D11UnorderedAccessView *         m_bufferUAV = nullptr;
        ID3D11ShaderResourceView *          m_bufferSRV = nullptr;

    protected:
        friend class vaConstantBuffer;
        explicit                            vaStructuredBufferDX11( const vaRenderingModuleParams & params );
        virtual                             ~vaStructuredBufferDX11( );

        virtual void                        Update( vaRenderDeviceContext & renderContext, const void * data, uint32 dataSize ) override;
        virtual bool                        Create( int elementCount, int structureByteSize, bool hasCounter, const void * initialData ) override;
        virtual void                        Destroy( ) override;

    public:
        virtual vaResourceBindSupportFlags  GetBindSupportFlags( ) const override                           { return vaResourceBindSupportFlags::ShaderResource | vaResourceBindSupportFlags::UnorderedAccess; }
        virtual ID3D11Buffer *              GetBuffer( ) const override { return m_buffer; }
        virtual ID3D11UnorderedAccessView * GetUAV( ) const override    { return m_bufferUAV; }
        virtual ID3D11ShaderResourceView *  GetSRV( ) const override    { return m_bufferSRV; }

    };

}