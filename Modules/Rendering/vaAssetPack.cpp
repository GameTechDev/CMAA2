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

#include "vaAssetPack.h"

#include "Rendering/vaRenderMesh.h"

#include "Scene/vaAssetImporter.h"

#include "Rendering/Misc/vaTextureReductionTestTool.h"

#include "Core/System/vaCompressionStream.h"

#include "Core/System/vaFileTools.h"

#include "IntegratedExternals/vaImguiIntegration.h"

using namespace VertexAsylum;

vaAssetTexture::vaAssetTexture( vaAssetPack & pack, const shared_ptr<vaTexture> & texture, const string & name ) 
    : vaAsset( pack, vaAssetType::Texture, name, texture )
{ }

vaAssetRenderMesh::vaAssetRenderMesh( vaAssetPack & pack, const shared_ptr<vaRenderMesh> & mesh, const string & name ) 
    : vaAsset( pack, vaAssetType::RenderMesh, name, mesh )
{ }

vaAssetRenderMaterial::vaAssetRenderMaterial( vaAssetPack & pack, const shared_ptr<vaRenderMaterial> & material, const string & name ) 
    : vaAsset( pack, vaAssetType::RenderMaterial, name, material )
{ }

vaAssetPack::vaAssetPack( vaAssetPackManager & assetPackManager, const string & name ) : m_assetPackManager(assetPackManager), m_name( name ), vaUIPanel( "asset", 0, true, vaUIPanel::DockLocation::DockedRight, "Assets" )
{
    assert( vaThreading::IsMainThread() );
}

vaAssetPack::~vaAssetPack( )
{
    assert( vaThreading::IsMainThread() );

    WaitUntilIOTaskFinished( );
    RemoveAll( true );
}

void vaAssetPack::InsertAndTrackMe( shared_ptr<vaAsset> newAsset, bool lockMutex )
{
    std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock );    if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller();

    m_assetMap.insert( std::pair< string, shared_ptr<vaAsset> >( vaStringTools::ToLower( newAsset->Name() ), newAsset ) );
    //    if( storagePath != L"" )
    //        m_assetMapByStoragePath.insert( std::pair< wstring, shared_ptr<vaAsset> >( vaStringTools::ToLower(newAsset->storagePath), newAsset ) );

    assert( newAsset->m_parentPackStorageIndex == -1 );
    m_assetList.push_back( newAsset );
    newAsset->m_parentPackStorageIndex = ((int)m_assetList.size())-1;

    // it's ok if it's already tracked - for ex. render meshes are always tracked
    //if( !
        newAsset->GetResource()->UIDObject_Track( ); // )
    //    VA_LOG_ERROR_STACKINFO( "Error registering asset '%s' - UID already used; this means there's another asset somewhere with the same UID", newAsset->Name().c_str() );
}

string vaAssetPack::FindSuitableAssetName( const string & _nameSuggestion, bool lockMutex )
{
    std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock );    if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller();

    const string & nameSuggestion = _nameSuggestion;

    if( Find( nameSuggestion, false ) == nullptr )
        return nameSuggestion;

    int index = 0;
    do 
    {
        string newSuggestion = vaStringTools::Format( "%s_%d", nameSuggestion.c_str(), index );
        if( Find( newSuggestion, false ) == nullptr )
            return newSuggestion;

        index++;
    } while ( true );

}

shared_ptr<vaAssetTexture> vaAssetPack::Add( const std::shared_ptr<vaTexture> & texture, const string & name, bool lockMutex )
{
    std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock );    if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller();

    if( Find( name, false ) != nullptr )
    {
        assert( false );
        VA_LOG_ERROR( "Unable to add asset '%s' to the asset pack '%s' because the name already exists", name.c_str( ), m_name.c_str( ) );
        return nullptr;
    }
//    assert( ( storagePath == L"" ) || ( FindByStoragePath( storagePath ) == nullptr ) );    // assets in packs must have unique names
    
    shared_ptr<vaAssetTexture> newItem = shared_ptr<vaAssetTexture>( new vaAssetTexture( *this, texture, name ) );

    InsertAndTrackMe( newItem, false );

    return newItem;
}

shared_ptr<vaAssetRenderMesh> vaAssetPack::Add( const std::shared_ptr<vaRenderMesh> & mesh, const string & name, bool lockMutex )
{
    std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock );    if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller();

    if( Find( name, false ) != nullptr )
    {
        assert( false );
        VA_LOG_ERROR( "Unable to add asset '%s' to the asset pack '%s' because the name already exists", name.c_str( ), m_name.c_str( ) );
        return nullptr;
    }
//    assert( ( storagePath == L"" ) || ( FindByStoragePath( storagePath ) == nullptr ) );    // assets in packs must have unique names

    shared_ptr<vaAssetRenderMesh> newItem = shared_ptr<vaAssetRenderMesh>( new vaAssetRenderMesh( *this, mesh, name ) );

    InsertAndTrackMe( newItem, false );

    return newItem;
}

shared_ptr<vaAssetRenderMaterial> vaAssetPack::Add( const std::shared_ptr<vaRenderMaterial> & material, const string & name, bool lockMutex )
{
    std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock );    if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller();

    if( Find( name, false ) != nullptr )
    {
        assert( false );
        VA_LOG_ERROR( "Unable to add asset '%s' to the asset pack '%s' because the name already exists", name.c_str(), m_name.c_str() );
        return nullptr;
    }
//    assert( (storagePath == L"") || (FindByStoragePath( storagePath ) == nullptr) );    // assets in packs must have unique names

    shared_ptr<vaAssetRenderMaterial> newItem = shared_ptr<vaAssetRenderMaterial>( new vaAssetRenderMaterial( *this, material, name ) );

    InsertAndTrackMe( newItem, false );

    return newItem;
}

string vaAsset::GetTypeNameString( vaAssetType assetType )
{
    switch( assetType )
    {
    case vaAssetType::Texture:          return "texture";       break;
    case vaAssetType::RenderMesh:       return "rendermesh";    break;
    case vaAssetType::RenderMaterial:   return "material";      break;
    default: assert( false );           return "unknown";       break;    
    }
}

bool vaAsset::Rename( const string & newName )    
{ 
    return m_parentPack.RenameAsset( *this, newName, true ); 
}

bool vaAssetPack::RenameAsset( vaAsset & asset, const string & newName, bool lockMutex )
{
    std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock );    if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller();

    if( &asset.m_parentPack != this )
    {
        VA_LOG_ERROR( "Unable to change asset name from '%s' to '%s' in asset pack '%s' - not correct parent pack!", asset.Name().c_str(), newName.c_str(), this->m_name.c_str()  );
        return false;
    }
    if( newName == asset.Name() )
    {
        VA_LOG( "Changing asset name from '%s' to '%s' in asset pack '%s' - same name requested? Nothing changed.", asset.Name().c_str(), newName.c_str(), this->m_name.c_str()  );
        return true;
    }
    if( Find( newName, false ) != nullptr )
    {
        VA_LOG_ERROR( "Unable to change asset name from '%s' to '%s' in asset pack '%s' - name already used by another asset!", asset.Name().c_str(), newName.c_str(), this->m_name.c_str()  );
        return false;
    }

    {
        auto it = m_assetMap.find( vaStringTools::ToLower( asset.Name() ) );

        shared_ptr<vaAsset> assetSharedPtr;

        if( it != m_assetMap.end() ) 
        {
            assetSharedPtr = it->second;
            assert( assetSharedPtr.get() == &asset );
            m_assetMap.erase( it );
        }
        else
        {
            VA_LOG_ERROR( "Error changing asset name from '%s' to '%s' in asset pack '%s' - original asset not found!", asset.Name().c_str(), newName.c_str(), this->m_name.c_str()  );
            return false;
        }

        assetSharedPtr->m_name = newName;

        m_assetMap.insert( std::pair< string, shared_ptr<vaAsset> >( vaStringTools::ToLower( assetSharedPtr->Name() ), assetSharedPtr ) );
    }

    VA_LOG( "Changing asset name from '%s' to '%s' in asset pack '%s' - success!", asset.Name().c_str(), newName.c_str(), this->m_name.c_str() );
    return true;
}

