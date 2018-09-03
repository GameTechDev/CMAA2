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

#include "vaDirectXTools.h"

#include <assert.h>

#include "sys/stat.h"

#include <D3DCompiler.h>

#include <sstream>

#include <inttypes.h>

#include "vaShaderDX11.h"

#include "vaRenderingToolsDX11.h"

#include "Rendering/DirectX/vaRenderDeviceDX11.h"

#include "Rendering/DirectX/vaRenderDeviceContextDX11.h"

#include "Core/Misc/vaCRC64.h"

#include "Core/System/vaFileTools.h"


using namespace VertexAsylum;

#ifdef NDEBUG
#define           STILL_LOAD_FROM_CACHE_IF_ORIGINAL_FILE_MISSING
#endif

using namespace std;

namespace VertexAsylum
{

    const D3D_SHADER_MACRO c_builtInMacros[] = { { "VA_COMPILED_AS_SHADER_CODE", "1" }, { 0, 0 } };

    // this is used to make the error reporting report to correct file (or embedded storage)
    string CorrectErrorIfNotFullPath( const string & errorText )
    {
        string ret;

        string codeStr( errorText );

        stringstream ss( codeStr );
        string line;
        //int lineNum = 1;
        while( std::getline( ss, line ) )
        {
            size_t fileSeparator = line.find( ':' );
            if( fileSeparator != string::npos )
            {
                string filePlusLinePart = line.substr( 0, fileSeparator );
                string errorPart = line.substr( fileSeparator );

                size_t lineInfoSeparator = filePlusLinePart.find( '(' );
                if( lineInfoSeparator != string::npos )
                {
                    string filePart = filePlusLinePart.substr( 0, lineInfoSeparator );
                    string lineInfoPart = filePlusLinePart.substr( lineInfoSeparator );

                    wstring fileName = vaStringTools::SimpleWiden( filePart );

                    // First try the file system...
                    wstring fullFileName = vaDirectX11ShaderManager::GetInstance( ).FindShaderFile( fileName.c_str( ) );
                    if( fullFileName != L"" )
                    {
                        filePart = vaStringTools::SimpleNarrow( fullFileName );
                    }
                    else
                    {
                        // ...then try embedded storage
                        vaFileTools::EmbeddedFileData embeddedData = vaFileTools::EmbeddedFilesFind( wstring( L"shaders:\\" ) + fileName );
                        if( embeddedData.HasContents( ) )
                            filePart = vaStringTools::SimpleNarrow( wstring( L"shaders:\\" ) + fileName );
                    }

                    line = filePart + lineInfoPart + errorPart;
                }
            }
            ret += line + "\n";
        }
        return ret;
    }

    class vaShaderIncludeHelper : public ID3DInclude
    {
        std::vector<vaShaderCacheEntry::FileDependencyInfo> &       m_dependenciesCollector;

        wstring                                                     m_relativePath;

        string                                                      m_macrosAsIncludeFile;

        vaShaderIncludeHelper( const vaShaderIncludeHelper & c ) = delete; //: m_dependenciesCollector( c.m_dependenciesCollector ), m_relativePath( c.m_relativePath ), m_macros( c.m_macros )
        //{
        //    assert( false );
        //}    // to prevent warnings (and also this object doesn't support copying/assignment)
        void operator = ( const vaShaderIncludeHelper & ) = delete;
        //{
        //    assert( false );
        //}    // to prevent warnings (and also this object doesn't support copying/assignment)

    public:
        vaShaderIncludeHelper( std::vector<vaShaderCacheEntry::FileDependencyInfo> & dependenciesCollector, const wstring & relativePath, const string & macrosAsIncludeFile ) : m_dependenciesCollector( dependenciesCollector ), m_relativePath( relativePath ), m_macrosAsIncludeFile( macrosAsIncludeFile )
        {
        }

        STDMETHOD( Open )( D3D10_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes )
        {
            IncludeType; // unreferenced
            pParentData; // unreferenced

            // special case to handle macros - no need to add dependencies here, macros are CRC-ed separately
            if( pFileName == string( "MagicMacrosMagicFile.h" ) || pFileName == string( "magicmacrosmagicfile.h" ) )
            {
                int length = (int)m_macrosAsIncludeFile.size( );
                void * pData = new char[length];
                memcpy( pData, m_macrosAsIncludeFile.c_str( ), length );
                *ppData = pData;
                *pBytes = length;
                return S_OK;
            }

            vaShaderCacheEntry::FileDependencyInfo fileDependencyInfo;
            std::shared_ptr<vaMemoryStream>        memBuffer;

            wstring fileName = m_relativePath + vaStringTools::SimpleWiden( string( pFileName ) );

            // First try the file system...
            wstring fullFileName = vaDirectX11ShaderManager::GetInstance( ).FindShaderFile( fileName.c_str( ) );
            if( fullFileName != L"" )
            {
                fileDependencyInfo = vaShaderCacheEntry::FileDependencyInfo( fileName.c_str( ) );
                memBuffer = vaFileTools::LoadFileToMemoryStream( fullFileName.c_str( ) );
            }
            else
            {
                // ...then try embedded storage
                vaFileTools::EmbeddedFileData embeddedData = vaFileTools::EmbeddedFilesFind( wstring( L"shaders:\\" ) + fileName );

                if( !embeddedData.HasContents( ) )
                {
                    VA_ERROR( L"Error trying to find shader file '%s'!", fileName.c_str( ) );
                    return E_FAIL;
                }
                fileDependencyInfo = vaShaderCacheEntry::FileDependencyInfo( fileName.c_str( ), embeddedData.TimeStamp );
                memBuffer = embeddedData.MemStream;
            }

            m_dependenciesCollector.push_back( fileDependencyInfo );

            int length = (int)memBuffer->GetLength( );
            void * pData = new char[length];
            memcpy( pData, memBuffer->GetBuffer( ), length );
            *ppData = pData;
            *pBytes = length;
            return S_OK;
        }
        STDMETHOD( Close )( LPCVOID pData )
        {
            delete[] pData;
            return S_OK;
        }
    };

