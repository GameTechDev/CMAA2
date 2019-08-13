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

#include "Core/vaCoreIncludes.h"
#include "Core/vaUI.h"

#include "vaRendering.h"

#include "vaTriangleMesh.h"
#include "vaTexture.h"

#include "Rendering/Shaders/vaSharedTypes.h"

// vaRenderMesh and vaRenderMeshManager are a generic render mesh implementation

namespace VertexAsylum
{
    enum class vaAssetType : int32
    {
        Texture,
        RenderMesh,
        RenderMaterial,

        MaxVal
    };

    class vaTexture;
    class vaRenderMesh;
    class vaRenderMaterial;
    class vaAssetPack;

    // this needs to be converted to a class, along with type names and other stuff (it started as a simple struct)
    struct vaAsset : public vaUIPropertiesItem
    {
    protected:
        friend class vaAssetPack;
        shared_ptr<vaAssetResource>                     m_resource;
        string                                          m_name;                     // warning, never change this except by using Rename
        vaAssetPack &                                   m_parentPack;
        int                                             m_parentPackStorageIndex;   // referring to vaAssetPack::m_assetList

    protected:
        vaAsset( vaAssetPack & pack, const vaAssetType type, const string & name, const shared_ptr<vaAssetResource> & resourceBasePtr ) : m_parentPack( pack ), Type( type ), m_name( name ), m_resource( resourceBasePtr ), m_parentPackStorageIndex( -1 )
                                                        { assert( m_resource != nullptr ); m_resource->SetParentAsset( this ); }
        virtual ~vaAsset( )                             { assert( m_resource != nullptr ); m_resource->SetParentAsset( nullptr ); }

        void                                            ReplaceAsset( const shared_ptr<vaAssetResource> & newResourceBasePtr );

    public:
        const vaAssetType                               Type;
//        const wstring                                   StoragePath;

        const vaAssetPack &                             GetAssetPack( ) const                   { return m_parentPack; }

        virtual const vaGUID &                          GetResourceObjectUID( )                 { assert( m_resource != nullptr ); return m_resource->UIDObject_GetUID(); }
        const shared_ptr<vaAssetResource> &             GetResource( )                          { return m_resource; }

        const string &                                  Name( ) const                           { return m_name; }

        bool                                            Rename( const string & newName );

        virtual bool                                    SaveAPACK( vaStream & outStream );
        virtual bool                                    SerializeUnpacked( vaXMLSerializer & serializer, const wstring & assetFolder );

        //void                                            ReconnectDependencies( )                { m_resourceBasePtr->ReconnectDependencies(); }

        static string                                   GetTypeNameString( vaAssetType );

    protected:
        virtual string                                  UIPropertiesItemGetDisplayName( ) const override { return vaStringTools::Format( "%s", m_name.c_str() ); }
        virtual void                                    UIPropertiesItemDraw( ) override;
    };

    struct vaAssetTexture : public vaAsset
    {
    private:
        friend class vaAssetPack;
        vaAssetTexture( vaAssetPack & pack, const shared_ptr<vaTexture> & texture, const string & name );
    
    public:
        shared_ptr<vaTexture>                           GetTexture( ) const                                                     { return std::dynamic_pointer_cast<vaTexture>(m_resource); }
        void                                            ReplaceTexture( const shared_ptr<vaTexture> & newTexture );

        static vaAssetTexture *                         CreateAndLoadAPACK( vaAssetPack & pack, const string & name, vaStream & inStream );
        static vaAssetTexture *                         CreateAndLoadUnpacked( vaAssetPack & pack, const string & name, const vaGUID & uid, vaXMLSerializer & serializer, const wstring & assetFolder );

        static shared_ptr<vaAssetTexture>               SafeCast( const shared_ptr<vaAsset> & asset ) { assert( asset->Type == vaAssetType::RenderMesh ); return std::dynamic_pointer_cast<vaAssetTexture, vaAsset>( asset ); }

    private:
        virtual string                                  UIPropertiesItemGetDisplayName( ) const override   { return vaStringTools::Format( "texture: %s ", Name().c_str() ); }
        virtual void                                    UIPropertiesItemDraw( ) override;
    };

    struct vaAssetRenderMesh : public vaAsset
    {
    private:
        friend class vaAssetPack;
        vaAssetRenderMesh( vaAssetPack & pack, const shared_ptr<vaRenderMesh> & mesh, const string & name );

    public:
        shared_ptr<vaRenderMesh>                        GetRenderMesh( ) const                                                     { return std::dynamic_pointer_cast<vaRenderMesh>(m_resource); }
        void                                            ReplaceRenderMesh( const shared_ptr<vaRenderMesh> & newRenderMesh );

