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

#include "Rendering/DirectX/vaDirectXIncludes.h"
#include "Rendering/vaRenderingIncludes.h"

namespace VertexAsylum
{
    struct vaShaderCacheKey12;
    class vaShaderCacheEntry12;

    class vaShaderDX12 : public virtual vaShader
    {
    protected:
        ComPtr<ID3DBlob>                m_shaderBlob;

    public:
        vaShaderDX12( const vaRenderingModuleParams & params );
        virtual ~vaShaderDX12( );
        //
        virtual void                    Clear( ) override;
        //
        virtual bool                    IsCreated( ) override   { std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex, std::try_to_lock ); return allShaderDataLock.owns_lock() && m_shaderBlob.Get() != nullptr; }
        //
        // D3D12_SHADER_BYTECODE           GetBytecode( )          { if( IsCreated() ) return CD3DX12_SHADER_BYTECODE( m_shaderBlob.Get() ); else return { nullptr, 0 }; }
        //
        vaShader::State                 GetShader( ComPtr<ID3DBlob> & outBlob, int64 & outUniqueContentsID );
        //
    protected:
        virtual void                    CreateCacheKey( vaShaderCacheKey12 & outKey );
        //
    protected:
        //
        virtual void                    DestroyShader( ) override { DestroyShaderBase( ); }
        void                            DestroyShaderBase( );
        //
        ID3DBlob *                      CreateShaderBase( bool & loadedFromCache );
        //
        virtual void                    CreateShader( ) override;
    };

 #pragma warning ( push )
 #pragma warning ( disable : 4250 )

    class vaPixelShaderDX12 : public vaShaderDX12, public vaPixelShader
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );
    public:
        vaPixelShaderDX12( const vaRenderingModuleParams & params ) : vaShaderDX12( params ), vaShader( params ) { }
        virtual ~vaPixelShaderDX12( ) {}
    };

    class vaComputeShaderDX12 : public vaShaderDX12, public vaComputeShader
    {
    public:
        vaComputeShaderDX12( const vaRenderingModuleParams & params ) : vaShaderDX12( params ), vaShader( params ) { }
        virtual ~vaComputeShaderDX12( ) {}
    };

    class vaHullShaderDX12 : public vaShaderDX12, public vaHullShader
    {
    public:
        vaHullShaderDX12( const vaRenderingModuleParams & params ) : vaShaderDX12( params ), vaShader( params ) { }
        virtual ~vaHullShaderDX12( ) {}
    };

    class vaDomainShaderDX12 : public vaShaderDX12, public vaDomainShader
    {
    public:
        vaDomainShaderDX12( const vaRenderingModuleParams & params ) : vaShaderDX12( params ), vaShader( params ) { }
        virtual ~vaDomainShaderDX12( ) {}
    };

    class vaGeometryShaderDX12 : public vaShaderDX12, public vaGeometryShader
    {
    public:
        vaGeometryShaderDX12( const vaRenderingModuleParams & params ) : vaShaderDX12( params ), vaShader( params ) { }
        virtual ~vaGeometryShaderDX12( ) {}
    };

    class vaVertexShaderDX12 : public vaShaderDX12, public vaVertexShader
    {
        shared_ptr< vector< D3D12_INPUT_ELEMENT_DESC > >
                                    m_inputLayoutDX12;

    protected:
        void                        SetInputLayout( D3D12_INPUT_ELEMENT_DESC * elements, uint32 elementCount );

    public:
        vaVertexShaderDX12( const vaRenderingModuleParams & params ) : vaShaderDX12( params ), vaShader( params ) { }
        virtual ~vaVertexShaderDX12( );

    public:
        virtual void                CreateShaderAndILFromFile( const wstring & filePath, const string & shaderModel, const string & entryPoint, const vector<vaVertexInputElementDesc> & inputLayoutElements, const vaShaderMacroContaner & macros, bool forceImmediateCompile ) override;
        virtual void                CreateShaderAndILFromBuffer( const string & shaderCode, const string & shaderModel, const string & entryPoint, const vector<vaVertexInputElementDesc> & inputLayoutElements, const vaShaderMacroContaner & macros, bool forceImmediateCompile ) override;

        vaShader::State             GetShader( ComPtr<ID3DBlob> & outBlob, shared_ptr< vector< D3D12_INPUT_ELEMENT_DESC > > & outInputLayout, int64 & outUniqueContentsID );

        // platform independent <-> dx conversions
        static vaVertexInputLayoutDesc  LayoutVAFromDX( D3D12_INPUT_ELEMENT_DESC * elements, uint32 elementCount );

        // !!warning!! returned char pointers in descriptors outLayout point to inLayout values so make sure you keep inLayout alive until outLayout is alive
        static void                 LayoutDXFromVA( shared_ptr< vector< D3D12_INPUT_ELEMENT_DESC > > & outLayout, const vaVertexInputLayoutDesc & inLayout );

    public:

    protected:
        virtual void                CreateShader( ) override;
        virtual void                DestroyShader( ) override;

        virtual void                CreateCacheKey( vaShaderCacheKey12 & outKey );
    };