void vaAssetPack::Remove( const shared_ptr<vaAsset> & asset, bool lockMutex )
{
    std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock );    if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller();

    if( asset == nullptr )
        return;

    if( !asset->GetResource()->UIDObject_Untrack( ) )
        VA_LOG_ERROR_STACKINFO( "Error untracking asset '%s' - not sure why it wasn't properly tracked", asset->Name().c_str() );

    assert( m_assetList[asset->m_parentPackStorageIndex] == asset );
    if( (int)m_assetList.size( ) != ( asset->m_parentPackStorageIndex + 1 ) )
    {
        m_assetList[ asset->m_parentPackStorageIndex ] = m_assetList[ m_assetList.size( ) - 1 ];
        m_assetList[ asset->m_parentPackStorageIndex ]->m_parentPackStorageIndex = asset->m_parentPackStorageIndex;
    }
    asset->m_parentPackStorageIndex = -1;
    m_assetList.pop_back();

    {
        auto it = m_assetMap.find( vaStringTools::ToLower(asset->Name()) );

        // possible memory leak! does the asset belong to another asset pack?
        assert( it != m_assetMap.end() );
        if( it == m_assetMap.end() ) 
            return; 
    
        m_assetMap.erase( it );
    }
    
//    if( asset->StoragePath != L"" )
//    {
//        auto it = m_assetMapByStoragePath.find( vaStringTools::ToLower(asset->StoragePath) );
//
//        // can't not be in!
//        assert( it != m_assetMapByStoragePath.end( ) );
//        if( it == m_assetMapByStoragePath.end( ) )
//            return;
//
//        m_assetMapByStoragePath.erase( it );
//    }

}

void vaAssetPack::RemoveAll( bool lockMutex )
{
    std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock );    if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller();

    assert( m_ioTask == nullptr || vaBackgroundTaskManager::GetInstance().IsFinished(m_ioTask) );
    m_storageMode = vaAssetPack::StorageMode::Unknown;

    // untrack the asset resources so no one can find them anymore using vaUIDObjectRegistrar
    for( int i = 0; i < m_assetList.size( ); i++ )
    {
        if( !m_assetList[i]->GetResource()->UIDObject_Untrack( ) )
            VA_LOG_ERROR_STACKINFO( "Error untracking asset '%s' - not sure why it wasn't properly tracked", m_assetList[i]->Name().c_str() );
    }

    m_assetList.clear( );
//    m_assetMapByStoragePath.clear( );

    for( auto it = m_assetMap.begin( ); it != m_assetMap.end( ); it++ )
    {
        // if this fails it means someone is still holding a reference to assets from this pack - this shouldn't ever happen, they should have been released!
        assert( it->second.unique() );
    }
    m_assetMap.clear();
}

const int c_packFileVersion = 3;

bool vaAssetPack::IsBackgroundTaskActive( ) const
{
    assert( vaThreading::IsMainThread() );
    if( m_ioTask != nullptr )
        return !vaBackgroundTaskManager::GetInstance().IsFinished( m_ioTask );
    return false;
}

void vaAssetPack::WaitUntilIOTaskFinished( bool breakIfSafe )
{
    assert( vaThreading::IsMainThread() );
    assert( !breakIfSafe ); // not implemented/tested
    breakIfSafe;

    if( m_ioTask != nullptr )
    {
        vaBackgroundTaskManager::GetInstance().WaitUntilFinished( m_ioTask );
        m_ioTask = nullptr;
    }
    {
        std::unique_lock<mutex> apackStorageLock(m_apackStorageMutex);
        assert( !m_apackStorage.IsOpen() );
    }
}

bool vaAssetPack::SaveAPACK( const wstring & fileName, bool lockMutex )
{
    WaitUntilIOTaskFinished( );

    std::unique_lock<mutex> apackStorageLock(m_apackStorageMutex);
    if( !m_apackStorage.Open( fileName, FileCreationMode::Create ) )
    {
        VA_LOG_ERROR( L"vaAssetPack::SaveAPACK(%s) - unable to create file for saving", fileName.c_str() );
        return false;
    }

    vaStream & outStream = m_apackStorage;

    std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock );    if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller();

    VERIFY_TRUE_RETURN_ON_FALSE( outStream.CanSeek( ) );

    int64 posOfSize = outStream.GetPosition( );
    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<int64>( 0 ) );

    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<int32>( c_packFileVersion ) );

    bool useWholeFileCompression = true;
    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<bool>( useWholeFileCompression ) );

    // If compressing, have to write into memory buffer for later compression because seeking while
    // writing directly to a compression stream is not supported, and it's used here.
    vaMemoryStream memStream( (int64)0, (useWholeFileCompression)?( 16*1024 ): (0) );
    vaStream & outStreamInner = (useWholeFileCompression)?( memStream ):( outStream );

    VERIFY_TRUE_RETURN_ON_FALSE( outStreamInner.WriteValue<int32>( (int)m_assetMap.size() ) );

    for( auto it = m_assetMap.begin( ); it != m_assetMap.end( ); it++ )
    {
        int64 posOfSubSize = outStreamInner.GetPosition( );
        VERIFY_TRUE_RETURN_ON_FALSE( outStreamInner.WriteValue<int64>( 0 ) );

        // write type
        VERIFY_TRUE_RETURN_ON_FALSE( outStreamInner.WriteValue<int32>( (int32)it->second->Type ) );

        // write name
        VERIFY_TRUE_RETURN_ON_FALSE( outStreamInner.WriteString( it->first ) );
        assert( vaStringTools::CompareNoCase( it->first, it->second->Name() ) == 0 );

        // write asset resource ID
        VERIFY_TRUE_RETURN_ON_FALSE( outStreamInner.WriteValue<vaGUID>( it->second->GetResourceObjectUID() ) );

        // write asset
        VERIFY_TRUE_RETURN_ON_FALSE( it->second->SaveAPACK( outStreamInner ) );

        int64 calculatedSubSize = outStreamInner.GetPosition() - posOfSubSize;
        // auto aaaa = outStreamInner.GetPosition();
        outStreamInner.Seek( posOfSubSize );
        VERIFY_TRUE_RETURN_ON_FALSE( outStreamInner.WriteValue<int64>( calculatedSubSize ) );
        outStreamInner.Seek( posOfSubSize + calculatedSubSize );
    }

    if( useWholeFileCompression )
    {
        vaCompressionStream outCompressionStream( false, &outStream );
        // just dump the whole buffer and we're done!
        VERIFY_TRUE_RETURN_ON_FALSE( outCompressionStream.Write( memStream.GetBuffer(), memStream.GetLength() ) );
    }

    int64 calculatedSize = outStream.GetPosition( ) - posOfSize;
    outStream.Seek( posOfSize );
    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<int64>( calculatedSize ) );
    outStream.Seek( posOfSize + calculatedSize );

    m_apackStorage.Close();
    m_storageMode = StorageMode::APACK;

    return true;
}

bool vaAssetPack::LoadAPACKInner( vaStream & inStream, vector< shared_ptr<vaAsset> > & loadedAssets, vaBackgroundTaskManager::TaskContext & taskContext )
{
    m_assetStorageMutex.assert_locked_by_caller();

    int32 numberOfAssets = 0;
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int32>( numberOfAssets ) );

    for( int i = 0; i < numberOfAssets; i++ )
    {
        taskContext.Progress = float(i) / float(numberOfAssets-1);

        int64 subSize = 0;
        VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int64>( subSize ) );

        // read type
        vaAssetType assetType;
        VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int32>( (int32&)assetType ) );

        // read name
        string newAssetName;
        VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadString( newAssetName ) );

        string suitableName = FindSuitableAssetName( newAssetName, false );
        if( suitableName != newAssetName )
        {
            VA_LOG_WARNING( "There's already an asset with the name '%s' - renaming the new one to '%s'", newAssetName.c_str(), suitableName.c_str() );
            newAssetName = suitableName;
        }
        if( Find( newAssetName, false ) != nullptr )
        {
            VA_LOG_ERROR( L"vaAssetPack::Load(): duplicated asset name, stopping loading." );
            assert( false );
            return false;
        }

        shared_ptr<vaAsset> newAsset = nullptr;

        switch( assetType )
        {
        case VertexAsylum::vaAssetType::Texture:
            newAsset = shared_ptr<vaAsset>( vaAssetTexture::CreateAndLoadAPACK( *this, newAssetName, inStream ) );
            break;
        case VertexAsylum::vaAssetType::RenderMesh:
            newAsset = shared_ptr<vaAsset>( vaAssetRenderMesh::CreateAndLoadAPACK( *this, newAssetName, inStream ) );
            break;
        case VertexAsylum::vaAssetType::RenderMaterial:
            newAsset = shared_ptr<vaAsset>( vaAssetRenderMaterial::CreateAndLoadAPACK( *this, newAssetName, inStream ) );
            break;
        default:
            break;
        }

        if( newAsset == nullptr )
        {
            VA_LOG_ERROR( "Error while loading an asset - see log file above for more info - aborting loading." );
            return false;
        }
        else
        {
            VERIFY_TRUE_RETURN_ON_FALSE( newAsset != nullptr );

            InsertAndTrackMe( newAsset, false );

            loadedAssets.push_back( newAsset );
        }
    }
    return true;
}