    vaShaderDX11::vaShaderDX11( )
    {
        assert( vaThreading::IsMainThread() );  // creation only supported from main thread for now
        m_shader = nullptr;

#ifdef VA_HOLD_SHADER_DISASM
        m_disasmAutoDumpToFile = false;
#endif
    }
    //
    vaShaderDX11::~vaShaderDX11( )
    {
        assert( vaDirectX11ShaderManager::GetInstancePtr() != nullptr ); // this should have been created by now - if not, see why
        assert( vaThreading::IsMainThread() );  // creation only supported from main thread for now
        vaBackgroundTaskManager::GetInstance().WaitUntilFinished( m_backgroundCreationTask );

        std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex );
        DestroyShaderBase( );
    }
    //
    void vaShaderDX11::Clear( )
    {
        m_state = vaShader::State::Empty;
        assert( vaThreading::IsMainThread() );  // creation/cleaning only supported from main thread for now
        vaBackgroundTaskManager::GetInstance().WaitUntilFinished( m_backgroundCreationTask );

        std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex ); 
        DestroyShader( );
        m_shader            = nullptr;
        m_entryPoint        = "";
        m_shaderFilePath    = L"";
        m_shaderCode        = "";
        m_shaderModel       = "";
#ifdef VA_HOLD_SHADER_DISASM
        m_disasm            = "";