        static vaAssetRenderMesh *                      CreateAndLoadAPACK( vaAssetPack & pack, const string & name, vaStream & inStream );
        static vaAssetRenderMesh *                      CreateAndLoadUnpacked( vaAssetPack & pack, const string & name, const vaGUID & uid, vaXMLSerializer & serializer, const wstring & assetFolder );

        static shared_ptr<vaAssetRenderMesh>            SafeCast( const shared_ptr<vaAsset> & asset ) { assert( asset->Type == vaAssetType::RenderMesh ); return std::dynamic_pointer_cast<vaAssetRenderMesh, vaAsset>( asset ); }

    private:
        virtual string                                  UIPropertiesItemGetDisplayName( ) const override   { return vaStringTools::Format( "mesh: %s ", Name().c_str() ); }
        virtual void                                    UIPropertiesItemDraw( ) override;
    };

    struct vaAssetRenderMaterial : public vaAsset
    {
    private:
        friend class vaAssetPack;
        vaAssetRenderMaterial( vaAssetPack & pack, const shared_ptr<vaRenderMaterial> & material, const string & name );

    public:
        shared_ptr<vaRenderMaterial>                    GetRenderMaterial( ) const { return std::dynamic_pointer_cast<vaRenderMaterial>( m_resource ); }
        void                                            ReplaceRenderMaterial( const shared_ptr<vaRenderMaterial> & newRenderMaterial );

        static vaAssetRenderMaterial *                  CreateAndLoadAPACK( vaAssetPack & pack, const string & name, vaStream & inStream );
        static vaAssetRenderMaterial *                  CreateAndLoadUnpacked( vaAssetPack & pack, const string & name, const vaGUID & uid, vaXMLSerializer & serializer, const wstring & assetFolder );

        static shared_ptr<vaAssetRenderMaterial>        SafeCast( const shared_ptr<vaAsset> & asset ) { assert( asset->Type == vaAssetType::RenderMaterial ); return std::dynamic_pointer_cast<vaAssetRenderMaterial, vaAsset>( asset ); }

    private:
        virtual string                                  UIPropertiesItemGetDisplayName( ) const override   { return vaStringTools::Format( "material: %s ", Name().c_str() ); }
        virtual void                                    UIPropertiesItemDraw( ) override;
    };

    class vaAssetPack : public vaUIPanel//, public std::enable_shared_from_this<vaAssetPack>
    {
        enum class StorageMode
        {
            Unknown,
            Unpacked,
            APACK,
            //APACKStreamable,
        };

    protected:
        string                                              m_name;                 // warning - not protected by the mutex and can only be accessed by the main thread
        std::map< string, shared_ptr<vaAsset> >             m_assetMap;
        vector< shared_ptr<vaAsset> >                       m_assetList;
        mutable mutex                                       m_assetStorageMutex;

        vaAssetPackManager &                                m_assetPackManager;

        // changes on every load/save
        StorageMode                                         m_storageMode           = StorageMode::Unknown;
        bool                                                m_dirty                 = false;
        vaFileStream                                        m_apackStorage;
        mutex                                               m_apackStorageMutex;

        shared_ptr<vaBackgroundTaskManager::Task>           m_ioTask;

    private:
        friend class vaAssetPackManager;
        explicit vaAssetPack( vaAssetPackManager & assetPackManager, const string & name );
        vaAssetPack( const vaAssetPack & copy ) = delete;
    
    public:
        virtual ~vaAssetPack( );

    public:
        mutex &                                             GetAssetStorageMutex( ) { return m_assetStorageMutex; }

        string                                              FindSuitableAssetName( const string & nameSuggestion, bool lockMutex );

        shared_ptr<vaAsset>                                 Find( const string & _name, bool lockMutex = true );
        //shared_ptr<vaAsset>                                 FindByStoragePath( const wstring & _storagePath );
        void                                                Remove( const shared_ptr<vaAsset> & asset, bool lockMutex );
        void                                                RemoveAll( bool lockMutex );
        
        // save current contents
        bool                                                SaveAPACK( const wstring & fileName, bool lockMutex );
        // load contents (current contents are not deleted)
        bool                                                LoadAPACK( const wstring & fileName, bool async, bool lockMutex );

        // save current contents as XML & folder structure
        bool                                                SaveUnpacked( const wstring & folderRoot, bool lockMutex );
        // load contents as XML & folder structure (current contents are not deleted)
        bool                                                LoadUnpacked( const wstring & folderRoot, bool lockMutex );

        vaRenderDevice &                                    GetRenderDevice( );

    public:
        shared_ptr<vaAssetTexture>                          Add( const shared_ptr<vaTexture> & texture, const string & name, bool lockMutex );
        shared_ptr<vaAssetRenderMesh>                       Add( const shared_ptr<vaRenderMesh> & mesh, const string & name, bool lockMutex );
        shared_ptr<vaAssetRenderMaterial>                   Add( const shared_ptr<vaRenderMaterial> & material, const string & name, bool lockMutex );