bool vaAssetPack::LoadAPACK( const wstring & fileName, bool async, bool lockMutex )
{
    WaitUntilIOTaskFinished( );

    std::unique_lock<mutex> apackStorageLock(m_apackStorageMutex);
    if( !m_apackStorage.Open( fileName, FileCreationMode::Open, FileAccessMode::Read ) )
    {
        VA_LOG_ERROR( L"vaAssetPack::LoadAPACK(%s) - unable to open file for reading", fileName.c_str() );
        return false;
    }

    vaStream & inStream = m_apackStorage;

    std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock );    if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller();

    RemoveAll( false );

    int64 size = 0;
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int64>( size ) );

    int32 fileVersion = 0;
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int32>( fileVersion ) );
    if( fileVersion < 1 || fileVersion > 3 )
    {
        VA_LOG_ERROR( L"vaAssetPack::Load(): unsupported file version" );
        return false;
    }

    m_storageMode = StorageMode::APACK;

    bool useWholeFileCompression = false;
    if( fileVersion >= 3 )
    {
        VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<bool>( useWholeFileCompression ) );
    }

    // ok let the loading thread grab the locks again before continuing with file access 
    assetStorageMutexLock.unlock();
    apackStorageLock.unlock();

    // async stuff here. 
    auto loadingLambda = [this, &inStream, useWholeFileCompression]( vaBackgroundTaskManager::TaskContext & context ) 
    {
        vector< shared_ptr<vaAsset> > loadedAssets;

        std::unique_lock<mutex> apackStorageLock(m_apackStorageMutex);
        std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex);

        bool success;
        if( useWholeFileCompression )
        {
            vaCompressionStream decompressor( true, &inStream );
            success = LoadAPACKInner( decompressor, loadedAssets, context );
        }
        else
        {
            success = LoadAPACKInner( inStream, loadedAssets, context );
        }

        m_apackStorage.Close();

        if( !success )
        {
            VA_LOG_ERROR( L"vaAssetPack::Load(): internal error during loading" );
            RemoveAll( false );
        }

        return success;
    };

    if( async )
    {
        m_ioTask = vaBackgroundTaskManager::GetInstance().Spawn( vaStringTools::Format("Loading '%s.apack'", m_name.c_str() ), vaBackgroundTaskManager::SpawnFlags::ShowInUI, loadingLambda );
    }
    else
    {
        // assetStorageMutexLock.unlock();
        return loadingLambda( vaBackgroundTaskManager::TaskContext() );
    }

    return true;
}

bool vaAssetPack::SaveUnpacked( const wstring & folderRoot, bool lockMutex )
{
    assert(vaThreading::IsMainThread()); 
    WaitUntilIOTaskFinished( );

    std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock );    if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller();

    if( vaFileTools::DirectoryExists(folderRoot) && !vaFileTools::DeleteDirectory( folderRoot ) )
    {
        VA_LOG_ERROR( L"vaAssetPack::SaveUnpacked - Unable to delete current contents of the folder '%s'", folderRoot.c_str( ) );
        return false;
    }
    vaFileTools::EnsureDirectoryExists( folderRoot );

    vaFileStream headerFile;
    if( !headerFile.Open( folderRoot + L"AssetPack.xml", FileCreationMode::OpenOrCreate, FileAccessMode::Write ) )
    {
        VA_LOG_ERROR( L"vaAssetPack::SaveUnpacked - Unable to open '%s'", (folderRoot + L"/AssetPack.xml").c_str() );
        return false;
    }

    vaXMLSerializer serializer;

    serializer.SerializeOpenChildElement( "VertexAsylumAssetPack" );

    int32 packFileVersion = c_packFileVersion;
    serializer.Serialize<int32>( "FileVersion", packFileVersion );
    //serializer.WriteAttribute( "Name", m_name );

    bool hadError = false;

    for( vaAssetType assetType = (vaAssetType)0; assetType < vaAssetType::MaxVal; assetType = (vaAssetType)((int32)assetType+1) )
    {
        string assetTypeName = vaAsset::GetTypeNameString( assetType );
        wstring assetTypeFolder = folderRoot + vaStringTools::SimpleWiden(assetTypeName) + L"s";

        if( !vaFileTools::EnsureDirectoryExists( assetTypeFolder ) )
        {
            assert( false );
            hadError = true;
        }

        for( auto it = m_assetMap.begin( ); it != m_assetMap.end( ); it++ )
        {
            shared_ptr<vaAsset> & asset = it->second;
            if( asset->Type != assetType ) continue;
            if( asset->GetResourceObjectUID( ) == vaCore::GUIDNull( ) )
            {
                assert( false );
                hadError = true;
                continue;
            }

            wstring assetFolder = assetTypeFolder + L"\\" + vaStringTools::SimpleWiden( asset->Name() ) + L"." + vaCore::GUIDToString( asset->GetResourceObjectUID() );
            if( !vaFileTools::EnsureDirectoryExists( assetFolder ) )
            {
                assert( false );
                hadError = true;
                continue;
            }

            vaFileStream assetHeaderFile;
            if( !assetHeaderFile.Open( assetFolder + L"\\Asset.xml", FileCreationMode::OpenOrCreate, FileAccessMode::Write ) )
            {
                VA_LOG_ERROR( L"vaAssetPack::SaveUnpacked - Unable to open '%s'", (assetFolder + L"\\Asset.xml").c_str() );
                serializer.SerializePopToParentElement( "VertexAsylumAssetPack" );
                return false;
            }
            vaXMLSerializer assetSerializer;
            string storageName = string("Asset_") + assetTypeName;
            assetSerializer.SerializeOpenChildElement( storageName.c_str() );
            if( !it->second->SerializeUnpacked( assetSerializer, assetFolder ) )
            {
                assert( false );
                hadError = true;
            }
            assetSerializer.SerializePopToParentElement( storageName.c_str() );
            assetSerializer.WriterSaveToFile( assetHeaderFile );
            assetHeaderFile.Close();
        }
    }

    serializer.SerializePopToParentElement("VertexAsylumAssetPack");

    serializer.WriterSaveToFile( headerFile );

    headerFile.Close();

    m_storageMode = StorageMode::Unpacked;

    return !hadError;
}