#endif
    }

    void vaShaderDX11::DestroyShaderBase( )
    {
        m_allShaderDataMutex.assert_locked_by_caller();

        m_lastLoadedFromCache = false;
        SAFE_RELEASE( m_shader );
        if( m_state != State::Empty )
            m_state             = State::Uncooked;
    }
    //
    void vaShaderDX11::CreateCacheKey( vaShaderCacheKey & outKey )
    {
        m_allShaderDataMutex.assert_locked_by_caller();

        // supposed to be empty!
        assert( outKey.StringPart.size( ) == 0 );
        outKey.StringPart.clear( );

        outKey.StringPart += vaStringTools::Format( "%d", (int)m_macros.size( ) ) + " ";
        for( uint32 i = 0; i < m_macros.size( ); i++ )
        {
            outKey.StringPart += m_macros[i].first + " ";
            outKey.StringPart += m_macros[i].second + " ";
        }
        outKey.StringPart += m_shaderModel + " ";
        outKey.StringPart += m_entryPoint + " ";
        outKey.StringPart += vaStringTools::ToLower( vaStringTools::SimpleNarrow( m_shaderFilePath ) ) + " ";
    }
    //
    ID3D11PixelShader * vaPixelShaderDX11::GetShader( )
    {
        std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex, std::try_to_lock ); 
        if( !allShaderDataLock.owns_lock() || m_shader == nullptr )
            return nullptr;

        HRESULT hr;
        ID3D11PixelShader * ret;
        V( m_shader->QueryInterface( IID_ID3D11PixelShader, (void**)&ret ) );
        ret->Release( );

        assert( ret != nullptr );
        return ret;
    }
    //
    ID3D11ComputeShader * vaComputeShaderDX11::GetShader( )
    {
        std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex, std::try_to_lock ); 
        if( !allShaderDataLock.owns_lock() || m_shader == nullptr )
            return nullptr;

        HRESULT hr;
        ID3D11ComputeShader * ret;
        V( m_shader->QueryInterface( IID_ID3D11ComputeShader, (void**)&ret ) );
        ret->Release( );

        assert( ret != nullptr );
        return ret;
    }
    //
    ID3D11VertexShader * vaVertexShaderDX11::GetShader( )
    {
        std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex, std::try_to_lock ); 
        if( !allShaderDataLock.owns_lock() || m_shader == nullptr )
            return nullptr;

        HRESULT hr;
        ID3D11VertexShader * ret;
        V( m_shader->QueryInterface( IID_ID3D11VertexShader, (void**)&ret ) );
        ret->Release( );

        assert( ret != nullptr );
        return ret;
    }
    //
    ID3D11InputLayout * vaVertexShaderDX11::GetInputLayout( ) 
    { 
        std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex, std::try_to_lock ); 
        if( !allShaderDataLock.owns_lock() || m_shader == nullptr )
            return nullptr;

        return m_inputLayoutDX; 
    }
    //
    ID3D11HullShader * vaHullShaderDX11::GetShader( )
    {
        std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex, std::try_to_lock ); 
        if( !allShaderDataLock.owns_lock() || m_shader == nullptr )
            return nullptr;

        HRESULT hr;
        ID3D11HullShader * ret;
        V( m_shader->QueryInterface( IID_ID3D11HullShader, (void**)&ret ) );
        ret->Release( );

        assert( ret != nullptr );
        return ret;
    }
    //
    ID3D11DomainShader * vaDomainShaderDX11::GetShader( )
    {
        std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex, std::try_to_lock ); 
        if( !allShaderDataLock.owns_lock() || m_shader == nullptr )
            return nullptr;

        HRESULT hr;
        ID3D11DomainShader * ret;
        V( m_shader->QueryInterface( IID_ID3D11DomainShader, (void**)&ret ) );
        ret->Release( );

        assert( ret != nullptr );
        return ret;
    }
    //
    ID3D11GeometryShader * vaGeometryShaderDX11::GetShader( )
    {
        std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex, std::try_to_lock ); 
        if( !allShaderDataLock.owns_lock() || m_shader == nullptr )
            return nullptr;

        HRESULT hr;
        ID3D11GeometryShader * ret;
        V( m_shader->QueryInterface( IID_ID3D11GeometryShader, (void**)&ret ) );
        ret->Release( );

        assert( ret != nullptr );
        return ret;
    }
    //
    void vaVertexShaderDX11::CreateCacheKey( vaShaderCacheKey & outKey )
    {
        m_allShaderDataMutex.assert_locked_by_caller();

        vaShaderDX11::CreateCacheKey( outKey );
        
        outKey.StringPart += m_inputLayout.GetHashString();
    }
    //
    static HRESULT CompileShaderFromFile( const wchar_t* szFileName, const string & macrosAsIncludeFile, LPCSTR szEntryPoint,
        LPCSTR szShaderModel, ID3DBlob** ppBlobOut, vector<vaShaderCacheEntry::FileDependencyInfo> & outDependencies, HWND hwnd )
    {
        HRESULT hr = S_OK;


        DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
//#if defined( DEBUG ) || defined( _DEBUG )
        // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
        // Setting this flag improves the shader debugging experience, but still allows 
        // the shaders to be optimized and to run exactly the way they will run in 
        // the release configuration of this program.
        dwShaderFlags |= D3DCOMPILE_DEBUG;
//#endif
        dwShaderFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;

        if( vaDirectX11ShaderManager::GetInstance( ).Settings( ).WarningsAreErrors )
            dwShaderFlags |= D3DCOMPILE_WARNINGS_ARE_ERRORS;

        // find the file
        wstring fullFileName = vaDirectX11ShaderManager::GetInstance( ).FindShaderFile( szFileName );

        if( fullFileName != L"" )
        {
            bool attemptReload;

            do
            {
                outDependencies.clear( );
                outDependencies.push_back( vaShaderCacheEntry::FileDependencyInfo( szFileName ) );

                wstring relativePath;
                vaFileTools::SplitPath( szFileName, &relativePath, nullptr, nullptr );

                vaShaderIncludeHelper includeHelper( outDependencies, relativePath, macrosAsIncludeFile );

                attemptReload = false;
                ID3DBlob* pErrorBlob;
                hr = D3DCompileFromFile( fullFileName.c_str( ), c_builtInMacros, (ID3DInclude*)&includeHelper,
                    szEntryPoint, szShaderModel, dwShaderFlags, 0, ppBlobOut, &pErrorBlob );
                if( FAILED( hr ) )
                {
                    if( pErrorBlob != nullptr )
                    {
                        wstring absFileName = vaFileTools::GetAbsolutePath( fullFileName );

                        OutputDebugStringA( vaStringTools::Format( "\nError while compiling '%s' shader, SM: '%s', EntryPoint: '%s' :\n", vaStringTools::SimpleNarrow( absFileName ).c_str( ), szShaderModel, szEntryPoint ).c_str( ) );
                        OutputDebugStringA( CorrectErrorIfNotFullPath( (char*)pErrorBlob->GetBufferPointer( ) ).c_str( ) );
#if 1 || defined( _DEBUG ) // display always for now
                        wstring errorMsg = vaStringTools::SimpleWiden( (char*)pErrorBlob->GetBufferPointer( ) );
                        wstring shmStr = vaStringTools::SimpleWiden( szShaderModel );
                        wstring entryStr = vaStringTools::SimpleWiden( szEntryPoint );
                        wstring fullMessage = vaStringTools::Format( L"Error while compiling '%s' shader, SM: '%s', EntryPoint: '%s'."
                            L"\n\n---\n%s\n---\nAttempt reload?",
                            fullFileName.c_str( ), shmStr.c_str( ), entryStr.c_str( ), errorMsg.c_str( ) );
                        if( MessageBox( hwnd, fullMessage.c_str( ), L"Shader compile error", MB_YESNO | MB_SETFOREGROUND ) == IDYES )
                            attemptReload = true;
#endif
                    }
                    SAFE_RELEASE( pErrorBlob );
                    if( !attemptReload )
                        return hr;
                }
                SAFE_RELEASE( pErrorBlob );
            } while( attemptReload );
        }
        else
        {
            // ...then try embedded storage
            vaFileTools::EmbeddedFileData embeddedData = vaFileTools::EmbeddedFilesFind( wstring( L"shaders:\\" ) + wstring( szFileName ) );

            if( !embeddedData.HasContents( ) )
            {
                VA_ERROR( L"Error trying to find shader file '%s'!", szFileName );
                return E_FAIL;
            }

            vaShaderCacheEntry::FileDependencyInfo fileDependencyInfo = vaShaderCacheEntry::FileDependencyInfo( szFileName, embeddedData.TimeStamp );

            outDependencies.clear( );
            outDependencies.push_back( fileDependencyInfo );

            wstring relativePath;
            vaFileTools::SplitPath( szFileName, &relativePath, nullptr, nullptr );

            vaShaderIncludeHelper includeHelper( outDependencies, relativePath, macrosAsIncludeFile );

            ID3DBlob* pErrorBlob;
            string ansiName = vaStringTools::SimpleNarrow( embeddedData.Name );
            hr = D3DCompile( (LPCSTR)embeddedData.MemStream->GetBuffer( ), (SIZE_T)embeddedData.MemStream->GetLength( ), ansiName.c_str( ), c_builtInMacros, (ID3DInclude*)&includeHelper,
                szEntryPoint, szShaderModel, dwShaderFlags, 0, ppBlobOut, &pErrorBlob );
            if( FAILED( hr ) )
            {
                if( pErrorBlob != nullptr )
                {
                    OutputDebugStringA( CorrectErrorIfNotFullPath( (char*)pErrorBlob->GetBufferPointer( ) ).c_str( ) );
#if 1 || defined( _DEBUG ) // display always for now
                    wstring errorMsg = vaStringTools::SimpleWiden( (char*)pErrorBlob->GetBufferPointer( ) );
                    wstring shmStr = vaStringTools::SimpleWiden( szShaderModel );
                    wstring entryStr = vaStringTools::SimpleWiden( szEntryPoint );
                    wstring fullMessage = vaStringTools::Format( L"Error while compiling '%s' shader, SM: '%s', EntryPoint: '%s'."
                        L"\n\n---\n%s\n---",
                        fullFileName.c_str( ), shmStr.c_str( ), entryStr.c_str( ), errorMsg.c_str( ) );
                    MessageBox( hwnd, fullMessage.c_str( ), L"Shader compile error", MB_OK | MB_SETFOREGROUND );
#endif
                }
                SAFE_RELEASE( pErrorBlob );
                return hr;
            }
            SAFE_RELEASE( pErrorBlob );
        }

        return S_OK;
    }
    //
    static HRESULT CompileShaderFromBuffer( const string & shaderCode, string & macrosAsIncludeFile, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, HWND hwnd )
    {
        HRESULT hr = S_OK;

        DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
        // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
        // Setting this flag improves the shader debugging experience, but still allows 
        // the shaders to be optimized and to run exactly the way they will run in 
        // the release configuration of this program.
        dwShaderFlags |= D3D10_SHADER_DEBUG;
#endif

        string combinedShaderCode = macrosAsIncludeFile;
        if( macrosAsIncludeFile.size() > 0 ) 
            combinedShaderCode +="\n";
        combinedShaderCode += shaderCode;

        ID3DBlob* pErrorBlob;
        hr = D3DCompile( combinedShaderCode.c_str( ), combinedShaderCode.size( ), nullptr, c_builtInMacros, nullptr, szEntryPoint, szShaderModel,
            dwShaderFlags, 0, ppBlobOut, &pErrorBlob );
        if( FAILED( hr ) )
        {
            if( pErrorBlob != nullptr )
            {
                OutputDebugStringA( CorrectErrorIfNotFullPath( (char*)pErrorBlob->GetBufferPointer( ) ).c_str( ) );
#if 1 || defined( _DEBUG ) // display always for now
                //wstring errorMsg = vaStringTools::SimpleWiden( (char*)pErrorBlob->GetBufferPointer( ) );
                string fullMessage = vaStringTools::Format( "Error while compiling '%s' shader, SM: '%s', EntryPoint: '%s'. \n\n---\n%s\n---",
                    "<from_buffer>", szShaderModel, szEntryPoint, pErrorBlob->GetBufferPointer( ) );
                MessageBoxA( hwnd, fullMessage.c_str( ), "Shader compile error", MB_OK | MB_SETFOREGROUND );
#endif
            }
            SAFE_RELEASE( pErrorBlob );
            return hr;
        }
        SAFE_RELEASE( pErrorBlob );
        return S_OK;
    }
    //
    ID3DBlob * vaShaderDX11::CreateShaderBase( bool & loadedFromCache )
    {
        m_allShaderDataMutex.assert_locked_by_caller();

        ID3DBlob * shaderBlob = nullptr;
        loadedFromCache = false;

        ID3D11Device * device = GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice();
        if( device == nullptr )
            return nullptr;
        if( ( m_shaderFilePath.size( ) == 0 ) && ( m_shaderCode.size( ) == 0 ) )
            return nullptr; // still no path or code set provided

        string macrosAsIncludeFile;
        GetMacrosAsIncludeFile( macrosAsIncludeFile );

        if( m_shaderFilePath.size( ) != 0 )
        {
            vaShaderCacheKey cacheKey;
            CreateCacheKey( cacheKey );

#ifdef VA_SHADER_SHADER_CACHE_PERSISTENT_STORAGE_ENABLE
            bool foundButModified;
            shaderBlob = vaDirectX11ShaderManager::GetInstance( ).FindInCache( cacheKey, foundButModified );

            if( shaderBlob == nullptr )
            {
                wstring entryw = vaStringTools::SimpleWiden( m_entryPoint );
                wstring shadermodelw = vaStringTools::SimpleWiden( m_shaderModel );

                if( foundButModified )
                    vaLog::GetInstance( ).Add( LOG_COLORS_SHADERS, L" > file '%s' for '%s', entry '%s', found in cache but modified; recompiling...", m_shaderFilePath.c_str( ), shadermodelw.c_str( ), entryw.c_str( ) );
                else
                    vaLog::GetInstance( ).Add( LOG_COLORS_SHADERS, L" > file '%s' for '%s', entry '%s', not found in cache; recompiling...", m_shaderFilePath.c_str( ), shadermodelw.c_str( ), entryw.c_str( ) );
            }
#endif

            loadedFromCache = shaderBlob != nullptr;

            if( shaderBlob == nullptr )
            {
                vector<vaShaderCacheEntry::FileDependencyInfo> dependencies;

                CompileShaderFromFile( m_shaderFilePath.c_str( ), macrosAsIncludeFile, m_entryPoint.c_str( ), m_shaderModel.c_str( ), &shaderBlob, dependencies, GetRenderDevice().SafeCast<vaRenderDeviceDX11*>()->GetHWND( ) );

                if( shaderBlob != nullptr )
                {
                    vaDirectX11ShaderManager::GetInstance( ).AddToCache( cacheKey, shaderBlob, dependencies );
                }
            }
        }
        else if( m_shaderCode.size( ) != 0 )
        {
            CompileShaderFromBuffer( m_shaderCode, macrosAsIncludeFile, m_entryPoint.c_str( ), m_shaderModel.c_str( ), &shaderBlob, GetRenderDevice().SafeCast<vaRenderDeviceDX11*>()->GetHWND( ) );
        }
        else
        {
            assert( false );
        }


#ifdef VA_HOLD_SHADER_DISASM
        if( shaderBlob != nullptr )
        {
            ID3DBlob * disasmBlob = nullptr;

            HRESULT hr = D3DDisassemble( shaderBlob->GetBufferPointer( ), shaderBlob->GetBufferSize( ), D3D_DISASM_ENABLE_INSTRUCTION_NUMBERING, nullptr, &disasmBlob );
            assert( SUCCEEDED( hr ) ); hr;

            m_disasm = "";
            if( ( disasmBlob != nullptr ) && ( disasmBlob->GetBufferPointer( ) != nullptr ) )
            {
                m_disasm = (const char *)disasmBlob->GetBufferPointer( );

                if( m_disasmAutoDumpToFile )
                {
                    wstring fileName = vaCore::GetWorkingDirectory( );

                    vaShaderCacheKey cacheKey;
                    CreateCacheKey( cacheKey );
                    vaCRC64 crc;
                    crc.AddString( cacheKey.StringPart );

                    fileName += L"shaderdump_" + vaStringTools::SimpleWiden( m_entryPoint ) + L"_" + vaStringTools::SimpleWiden( m_shaderModel ) /*+ L"_" + vaStringTools::SimpleWiden( vaStringTools::Format( "0x%" PRIx64, crc.GetCurrent() ) )*/ + L".txt";

                    vaStringTools::WriteTextFile( fileName, m_disasm );
                }

            }

            SAFE_RELEASE( disasmBlob );
        }
#endif

        return shaderBlob;
    }
    //
    void vaPixelShaderDX11::CreateShader( )
    {
        m_allShaderDataMutex.assert_locked_by_caller();

        HRESULT hr;
        ID3DBlob * shaderBlob = CreateShaderBase( m_lastLoadedFromCache );
        if( shaderBlob == nullptr )
            return;

        ID3D11PixelShader * shader = nullptr;

        hr = GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice()->CreatePixelShader( shaderBlob->GetBufferPointer( ), shaderBlob->GetBufferSize( ), nullptr, &shader );
        assert( SUCCEEDED( hr ) );
        if( SUCCEEDED( hr ) )
        {
            V( shader->QueryInterface( IID_IUnknown, (void**)&m_shader ) );
            shader->Release( );
            GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->NameObject( shader, "vaPixelShaderDX11 shader object" );
        }
        SAFE_RELEASE( shaderBlob );
        m_state             = State::Cooked;
    }
    //
    void vaComputeShaderDX11::CreateShader( )
    {
        m_allShaderDataMutex.assert_locked_by_caller();

        HRESULT hr;
        ID3DBlob * shaderBlob = CreateShaderBase( m_lastLoadedFromCache );
        if( shaderBlob == nullptr )
            return;

        ID3D11ComputeShader * shader = nullptr;
        hr = GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice()->CreateComputeShader( shaderBlob->GetBufferPointer( ), shaderBlob->GetBufferSize( ), nullptr, &shader );
        assert( SUCCEEDED( hr ) );
        if( SUCCEEDED( hr ) )
        {
            V( shader->QueryInterface( IID_IUnknown, (void**)&m_shader ) );
            shader->Release( );
            GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->NameObject( shader, "vaComputeShaderDX11 shader object" );
        }
        SAFE_RELEASE( shaderBlob );
        m_state             = State::Cooked;
    }
    //
    vaVertexShaderDX11::vaVertexShaderDX11( const vaRenderingModuleParams & params ) : vaShader( params ) 
    {
        m_inputLayoutDX = nullptr;
    }
    //
    vaVertexShaderDX11::~vaVertexShaderDX11( )
    {
        vaBackgroundTaskManager::GetInstance().WaitUntilFinished( m_backgroundCreationTask );

        SAFE_RELEASE( m_inputLayoutDX );
    }
    //
    void vaVertexShaderDX11::SetInputLayout( D3D11_INPUT_ELEMENT_DESC * elements, uint32 elementCount )
    {
        std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex ); 

        m_inputLayout.Clear( ); // <- why?
        m_inputLayout = LayoutVAFromDX( elements, elementCount );
    }
    //
    void vaVertexShaderDX11::CreateShaderAndILFromFile( const wstring & filePath, const string & shaderModel, const string & entryPoint, const vector<vaVertexInputElementDesc> & inputLayoutElements, const vaShaderMacroContaner & macros, bool forceImmediateCompile )
    {
        assert( vaDirectX11ShaderManager::GetInstancePtr() != nullptr ); // this should have been created by now - if not, see why
        assert( vaThreading::IsMainThread() );  // creation only supported from main thread for now
        vaBackgroundTaskManager::GetInstance().WaitUntilFinished( m_backgroundCreationTask );

        {
            std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex ); 
            m_inputLayout = vaVertexInputLayoutDesc( inputLayoutElements );
        }

        vaShaderDX11::CreateShaderFromFile( filePath, shaderModel, entryPoint, macros, forceImmediateCompile );
    }
    //
    void vaVertexShaderDX11::CreateShaderAndILFromBuffer( const string & shaderCode, const string & shaderModel, const string & entryPoint, const vector<vaVertexInputElementDesc> & inputLayoutElements, const vaShaderMacroContaner & macros, bool forceImmediateCompile )
    {
        assert( vaDirectX11ShaderManager::GetInstancePtr() != nullptr ); // this should have been created by now - if not, see why
        assert( vaThreading::IsMainThread() );  // creation only supported from main thread for now
        vaBackgroundTaskManager::GetInstance().WaitUntilFinished( m_backgroundCreationTask );

        {
            std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex ); 
            m_inputLayout = vaVertexInputLayoutDesc( inputLayoutElements );
        }
        vaShaderDX11::CreateShaderFromBuffer( shaderCode, shaderModel, entryPoint, macros, forceImmediateCompile );
    }
    //
    void vaVertexShaderDX11::CreateShader( )
    {
        assert( vaDirectX11ShaderManager::GetInstancePtr() != nullptr ); // this should have been created by now - if not, see why
        m_allShaderDataMutex.assert_locked_by_caller();

        HRESULT hr;
        ID3DBlob * shaderBlob = nullptr;

        if( m_shader == nullptr )
        {
            shaderBlob = CreateShaderBase( m_lastLoadedFromCache );
            if( shaderBlob == nullptr )
                return;

            ID3D11VertexShader * shader = nullptr;
            hr = GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice()->CreateVertexShader( shaderBlob->GetBufferPointer( ), shaderBlob->GetBufferSize( ), nullptr, &shader );
            assert( SUCCEEDED( hr ) );
            if( SUCCEEDED( hr ) )
            {
                V( shader->QueryInterface( IID_IUnknown, (void**)&m_shader ) );
                shader->Release( );
                GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->NameObject( shader, "vaVertexShaderDX11 shader object" );
            }
        }

        if( ( m_inputLayoutDX == nullptr ) && ( m_inputLayout.ElementArray().size( ) > 0 ) && ( shaderBlob != nullptr ) )
        {
            vector<D3D11_INPUT_ELEMENT_DESC> layoutDesc;
            LayoutDXFromVA( layoutDesc, m_inputLayout );

            V( GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice()->CreateInputLayout( layoutDesc.data(), (UINT)layoutDesc.size( ),
                shaderBlob->GetBufferPointer( ), shaderBlob->GetBufferSize( ), &m_inputLayoutDX ) );

            GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->NameObject( m_inputLayoutDX, "vaVertexShaderDX11 input layout object" );
        }
        SAFE_RELEASE( shaderBlob );
        m_state             = State::Cooked;
    }
    //
    void vaHullShaderDX11::CreateShader( )
    {
        assert( vaDirectX11ShaderManager::GetInstancePtr() != nullptr ); // this should have been created by now - if not, see why
        m_allShaderDataMutex.assert_locked_by_caller();

        HRESULT hr;
        ID3DBlob * shaderBlob = CreateShaderBase( m_lastLoadedFromCache );
        if( shaderBlob == nullptr )
            return;

        ID3D11HullShader * shader = nullptr;
        hr = GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice()->CreateHullShader( shaderBlob->GetBufferPointer( ), shaderBlob->GetBufferSize( ), nullptr, &shader );
        assert( SUCCEEDED( hr ) );
        if( SUCCEEDED( hr ) )
        {
            V( shader->QueryInterface( IID_IUnknown, (void**)&m_shader ) );
            shader->Release( );
            GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->NameObject( shader, "vaHullShaderDX11 shader object" );
        }

        SAFE_RELEASE( shaderBlob );
        m_state             = State::Cooked;
    }
    //
    void vaDomainShaderDX11::CreateShader( )
    {
        assert( vaDirectX11ShaderManager::GetInstancePtr() != nullptr ); // this should have been created by now - if not, see why
        m_allShaderDataMutex.assert_locked_by_caller();

        HRESULT hr;
        ID3DBlob * shaderBlob = CreateShaderBase( m_lastLoadedFromCache );
        if( shaderBlob == nullptr )
            return;

        ID3D11DomainShader * shader = nullptr;
        hr = GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice()->CreateDomainShader( shaderBlob->GetBufferPointer( ), shaderBlob->GetBufferSize( ), nullptr, &shader );
        assert( SUCCEEDED( hr ) );
        if( SUCCEEDED( hr ) )
        {
            V( shader->QueryInterface( IID_IUnknown, (void**)&m_shader ) );
            shader->Release( );
            GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->NameObject( shader, "vaDomainShaderDX11 shader object" );
        }

        SAFE_RELEASE( shaderBlob );
        m_state             = State::Cooked;
    }
    //
    void vaGeometryShaderDX11::CreateShader( )
    {
        assert( vaDirectX11ShaderManager::GetInstancePtr() != nullptr ); // this should have been created by now - if not, see why
        m_allShaderDataMutex.assert_locked_by_caller();

        HRESULT hr;
        ID3DBlob * shaderBlob = CreateShaderBase( m_lastLoadedFromCache );
        if( shaderBlob == nullptr )
            return;

        ID3D11GeometryShader * shader = nullptr;
        hr = GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice()->CreateGeometryShader( shaderBlob->GetBufferPointer( ), shaderBlob->GetBufferSize( ), nullptr, &shader );
        assert( SUCCEEDED( hr ) );
        if( SUCCEEDED( hr ) )
        {
            V( shader->QueryInterface( IID_IUnknown, (void**)&m_shader ) );
            shader->Release( );
            GetRenderDevice().SafeCast<vaRenderDeviceDX11*>( )->NameObject( shader, "vaGeometryShaderDX11 shader object" );
        }

        SAFE_RELEASE( shaderBlob );
        m_state             = State::Cooked;
    }
    //
    void vaVertexShaderDX11::DestroyShader( )
    {
        assert( vaDirectX11ShaderManager::GetInstancePtr() != nullptr ); // this should have been created by now - if not, see why
        m_allShaderDataMutex.assert_locked_by_caller();
        vaShaderDX11::DestroyShader( );

        SAFE_RELEASE( m_inputLayoutDX );
    }
    //
    vaVertexInputLayoutDesc vaVertexShaderDX11::LayoutVAFromDX( D3D11_INPUT_ELEMENT_DESC * elements, uint32 elementCount )
    {
        vector<vaVertexInputElementDesc> descArray;

        for( int i = 0; i < (int)elementCount; i++ )
        {
            const D3D11_INPUT_ELEMENT_DESC & src = elements[i];
            vaVertexInputElementDesc desc;
            desc.SemanticName           = src.SemanticName;
            desc.SemanticIndex          = src.SemanticIndex;
            desc.Format                 = (vaResourceFormat)src.Format;
            desc.InputSlot              = src.InputSlot;
            desc.AlignedByteOffset      = src.AlignedByteOffset;
            desc.InputSlotClass         = (vaVertexInputElementDesc::InputClassification)src.InputSlotClass;
            desc.InstanceDataStepRate   = src.InstanceDataStepRate;

            descArray.push_back( desc );
        }
        return vaVertexInputLayoutDesc(descArray);
    }
    //
    // !!warning!! returned char pointers in descriptors outLayout point to inLayout values so make sure you keep inLayout alive until outLayout is alive
    void vaVertexShaderDX11::LayoutDXFromVA( vector<D3D11_INPUT_ELEMENT_DESC> & outLayout, const vaVertexInputLayoutDesc & inLayout )
    {
        const vector<vaVertexInputElementDesc> & srcArray = inLayout.ElementArray( );

        assert( outLayout.size() == 0 );

        for( int i = 0; i < srcArray.size(); i++ )
        {
            const vaVertexInputElementDesc & src = srcArray[i];
            D3D11_INPUT_ELEMENT_DESC desc;
            
            // I hate doing this
            desc.SemanticName           = src.SemanticName.c_str();
            desc.SemanticIndex          = src.SemanticIndex;
            desc.Format                 = (DXGI_FORMAT)src.Format;
            desc.InputSlot              = src.InputSlot;
            desc.AlignedByteOffset      = src.AlignedByteOffset;
            desc.InputSlotClass         = ( D3D11_INPUT_CLASSIFICATION )src.InputSlotClass;
            desc.InstanceDataStepRate   = src.InstanceDataStepRate;

            outLayout.push_back( desc );
        }
    }
    //
    vaDirectX11ShaderManager::vaDirectX11ShaderManager( vaRenderDevice & device ) : vaShaderManager( device )
    {
        assert( vaThreading::IsMainThread() );
        {
#ifdef VA_SHADER_SHADER_CACHE_PERSISTENT_STORAGE_ENABLE
            // lock here just in case we put something after the loading task 
            std::unique_lock<mutex> cacheMutexLock( m_cacheMutex );

            // this should maybe be set externally, but good enough for now
            m_cacheFilePath = vaCore::GetExecutableDirectory( ) + L"cache\\";

            // Do we need per-adapter caches? probably not but who cares - different adapter >usually< means different machine so recaching anyway
#if defined( DEBUG ) || defined( _DEBUG )
        m_cacheFilePath += L"shaders_debug_" + GetRenderDevice().GetAdapterNameID( );
#else
        m_cacheFilePath += L"shaders_release_" + GetRenderDevice().GetAdapterNameID( );
#endif
        }

        LoadCacheInternal();
#endif // VA_SHADER_SHADER_CACHE_PERSISTENT_STORAGE_ENABLE
    }
    //
    //
    vaDirectX11ShaderManager::~vaDirectX11ShaderManager( )
    {
        assert( vaThreading::IsMainThread() );

#ifdef VA_SHADER_SHADER_CACHE_PERSISTENT_STORAGE_ENABLE
        {
            std::unique_lock<mutex> cacheMutexLock( m_cacheMutex );    // this is also needed to make sure loading finished!
            SaveCacheInternal( );
        }
#endif
        ClearCache( );

        // Ensure no shaders remain
        {
            std::unique_lock<mutex> shaderListLock( vaShader::GetAllShaderListMutex( ) );
            for( auto it : vaShader::GetAllShaderList( ) )
            {
                VA_LOG_ERROR( "Shader '%s' not unloaded", it->GetEntryPoint( ).c_str() );
            }
            assert( vaShader::GetAllShaderList().size( ) == 0 );
        }
    }
    //
    void vaDirectX11ShaderManager::RegisterShaderSearchPath( const std::wstring & path, bool pushBack )
    {
        wstring cleanedSearchPath = vaFileTools::CleanupPath( path + L"\\", false );
        if( pushBack )
            m_searchPaths.push_back( cleanedSearchPath );
        else
            m_searchPaths.push_front( cleanedSearchPath );
    }
    //
    wstring vaDirectX11ShaderManager::FindShaderFile( const wstring & fileName )
    {
        assert( m_searchPaths.size() > 0 ); // forgot to call RegisterShaderSearchPath?
        for( unsigned int i = 0; i < m_searchPaths.size( ); i++ )
        {
            std::wstring filePath = m_searchPaths[i] + L"\\" + fileName;
            if( vaFileTools::FileExists( filePath.c_str( ) ) )
            {
                return vaFileTools::CleanupPath( filePath, false );
            }
            if( vaFileTools::FileExists( ( vaCore::GetWorkingDirectory( ) + filePath ).c_str( ) ) )
            {
                return vaFileTools::CleanupPath( vaCore::GetWorkingDirectory( ) + filePath, false );
            }
        }

        if( vaFileTools::FileExists( fileName ) )
            return vaFileTools::CleanupPath( fileName, false );

        if( vaFileTools::FileExists( ( vaCore::GetWorkingDirectory( ) + fileName ).c_str( ) ) )
            return vaFileTools::CleanupPath( vaCore::GetWorkingDirectory( ) + fileName, false );

        return L"";
    }
    //
    void vaDirectX11ShaderManager::LoadCacheInternal( )
    {
        auto loadingLambda = [this]( vaBackgroundTaskManager::TaskContext & context ) 
        {
            // lock here just in case we put something after the loading task 
            std::unique_lock<mutex> cacheMutexLock( m_cacheMutex );

            {
                std::unique_lock<std::mutex> cacheLoadStartedMutexLock( m_cacheLoadStartedMutex );
                m_cacheLoadStarted = true;
            }
            m_cacheLoadStartedCV.notify_all();

            wstring fullFileName = m_cacheFilePath;

            if( fullFileName == L"" )
                return false;

            if( !vaFileTools::FileExists( fullFileName ) )
                return false;

            ClearCacheInternal( );

            vaFileStream inFile;
            if( inFile.Open( fullFileName.c_str( ), FileCreationMode::Open, FileAccessMode::Read ) )
            {
                int version = -1;
                inFile.ReadValue<int32>( version );

                assert( version == 0 );

                int32 entryCount = 0;
                inFile.ReadValue<int32>( entryCount );

                for( int i = 0; i < entryCount; i++ )
                {
                    context.Progress = float(i)/float(entryCount-1);

                    vaShaderCacheKey key;
                    key.Load( inFile );
                    vaShaderCacheEntry * entry = new vaShaderCacheEntry( inFile );

                    m_cache.insert( std::pair<vaShaderCacheKey, vaShaderCacheEntry *>( key, entry ) );
                }

                int32 terminator;
                inFile.ReadValue<int32>( terminator );
                assert( terminator == 0xFF );
            }
            return true;
        };

        bool multithreaded = true;
        if( multithreaded )
        {
            vaBackgroundTaskManager::GetInstance().Spawn( "Loading Shader Cache", vaBackgroundTaskManager::SpawnFlags::ShowInUI, loadingLambda );
            
            // make sure we've started and taken the thread
            {
                std::unique_lock<std::mutex> lk(m_cacheLoadStartedMutex);
                m_cacheLoadStartedCV.wait(lk, [this] { return (bool)m_cacheLoadStarted; } );
            }
        }
        else
        {
            loadingLambda( vaBackgroundTaskManager::TaskContext() );
        }
    }
    //
    void vaDirectX11ShaderManager::SaveCacheInternal( ) const
    {
        // must already be locked by this point!
        // std::unique_lock<mutex> cacheMutexLock( m_cacheMutex );
        m_cacheMutex.assert_locked_by_caller();

        wstring fullFileName = m_cacheFilePath;

        wstring cacheDir;
        vaFileTools::SplitPath( fullFileName, &cacheDir, nullptr, nullptr );
        vaFileTools::EnsureDirectoryExists( cacheDir.c_str( ) );

        vaFileStream outFile;
        outFile.Open( fullFileName.c_str( ), FileCreationMode::Create );

        outFile.WriteValue<int32>( 0 );                 // version;

        outFile.WriteValue<int32>( (int32)m_cache.size( ) );    // number of entries

        for( std::map<vaShaderCacheKey, vaShaderCacheEntry *>::const_iterator it = m_cache.cbegin( ); it != m_cache.cend( ); ++it )
        {
            // Save key
            ( *it ).first.Save( outFile );

            // Save data
            ( *it ).second->Save( outFile );
        }

        outFile.WriteValue<int32>( 0xFF );  // EOF;
    }
    //
    void vaDirectX11ShaderManager::ClearCacheInternal( )
    {
        m_cacheMutex.assert_locked_by_caller();
        for( std::map<vaShaderCacheKey, vaShaderCacheEntry *>::iterator it = m_cache.begin( ); it != m_cache.end( ); ++it )
        {
            delete ( *it ).second;;
        }
        m_cache.clear( );
    }
    //
    void vaDirectX11ShaderManager::ClearCache( )
    {
        std::unique_lock<mutex> cacheMutexLock( m_cacheMutex );
        ClearCacheInternal();
    }
    //
    ID3DBlob * vaDirectX11ShaderManager::FindInCache( vaShaderCacheKey & key, bool & foundButModified )
    {
        std::unique_lock<mutex> cacheMutexLock( m_cacheMutex );

        foundButModified = false;

        std::map<vaShaderCacheKey, vaShaderCacheEntry *>::iterator it = m_cache.find( key );

        if( it != m_cache.end( ) )
        {
            if( ( *it ).second->IsModified( ) )
            {
                foundButModified = true;

                // Have to recompile...
                delete ( *it ).second;
                m_cache.erase( it );

                return nullptr;
            }

            ID3DBlob * retBlob = ( *it ).second->GetCompiledShader( );
            return retBlob;
        }
        return nullptr;
    }
    //
    void vaDirectX11ShaderManager::AddToCache( vaShaderCacheKey & key, ID3DBlob * shaderBlob, std::vector<vaShaderCacheEntry::FileDependencyInfo> & dependencies )
    {
        std::unique_lock<mutex> cacheMutexLock( m_cacheMutex );

        std::map<vaShaderCacheKey, vaShaderCacheEntry *>::iterator it = m_cache.find( key );

        if( it != m_cache.end( ) )
        {
            // Already in? can happen with parallel compilation I guess?
            // well let's just not do anything then
            return;
            // delete ( *it ).second;
            // m_cache.erase( it );
        }

        m_cache.insert( std::pair<vaShaderCacheKey, vaShaderCacheEntry *>( key, new vaShaderCacheEntry( shaderBlob, dependencies ) ) );
    }
    //
    vaShaderCacheEntry::FileDependencyInfo::FileDependencyInfo( const wstring & filePath )
    {
        wstring fullFileName = vaDirectX11ShaderManager::GetInstance( ).FindShaderFile( filePath );

        if( fullFileName == L"" )
        {
            VA_ERROR( L"Error trying to find shader file '%s'!", filePath.c_str() );

            assert( false );
            this->FilePath = L"";
            this->ModifiedTimeDate = 0;
        }
        else
        {
            this->FilePath = filePath;

            // struct _stat64 fileInfo;
            // _wstat64( fullFileName.c_str( ), &fileInfo ); // check error code?
            //this->ModifiedTimeDate = fileInfo.st_mtime;

            // maybe add some CRC64 here too? that would require reading contents of every file and every dependency which would be costly! 
            WIN32_FILE_ATTRIBUTE_DATA attrInfo;
            ::GetFileAttributesEx( fullFileName.c_str( ), GetFileExInfoStandard, &attrInfo );
            this->ModifiedTimeDate = ( ( (int64)attrInfo.ftLastWriteTime.dwHighDateTime ) << 32 ) | ( (int64)attrInfo.ftLastWriteTime.dwLowDateTime );
        }
    }
    //
    vaShaderCacheEntry::FileDependencyInfo::FileDependencyInfo( const wstring & filePath, int64 modifiedTimeDate )
    {
        this->FilePath = filePath;
        this->ModifiedTimeDate = modifiedTimeDate;
    }
    //
    bool vaShaderCacheEntry::FileDependencyInfo::IsModified( )
    {
        wstring fullFileName = vaDirectX11ShaderManager::GetInstance( ).FindShaderFile( this->FilePath.c_str( ) );

        //vaLog::GetInstance( ).Add( LOG_COLORS_SHADERS, (L"vaShaderCacheEntry::FileDependencyInfo::IsModified, file name %s", fullFileName.c_str() ) );

        if( fullFileName == L"" )  // Can't find the file?
        {
            vaFileTools::EmbeddedFileData embeddedData = vaFileTools::EmbeddedFilesFind( wstring( L"shaders:\\" ) + this->FilePath );

            if( !embeddedData.HasContents( ) )
            {
                //vaLog::GetInstance( ).Add( LOG_COLORS_SHADERS, L"        > embedded data has NO contents" );
#ifdef STILL_LOAD_FROM_CACHE_IF_ORIGINAL_FILE_MISSING
                return false;
#else
                VA_ERROR( L"Error trying to find shader file '%s'!", this->FilePath.c_str( ) );
                return true;
#endif
            }
            else
            {
                //vaLog::GetInstance( ).Add( LOG_COLORS_SHADERS,  L"        > embedded data has contents" );
                return this->ModifiedTimeDate != embeddedData.TimeStamp;
            }
        }

        // struct _stat64 fileInfo;
        // _wstat64( fullFileName.c_str( ), &fileInfo ); // check error code?
        // return this->ModifiedTimeDate != fileInfo.st_mtime;

        // maybe add some CRC64 here too? that would require reading contents of every file and every dependency which would be costly! 
        WIN32_FILE_ATTRIBUTE_DATA attrInfo;
        ::GetFileAttributesEx( fullFileName.c_str( ), GetFileExInfoStandard, &attrInfo );
        bool ret = this->ModifiedTimeDate != ( ( ( (int64)attrInfo.ftLastWriteTime.dwHighDateTime ) << 32 ) | ( (int64)attrInfo.ftLastWriteTime.dwLowDateTime ) );

        // if( ret )
        // {
        //     vaLog::GetInstance( ).Add( LOG_COLORS_SHADERS,  (const wchar_t*)((ret)?(L"   > shader file '%s' modified "):(L"   > shader file '%s' NOT modified ")), fullFileName.c_str() );
        // }

        return ret;
    }
    //
    vaShaderCacheEntry::vaShaderCacheEntry( ID3DBlob * compiledShader, std::vector<vaShaderCacheEntry::FileDependencyInfo> & dependencies )
    {
        m_compiledShader = compiledShader;
        m_compiledShader->AddRef( );

        m_dependencies = dependencies;
    }
    //
    vaShaderCacheEntry::~vaShaderCacheEntry( )
    {
        m_compiledShader->Release( );
    }
    //
    bool vaShaderCacheEntry::IsModified( )
    {
        for( std::vector<vaShaderCacheEntry::FileDependencyInfo>::iterator it = m_dependencies.begin( ); it != m_dependencies.end( ); ++it )
        {
            if( ( *it ).IsModified( ) )
                return true;
        }
        return false;
    }
    //
    void vaShaderCacheKey::Save( vaStream & outStream ) const
    {
        outStream.WriteString( this->StringPart.c_str( ) );
    }
    //
    void vaShaderCacheEntry::FileDependencyInfo::Save( vaStream & outStream ) const
    {
        outStream.WriteString( this->FilePath.c_str( ) );
        outStream.WriteValue<int64>( this->ModifiedTimeDate );
    }
    //
    void vaShaderCacheEntry::Save( vaStream & outStream ) const
    {
        outStream.WriteValue<int32>( ( int32 )this->m_dependencies.size( ) );       // number of dependencies

        for( std::vector<vaShaderCacheEntry::FileDependencyInfo>::const_iterator it = m_dependencies.cbegin( ); it != m_dependencies.cend( ); ++it )
        {
            ( *it ).Save( outStream );
        }

        int bufferSize = (int)m_compiledShader->GetBufferSize( );

        outStream.WriteValue<int32>( bufferSize );
        outStream.Write( m_compiledShader->GetBufferPointer( ), bufferSize );
    }
    //
    void vaShaderCacheKey::Load( vaStream & inStream )
    {
        inStream.ReadString( this->StringPart );
    }
    //
    void vaShaderCacheEntry::FileDependencyInfo::Load( vaStream & inStream )
    {
        inStream.ReadString( this->FilePath );
        inStream.ReadValue<int64>( this->ModifiedTimeDate );
    }
    //
    void vaShaderCacheEntry::Load( vaStream & inStream )
    {
        int dependencyCount;
        inStream.ReadValue<int32>( dependencyCount );

        for( int i = 0; i < dependencyCount; i++ )
        {
            m_dependencies.push_back( vaShaderCacheEntry::FileDependencyInfo( inStream ) );
        }

        int bufferSize;
        inStream.ReadValue<int32>( bufferSize );

        D3DCreateBlob( bufferSize, &m_compiledShader );
        inStream.Read( m_compiledShader->GetBufferPointer( ), bufferSize );
    }
}




void RegisterShaderDX11( )
{
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaPixelShader,    vaPixelShaderDX11     );
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaComputeShader,  vaComputeShaderDX11   );
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaHullShader,     vaHullShaderDX11      );
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaDomainShader,   vaDomainShaderDX11    );
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaGeometryShader, vaGeometryShaderDX11  );
    VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaVertexShader,   vaVertexShaderDX11    );
}
