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

#include "vaShader.h"

using namespace VertexAsylum;

std::vector<vaShader *> vaShader::s_allShaderList;
mutex vaShader::s_allShaderListMutex;
std::atomic_int vaShader::s_activelyCompilingShaderCount = 0;

vaShader::vaShader( const vaRenderingModuleParams & params ) : vaRenderingModule( params )
{ 
    assert( vaThreading::IsMainThread() );
    m_lastLoadedFromCache = false;

    {
        std::unique_lock<mutex> shaderListLock( s_allShaderListMutex );
        s_allShaderList.push_back( this ); 
        m_allShaderListIndex = (int)s_allShaderList.size()-1;
    }
}

vaShader::~vaShader( ) 
{ 
    // you probably need to finish task in all child classes before destroying outputs
    assert( m_backgroundCreationTask == nullptr || vaBackgroundTaskManager::GetInstance().IsFinished(m_backgroundCreationTask) );

    assert( vaThreading::IsMainThread() );
    {
        std::unique_lock<mutex> shaderListLock( s_allShaderListMutex );
        assert( s_allShaderList[m_allShaderListIndex] == this );
        if( s_allShaderList.size( ) - 1 != m_allShaderListIndex )
        {
            int lastIndex = (int)s_allShaderList.size()-1;
            s_allShaderList[lastIndex]->m_allShaderListIndex = m_allShaderListIndex;
            std::swap( s_allShaderList[m_allShaderListIndex], s_allShaderList[lastIndex] );
        }
        s_allShaderList.pop_back( );

        if( s_allShaderList.size( ) == 0 )
        {
            s_allShaderList.shrink_to_fit();
        }
    }
}

//
void vaShader::CreateShaderFromFile( const wstring & filePath, const string & shaderModel, const string & entryPoint, const vector<pair<string, string>> & macros, bool forceImmediateCompile )
{
    assert( vaThreading::IsMainThread() );  // creation only supported from main thread for now
    vaBackgroundTaskManager::GetInstance().WaitUntilFinished( m_backgroundCreationTask );

    // clean the current contents
    Clear();

    // Set creation params
    {
        std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex ); 
        m_state                 = State::Uncooked;
        m_shaderCode            = "";
        m_shaderFilePath        = filePath;
        m_entryPoint            = entryPoint;
        m_shaderModel           = shaderModel;
        m_macros                = macros;
        m_forceImmediateCompile = forceImmediateCompile;
    }

    // increase the number BEFORE launching the threads (otherwise it is 0 while we're waiting for the compile thread to spawn)
    { /*std::unique_lock<mutex> shaderListLock( s_allShaderListMutex );*/ s_activelyCompilingShaderCount++; }

    auto shaderCompileLambda = [this]( vaBackgroundTaskManager::TaskContext & ) 
    {
        {
            std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex ); 
            CreateShader( ); 
        }
        { /*std::unique_lock<mutex> shaderListLock( s_allShaderListMutex );*/ s_activelyCompilingShaderCount--; }
        return true;   
    };

#ifndef VA_SHADER_SHADER_BACKGROUND_COMPILATION_ENABLE
    forceImmediateCompile = false;
#endif
    if( forceImmediateCompile )
        shaderCompileLambda( vaBackgroundTaskManager::TaskContext() );
    else
        vaBackgroundTaskManager::GetInstance().Spawn( m_backgroundCreationTask, vaStringTools::Format("Compiling shader %s %s %s", vaStringTools::SimpleNarrow(m_shaderFilePath).c_str(), m_entryPoint.c_str(), m_shaderModel.c_str() ).c_str(), 
            vaBackgroundTaskManager::SpawnFlags::UseThreadPool, shaderCompileLambda );
}

void vaShader::CreateShaderFromBuffer( const string & shaderCode, const string & shaderModel, const string & entryPoint, const vector<pair<string, string>> & macros, bool forceImmediateCompile )
{
    assert( vaThreading::IsMainThread() );  // creation only supported from main thread for now
    vaBackgroundTaskManager::GetInstance().WaitUntilFinished( m_backgroundCreationTask );

    // clean the current contents
    Clear();

    // Set creation params
    {
        std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex ); 
        m_state                 = State::Uncooked;
        m_shaderCode            = shaderCode;
        m_shaderFilePath        = L"";
        m_entryPoint            = entryPoint;
        m_shaderModel           = shaderModel;
        m_macros                = macros;
        m_forceImmediateCompile = forceImmediateCompile;
    }

    // increase the number BEFORE launching the threads (otherwise it is 0 while we're waiting for the compile thread to spawn)
    { /*std::unique_lock<mutex> shaderListLock( s_allShaderListMutex );*/ s_activelyCompilingShaderCount++; }

    auto shaderCompileLambda = [this]( vaBackgroundTaskManager::TaskContext & ) 
    {
        {
            std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex ); 
            CreateShader( ); 
        }
        { /*std::unique_lock<mutex> shaderListLock( s_allShaderListMutex );*/ s_activelyCompilingShaderCount--; }
        return true;   
    };

#ifndef VA_SHADER_SHADER_BACKGROUND_COMPILATION_ENABLE
    forceImmediateCompile = false;