bool vaAssetPack::LoadUnpacked( const wstring & folderRoot, bool lockMutex )
{
    std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock );    if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller();

    RemoveAll( false );

    vaFileStream headerFile;
    if( !headerFile.Open( folderRoot + L"\\AssetPack.xml", FileCreationMode::Open, FileAccessMode::Read ) )
    {
        VA_LOG_ERROR( L"vaAssetPack::LoadUnpacked - Unable to open '%s'", (folderRoot + L"\\AssetPack.xml").c_str() );
        return false;
    }

    vaXMLSerializer serializer( headerFile );
    headerFile.Close();

    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeOpenChildElement( "VertexAsylumAssetPack" ) );

    int32 fileVersion = -1;
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<int32>( "FileVersion", (int32)fileVersion ) );
    // VERIFY_TRUE_RETURN_ON_FALSE( fileVersion == c_packFileVersion );

    // already named at creation
    // VERIFY_TRUE_RETURN_ON_FALSE( serializer.ReadAttribute( "Name", m_name ) );

    bool hadError = false;
    vector< shared_ptr<vaAsset> > loadedAssets;

    for( vaAssetType assetType = (vaAssetType)0; assetType < vaAssetType::MaxVal; assetType = (vaAssetType)((int32)assetType+1) )
    {
        string assetTypeName = vaAsset::GetTypeNameString( assetType );
        wstring assetTypeFolder = folderRoot + L"\\" + vaStringTools::SimpleWiden(assetTypeName) + L"s";

        if( !vaFileTools::DirectoryExists( assetTypeFolder ) )
        {
            // ok, just no assets of this type
            continue;
        }

        vector<wstring> subDirs = vaFileTools::FindDirectories( assetTypeFolder + L"\\" );

        for( const wstring & subDir : subDirs )
        { 
            size_t nameStartA = subDir.rfind('\\');
            size_t nameStartB = subDir.rfind('/');
            nameStartA = ( nameStartA == wstring::npos ) ? ( 0 ) : ( nameStartA + 1 );
            nameStartB = ( nameStartB == wstring::npos ) ? ( 0 ) : ( nameStartB + 1 );
            wstring dirName = subDir.substr( vaMath::Max( nameStartA, nameStartB ) );
            if( dirName.length() == 0 ) 
                { assert( false ); continue; }
            size_t separator = dirName.find('.');
            if( separator == wstring::npos )
                { assert( false ); continue; }
            string assetName   = vaStringTools::SimpleNarrow( dirName.substr( 0, separator ) );
            vaGUID assetGUID   = vaCore::GUIDFromString( dirName.substr( separator+1 ) );
            if( assetName == "" )
                { assert( false ); continue; }
            if( assetGUID == vaCore::GUIDNull() )
                { assert( false ); continue; }
            
            vaFileStream assetHeaderFile;
            if( !assetHeaderFile.Open( subDir + L"\\Asset.xml", FileCreationMode::Open, FileAccessMode::Read ) )
            {
                VA_LOG_ERROR( L"vaAssetPack::LoadUnpacked - Unable to open '%s'", (subDir + L"\\Asset.xml").c_str() );
                return false;
            }
            vaXMLSerializer assetSerializer( assetHeaderFile );
            assetHeaderFile.Close();

            string suitableName = FindSuitableAssetName( assetName, false );
            if( suitableName != assetName )
            {
                VA_LOG_WARNING( "There's already an asset with the name '%s' - renaming the new one to '%s'", assetName.c_str(), suitableName.c_str() );
                assetName = suitableName;
            }
            if( Find( assetName, false ) != nullptr )
            {
                VA_LOG_ERROR( L"vaAssetPack::LoadUnpacked(): duplicated asset name, stopping loading." );
                assert( false );
                return false;
            }

            string storageName = string("Asset_") + assetTypeName;

            VERIFY_TRUE_RETURN_ON_FALSE( assetSerializer.SerializeOpenChildElement( storageName.c_str() ) );

            shared_ptr<vaAsset> newAsset = nullptr;

            switch( assetType )
            {
            case VertexAsylum::vaAssetType::Texture:
                newAsset = shared_ptr<vaAsset>( vaAssetTexture::CreateAndLoadUnpacked( *this, assetName, assetGUID, assetSerializer, subDir ) );
                break;
            case VertexAsylum::vaAssetType::RenderMesh:
                newAsset = shared_ptr<vaAsset>( vaAssetRenderMesh::CreateAndLoadUnpacked( *this, assetName, assetGUID, assetSerializer, subDir ) );
                break;
            case VertexAsylum::vaAssetType::RenderMaterial:
                newAsset = shared_ptr<vaAsset>( vaAssetRenderMaterial::CreateAndLoadUnpacked( *this, assetName, assetGUID, assetSerializer, subDir ) );
                break;
            default:
                break;
            }
            VERIFY_TRUE_RETURN_ON_FALSE( newAsset != nullptr );

            InsertAndTrackMe( newAsset, false );

            loadedAssets.push_back( newAsset );

            // if( !it->second->SerializeUnpacked( assetSerializer, assetFolder ) )
            // {
            //     assert( false );
            //     hadError = true;
            // }
            VERIFY_TRUE_RETURN_ON_FALSE( assetSerializer.SerializePopToParentElement( storageName.c_str() ) );
        }
    }

    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializePopToParentElement( "VertexAsylumAssetPack" ) );

    if( !hadError )
    {
        m_storageMode = StorageMode::Unpacked;
        return true;
    }
    else
    {
        RemoveAll( false );
        return false;
    }
}

void vaAsset::ReplaceAsset( const shared_ptr<vaAssetResource> & newResource )
{
    assert( m_resource != nullptr );

    // m_parentPack.GetAssetStorageMutex() ?

    assert( m_resource->UIDObject_IsTracked() );

    // This is done so that all other assets or systems referencing the texture by the ID now
    // point to the new one!
    vaUIDObjectRegistrar::GetInstance( ).SwapIDs( *m_resource, *newResource );

    m_resource->SetParentAsset( nullptr );
    m_resource = newResource;
    m_resource->SetParentAsset( this );

    assert( m_resource->UIDObject_IsTracked() );
}

bool vaAsset::SaveAPACK( vaStream & outStream )
{
    assert( m_resource != nullptr );
    return m_resource->SaveAPACK( outStream );
}

//void vaAsset::LoadAPACK( vaStream & inStream )
//{
//    assert( m_resource != nullptr );
//    m_resource->SaveAPACK( inStream );
//}

bool vaAsset::SerializeUnpacked( vaXMLSerializer & serializer, const wstring & assetFolder )
{
    assert( m_resource != nullptr );
    return m_resource->SerializeUnpacked( serializer, assetFolder );
}

vaAssetRenderMesh * vaAssetRenderMesh::CreateAndLoadAPACK( vaAssetPack & pack, const string & name, vaStream & inStream )
{
    vaGUID uid;
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<vaGUID>( uid ) );

    shared_ptr<vaRenderMesh> newResource = pack.GetRenderDevice().GetMeshManager().CreateRenderMesh( uid );

    if( newResource == nullptr )
        return nullptr;

    if( newResource->LoadAPACK( inStream ) )
    {
        return new vaAssetRenderMesh( pack, newResource, name );
    }
    else
    {
        VA_LOG_ERROR_STACKINFO( "Error loading asset render mesh" );
        return nullptr;
    }
}

vaAssetRenderMaterial * vaAssetRenderMaterial::CreateAndLoadAPACK( vaAssetPack & pack, const string & name, vaStream & inStream )
{
    vaGUID uid;
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<vaGUID>( uid ) );

    shared_ptr<vaRenderMaterial> newResource = pack.GetRenderDevice().GetMaterialManager().CreateRenderMaterial( uid );

    if( newResource == nullptr )
        return nullptr;

    if( newResource->LoadAPACK( inStream ) )
    {
        return new vaAssetRenderMaterial( pack, newResource, name );
    }
    else
    {
        return nullptr;
    }
}

vaAssetTexture * vaAssetTexture::CreateAndLoadAPACK( vaAssetPack & pack, const string & name, vaStream & inStream )
{
    vaGUID uid;
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<vaGUID>( uid ) );

    shared_ptr<vaTexture> newResource = VA_RENDERING_MODULE_CREATE_SHARED( vaTexture, vaTextureConstructorParams( pack.GetRenderDevice(), uid ) );

    if( newResource == nullptr )
        return nullptr;

    if( newResource->LoadAPACK( inStream ) )
    {
        return new vaAssetTexture( pack, newResource, name );
    }
    else
    {
        return nullptr;
    }
}

vaAssetRenderMesh * vaAssetRenderMesh::CreateAndLoadUnpacked( vaAssetPack & pack, const string & name, const vaGUID & uid, vaXMLSerializer & serializer, const wstring & assetFolder )
{
    shared_ptr<vaRenderMesh> newResource = pack.GetRenderDevice().GetMeshManager().CreateRenderMesh( uid );

    if( newResource == nullptr )
        return nullptr;

    if( newResource->SerializeUnpacked( serializer, assetFolder ) )
    {
        return new vaAssetRenderMesh( pack, newResource, name );
    }
    else
    {
        return nullptr;
    }
}

vaAssetRenderMaterial * vaAssetRenderMaterial::CreateAndLoadUnpacked( vaAssetPack & pack, const string & name, const vaGUID & uid, vaXMLSerializer & serializer, const wstring & assetFolder )
{
    shared_ptr<vaRenderMaterial> newResource = pack.GetRenderDevice().GetMaterialManager().CreateRenderMaterial( uid );

    if( newResource == nullptr )
        return nullptr;

    if( newResource->SerializeUnpacked( serializer, assetFolder ) )
    {
        return new vaAssetRenderMaterial( pack, newResource, name );
    }
    else
    {
        return nullptr;
    }
}