#pragma warning ( pop )

    struct vaShaderCacheKey12
    {
        std::string                StringPart;

        bool                       operator == ( const vaShaderCacheKey12 & cmp ) const { return this->StringPart == cmp.StringPart; }
        bool                       operator < ( const vaShaderCacheKey12 & cmp ) const { return this->StringPart < cmp.StringPart; }
        bool                       operator >( const vaShaderCacheKey12 & cmp ) const { return this->StringPart > cmp.StringPart; }

        void                       Save( vaStream & outStream ) const;
        void                       Load( vaStream & inStream );
    };

    class vaShaderCacheEntry12
    {
    public:
        struct FileDependencyInfo
        {
            std::wstring            FilePath;
            int64                   ModifiedTimeDate;

            FileDependencyInfo( ) : FilePath( L"" ), ModifiedTimeDate( 0 ) { }
            FileDependencyInfo( const wstring & filePath );
            FileDependencyInfo( const wstring & filePath, int64 modifiedTimeDate );
            FileDependencyInfo( vaStream & inStream ) { Load( inStream ); }

            bool                    IsModified( );

            void                    Save( vaStream & outStream ) const;
            void                    Load( vaStream & inStream );
        };

    private:

        ID3DBlob *                                               m_compiledShader;
        std::vector<vaShaderCacheEntry12::FileDependencyInfo>      m_dependencies;

    private:
        vaShaderCacheEntry12( ) {}
        explicit vaShaderCacheEntry12( const vaShaderCacheEntry12 & copy ) { copy; }
    public:
        vaShaderCacheEntry12( ID3DBlob * compiledShader, std::vector<vaShaderCacheEntry12::FileDependencyInfo> & dependencies );
        explicit vaShaderCacheEntry12( vaStream & inFile ) { m_compiledShader = NULL; Load( inFile ); }
        ~vaShaderCacheEntry12( );

        bool                       IsModified( );
        ID3DBlob *                 GetCompiledShader( ) { m_compiledShader->AddRef( ); return m_compiledShader; }

        void                       Save( vaStream & outStream ) const;
        void                       Load( vaStream & inStream );
    };

    // Singleton utility class for handling shaders
    class vaDirectX12ShaderManager : public vaShaderManager, public vaSingletonBase < vaDirectX12ShaderManager > // oooo I feel so dirty here but oh well
    {
    private:
        friend class vaShaderSharedManager;
        friend class vaMaterialManager;
        friend class vaShaderDX12;

    private:
        std::map<vaShaderCacheKey12, vaShaderCacheEntry12 *>    m_cache;
        mutable mutex                                       m_cacheMutex;
#ifdef VA_SHADER_SHADER_CACHE_PERSISTENT_STORAGE_ENABLE
        wstring                                             m_cacheFilePath;

        // since cache loading from the disk happens only once, use this to ensure loading proc has grabbed m_cacheMutex - once it's unlocked we know it finished
        std::atomic_bool                                    m_cacheLoadStarted = false;
        std::mutex                                          m_cacheLoadStartedMutex;
        std::condition_variable                             m_cacheLoadStartedCV;
#endif

        shared_ptr<int>                                     m_objLifetimeToken;

    public:
        vaDirectX12ShaderManager( vaRenderDevice & device );
        ~vaDirectX12ShaderManager( );

    public:
        ID3DBlob *                  FindInCache( vaShaderCacheKey12 & key, bool & foundButModified );
        void                        AddToCache( vaShaderCacheKey12 & key, ID3DBlob * shaderBlob, std::vector<vaShaderCacheEntry12::FileDependencyInfo> & dependencies );
        void                        ClearCache( );

        // pushBack (searched last) or pushFront (searched first)
        virtual void                RegisterShaderSearchPath( const std::wstring & path, bool pushBack = true )     override;

        virtual wstring             FindShaderFile( const wstring & fileName )                                      override;

    private:
        void                        ClearCacheInternal( );          // will not lock m_cacheMutex - has to be done before calling!!
        void                        LoadCacheInternal( );           // will not lock m_cacheMutex - has to be done before calling!!
        void                        SaveCacheInternal( ) const;     // will not lock m_cacheMutex - has to be done before calling!!
    };

    inline vaShader::State          vaShaderDX12::GetShader( ComPtr<ID3DBlob> & outBlob, int64 & outUniqueContentsID )
    { 
        std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex, std::try_to_lock ); 
        if (!allShaderDataLock.owns_lock())     // don't block, don't wait
        {
            outBlob             = nullptr;
            outUniqueContentsID = -1;
            return vaShader::State::Uncooked;   // if there's a lock on it then it's either being compiled (most likely) or reset or similar - the end result is the same, shader is potentially valid and will probably be available later
        }
        outBlob             = m_shaderBlob; 
        outUniqueContentsID = m_uniqueContentsID; 
        return m_state; 
    }

    inline vaShader::State          vaVertexShaderDX12::GetShader( ComPtr<ID3DBlob> & outBlob, shared_ptr< vector< D3D12_INPUT_ELEMENT_DESC > > & outInputLayout, int64 & outUniqueContentsID )
    { 
        std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex, std::try_to_lock ); 
        if (!allShaderDataLock.owns_lock())     // don't block, don't wait
        {
            outBlob             = nullptr;
            outUniqueContentsID = -1;
            outInputLayout      = nullptr;
            return vaShader::State::Uncooked;   // if there's a lock on it then it's either being compiled (most likely) or reset or similar - the end result is the same, shader is potentially valid and will probably be available later
        }
        outBlob             = m_shaderBlob; 
        outUniqueContentsID = m_uniqueContentsID; 
        outInputLayout      = m_inputLayoutDX12;
        return m_state; 
    }

    inline vaVertexShaderDX12 &     AsDX12( vaVertexShader & shader )       { return *shader.SafeCast<vaVertexShaderDX12*>(); }
    inline vaVertexShaderDX12 *     AsDX12( vaVertexShader * shader )       { return shader->SafeCast<vaVertexShaderDX12*>(); }
    
    inline vaPixelShaderDX12 &      AsDX12( vaPixelShader & shader )        { return *shader.SafeCast<vaPixelShaderDX12*>(); }
    inline vaPixelShaderDX12 *      AsDX12( vaPixelShader * shader )        { return shader->SafeCast<vaPixelShaderDX12*>(); }
    
    inline vaGeometryShaderDX12 &   AsDX12( vaGeometryShader & shader )     { return *shader.SafeCast<vaGeometryShaderDX12*>(); }
    inline vaGeometryShaderDX12 *   AsDX12( vaGeometryShader * shader )     { return shader->SafeCast<vaGeometryShaderDX12*>(); }
    
    inline vaDomainShaderDX12 &     AsDX12( vaDomainShader & shader )       { return *shader.SafeCast<vaDomainShaderDX12*>(); }
    inline vaDomainShaderDX12 *     AsDX12( vaDomainShader * shader )       { return shader->SafeCast<vaDomainShaderDX12*>(); }
    
    inline vaHullShaderDX12 &       AsDX12( vaHullShader & shader )         { return *shader.SafeCast<vaHullShaderDX12*>(); }
    inline vaHullShaderDX12 *       AsDX12( vaHullShader * shader )         { return shader->SafeCast<vaHullShaderDX12*>(); }

    inline vaComputeShaderDX12 &    AsDX12( vaComputeShader & shader )      { return *shader.SafeCast<vaComputeShaderDX12*>(); }
    inline vaComputeShaderDX12 *    AsDX12( vaComputeShader * shader )      { return shader->SafeCast<vaComputeShaderDX12*>(); }
   
}