#endif
    if( forceImmediateCompile )
        shaderCompileLambda( vaBackgroundTaskManager::TaskContext() );
    else
        vaBackgroundTaskManager::GetInstance().Spawn( m_backgroundCreationTask, vaStringTools::Format("Compiling shader %s %s %s", vaStringTools::SimpleNarrow(m_shaderFilePath).c_str(), m_entryPoint.c_str(), m_shaderModel.c_str() ).c_str(), 
            vaBackgroundTaskManager::SpawnFlags::UseThreadPool, shaderCompileLambda );
}
//
void vaShader::Reload( ) 
{ 
    assert( vaThreading::IsMainThread() );  // creation only supported from main thread for now
    vaBackgroundTaskManager::GetInstance().WaitUntilFinished( m_backgroundCreationTask );

    // increase the number BEFORE launching the threads (otherwise it is 0 while we're waiting for the compile thread to spawn)
    { /*std::unique_lock<mutex> shaderListLock( s_allShaderListMutex );*/ s_activelyCompilingShaderCount++; }
    bool forceImmediateCompile;
    { std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex ); forceImmediateCompile = m_forceImmediateCompile; }

    auto shaderCompileLambda = [this]( vaBackgroundTaskManager::TaskContext & ) 
    {
        {
            std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex ); 
            DestroyShader( );   // cooked -> uncooked
            CreateShader( ); 
        }
        { /*std::unique_lock<mutex> shaderListLock( s_allShaderListMutex );*/ s_activelyCompilingShaderCount--; }
        return true;   
    };

#ifndef VA_SHADER_SHADER_BACKGROUND_COMPILATION_ENABLE
    forceImmediateCompile = false;
#endif
    if( forceImmediateCompile )
        shaderCompileLambda( vaBackgroundTaskManager::TaskContext() );
    else
        vaBackgroundTaskManager::GetInstance().Spawn( m_backgroundCreationTask, vaStringTools::Format("Compiling shader %s %s %s", vaStringTools::SimpleNarrow(m_shaderFilePath).c_str(), m_entryPoint.c_str(), m_shaderModel.c_str() ).c_str(), 
            vaBackgroundTaskManager::SpawnFlags::UseThreadPool, shaderCompileLambda );
}
//
void vaShader::WaitFinishIfBackgroundCreateActive( )
{
    assert( vaThreading::IsMainThread() );  // creation only supported from main thread for now
    vaBackgroundTaskManager::GetInstance().WaitUntilFinished( m_backgroundCreationTask );
}

//
void vaShader::ReloadAll( )
{
    assert( vaThreading::IsMainThread() );  // only supported from main thread for now

#ifndef VA_SHADER_SHADER_BACKGROUND_COMPILATION_ENABLE
    vaLog::GetInstance( ).Add( LOG_COLORS_SHADERS, L"Recompiling shaders..." );
#else
    vaLog::GetInstance( ).Add( LOG_COLORS_SHADERS, L"Recompiling shaders (spawning multithreaded recompile)..." );
#endif

    {
        std::unique_lock<mutex> shaderListLock( GetAllShaderListMutex( ) );
#ifndef VA_SHADER_SHADER_BACKGROUND_COMPILATION_ENABLE
        int totalLoaded = (int)GetAllShaderList( ).size( );
        int totalLoadedFromCache = 0;
#endif
        for( size_t i = 0; i < GetAllShaderList( ).size( ); i++ )
        {
            GetAllShaderList( )[i]->Reload( );
#ifndef VA_SHADER_SHADER_BACKGROUND_COMPILATION_ENABLE
            if( GetAllShaderList( )[i]->IsLoadedFromCache( ) )
                totalLoadedFromCache++;
#endif
        }
#ifndef VA_SHADER_SHADER_BACKGROUND_COMPILATION_ENABLE
        vaLog::GetInstance( ).Add( LOG_COLORS_SHADERS, L"... %d shaders reloaded (%d from cache)", totalLoaded, totalLoadedFromCache );
#endif
    }
}

void vaShader::GetMacrosAsIncludeFile( string & outString )
{
    //std::unique_lock<mutex> allShaderDataLock( m_allShaderDataMutex ); 
    m_allShaderDataMutex.assert_locked_by_caller();

    outString.clear( );
    for( int i = 0; i < m_macros.size( ); i++ )
    {
        outString += "#define " + m_macros[i].first + " " + m_macros[i].second + "\n";
    }
}

vaShaderManager::vaShaderManager( vaRenderDevice & device )  : vaRenderingModule( vaRenderingModuleParams( device ) ) 
{ 
    auto progressInfoLambda = [this]( vaBackgroundTaskManager::TaskContext & context ) 
    {
        while( !context.ForceStop )
        {
            int numberOfCompilingShaders = vaShader::GetNumberOfCompilingShaders();
            if( numberOfCompilingShaders != 0 )
            {
                std::unique_lock<mutex> shaderListLock( vaShader::GetAllShaderListMutex(), std::try_to_lock );
                if( shaderListLock.owns_lock() )
                {
                    context.Progress = 1.0f - float(numberOfCompilingShaders) / float(vaShader::GetAllShaderList().size()-1);
                }
                context.HideInUI = false;
            }
            else
            {
                context.HideInUI = true;
            }
            vaThreading::Sleep(100);
        }
        return true;
    };
    vaBackgroundTaskManager::GetInstance().Spawn( m_backgroundShaderCompilationProgressIndicator, "Compiling shaders...", vaBackgroundTaskManager::SpawnFlags::ShowInUI, progressInfoLambda );
}
vaShaderManager::~vaShaderManager( )
{ 
    vaBackgroundTaskManager::GetInstance().MarkForStopping( m_backgroundShaderCompilationProgressIndicator );
    vaBackgroundTaskManager::GetInstance().WaitUntilFinished( m_backgroundShaderCompilationProgressIndicator );
}