vaAssetTexture * vaAssetTexture::CreateAndLoadUnpacked( vaAssetPack & pack, const string & name, const vaGUID & uid, vaXMLSerializer & serializer, const wstring & assetFolder )
{
    shared_ptr<vaTexture> newResource = VA_RENDERING_MODULE_CREATE_SHARED( vaTexture, vaTextureConstructorParams( pack.GetRenderDevice(), uid ) );

    if( newResource == nullptr )
        return nullptr;

    if( newResource->SerializeUnpacked( serializer, assetFolder ) )
    {
        return new vaAssetTexture( pack, newResource, name );
    }
    else
    {
        return nullptr;
    }
}

void vaAssetTexture::ReplaceTexture( const shared_ptr<vaTexture> & newTexture ) 
{ 
    ReplaceAsset( newTexture ); 
}

void vaAssetRenderMesh::ReplaceRenderMesh( const shared_ptr<vaRenderMesh> & newRenderMesh ) 
{ 
    ReplaceAsset( newRenderMesh );
}

void vaAssetRenderMaterial::ReplaceRenderMaterial( const shared_ptr<vaRenderMaterial> & newRenderMaterial ) 
{ 
    ReplaceAsset( newRenderMaterial ); 
}

void vaAsset::UIPropertiesItemDraw( )                     
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    if( ImGui::Button( "Rename" ) )
        ImGuiEx_PopupInputStringBegin( "Rename asset pack", Name() );

    string newName;
    if( ImGuiEx_PopupInputStringTick( newName ) )
    {
        if( Rename( newName ) )
        {
            VA_LOG( "Asset name changed to '%s'", Name().c_str() );
        }
        else
        {
            VA_LOG_ERROR( "Unable to rename asset to '%s'", newName.c_str() );
        }
    }

    // bool showSelected = m_resource->GetUIShowSelected();
    // ImGui::Checkbox( "Highlight", &showSelected );
    // m_resource->SetUIShowSelected( showSelected );
#endif
}

void vaAssetRenderMesh::UIPropertiesItemDraw( )
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    vaAsset::UIPropertiesItemDraw( );

    shared_ptr<vaRenderMesh> mesh = GetRenderMesh( );

    ImGui::Text( "Number of vertices: %d", (int)mesh->GetTriangleMesh()->Vertices().size() );
    ImGui::Text( "Number of indices: %d", (int)mesh->GetTriangleMesh()->Indices().size() );
    //ImGui::Text( "Number of subparts: %d", (int)mesh->GetSubParts().size() );
    
    if( ImGui::Button( "Rebuild normals" ) )
    {
        mesh->RebuildNormals();
    }
    //mesh->GetFrontFaceWindingOrder()
#endif
}

void vaAssetTexture::UIPropertiesItemDraw( )
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    vaAsset::UIPropertiesItemDraw();

    shared_ptr<vaTexture> texture = GetTexture( );
    assert( texture != nullptr );

    texture->GetRenderDevice().GetTextureTools().UIDrawImGuiInfo( texture );
#endif
}

#ifdef VA_IMGUI_INTEGRATION_ENABLED
static void UIDrawTextureInfo( string formatStr, const shared_ptr<vaTexture> & tex )
{
    if( tex != nullptr )
    {
        const vaAsset * parentAsset = tex->GetParentAsset( );
        if( parentAsset != nullptr )
        {
            ImGui::Text( formatStr.c_str(), parentAsset->Name().c_str() );
        }
        else
        {
            ImGui::Text( formatStr.c_str(), "no parent asset (procedural or etc.)" );
        }
    }
    else
    {
        ImGui::Text( formatStr.c_str(), "null" );
    }
}
static void UIDrawMaterialInputValueInfo( vaRenderMaterial::MaterialInput & materialInput )
{
    ImGui::PushID( materialInput.Name.c_str() );
    switch( materialInput.Type )
    {
    case( vaRenderMaterial::MaterialInput::InputType::Bool    ) : { bool val        = materialInput.Value.Bool;     ImGui::Checkbox( "", &val )     ;  materialInput.Value.Bool    = val; } break;   
    case( vaRenderMaterial::MaterialInput::InputType::Integer ) : { int val         = materialInput.Value.Integer;  ImGui::InputInt( "", &val )     ;  materialInput.Value.Integer = val; } break;   
    case( vaRenderMaterial::MaterialInput::InputType::Scalar  ) : { float val       = materialInput.Value.Scalar;   ImGui::InputFloat( "", &val )   ;  materialInput.Value.Scalar  = val; } break;      
    case( vaRenderMaterial::MaterialInput::InputType::Vector4 ) : { vaVector4 val   = materialInput.Value.Vector4;  ImGui::InputFloat4( "", &val.x );  materialInput.Value.Vector4 = val; } break;   
    case( vaRenderMaterial::MaterialInput::InputType::Color4  ) : { vaVector4 val   = materialInput.Value.Vector4;  ImGui::InputFloat4( "", &val.x );  materialInput.Value.Vector4 = val; } break;   
    default: ImGui::Text("error"); break;
    }
    ImGui::PopID();
}
#endif

void vaAssetRenderMaterial::UIPropertiesItemDraw( )
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    vaAsset::UIPropertiesItemDraw();

    ImGui::TextColored( ImVec4( 1.0f, 0.7f, 0.7f, 1.0f ), "Inputs:" );
    //ImGui::Indent();

    shared_ptr<vaRenderMaterial> renderMaterial = GetRenderMaterial();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4,1)); // Tighten spacing
    for( int i = 0; i < (int)renderMaterial->GetInputs( ).size(); i++ )
    {
        vaRenderMaterial::MaterialInput materialInput = renderMaterial->GetInputs( )[i];
        string typeName;
        switch( materialInput.Type )
        {
        case( vaRenderMaterial::MaterialInput::InputType::Bool    ) : typeName = "Bool";    break;   
        case( vaRenderMaterial::MaterialInput::InputType::Integer ) : typeName = "Integer"; break;
        case( vaRenderMaterial::MaterialInput::InputType::Scalar  ) : typeName = "Scalar";  break;   
        case( vaRenderMaterial::MaterialInput::InputType::Vector4 ) : typeName = "Vector4"; break;
        case( vaRenderMaterial::MaterialInput::InputType::Color4  ) : typeName = "Color4";  break;
        default: typeName = "error"; break;
        }
        ImGui::Text(" ");
        ImGui::SameLine();
        ImGui::TextColored( ImVec4( 0.7f, 1.0f, 0.7f, 1.0f ), materialInput.Name.c_str() );
        ImGui::SameLine();
        ImGui::Text(" ( %s, %s ):", typeName.c_str(), materialInput.IsValueDynamic?"dynamic":"static" );

        if( materialInput.SourceTextureID != vaCore::GUIDNull() )
        {
            UIDrawTextureInfo( "  val: texture, %s", materialInput.GetSourceTexture() );
        }
        else
        {
            ImGui::Text( "  val: " );
            ImGui::SameLine();
            UIDrawMaterialInputValueInfo( materialInput );
        }
        if( materialInput != renderMaterial->GetInputs( )[i] )
            renderMaterial->SetInput( i, materialInput );
    }
    ImGui::PopStyleVar();

    ImGui::TextColored( ImVec4( 0.7f, 0.7f, 1.0f, 1.0f ), "Settings:");
    vaRenderMaterial::MaterialSettings settings = renderMaterial->GetMaterialSettings();
    ImGui::Combo( "Culling mode", (int*)&settings.FaceCull, "None\0Front\0Back\0\0" );
    ImGui::Checkbox( "Transparent", &settings.Transparent );
    ImGui::Checkbox( "AlphaTest", &settings.AlphaTest );
    ImGui::InputFloat( "AlphaTestThreshold", &settings.AlphaTestThreshold );
    settings.AlphaTestThreshold = vaMath::Clamp( settings.AlphaTestThreshold, 0.0f, 1.0f );
    ImGui::Checkbox( "ReceiveShadows", &settings.ReceiveShadows );
    ImGui::Checkbox( "CastShadows", &settings.CastShadows );
    ImGui::Checkbox( "Wireframe", &settings.Wireframe );
    ImGui::Checkbox( "AdvancedSpecularShader", &settings.AdvancedSpecularShader );
    ImGui::Checkbox( "SpecialEmissiveLight", &settings.SpecialEmissiveLight );
    
    if( renderMaterial->GetMaterialSettings() != settings )
        renderMaterial->SetMaterialSettings( settings );

    //ImGui::Unindent();
    
    // ImGui::Separator();
    // 
    // ImGui::Text("NEW Material info");
    // 
    // ImGui::Indent();
    // 
    // struct MaterialInputUI : public vaImguiHierarchyObject
    // {
    //     const vaRenderMaterial::MaterialInput &     Input;
    // 
    //     MaterialInputUI( ) = delete;
    //     MaterialInputUI( const vaRenderMaterial::MaterialInput & input ) : Input( input ) { };
    // 
    //     virtual string IHO_GetInstanceName( ) const override   { return Input.Name; }
    //     virtual void   IHO_Draw( ) override
    //     {
    //         ImGui::Text( Input.Name.c_str() );
    //     }
    // };
    // 
    // const std::vector<vaRenderMaterial::MaterialInput> & inputs = renderMaterial->GetInputs( );
    // for( const vaRenderMaterial::MaterialInput & i : inputs )
    // {
    //     MaterialInputUI iui(i);
    //     iui.IHO_Draw();
    // }
    // 
    // ImGui::Unindent();