        string                                              GetName( ) const                            { assert(vaThreading::IsMainThread()); return m_name; }

        bool                                                RenameAsset( vaAsset & asset, const string & newName, bool lockMutex );

        size_t                                              Count( bool lockMutex ) const               { std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock ); if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller(); assert( m_assetList.size() == m_assetMap.size() ); return m_assetList.size(); }

        shared_ptr<vaAsset>                                 AssetAt( size_t index, bool lockMutex )     { std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock ); if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller(); if( index >= m_assetList.size() ) return nullptr; else return m_assetList[index]; }

        const shared_ptr<vaBackgroundTaskManager::Task> &   GetCurrentIOTask( )                         { return m_ioTask; }
        void                                                WaitUntilIOTaskFinished( bool breakIfSafe = false );
        bool                                                IsBackgroundTaskActive( ) const;

    protected:
        // these are leftovers - need to be removed 
        // virtual string                                      IHO_GetInstanceName( ) const                { return vaStringTools::Format("Asset Pack '%s'", Name().c_str() ); }
        //virtual void                                        IHO_Draw( );
        virtual string                                      UIPanelGetDisplayName( ) const override     { return m_name; }
        virtual void                                        UIPanelDraw( ) override;

    private:
        void                                                InsertAndTrackMe( shared_ptr<vaAsset> newAsset, bool lockMutex );

        bool                                                LoadAPACKInner( vaStream & inStream, vector< shared_ptr<vaAsset> > & loadedAssets, vaBackgroundTaskManager::TaskContext & taskContext );
    };

    class vaAssetImporter;

    class vaAssetPackManager// : public vaUIPanel
    {
        // contains some standard meshes, etc.
        weak_ptr<vaAssetPack>                               m_defaultPack;

        shared_ptr<vaAssetImporter>                         m_assetImporter;

        vaRenderDevice &                                    m_renderDevice;

    protected:
        vector<shared_ptr<vaAssetPack>>                     m_assetPacks;           // tracks all vaAssetPacks that belong to this vaAssetPackManager
        int                                                 m_UIAssetPackIndex      = 0;


    public:
                                                            vaAssetPackManager( vaRenderDevice & renderDevice );
                                                            ~vaAssetPackManager( );

    public:
        shared_ptr<vaAssetPack>                             GetDefaultPack( )                                       { return m_defaultPack.lock(); }
        
        // wildcard '*' or name supported (name with wildcard not yet supported but feel free to add!)
        shared_ptr<vaAssetPack>                             CreatePack( const string & assetPackName );
        void                                                LoadPacks( const string & nameOrWildcard, bool allowAsync = false );
        shared_ptr<vaAssetPack>                             FindLoadedPack( const string & assetPackName );
        shared_ptr<vaAssetPack>                             FindOrLoadPack( const string & assetPackName, bool allowAsync = true );
        void                                                UnloadPack( shared_ptr<vaAssetPack> & pack );
        void                                                UnloadAllPacks( );

        bool                                                AnyAsyncOpExecuting( );

        // // this searches all packs by name <error - not yet ensured that names will never overlap - needs to be done before this makes any sense>
        // shared_ptr<vaAssetPack>                             FindAsset( )

        vaAssetImporter &                                   GetImporter( )                                          { return *m_assetImporter; }

        vaRenderDevice &                                    GetRenderDevice( )                                      { return m_renderDevice; }

        wstring                                             GetAssetFolderPath( )                                   { return vaCore::GetExecutableDirectory() + L"Media\\AssetPacks\\"; }

    protected:

        // Many assets have DirectX/etc. resource locks so make sure we're not holding any references 
        friend class vaDirectXCore; // <- these should be reorganized so that this is not called from anything that is API-specific
        void                                                OnRenderingAPIAboutToShutdown( );

    //public:
    //    virtual void                                        UIPanelDraw( ) override;
    };


    //////////////////////////////////////////////////////////////////////////

    inline shared_ptr<vaAsset> vaAssetPack::Find( const string & _name, bool lockMutex ) 
    { 
        std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock );    if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller();
        auto it = m_assetMap.find( vaStringTools::ToLower( _name ) );                        
        if( it == m_assetMap.end( ) ) 
            return nullptr; 
        else 
            return it->second; 
    }
    //inline shared_ptr<vaAsset> vaAssetPack::FindByStoragePath( const wstring & _storagePath ) 
    //{ 
    //    auto it = m_assetMapByStoragePath.find( vaStringTools::ToLower( _storagePath ) );    
    //    if( it == m_assetMapByStoragePath.end( ) ) 
    //        return nullptr; 
    //    else 
    //        return it->second; 
    //}

}

#include "Scene/vaAssetImporter.h"