#endif
}

void vaAssetPack::UIPanelDraw( )
{ 
    assert(vaThreading::IsMainThread()); 

#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGui::PushItemWidth( 200.0f );

    ImGui::PushID( this );

    bool disableEdit = m_name == "default";

    if( m_ioTask != nullptr && !vaBackgroundTaskManager::GetInstance( ).IsFinished( m_ioTask ) )
    {
        vaBackgroundTaskManager::GetInstance( ).ImGuiTaskProgress( m_ioTask );
        disableEdit = true;
    }

    if( !disableEdit )
    {
        std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex );

        // rename UI
        if( ImGui::Button( "Rename" ) )
            ImGuiEx_PopupInputStringBegin( "Rename asset pack", GetName() );

        string newName;
        if( ImGuiEx_PopupInputStringTick( newName ) )
        {
            if( m_assetPackManager.FindLoadedPack( newName ) != nullptr )
            {
                VA_LOG_WARNING( "Cannot change name from '%s' to '%s' as the new name is already in use", m_name.c_str(), newName.c_str() );
            }
            else if( vaStringTools::ToLower( newName ) == vaStringTools::ToLower( "Importer_AssetPack" ) )
            {
                VA_LOG_WARNING( "Cannot change name from '%s' to '%s' as the new name is reserved", m_name.c_str(), newName.c_str() );
            }
            else
            {
                m_name = newName;
                VA_LOG( "Asset pack name changed to '%s'", m_name.c_str() );
            }
        }

        ImGui::Separator();

        switch( m_storageMode )
        {
        case vaAssetPack::StorageMode::Unknown:             ImGui::Text( "Current storage mode: UNKNOWN" );     break;
        case vaAssetPack::StorageMode::Unpacked:            ImGui::Text( "Current storage mode: Unpacked" );    break;
        case vaAssetPack::StorageMode::APACK:               ImGui::Text( "Current storage mode: APACK" );       break;
        default: assert( false );  break;
        }

        wstring packedStoragePath = m_assetPackManager.GetAssetFolderPath( ) + vaStringTools::SimpleWiden( m_name ) + L".apack";

        ImGui::Text( ".apack storage:" );
        ImGui::Text( vaStringTools::SimpleNarrow(packedStoragePath).c_str() );
        //ImGui::SameLine( ImGui::GetContentRegionAvailWidth() * 3 / 4 );
        ImGui::PushID( "APACK" );
        if( ImGui::Button( " Load " ) )
        {
            //wstring fileName = vaFileTools::OpenFileDialog( L"", vaCore::GetExecutableDirectory(), L".apack files\0*.apack\0\0" );
            vaFileStream fileIn;
            if( vaFileTools::FileExists( packedStoragePath ) )
            {
                if( !LoadAPACK( packedStoragePath, true, false ) )
                {
                    VA_WARN( L"LoadAPACK('%s') failed", packedStoragePath.c_str() );
                }
                else
                {
                    ImGui::PopID();
                    return;
                }

            }
            else
            {
                VA_WARN( L"File '%s' does not exist or unable to open for reading", packedStoragePath.c_str() );
            }
        }
        ImGui::SameLine( );
        if( ImGui::Button( " Save " ) )
        {
            if( !SaveAPACK( packedStoragePath, false ) )
            {
                VA_WARN( L"Unable to open file '%s' for writing", packedStoragePath.c_str() );
            }
        }
        ImGui::PopID( );

        ImGui::Separator();

        wstring unpackedStoragePath = m_assetPackManager.GetAssetFolderPath( ) + vaStringTools::SimpleWiden( m_name ) + L"\\";
        ImGui::Text( "Unpacked storage: " );
        ImGui::Text( vaStringTools::SimpleNarrow(unpackedStoragePath).c_str() );
        //ImGui::SameLine( ImGui::GetContentRegionAvailWidth() * 3 / 4 );
        ImGui::PushID( "UNPACKED" );
        if( ImGui::Button( " Load " ) )
        {
            //wstring storageDirectory = vaFileTools::SelectFolderDialog( vaCore::GetExecutableDirectory() );

            // error messages handled internally
            LoadUnpacked( unpackedStoragePath, false );
        }
        ImGui::SameLine( );
        if( ImGui::Button( " Save " ) )
        {
            //wstring storageDirectory = vaFileTools::SelectFolderDialog( vaCore::GetExecutableDirectory() );
            //if( unpackedStoragePath != L"" )

            // error messages handled internally
            SaveUnpacked( unpackedStoragePath, false );
        }
        ImGui::PopID( );

        ImGui::Separator( );

        if( ImGui::Button( "Compress uncompressed textures" ) )
        {
            vector< shared_ptr<vaAsset> > meshes, materials, textures;
            for( auto asset : m_assetList )
            {
                if( asset->Type == vaAssetType::Texture )
                {
                    shared_ptr<vaAssetTexture> assetTexture = std::dynamic_pointer_cast<vaAssetTexture>(asset);

                    {
                        vaSimpleScopeTimerLog timer( "Compressing texture '"+asset->Name()+"'" );
                        shared_ptr<vaTexture> texture = assetTexture->GetTexture();

                        shared_ptr<vaTexture> compressedTexture = shared_ptr<vaTexture>( texture->TryCompress() );
                        if( compressedTexture != nullptr )
                        {
                            VA_LOG( "Conversion successful, replacing vaAssetTexture's old resource with the newly compressed." );
                            assetTexture->ReplaceTexture( compressedTexture );
                        }
                        else
                        {
                            VA_LOG( "Conversion skipped or failed." );
                        }
                    }
                    VA_LOG( "" );
                }
            }
        }

        if( vaTextureReductionTestTool::GetSupportedByApp() )
        {
            if( ImGui::Button( "Test textures for resolution reduction impact tool" ) )
            {
                if( vaTextureReductionTestTool::GetInstancePtr( ) == nullptr )
                {
                    vector<vaTextureReductionTestTool::TestItemType> paramTextures;
                    vector<shared_ptr<vaAssetTexture>> paramTextureAssets;
                    for( auto asset : m_assetList )
                        if( asset->Type == vaAssetType::Texture )
                        {
                            paramTextures.push_back( make_pair( std::dynamic_pointer_cast<vaAssetTexture>( asset )->GetTexture(), asset->Name() ) );
                            paramTextureAssets.push_back( std::dynamic_pointer_cast<vaAssetTexture>( asset ) );
                        }

                    new vaTextureReductionTestTool( paramTextures, paramTextureAssets );
                }
            }
        }

        ImGui::Separator( );

            // remove all assets UI
        {
            const char * deleteAssetPackPopup = "Remove all assets";
            if( ImGui::Button( "Remove all assets" ) )
            {
                ImGui::OpenPopup( deleteAssetPackPopup );
            }

            if( ImGui::BeginPopupModal( deleteAssetPackPopup ) )
            {
                ImGui::Text( "Are you sure that you want to remove all assets?" ); 
                if( ImGui::Button( "Yes" ) )
                {
                    RemoveAll(false);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if( ImGui::Button( "Cancel" ) )
                {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
    }
    //ImGui::Separator();

    vector< shared_ptr<vaAsset> > meshes, materials, textures;
    for( auto asset : m_assetList )
    {
        if( asset->Type == vaAssetType::RenderMesh )
            meshes.push_back(asset);
        else if( asset->Type == vaAssetType::RenderMaterial )
            materials.push_back(asset);
        else if( asset->Type == vaAssetType::Texture )
            textures.push_back(asset);
    }

    ImGuiTabBarFlags tabBarFlags = 0;
    tabBarFlags |= ImGuiTabBarFlags_Reorderable;
    //tabBarFlags |= ImGuiTabBarFlags_AutoSelectNewTabs;
    tabBarFlags |= ImGuiTabBarFlags_NoCloseWithMiddleMouseButton;
    tabBarFlags |= ImGuiTabBarFlags_FittingPolicyScroll;    // ImGuiTabBarFlags_FittingPolicyResizeDown
    if (ImGui::BeginTabBar("MyTabBar", tabBarFlags))
    {
        if( ImGui::BeginTabItem( vaStringTools::Format("Meshes (%d)###Meshes", (int)meshes.size() ).c_str( ) ) ) //ImGuiWindowFlags_NoBackground ) )
        {
            for( auto asset : meshes )
            {
                bool nodeOpen = ImGui::TreeNode( asset->Name().c_str() );
                if( asset->GetResource( ) != nullptr )
                    asset->GetResource( )->SetUIShowSelected( ImGui::IsItemHovered( ) );
                if( nodeOpen )
                {
                    if( !disableEdit )
                        asset->UIPropertiesItemDraw();
                    ImGui::TreePop();
                }
            }
            ImGui::EndTabItem();
        }
        if( ImGui::BeginTabItem( vaStringTools::Format("Materials (%d)###Materials", (int)materials.size() ).c_str( ) ) ) //ImGuiWindowFlags_NoBackground ) )
        {
            for( auto asset : materials )
            {
                bool nodeOpen = ImGui::TreeNode( asset->Name( ).c_str( ) );
                if( asset->GetResource( ) != nullptr )
                    asset->GetResource( )->SetUIShowSelected( ImGui::IsItemHovered( ) );
                if( nodeOpen )
                {
                    if( !disableEdit )
                        asset->UIPropertiesItemDraw();
                    ImGui::TreePop();
                }
            }
            ImGui::EndTabItem();
        }
        if( ImGui::BeginTabItem( vaStringTools::Format("Textures (%d)###Textures", (int)textures.size() ).c_str( ) ) ) //ImGuiWindowFlags_NoBackground ) )
        {
            for( auto asset : textures )
            {
                bool nodeOpen = ImGui::TreeNode( asset->Name( ).c_str( ) );
                if( asset->GetResource( ) != nullptr )
                    asset->GetResource( )->SetUIShowSelected( ImGui::IsItemHovered( ) );
                if( nodeOpen )
                {
                    if( !disableEdit )
                        asset->UIPropertiesItemDraw();
                    ImGui::TreePop();
                    if( asset->GetResource( ) != nullptr )
                        asset->GetResource( )->SetUIShowSelected( ImGui::IsItemHovered( ) );
                }
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

#if 0
    // Meshes
    if( ImGui::TreeNode( "Meshes", "Meshes (%d)", meshes.size() ) )
    {
        for( auto asset : meshes )
        {
            bool nodeOpen = ImGui::TreeNode( asset->Name().c_str() );
            if( asset->GetResource( ) != nullptr )
                asset->GetResource( )->SetUIShowSelected( ImGui::IsItemHovered( ) );
            if( nodeOpen )
            {
                if( !disableEdit )
                    asset->UIPropertiesItemDraw();
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
    // Materials
    if( ImGui::TreeNode( "Materials", "Materials (%d)", materials.size() ) )
    {
        for( auto asset : materials )
        {
            bool nodeOpen = ImGui::TreeNode( asset->Name( ).c_str( ) );
            if( asset->GetResource( ) != nullptr )
                asset->GetResource( )->SetUIShowSelected( ImGui::IsItemHovered( ) );
            if( nodeOpen )
            {
                if( !disableEdit )
                    asset->UIPropertiesItemDraw();
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
    // Textures
    if( ImGui::TreeNode( "Textures", "Textures (%d)", textures.size() ) )
    {
        for( auto asset : textures )
        {
            bool nodeOpen = ImGui::TreeNode( asset->Name( ).c_str( ) );
            if( asset->GetResource( ) != nullptr )
                asset->GetResource( )->SetUIShowSelected( ImGui::IsItemHovered( ) );
            if( nodeOpen )
            {
                if( !disableEdit )
                    asset->UIPropertiesItemDraw();
                ImGui::TreePop();
                if( asset->GetResource( ) != nullptr )
                    asset->GetResource( )->SetUIShowSelected( ImGui::IsItemHovered( ) );
            }
        }
        ImGui::TreePop();
    }
#endif

    ImGui::PopID( );
    ImGui::PopItemWidth( );
#endif
}

vaRenderDevice & vaAssetPack::GetRenderDevice( )
{ 
    return m_assetPackManager.GetRenderDevice(); 
}

vaAssetPackManager::vaAssetPackManager( vaRenderDevice & renderDevice ) : m_renderDevice( renderDevice )//, vaUIPanel( "Assets" )
{
    assert( vaThreading::IsMainThread() );

    m_assetImporter = shared_ptr<vaAssetImporter>( new vaAssetImporter(m_renderDevice) );

    shared_ptr<vaAssetPack> defaultPack = CreatePack( "default" );
    m_defaultPack = defaultPack;

    // some default assets - commented out slower ones to reduce startup times <-> this should be done in a better way anyway, same as (async) loading except no file access
    {
        shared_ptr<vaRenderMesh> planeMesh        = vaRenderMesh::CreatePlane( m_renderDevice, vaMatrix4x4::Identity, 1.0f, 1.0f                            , vaCore::GUIDFromStringA( "706f42bc-df35-4dae-b706-688c451bd181" ) );
    //    shared_ptr<vaRenderMesh> tetrahedronMesh  = vaRenderMesh::CreateTetrahedron( m_renderDevice, vaMatrix4x4::Identity, false                           , vaCore::GUIDFromStringA( "45fa748c-982b-406b-a0d0-e0c49772ded7" ) );
        shared_ptr<vaRenderMesh> cubeMesh         = vaRenderMesh::CreateCube( m_renderDevice, vaMatrix4x4::Identity, false, 0.7071067811865475f             , vaCore::GUIDFromStringA( "11ec29ea-1524-4720-9109-14aa894750b0" ) );
    //    shared_ptr<vaRenderMesh> octahedronMesh   = vaRenderMesh::CreateOctahedron( m_renderDevice, vaMatrix4x4::Identity, false                            , vaCore::GUIDFromStringA( "6d813dea-1a19-426e-97af-c3edd94d4a99" ) );
    //    shared_ptr<vaRenderMesh> icosahedronMesh  = vaRenderMesh::CreateIcosahedron( m_renderDevice, vaMatrix4x4::Identity, false                           , vaCore::GUIDFromStringA( "d2b41829-9d06-429e-bddb-46579806b77a" ) );
    //    shared_ptr<vaRenderMesh> dodecahedronMesh = vaRenderMesh::CreateDodecahedron( m_renderDevice, vaMatrix4x4::Identity, false                          , vaCore::GUIDFromStringA( "cbf5502b-97e0-49c5-8ec7-d6afa41314b8" ) );
        shared_ptr<vaRenderMesh> sphereMesh       = vaRenderMesh::CreateSphere( m_renderDevice, vaMatrix4x4::Identity, 2, true                              , vaCore::GUIDFromStringA( "089282d6-9640-4cd8-b0f5-22d026ffcdb6" ) );
        shared_ptr<vaRenderMesh> cylinderMesh     = vaRenderMesh::CreateCylinder( m_renderDevice, vaMatrix4x4::Identity, 1.0, 1.0, 1.0, 12, false, false    , vaCore::GUIDFromStringA( "aaea48bd-e48c-4d15-8c98-c5be3e0b6ef0" ) );
        //shared_ptr<vaRenderMesh> teapotMesh       = vaRenderMesh::CreateTeapot( m_renderDevice, vaMatrix4x4::Identity                                       , vaCore::GUIDFromStringA( "3bab100b-7336-429b-bb79-e4b1b25ba443" ) );
    
        defaultPack->Add( planeMesh           , "SystemMesh_Plane", true );
    //    defaultPack->Add( tetrahedronMesh     , "SystemMesh_Tetrahedron", true );
        defaultPack->Add( cubeMesh            , "SystemMesh_Cube", true );
    //    defaultPack->Add( octahedronMesh      , "SystemMesh_Octahedron", true );
    //    defaultPack->Add( icosahedronMesh     , "SystemMesh_Icosahedron", true );
    //    defaultPack->Add( dodecahedronMesh    , "SystemMesh_Dodecahedron", true );
        defaultPack->Add( sphereMesh          , "SystemMesh_Sphere", true );
        defaultPack->Add( cylinderMesh        , "SystemMesh_Cylinder", true );
        //defaultPack->Add( teapotMesh          , "SystemMesh_Teapot", true );
    }
}

vaAssetPackManager::~vaAssetPackManager( )      
{ 
    assert( vaThreading::IsMainThread() );

    UnloadAllPacks( );
}

shared_ptr<vaAssetPack> vaAssetPackManager::CreatePack( const string & _assetPackName )
{
    assert( vaThreading::IsMainThread() );
    string assetPackName = vaStringTools::ToLower( _assetPackName );

    if( FindLoadedPack( assetPackName ) != nullptr )
    {
        VA_LOG_WARNING( "vaAssetPackManager::CreatePack(%s) - pack with the same name already exists, can't create another", assetPackName.c_str() );
        return nullptr;
    }
    shared_ptr<vaAssetPack> newPack = shared_ptr<vaAssetPack>( new vaAssetPack( *this, assetPackName ) );
    m_assetPacks.push_back( newPack );
    return newPack;
}

void vaAssetPackManager::LoadPacks( const string & _nameOrWildcard, bool allowAsync )
{
    assert( vaThreading::IsMainThread() );
    string nameOrWildcard = vaStringTools::ToLower( _nameOrWildcard );

    wstring assetPackFolder = GetAssetFolderPath( );

    if( nameOrWildcard == "*" )
    {
        vaSimpleScopeTimerLog timerLog( vaStringTools::Format( L"Enumerating/loading all asset packs in '%s'", assetPackFolder.c_str( ) ) );

        // first load unpacked
        auto dirList = vaFileTools::FindDirectories( assetPackFolder );
        for( auto dir : dirList )
        {
            if( vaFileTools::FileExists( dir + L"\\AssetPack.xml" ) )
            {
                string name = vaStringTools::SimpleNarrow(dir);
                size_t lastSep = name.find_last_of( L'\\' );
                if( lastSep != wstring::npos )
                    name = name.substr( lastSep+1 );

                LoadPacks( name, allowAsync );
            }
        }

        // then load packed
        auto fileList = vaFileTools::FindFiles( assetPackFolder, L"*.apack", false );
        for( auto fileName : fileList )
        {
            wstring justName, justExt;
            vaFileTools::SplitPath( fileName, nullptr, &justName, &justExt );
            justExt = vaStringTools::ToLower( justExt );
            assert( justExt == L".apack" );

            string name = vaStringTools::SimpleNarrow( justName );

            if( FindLoadedPack(name) == nullptr )
                LoadPacks( name, allowAsync );
        }
    }
    else
    {
        if( FindLoadedPack( nameOrWildcard ) != nullptr )
        {
            VA_LOG_WARNING( "vaAssetPackManager::LoadPacks(%s) - can't load, already loaded", nameOrWildcard.c_str() );
            return;
        }

        // first try unpacked...
        bool unpackedLoaded = false;
        wstring unpackDir = assetPackFolder + vaStringTools::SimpleWiden(nameOrWildcard) + L"\\";
        if( vaFileTools::FileExists( unpackDir + L"AssetPack.xml" ) )
        {
            unpackedLoaded = true;

            shared_ptr<vaAssetPack> newPack = CreatePack( nameOrWildcard );
            if( !newPack->LoadUnpacked( unpackDir, true ) )
            {
                VA_LOG_ERROR( L"vaAssetPackManager::LoadPacks - Error while loading asset pack from '%s'", unpackDir.c_str() );
                return;
            }
        }

        // ...then try packed
        wstring apackName = assetPackFolder + vaStringTools::SimpleWiden(nameOrWildcard) + L".apack";
        if( vaFileTools::FileExists( apackName ) )
        {
            if( unpackedLoaded )
            {
                VA_LOG_WARNING( "vaAssetPackManager::LoadPacks(%s) - pack with that name exists as .apack but already loaded as unpacked - you might want to remove one or the other to avoid confusion", nameOrWildcard.c_str() );
            }
            else
            {
                shared_ptr<vaAssetPack> newPack = CreatePack( nameOrWildcard );

                if( !newPack->LoadAPACK( apackName, allowAsync, true ) )
                {
                    VA_LOG_ERROR( "vaAssetPackManager::LoadPacks(%s) - error loading .apack file", nameOrWildcard.c_str() );
                }
            }
        }
        else
        {
            if( !unpackedLoaded )
            {
                VA_LOG_ERROR( "vaAssetPackManager::LoadPacks(%s) - unable to find asset with that name in the asset folder", nameOrWildcard.c_str() );
            }
        }
    }
}

shared_ptr<vaAssetPack> vaAssetPackManager::FindOrLoadPack( const string & _assetPackName, bool allowAsync )
{
    assert( vaThreading::IsMainThread() );
    string assetPackName = vaStringTools::ToLower( _assetPackName );

    shared_ptr<vaAssetPack> found = FindLoadedPack(assetPackName);
    if( found = nullptr )
    {
        LoadPacks( assetPackName, allowAsync );
        found = FindLoadedPack(assetPackName);
    }
    return found;
}

shared_ptr<vaAssetPack> vaAssetPackManager::FindLoadedPack( const string & _assetPackName )
{
    assert( vaThreading::IsMainThread() );
    string assetPackName = vaStringTools::ToLower( _assetPackName );

    for( int i = 0; i < m_assetPacks.size(); i++ )
        if( m_assetPacks[i]->GetName() == assetPackName )
            return m_assetPacks[i];
    return nullptr;
}

void vaAssetPackManager::UnloadPack( shared_ptr<vaAssetPack> & pack )
{
    assert( vaThreading::IsMainThread() );
    assert( pack != nullptr );
    for( int i = 0; i < m_assetPacks.size(); i++ )
    {
        if( m_assetPacks[i] == pack )
        {
            pack.reset();
            // check if anyone else is holding a smart pointer to this pack - in case they are, they need to .reset() it before we get here
            assert( m_assetPacks[i].unique() );
            if( i != (m_assetPacks.size()-1) )
                m_assetPacks[m_assetPacks.size()-1] = m_assetPacks[i];
            m_assetPacks.pop_back();
            return;
        }
    }
}
void vaAssetPackManager::UnloadAllPacks( )
{
    m_assetImporter->Clear();

    assert( vaThreading::IsMainThread() );
    m_defaultPack.reset();
    for( int i = 0; i < m_assetPacks.size(); i++ )
    {
        // check if anyone else is holding a smart pointer to this pack - in case they are, they need to .reset() it before we get here
        assert( m_assetPacks[i].unique() );
        m_assetPacks[i] = nullptr;
    }
    m_assetPacks.clear();
}

bool vaAssetPackManager::AnyAsyncOpExecuting( )
{
    assert( vaThreading::IsMainThread() );
    for( int i = 0; i < m_assetPacks.size(); i++ )
        if( m_assetPacks[i]->IsBackgroundTaskActive() )
            return true;
    return false;
}

void vaAssetPackManager::OnRenderingAPIAboutToShutdown( )
{
    UnloadAllPacks();
}

// void vaAssetPackManager::UIPanelDraw( )
// { 
//     assert( vaThreading::IsMainThread() );
// #ifdef VA_IMGUI_INTEGRATION_ENABLED
//     vector<string> list;
//     for( size_t i = 0; i < m_assetPacks.size(); i++ )
//         list.push_back( m_assetPacks[i]->GetName() );
//     ImGui::Text( "Asset packs in memory:" );
//     ImGui::PushItemWidth(-1);
//     ImGuiEx_ListBox( "###Asset packs in memory:", m_UIAssetPackIndex, list );
//     ImGui::PopItemWidth();
//     if( m_UIAssetPackIndex >= 0 && m_UIAssetPackIndex < m_assetPacks.size() )
//         m_assetPacks[m_UIAssetPackIndex]->UIDraw();
// #endif
// }
