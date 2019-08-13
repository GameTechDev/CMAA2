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

#include "vaScene.h"

#include "Rendering/vaAssetPack.h"

#include "Rendering/vaDebugCanvas.h"

#include "Core/System/vaFileTools.h"

#include "IntegratedExternals/vaImguiIntegration.h"

using namespace VertexAsylum;


vaSceneObject::vaSceneObject( )
{ 
}
vaSceneObject::~vaSceneObject( )                   
{
    // children/parents must be disconnected!
    assert( m_parent == nullptr );
    assert( m_children.size() == 0 );
}

vaMatrix4x4 vaSceneObject::GetWorldTransform( ) const
{
#ifdef _DEBUG
    shared_ptr<vaScene> scene = GetScene();
    assert( scene != nullptr );
    assert( m_lastSceneTickIndex == scene->GetTickIndex( ) );   // for whatever reason, you're getting stale data
#endif
    return m_computedWorldTransform;
}

void vaSceneObject::FindClosestRecursive( const vaVector3 & worldLocation, shared_ptr<vaSceneObject> & currentClosest, float & currentDistance )
{
    // can't get any closer than this
    if( currentDistance == 0.0f )
        return;

    for( int i = 0; i < m_children.size( ); i++ )
    {
        m_children[i]->FindClosestRecursive( worldLocation, currentClosest, currentDistance );
    }

    float distance = GetGlobalAABB().NearestDistanceToPoint( worldLocation );

    if( distance < currentDistance )
    {
        currentDistance = distance;
        currentClosest = this->shared_from_this();
    }
}

void vaSceneObject::TickRecursive( vaScene & scene, float deltaTime )
{
    assert( m_scene.lock() == scene.shared_from_this() );

    assert( !m_destroyedButNotYetRemovedFromScene );
    assert( !m_createdButNotYetAddedToScene );

    if( m_computedLocalBoundingBox == vaBoundingBox::Degenerate && m_renderMeshes.size( ) > 0 )
    {
        UpdateLocalBoundingBox();
    }

    // Update transforms (OK to rely on parent's transforms, they've already been updated)
    if( m_parent == nullptr )
        m_computedWorldTransform = GetLocalTransform();
    else
        m_computedWorldTransform = GetLocalTransform() * m_parent->GetWorldTransform();

    // Update tick index (transforms ok, but beware, bounding boxes not yet)
    m_lastSceneTickIndex = scene.GetTickIndex();

    // Our bounding box, oriented in world space
    vaOrientedBoundingBox oobb = vaOrientedBoundingBox( m_computedLocalBoundingBox, m_computedWorldTransform );
    //vaDebugCanvas3D::GetInstance().DrawBox( oobb, 0xFF000000, 0x20FF8080 );
    m_computedGlobalBoundingBox = oobb.ComputeEnclosingAABB();
    //vaDebugCanvas3D::GetInstance().DrawBox( m_computedGlobalBoundingBox, 0xFF000000, 0x20FF8080 );

    for( int i = 0; i < m_children.size( ); i++ )
    {
        m_children[i]->TickRecursive( scene, deltaTime );
        m_computedGlobalBoundingBox = vaBoundingBox::Combine( m_computedGlobalBoundingBox, m_children[i]->GetGlobalAABB() );
    }
}

shared_ptr<vaRenderMesh> vaSceneObject::GetRenderMesh( int index ) const
{
    if( index < 0 || index >= m_renderMeshes.size( ) )
    {
        assert( false );
        return nullptr;
    }
    return vaUIDObjectRegistrar::GetInstance().FindCached<vaRenderMesh>( m_renderMeshes[index], m_cachedRenderMeshes[index] );
}

void vaSceneObject::AddRenderMeshRef( const shared_ptr<vaRenderMesh> & renderMesh )    
{ 
    const vaGUID & uid = renderMesh->UIDObject_GetUID();
    assert( std::find( m_renderMeshes.begin(), m_renderMeshes.end(), uid ) == m_renderMeshes.end() ); 
    m_renderMeshes.push_back( uid ); 
    m_cachedRenderMeshes.push_back( renderMesh );
    assert( m_cachedRenderMeshes.size() == m_renderMeshes.size() );
    m_computedLocalBoundingBox = vaBoundingBox::Degenerate;
}

bool vaSceneObject::RemoveRenderMeshRef( int index )
{
    assert( m_cachedRenderMeshes.size( ) == m_renderMeshes.size( ) );
    if( index < 0 || index >= m_renderMeshes.size( ) )
    {
        assert( false );
        return false;
    }

    m_renderMeshes[index] = m_renderMeshes.back();
    m_renderMeshes.pop_back();
    m_cachedRenderMeshes[index] = m_cachedRenderMeshes.back( );
    m_cachedRenderMeshes.pop_back( );
    m_computedLocalBoundingBox = vaBoundingBox::Degenerate;
    return true;
}

bool vaSceneObject::RemoveRenderMeshRef( const shared_ptr<vaRenderMesh> & renderMesh ) 
{ 
    assert( m_cachedRenderMeshes.size() == m_renderMeshes.size() );
    const vaGUID & uid = renderMesh->UIDObject_GetUID();
    int indexRemoved = vector_find_and_remove( m_renderMeshes, uid );
    if( indexRemoved == -1 )
    {
        assert( false );
        return false;
    }
    if( indexRemoved < (m_cachedRenderMeshes.size()-1) )
        m_cachedRenderMeshes[indexRemoved] = m_cachedRenderMeshes.back();
    m_cachedRenderMeshes.pop_back();
    m_computedLocalBoundingBox = vaBoundingBox::Degenerate;
    return true;
}

bool vaSceneObject::Serialize( vaXMLSerializer & serializer )
{
    auto scene = m_scene.lock();
    assert( scene != nullptr );

    // element opened by the parent, just fill in attributes

    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<string>( "Name", m_name ) );

    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<vaVector4>( "TransformR0", m_localTransform.r0 ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<vaVector4>( "TransformR1", m_localTransform.r1 ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<vaVector4>( "TransformR2", m_localTransform.r2 ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<vaVector4>( "TransformR3", m_localTransform.r3 ) );

    // VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<vaVector3>( "AABBMin", m_boundingBox.Min ) );
    // VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<vaVector3>( "AABBSize", m_boundingBox.Size ) );

    if( serializer.GetVersion() > 0 )
        serializer.SerializeArray( "RenderMeshes", "RenderMesh", m_renderMeshes );
    else
        VERIFY_TRUE_RETURN_ON_FALSE( serializer.OldSerializeValueVector( "RenderMeshes", m_renderMeshes ) );

    VERIFY_TRUE_RETURN_ON_FALSE( scene->SerializeObjectsRecursive( serializer, "ChildObjects", m_children, this->shared_from_this() ) );

    if( serializer.IsReading( ) )
    {
        m_lastSceneTickIndex = -1;
        m_computedWorldTransform = vaMatrix4x4::Identity;
        m_computedLocalBoundingBox = vaBoundingBox::Degenerate;
        m_computedGlobalBoundingBox = vaBoundingBox::Degenerate;
        m_cachedRenderMeshes.resize( m_renderMeshes.size() );

        m_localTransform.r0.w = 0.0f;
        m_localTransform.r1.w = 0.0f;
        m_localTransform.r2.w = 0.0f;
        m_localTransform.r3.w = 1.0f;

        SetLocalTransform( m_localTransform );
    }

    return true;
}

void vaSceneObject::MarkAsDestroyed( bool markChildrenAsWell )
{
    // already destroyed? this shouldn't ever happen - avoid at all cost
    assert( !IsDestroyed() );

    m_destroyedButNotYetRemovedFromScene = true;
    if( markChildrenAsWell )
    {
        for( int i = 0; i < (int)m_children.size( ); i++ )
            m_children[i]->MarkAsDestroyed( true );
    }
}

void vaSceneObject::RegisterChildAdded( const shared_ptr<vaSceneObject> & child )
{
    assert( std::find( m_children.begin(), m_children.end(), child ) == m_children.end() );

    m_children.push_back( child );
}

void vaSceneObject::RegisterChildRemoved( const shared_ptr<vaSceneObject> & child )
{
    bool allOk = vector_find_and_remove( m_children, child ) != -1;
    assert( allOk ); // if this fires, there's a serious error somewhere!
    allOk;
}

void vaSceneObject::SetParent( const shared_ptr<vaSceneObject> & parent )
{
    if( parent == m_parent )
    {
        // nothing to change, nothing to do
        return;
    }
    auto scene = m_scene.lock();
    assert( scene != nullptr );
    if( scene == nullptr )
        return;

    // changing parent? then unregister us from previous (either root or another object)
    if( m_parent != nullptr )
        m_parent->RegisterChildRemoved( this->shared_from_this() );
    else
        scene->RegisterRootObjectRemoved( this->shared_from_this() );

    // removing parent? just register as a root object
    if( parent == nullptr )
    {
        m_parent = nullptr;
        scene->RegisterRootObjectAdded( this->shared_from_this() );
        return;
    }

    // make sure we're not setting up a no circular dependency or breaking the max node tree depth
    const int c_maxObjectTreeDepth = 32;
    bool allOk = false;
    shared_ptr<vaSceneObject> pp = parent;
    for( int i = 0; i < c_maxObjectTreeDepth; i++ )
    {
        if( pp == nullptr )
        {
            allOk = true;
            break;
        }
        if( pp.get( ) == this )
        {
            VA_LOG_ERROR( "vaSceneObject::SetParent failed - circular dependency detected" );
            allOk = false;
            return;
        }
        pp = pp->GetParent();
    }
    if( !allOk )
    {
        VA_LOG_ERROR( "vaSceneObject::SetParent failed - tree depth over the limit of %d", c_maxObjectTreeDepth );
        return;
    }
    m_parent = parent;
    m_parent->RegisterChildAdded( this->shared_from_this() );
}

// static vaBoundingSphere g_cullOutsideBS( vaVector3( 0, 0, 0 ), 0.0f );
// static vaBoundingBox g_cullInsideBox( vaVector3( 0, 0, 0 ), vaVector3( 0, 0, 0 ) );
// static bool g_doCull = false;
// static int64 g_isInside = 0;
// static int64 g_isOutside = 0;

vaDrawResultFlags vaSceneObject::SelectForRendering( vaRenderSelection & renderSelection )
{
    assert( renderSelection.MeshList != nullptr );
    if( renderSelection.MeshList == nullptr )
        return vaDrawResultFlags::None;

    vaDrawResultFlags drawResults = vaDrawResultFlags::None;

    vaMatrix4x4 worldTransform = GetWorldTransform( );
    for( int i = 0; i < m_renderMeshes.size(); i++ )
    {
        auto renderMesh = GetRenderMesh(i);
        if( renderMesh != nullptr )
            renderSelection.MeshList->Insert( renderMesh, worldTransform, renderSelection.Filter.SortCenter, renderSelection.Filter.SortCenterType );
        else
            drawResults |= vaDrawResultFlags::AssetsStillLoading;

// for manually deleting triangles in a bounding box for merged meshes (don't ask...)
#if 0
        if( g_doCull )
        {
            const shared_ptr<vaRenderMesh::StandardTriangleMesh> & triMesh = GetRenderMesh(i)->GetTriangleMesh();

            // if( triMesh->Indices.size() < 1000 )
            //     continue;

            // vaBoundingBox hackBB1 = vaBoundingBox( vaVector3( -30.0f, -55.0f, -10.0f ), vaVector3( 50.0f, 74.0f, 60.0f ) );
            // vaBoundingBox hackBB2 = vaBoundingBox( vaVector3( -35.0f, -60.0f, -10.0f ), vaVector3( 80.0f, 90.0f, 10.24f ) );
            for( int j = (int)(triMesh->Indices.size())-3; j >= 0; j -= 3 )
            {
                vaVector3 p0 = vaVector3::TransformCoord( triMesh->Vertices[triMesh->Indices[j + 0]].Position, worldTransform );
                vaVector3 p1 = vaVector3::TransformCoord( triMesh->Vertices[triMesh->Indices[j + 1]].Position, worldTransform );
                vaVector3 p2 = vaVector3::TransformCoord( triMesh->Vertices[triMesh->Indices[j + 2]].Position, worldTransform );

                // bool isIn = hackBB1.PointInside( p0 ) || hackBB1.PointInside( p1 ) || hackBB1.PointInside( p2 );
                // isIn |= hackBB2.PointInside( p0 ) || hackBB2.PointInside( p1 ) || hackBB2.PointInside( p2 );
                bool cull = !(g_cullOutsideBS.PointInside( p0 ) || g_cullOutsideBS.PointInside( p1 ) || g_cullOutsideBS.PointInside( p2 ));

                cull |= g_cullInsideBox.PointInside( p0 ) && g_cullInsideBox.PointInside( p1 ) && g_cullInsideBox.PointInside( p2 );
            
                if( cull )
                {
                    //vaDebugCanvas3D::GetInstance().DrawTriangle( p0, p1, p2, (cull)?(0xFF00FF00):(0xFFFF0000) );
                    g_isOutside++;
                    triMesh->Indices.erase( triMesh->Indices.begin()+j, triMesh->Indices.begin()+j+3 );
                    triMesh->SetDataDirty();
                }
                else
                {
                    g_isInside++;
                }
            }

            auto subParts = GetRenderMesh(i)->GetSubParts();
            for( int j = 0; j < subParts.size( ); j++ )
            {
                vaRenderMesh::SubPart subPart = subParts[j];
                if( ( subPart.IndexStart + subPart.IndexCount ) > triMesh->Indices.size( ) )
                {
                    subPart.IndexCount = (int)triMesh->Indices.size( ) - subPart.IndexStart;
                    
                    assert( subPart.IndexCount > 0 );
                    GetRenderMesh(i)->SetSubPart( j, subPart );
                }
            }
    }
#endif

    }
    return drawResults;
}

void vaSceneObject::RegisterUsedAssetPacks( std::function<void( const vaAssetPack & )> registerFunction )
{
    for( int i = 0; i < m_renderMeshes.size(); i++ )
    {
        auto renderMesh = GetRenderMesh(i);
        if( renderMesh != nullptr && renderMesh->GetParentAsset() != nullptr )
            registerFunction( renderMesh->GetParentAsset()->GetAssetPack() );
    }
}

void vaSceneObject::UpdateLocalBoundingBox( )
{
    assert( m_cachedRenderMeshes.size() == m_renderMeshes.size() );
    m_computedLocalBoundingBox = vaBoundingBox::Degenerate;
    m_computedGlobalBoundingBox = vaBoundingBox::Degenerate;
    for( int i = 0; i < m_renderMeshes.size(); i++ )
    {
        auto renderMesh = GetRenderMesh(i);
        if( renderMesh != nullptr )
            m_computedLocalBoundingBox = vaBoundingBox::Combine( m_computedLocalBoundingBox, renderMesh->GetAABB() );
    }
}

void vaSceneObject::UIPropertiesItemDraw( )
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    auto scene = m_scene.lock();
    assert( scene != nullptr );

    if( ImGui::Button( "Rename" ) )
        ImGuiEx_PopupInputStringBegin( "Rename object", m_name );
    ImGuiEx_PopupInputStringTick( m_name );

    ImGui::SameLine();
    if( ImGui::Button( "Delete" ) )
    {
        scene->DestroyObject( this->shared_from_this(), false, true );
        return;
    }


    bool transChanged = false;
    vaMatrix4x4 localTransform = m_localTransform;
    transChanged |= ImGui::InputFloat4( "LocalTransformRow0", &localTransform.m[0][0] );
    transChanged |= ImGui::InputFloat4( "LocalTransformRow1", &localTransform.m[1][0] );
    transChanged |= ImGui::InputFloat4( "LocalTransformRow2", &localTransform.m[2][0] );
    transChanged |= ImGui::InputFloat4( "LocalTransformRow3", &localTransform.m[3][0] );

    if( transChanged )
    {
        SetLocalTransform( localTransform );
    }

    /*
    vaVector3 raxis; float rangle;
    m_rotation.ToAxisAngle( raxis, rangle );
    rangle = rangle * 180.0f / VA_PIf;
    bool changed =  ImGui::InputFloat3( "Rotation axis", &raxis.x );
    changed |=      ImGui::InputFloat( "Rotation angle", &rangle, 5.0f, 30.0f, 3 );
    if( changed )
        m_rotation = vaQuaternion::RotationAxis( raxis, rangle * VA_PIf / 180.0f );
        */

    string parentName = (m_parent == nullptr)?("none"):(m_parent->GetName().c_str() );
    ImGui::Text( "Parent: %s", parentName.c_str() );
    ImGui::Text( "Child object count: %d", m_children.size() );
    ImGui::Text( "Render mesh count: %d", m_renderMeshes.size() );
#endif
}


vaScene::vaScene( ) : vaUIPanel( "Scene", 0, true, vaUIPanel::DockLocation::DockedLeft, "Scenes" )
{
    Clear();
}

vaScene::vaScene( const string & name ) : vaUIPanel( "Scene", 0, true, vaUIPanel::DockLocation::DockedLeft, "Scenes"  )
{
    Clear( );
    m_name = name;
}

vaScene::~vaScene( )
{
    // this might be ok or might not be ok - you've got some 
    assert( m_deferredObjectActions.size() == 0 );

    Clear();
}

void vaScene::Clear( )
{
    m_deferredObjectActions.clear();
    DestroyAllObjects( );
    ApplyDeferredObjectActions();
    m_name  = "";
    m_lights.clear();
    assert( m_allObjects.size() == 0 );
    assert( m_rootObjects.size() == 0 );

    m_tickIndex = 0;
    m_sceneTime = 0.0;

    assert( !m_isInTick );
    m_isInTick = false;
}

shared_ptr<vaSceneObject> vaScene::CreateObject( const shared_ptr<vaSceneObject> & parent )
{
    return CreateObject( "Unnamed", vaMatrix4x4::Identity, parent );
}

shared_ptr<vaSceneObject> vaScene::CreateObject( const string & name, const vaMatrix4x4 & localTransform, const shared_ptr<vaSceneObject> & parent )
{
    m_deferredObjectActions.push_back( DeferredObjectAction( std::make_shared<vaSceneObject>( ), parent, vaScene::DeferredObjectAction::AddObject ) );

    m_deferredObjectActions.back().Object->SetName( name );
    m_deferredObjectActions.back().Object->SetScene( this->shared_from_this() );
    m_deferredObjectActions.back().Object->SetLocalTransform( localTransform );

    return m_deferredObjectActions.back().Object;
}

shared_ptr<vaSceneObject> vaScene::CreateObject( const string & name, const vaVector3 & localScale, const vaQuaternion & localRot, const vaVector3 & localPos, const shared_ptr<vaSceneObject> & parent )
{
    return CreateObject( name, vaMatrix4x4::FromScaleRotationTranslation( localScale, localRot, localPos ), parent );
}

void vaScene::DestroyObject( const shared_ptr<vaSceneObject> & ptr, bool destroyChildrenRecursively, bool applyOwnTransformToChildren )
{
    assert( ptr->GetScene() != nullptr );
    if( applyOwnTransformToChildren )
    {
        // doesn't make sense to apply transform to children if you're destroying them as well
        assert( !destroyChildrenRecursively );
    }

    // mark the object as no longer functional
    ptr->MarkAsDestroyed( destroyChildrenRecursively );

    if( destroyChildrenRecursively )
    {
        m_deferredObjectActions.push_back( DeferredObjectAction( ptr, nullptr, vaScene::DeferredObjectAction::RemoveObjectAndChildren ) );
    }
    else
    {
        // we can do this right now
        if( applyOwnTransformToChildren )
        {
            const vaMatrix4x4 & parentTransform = ptr->GetLocalTransform();
            const vector<shared_ptr<vaSceneObject>> & children = ptr->GetChildren();
            for( int i = 0; i < (int)children.size(); i++ )
            {
                const vaMatrix4x4 newTransform = parentTransform * children[i]->GetLocalTransform();
                children[i]->SetLocalTransform( newTransform );
            }
        }
        m_deferredObjectActions.push_back( DeferredObjectAction( ptr, nullptr, vaScene::DeferredObjectAction::RemoveObject ) );
    }

    //assert( m_deferredObjectActions.back().Object.unique() );
}

void vaScene::DestroyAllObjects( )
{
    m_deferredObjectActions.push_back( DeferredObjectAction( nullptr, nullptr, vaScene::DeferredObjectAction::RemoveAllObjects ) );
}

void vaScene::RegisterRootObjectAdded( const shared_ptr<vaSceneObject> & object )
{
    if( m_isInTick )
    {
        assert( false );    // Can't call this while in tick
        return;
    }

    assert( std::find( m_rootObjects.begin(), m_rootObjects.end(), object ) == m_rootObjects.end() );
    m_rootObjects.push_back( object );
}

void vaScene::RegisterRootObjectRemoved( const shared_ptr<vaSceneObject> & object )
{
    if( m_isInTick )
    {
        assert( false );    // Can't call this while in tick
        return;
    }

    bool allOk = vector_find_and_remove( m_rootObjects, object ) != -1;
    assert( allOk ); // if this fires, there's a serious error somewhere!
    allOk;
}

void vaScene::ApplyDeferredObjectActions( )
{
    if( m_isInTick )
    {
        assert( false );    // Can't call this while in tick
        return;
    }

    for( int i = 0; i < m_deferredObjectActions.size( ); i++ )
    {
        const DeferredObjectAction & mod = m_deferredObjectActions[i];

        if( mod.Action == vaScene::DeferredObjectAction::RemoveAllObjects )
        {
            for( int j = 0; j < (int)m_allObjects.size(); j++ )
                m_allObjects[j]->NukeGraph( );
            m_rootObjects.clear();

            for( int j = 0; j < m_allObjects.size( ); j++ )
            {
                // this pointer should be unique now. otherwise you're holding a pointer somewhere although the object was destroyed - is this
                // intended? if there's a case where this is intended, think it through in detail - there's a lot of potential for unforeseen 
                // consequences from accessing vaSceneObject-s in this state.
                assert( m_allObjects[i].unique() );
            }
            m_allObjects.clear();
        }
        else if( mod.Action == vaScene::DeferredObjectAction::AddObject )
        {
            // by default in root, will be removed on "add parent" - this can be optimized out if desired (no need to call if parent != nullptr) but 
            // it leaves the object in a broken state temporarily and SetParent needs to get adjusted and maybe something else too
            m_rootObjects.push_back( mod.Object );

            mod.Object->SetParent( mod.ParentObject );

            m_allObjects.push_back( mod.Object );

            mod.Object->SetAddedToScene();
        }
        else if( mod.Action == vaScene::DeferredObjectAction::RemoveObject || mod.Action == vaScene::DeferredObjectAction::RemoveObjectAndChildren )
        {
            bool destroyChildrenRecursively = mod.Action == vaScene::DeferredObjectAction::RemoveObjectAndChildren;
            // they should automatically remove themselves from the list as they get deleted or reattached to this object's parent
            if( !destroyChildrenRecursively )
            {
                const vector<shared_ptr<vaSceneObject>> & children = mod.Object->GetChildren();

                while( children.size() > 0 )
                {
                    // we >have< to make this a temp, as the SetParent modifies the containing array!!
                    shared_ptr<vaSceneObject> temp = children[0];
                    assert( this->shared_from_this() == temp->GetScene() );
                    temp->SetParent( mod.Object->GetParent() );
                }
            }
            DestroyObjectImmediate( mod.Object, destroyChildrenRecursively );
        }
    }

    m_deferredObjectActions.clear();
}

void vaScene::DestroyObjectImmediate( const shared_ptr<vaSceneObject> & obj, bool recursive )
{
    assert( obj->IsDestroyed() );           // it must have already been destroyed
    //assert( obj->GetScene() == nullptr );   // it must have been already set to null
    obj->SetParent( nullptr );

    obj->SetScene( nullptr );

    bool allOk = vector_find_and_remove( m_rootObjects, obj ) != -1;
    assert( allOk );
    allOk = vector_find_and_remove( m_allObjects, obj ) != -1;
    assert( allOk );

    if( recursive )
    {
        const vector<shared_ptr<vaSceneObject>> & children = obj->GetChildren();
        while( obj->GetChildren().size( ) > 0 )
        {
            // we >have< to make this a temp, as the DestroyObject modifies both the provided reference and the containing array!!
            shared_ptr<vaSceneObject> temp = children[0];
            DestroyObjectImmediate( temp, true );
        }
    }
    
    // this pointer should be unique now. otherwise you're holding a pointer somewhere although the object was destroyed - is this
    // intended? if there's a case where this is intended, think it through in detail - there's a lot of potential for unforeseen 
    // consequences from accessing vaSceneObject-s in this state.
    assert( obj.unique() );
}

void vaScene::Tick( float deltaTime )
{
    ApplyDeferredObjectActions();

    if( deltaTime > 0 )
    {
        m_sceneTime += deltaTime;

        assert( !m_isInTick );
        m_isInTick = true;
        m_tickIndex++;
        for( int i = 0; i < m_rootObjects.size( ); i++ )
        {
            m_rootObjects[i]->TickRecursive( *this, deltaTime );
        }
        assert( m_isInTick );
        m_isInTick = false;

        ApplyDeferredObjectActions();

        m_UI_MouseClickIndicatorRemainingTime = vaMath::Max( m_UI_MouseClickIndicatorRemainingTime - deltaTime, 0.0f );
    }
}

void vaScene::ApplyLighting( vaLighting & lighting )
{
    lighting.SetLights( m_lights );
    lighting.SetEnvmap( m_envmapTexture, m_envmapRotation, m_envmapColorMultiplier );
    lighting.FogSettings() = m_fog;
}

bool vaScene::SerializeObjectsRecursive( vaXMLSerializer & serializer, const string & name, vector<shared_ptr<vaSceneObject>> & objectList, const shared_ptr<vaSceneObject> & parent )
{
    if( m_isInTick )
    {
        assert( false );    // Can't call this while in tick
        return false;
    }

    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeOpenChildElement( name.c_str() ) );

    uint32 count = (uint32)objectList.size();
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<uint32>( "count", count ) );

    if( serializer.IsReading() )
    {
        assert( objectList.size() == 0 );
        // objectList.resize( count );
    }

    string elementName;
    for( uint32 i = 0; i < count; i++ )
    {
        elementName = vaStringTools::Format( "%s_%d", name.c_str(), i );
        VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeOpenChildElement( elementName.c_str() ) );

        shared_ptr<vaSceneObject> obj = nullptr;
        if( serializer.IsReading() )
            obj = CreateObject( parent );
        else
            obj = objectList[i];

        obj->Serialize( serializer );

        VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializePopToParentElement( elementName.c_str() ) );
    }

    // if( serializer.IsReading() )
    // {
    //     // should have count additions
    //     // assert( m_deferredObjectActions.size() == count );
    // }

    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializePopToParentElement( name.c_str() ) );

    return true;
}

bool vaScene::Serialize( vaXMLSerializer & serializer, bool mergeToExistingIfLoading )
{
    if( m_isInTick )
    {
        assert( false );    // Can't call this while in tick
        return false;
    }

    if( serializer.IsWriting( ) && mergeToExistingIfLoading )
    {
        assert( false ); // this combination makes no sense
        mergeToExistingIfLoading = false;
    }

    // calling Serialize with non-applied changes? probably a bug somewhere (if not, just call ApplyDeferredObjectActions() before getting here)
    assert( m_deferredObjectActions.size() == 0 );
    if( m_deferredObjectActions.size() != 0 )
        return false;

    if( serializer.SerializeOpenChildElement( "VertexAsylumScene" ) )
    {
        if( serializer.IsReading() && !mergeToExistingIfLoading )
            Clear();

        string mergingName;
        if( mergeToExistingIfLoading )
        {
            VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<string>( "Name", mergingName ) );
            //vaVector3 mergingAmbientLight;
            //VERIFY_TRUE_RETURN_ON_FALSE( serializer.OldSerializeValue( "AmbientLight", mergingAmbientLight ) );
            vector<shared_ptr<vaLight>> mergingLights;
            
            if( serializer.GetVersion() > 0 )
                VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeArray( "Lights", "Light", mergingLights ) );
            else
                VERIFY_TRUE_RETURN_ON_FALSE( serializer.OldSerializeObjectVector( "Lights", mergingLights ) );
            
            m_lights.insert( m_lights.end(), mergingLights.begin(), mergingLights.end() );
        }
        else
        {
            VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<string>( "Name", m_name ) );
            //VERIFY_TRUE_RETURN_ON_FALSE( serializer.OldSerializeValue( "AmbientLight", m_lightAmbient ) );
            if( serializer.GetVersion() > 0 )
                VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeArray( "Lights", "Light", m_lights ) );
            else
                VERIFY_TRUE_RETURN_ON_FALSE( serializer.OldSerializeObjectVector( "Lights", m_lights ) );
            //serializer.SerializeArray( "Inputs", "Item", m_lights );
        }

        VERIFY_TRUE_RETURN_ON_FALSE( SerializeObjectsRecursive( serializer, "RootObjects", m_rootObjects, nullptr ) );

        serializer.Serialize<vaXMLSerializable>( "FogSphere", m_fog );
        ///*VERIFY_TRUE_RETURN_ON_FALSE(*/ m_fog.Serialize( serializer );

        bool ok = serializer.SerializePopToParentElement( "VertexAsylumScene" );
        assert( ok ); 

        if( serializer.IsReading() && ok )
        {
            return PostLoadInit();
        }

        return ok;
    }
    else
    {
        VA_LOG_WARNING( L"Unable to load scene, unexpected contents." );
    }

    return false;
}

bool vaScene::PostLoadInit( )
{
    ApplyDeferredObjectActions( );

    return true;
}

// void vaScene::UpdateUsedPackNames( )
// {
//     m_assetPackNames.clear();
//     for( auto sceneObject : m_allObjects )
//     {
//         sceneObject->RegisterUsedAssetPacks( std::bind( &vaScene::RegisterUsedAssetPackNameCallback, this, std::placeholders::_1 ) );
//     }
// }

void vaScene::RegisterUsedAssetPackNameCallback( const vaAssetPack & assetPack )
{
    for( size_t i = 0; i < m_assetPackNames.size(); i++ )
    {
        if( vaStringTools::ToLower( m_assetPackNames[i] ) == vaStringTools::ToLower( assetPack.GetName() ) )
        {
            // found, all is good
            return;
        }
    }
    // not found, add to list
    m_assetPackNames.push_back( assetPack.GetName() );
}

// void vaScene::LoadAndConnectAssets( )
// {
// }

#ifdef VA_IMGUI_INTEGRATION_ENABLED

static shared_ptr<vaSceneObject> ImGuiDisplaySceneObjectTreeRecursive( const vector<shared_ptr<vaSceneObject>> & elements, const shared_ptr<vaSceneObject> & selectedObject )
{
    ImGuiTreeNodeFlags defaultFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

    shared_ptr<vaSceneObject> ret = nullptr;

    for( int i = 0; i < elements.size(); i++ )
    {
        ImGuiTreeNodeFlags nodeFlags = defaultFlags | ((elements[i] == selectedObject)?(ImGuiTreeNodeFlags_Selected):(0)) | ((elements[i]->GetChildren().size() == 0)?(ImGuiTreeNodeFlags_Leaf):(0));
        bool nodeOpen = ImGui::TreeNodeEx( elements[i]->UIPropertiesItemGetUniqueID().c_str(), nodeFlags, elements[i]->GetName().c_str() );
                
        if( ImGui::IsItemClicked() )
        {
            ret = elements[i];
        }

        if( nodeOpen )
        {
            if( elements[i]->GetChildren().size() > 0 )
            {
                shared_ptr<vaSceneObject> retRec = ImGuiDisplaySceneObjectTreeRecursive( elements[i]->GetChildren(), selectedObject );
                if( retRec != nullptr )
                    ret = retRec;
            }

            ImGui::TreePop();
        }
    }
    return ret;
}
#endif

bool vaScene::Save( const wstring & fileName )
{
    // calling Save with non-applied changes? probably a bug somewhere (if not, just call ApplyDeferredObjectActions() before getting here)
    assert( m_deferredObjectActions.size() == 0 );

    vaFileStream fileOut;
    if( fileOut.Open( fileName, FileCreationMode::OpenOrCreate, FileAccessMode::Write ) )
    {
        vaXMLSerializer serializer;

        VA_LOG( L"Writing '%s'.", fileName.c_str() );

        if( !Serialize( serializer, false ) )
        {
            VA_LOG_WARNING( L"Error while serializing the scene" );
            fileOut.Close();
            return false;
        }

        serializer.WriterSaveToFile( fileOut );
        fileOut.Close();
        VA_LOG( L"Scene successfully saved to '%s'.", fileName.c_str() );
        return true;
    }
    else
    {
        VA_LOG_WARNING( L"Unable to save scene to '%s', file error.", fileName.c_str() );
        return false;
    }
}

bool vaScene::Load( const wstring & fileName, bool mergeToExisting )
{
    // calling Load with non-applied changes? probably a bug somewhere (if not, just call ApplyDeferredObjectActions() before getting here)
    assert( m_deferredObjectActions.size() == 0 );

    if( m_isInTick )
    {
        assert( false );    // Can't call this while in tick
        return false;
    }

    vaFileStream fileIn;
    if( vaFileTools::FileExists( fileName ) && fileIn.Open( fileName, FileCreationMode::Open ) )
    {
        vaXMLSerializer serializer( fileIn );
        fileIn.Close();

        if( serializer.IsReading( ) )
        {
            VA_LOG( L"Reading '%s'.", fileName.c_str() );

            if( !Serialize( serializer, mergeToExisting ) )
            {
                VA_LOG_WARNING( L"Error while serializing the scene" );
                fileIn.Close();
                return false;
            }
            VA_LOG( L"Scene successfully loaded from '%s', running PostLoadInit()", fileName.c_str() );
            return true;
        }
        else
        { 
            VA_LOG_WARNING( L"Unable to parse xml file '%s', file error.", fileName.c_str() );
            return false;
        }
    }
    else
    {
        VA_LOG_WARNING( L"Unable to load scene from '%s', file error.", fileName.c_str() );
        return false;
    }
}

void vaScene::UIPanelDraw( )
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED

    ImGui::PushItemWidth( 200.0f );

    if( ImGui::Button( " Rename " ) )
        ImGuiEx_PopupInputStringBegin( "Rename scene", m_name );
    if( ImGuiEx_PopupInputStringTick( m_name ) )
    {
        //m_name = vaStringTools::ToLower( m_name );
        VA_LOG( "Scene name changed to '%s'", m_name.c_str() );
    }

    ImGui::SameLine();

    if( ImGui::Button( " Delete all contents " ) )
    {
        Clear();
        return;
    }

    if( ImGui::Button( " Save As... " ) )
    {
        wstring fileName = vaFileTools::SaveFileDialog( L"", vaCore::GetExecutableDirectory(), L".xml scene files\0*.xml\0\0" );
        if( fileName != L"" )
        {
            if( vaFileTools::SplitPathExt( fileName ) == L"" ) // if no extension, add .xml
                fileName += L".xml";
            Save( fileName );
        }
    }

    ImGui::SameLine();
    if( ImGui::Button( " Load... " ) )
    {
        wstring fileName = vaFileTools::OpenFileDialog( L"", vaCore::GetExecutableDirectory(), L".xml scene files\0*.xml\0\0" );
        Load( fileName, false );
    }
    ImGui::SameLine();
    if( ImGui::Button( " Load and merge... " ) )
    {
        wstring fileName = vaFileTools::OpenFileDialog( L"", vaCore::GetExecutableDirectory(), L".xml scene files\0*.xml\0\0" );
        Load( fileName, true );
    }

    ImGui::Separator();

    if( m_allObjects.size() > 0 )
    {
        ImGui::Text( "Scene objects: %d", m_allObjects.size() );

        float uiListHeight = 120.0f;
        float uiPropertiesHeight = 180.0f;

        shared_ptr<vaSceneObject> selectedObject = m_UI_SelectedObject.lock();

        if( m_UI_ShowObjectsAsTree )
        {
            vector<shared_ptr<vaSceneObject>> elements = m_rootObjects;
            if( ImGui::BeginChild( "TreeFrame", ImVec2( 0.0f, uiListHeight ), true ) )
            {
                shared_ptr<vaSceneObject> objectClicked = ImGuiDisplaySceneObjectTreeRecursive( m_rootObjects, selectedObject );
                if( objectClicked != nullptr )
                {
                    selectedObject = objectClicked;
                    m_UI_SelectedObject = selectedObject;
                }
            }
            ImGui::EndChild();

            if( ImGui::BeginChild( "PropFrame", ImVec2( 0.0f, uiPropertiesHeight ), true ) )
            {
                if( selectedObject != nullptr )
                {
                    ImGui::PushID( selectedObject->UIPropertiesItemGetUniqueID( ).c_str( ) );
                    selectedObject->UIPropertiesItemDraw();
                    ImGui::PopID();
                }
                else
                {
                    ImGui::TextColored( ImVec4( 0.5f, 0.5f, 0.5f, 1.0f ), "Select an item to display properties" );
                }
            }
            ImGui::EndChild();
        }
        else
        {
            vaUIPropertiesItem * ptrsToDisplay[ 65536 ]; // if this ever becomes not enough, change DrawList into a template and make it accept allObjects directly...
            int countToShow = std::min( (int)m_allObjects.size(), (int)_countof(ptrsToDisplay) );
        
            int currentObject = -1;
            for( int i = 0; i < countToShow; i++ ) 
            {
                if( m_UI_SelectedObject.lock() == m_allObjects[i] )
                    currentObject = i;
                ptrsToDisplay[i] = m_allObjects[i].get();
            }

            vaUIPropertiesItem::DrawList( "Objects", ptrsToDisplay, countToShow, currentObject, 0.0f, uiListHeight, uiPropertiesHeight );

            if( currentObject >= 0 && currentObject < countToShow )
                m_UI_SelectedObject = m_allObjects[currentObject];
            else
                m_UI_SelectedObject.reset();
        }
    }
    else
    {
        ImGui::Text( "No objects" );
    }

    ImGui::Separator();

    ImGui::Text( "Scene lights: %d", m_lights.size() );

    if( m_lights.size() > 0 )
    {
        vaUIPropertiesItem * ptrsToDisplay[ 4096 ];
        int countToShow = std::min( (int)m_lights.size(), (int)_countof(ptrsToDisplay) );
        for( int i = 0; i < countToShow; i++ ) ptrsToDisplay[i] = m_lights[i].get();

        int currentLight = -1;
        for( int i = 0; i < countToShow; i++ ) 
        {
            if( m_UI_SelectedLight.lock() == m_lights[i] )
                currentLight = i;
            ptrsToDisplay[i] = m_lights[i].get();
        }

        vaUIPropertiesItem::DrawList( "Lights", ptrsToDisplay, countToShow, currentLight, 0.0f, 90, 200 );
        if( currentLight >= 0 && currentLight < countToShow )
            m_UI_SelectedLight = m_lights[currentLight];

        if( ImGui::Button( "Duplicate" ) )
        {
            m_lights.push_back( std::make_shared<vaLight>( *m_lights[currentLight] ) );
            m_lights.back()->Name += "_new";
            m_UI_SelectedLight = m_lights.back();
        }
        ImGui::SameLine();
        if( ImGui::Button( "Delete" ) )
        {
            m_lights.erase( m_lights.begin() + currentLight );
        }
    }
    else
    {
        ImGui::Text( "No lights" );
    }
    if( ImGui::Button( "Add light" ) )
    {
        m_lights.push_back( std::make_shared<vaLight>( vaLight::MakePoint( "NewLight", 0.2f, vaVector3( 0, 0, 0 ), vaVector3( 0, 0, 0 ) ) ) );
    }
    ImGui::Checkbox( "Debug draw scene lights", &m_UI_ShowLights );
    
    ImGui::Separator();
    
    ImGui::Text( "Scene cleanup tools:" );

    if( ImGui::Button( "Remove redundant hierarchy" ) )
    {
        for( int i = 0; i < m_allObjects.size(); i++ )
        {
            const shared_ptr<vaSceneObject> & obj = m_allObjects[i];
            if( obj->GetRenderMeshCount( ) == 0 && obj->GetChildren().size() == 1 )
            {
                DestroyObject( obj, false, true );
            }
        }
    }
    if( ImGui::Button( "Remove missing render mesh references" ) )
    {
        for( int i = 0; i < m_allObjects.size( ); i++ )
        {
            const shared_ptr<vaSceneObject> & obj = m_allObjects[i];
            for( int j = obj->GetRenderMeshCount( ) - 1; j >= 0; j-- )
            {
                if( obj->GetRenderMesh( j ) == nullptr )
                {
                    obj->RemoveRenderMeshRef( j );
                    VA_LOG( "Removed mesh %d from scene object '%s'", j, obj->GetName().c_str() );
                }
            }
        }
    }

    m_fog.UIItemDrawCollapsable( false, false );

    ImGui::PopItemWidth( );

#endif
}

void vaScene::DrawUI( vaCameraBase & camera, vaDebugCanvas2D & canvas2D, vaDebugCanvas3D & canvas3D )
{
    canvas2D;
    camera;

    float pulse = 0.5f * (float)vaMath::Sin( m_sceneTime * VA_PIf * 2.0f ) + 0.5f;

    auto selectedObject = m_UI_SelectedObject.lock();
    if( selectedObject != nullptr )
    {
        canvas3D.DrawBox( selectedObject->GetGlobalAABB(), vaVector4::ToBGRA( 0.0f, 0.0f + pulse, 0.0f, 1.0f ), vaVector4::ToBGRA( 0.5f, 0.5f, 0.5f, 0.1f ) );
    }

    if( m_UI_MouseClickIndicatorRemainingTime > 0 )
    {
        float factor = vaMath::Sin( (1.0f - m_UI_MouseClickIndicatorRemainingTime / m_UI_MouseClickIndicatorTotalTime) * VA_PIf );
        canvas3D.DrawSphere( m_UI_MouseClickIndicator, factor * 0.1f, 0xA0000000, 0x2000FF00 );
    }
    
    if( m_UI_ShowLights )
    {
        for( int i = 0; i < (int)m_lights.size(); i++ )
        {
            const vaLight & light = *m_lights[i];

            vaVector4 sphereColor = vaVector4( 0.5f, 0.5f, 0.5f, 0.1f );
            vaVector4 wireframeColor;
            switch( light.Type )
            {
                case( vaLight::Type::Ambient ):     wireframeColor = vaVector4( 0.5f, 0.5f, 0.5f, 1.0f ); break;
                case( vaLight::Type::Directional ): wireframeColor = vaVector4( 1.0f, 1.0f, 0.0f, 1.0f ); break;
                case( vaLight::Type::Point ):       wireframeColor = vaVector4( 0.0f, 1.0f, 0.0f, 1.0f ); break;
                case( vaLight::Type::Spot ):        wireframeColor = vaVector4( 0.0f, 0.0f, 1.0f, 1.0f ); break;
                default: assert( false );
            }

            float sphereSize = light.Size;

            bool isSelected = m_lights[i] == m_UI_SelectedLight.lock();
            if( isSelected )
                sphereColor.w += pulse * 0.2f - 0.09f;

            canvas3D.DrawSphere( light.Position, sphereSize, vaVector4::ToBGRA( wireframeColor ), vaVector4::ToBGRA( sphereColor ) );

            if( light.Type == vaLight::Type::Directional || light.Type == vaLight::Type::Spot )
                canvas3D.DrawLine( light.Position, light.Position + 2.0f * light.Direction * sphereSize, 0xFF000000 );

            if( light.Type == vaLight::Type::Spot && isSelected )
            {
                vaVector3 lightUp = vaVector3::Cross( vaVector3( 0.0f, 0.0f, 1.0f ), light.Direction );
                if( lightUp.Length() < 1e-3f )
                    lightUp = vaVector3::Cross( vaVector3( 0.0f, 1.0f, 0.0f ), light.Direction );
                lightUp = lightUp.Normalized();

                vaVector3 coneInner = vaVector3::TransformNormal( light.Direction, vaMatrix3x3::RotationAxis( lightUp, light.SpotInnerAngle ) );
                vaVector3 coneOuter = vaVector3::TransformNormal( light.Direction, vaMatrix3x3::RotationAxis( lightUp, light.SpotOuterAngle ) );

                float coneRadius = 0.1f + light.EffectiveRadius();

                const int lineCount = 50;
                for( int j = 0; j < lineCount; j++ )
                {
                    float angle = j / (float)(lineCount-1) * VA_PIf * 2.0f;
                    vaVector3 coneInnerR = vaVector3::TransformNormal( coneInner, vaMatrix3x3::RotationAxis( light.Direction, angle ) );
                    vaVector3 coneOuterR = vaVector3::TransformNormal( coneOuter, vaMatrix3x3::RotationAxis( light.Direction, angle ) );
                    canvas3D.DrawLine( light.Position, light.Position + coneRadius * sphereSize * coneInnerR, 0xFFFFFF00 );
                    canvas3D.DrawLine( light.Position, light.Position + coneRadius * sphereSize * coneOuterR, 0xFFFF0000 );
                }
            }

        }
    }
}

void vaScene::SetSkybox( vaRenderDevice & device, const string & texturePath, const vaMatrix3x3 & rotation, float colorMultiplier )
{
    shared_ptr<vaTexture> skyboxTexture = vaTexture::CreateFromImageFile( device, vaStringTools::SimpleNarrow(vaCore::GetExecutableDirectory()) + texturePath, vaTextureLoadFlags::Default );
    if( skyboxTexture == nullptr )
    {
        VA_LOG_WARNING( "vaScene::SetSkybox - unable to load '%'", texturePath.c_str() );
    }
    else
    {
        m_skyboxTexturePath     = texturePath;
        m_skyboxTexture         = skyboxTexture;
        m_skyboxRotation        = rotation;
        m_skyboxColorMultiplier = colorMultiplier;
    }
}

void vaScene::SetEnvmap( vaRenderDevice & device, const string & texturePath, const vaMatrix3x3 & rotation, float colorMultiplier )
{
    shared_ptr<vaTexture> skyboxTexture = vaTexture::CreateFromImageFile( device, vaStringTools::SimpleNarrow( vaCore::GetExecutableDirectory( ) ) + texturePath, vaTextureLoadFlags::Default );
    if( skyboxTexture == nullptr )
    {
        VA_LOG_WARNING( "vaScene::SetSkybox - unable to load '%'", texturePath.c_str( ) );
    }
    else
    {
        m_envmapTexturePath = texturePath;
        m_envmapTexture = skyboxTexture;
        m_envmapRotation = rotation;
        m_envmapColorMultiplier = colorMultiplier;
    }
}

vaDrawResultFlags vaScene::SelectForRendering( vaRenderSelection & renderSelection )
{
    // But we only have meshes here, no point calling this if it's empty?
    assert( renderSelection.MeshList != nullptr );
    if( renderSelection.MeshList == nullptr )
        return vaDrawResultFlags::None;

    vaDrawResultFlags drawResults = vaDrawResultFlags::None;

    // should I clear or add to existing?
    assert( renderSelection.MeshList->Count() == 0 );
    // renderSelection.MeshList->Reset();

    // g_cullOutsideBS = vaBoundingSphere( m_fog.Center, m_fog.RadiusOuter );
    // 
    // vaDebugCanvas3D::GetInstance().DrawSphere( g_cullOutsideBS, 0xFF000000, 0x05FF2020 );
    // 
    // vaDebugCanvas3D::GetInstance().DrawBox( g_cullInsideBox, 0xFF000000, 0x0520FF20 );

    for( auto object : m_allObjects )
        drawResults |= object->SelectForRendering( renderSelection );
    //renderSelection.MeshList.Insert()

    //g_doCull = false;
    return drawResults;
}

vector<shared_ptr<vaSceneObject>> vaScene::FindObjects( std::function<bool(vaSceneObject&obj)> searchCriteria )
{
    vector<shared_ptr<vaSceneObject>> ret;

    for( auto object : m_allObjects )
    {
        if( searchCriteria( *object ) )
            ret.push_back( object );
    }

    return std::move(ret);
}

void vaScene::OnMouseClick( const vaVector3 & worldClickLocation )
{
    worldClickLocation;
#if 0
    m_UI_MouseClickIndicatorRemainingTime = m_UI_MouseClickIndicatorTotalTime;
    m_UI_MouseClickIndicator = worldClickLocation;

    shared_ptr<vaSceneObject> closestHitObj = nullptr;
    float maxHitDist = 0.5f;
    for( auto rootObj : m_rootObjects )
        rootObj->FindClosestRecursive( worldClickLocation, closestHitObj, maxHitDist );

    VA_LOG( "vaScene - mouse clicked at (%.2f, %.2f, %.2f) world position.", worldClickLocation.x, worldClickLocation.y, worldClickLocation.z );

    // if same, just de-select
    if( closestHitObj != nullptr && m_UI_SelectedObject.lock() == closestHitObj )
        closestHitObj = nullptr;
    
    auto previousSel = m_UI_SelectedObject.lock();
    if( previousSel != nullptr )
    {
        VA_LOG( "vaScene - deselecting object '%s'", previousSel->GetName().c_str() );
    }

    if( closestHitObj != nullptr )
    {
        VA_LOG( "vaScene - selecting object '%s'", closestHitObj->GetName().c_str() );
    }

    m_UI_SelectedObject = closestHitObj;
#endif
}

shared_ptr<vaSceneObject> vaScene::CreateObjectWithSystemMesh( vaRenderDevice & device, const string & systemMeshName, const vaMatrix4x4 & transform )
{
    auto renderMeshAsset = device.GetAssetPackManager().GetDefaultPack()->Find( systemMeshName );
    if( renderMeshAsset == nullptr )
    {
        VA_WARN( "InsertSystemMeshByName failed - can't find system asset '%s'", systemMeshName.c_str() );
        return nullptr;
    }
    shared_ptr<vaRenderMesh> renderMesh = dynamic_cast<vaAssetRenderMesh*>(renderMeshAsset.get())->GetRenderMesh();

    auto sceneObject = CreateObject( "obj_" + systemMeshName, transform );
    sceneObject->AddRenderMeshRef( renderMesh );
    return sceneObject;
}

vector<shared_ptr<vaSceneObject>> vaScene::InsertAllPackMeshesToSceneAsObjects( vaScene & scene, vaAssetPack & pack, const vaMatrix4x4 & transform )
{
    vaVector3 scale, translation; vaQuaternion rotation; 
    transform.Decompose( scale, rotation, translation ); 

    vector<shared_ptr<vaSceneObject>> addedObjects;
    
    assert( !pack.IsBackgroundTaskActive() );
    std::unique_lock<mutex> assetStorageMutexLock( pack.GetAssetStorageMutex() );

    for( size_t i = 0; i < pack.Count( false ); i++ )
    {
        auto asset = pack.AssetAt( i, false );
        if( asset->Type == vaAssetType::RenderMesh )
        {
            shared_ptr<vaRenderMesh> renderMesh = dynamic_cast<vaAssetRenderMesh*>(asset.get())->GetRenderMesh();
            if( renderMesh == nullptr )
                continue;
            auto sceneObject = scene.CreateObject( "obj_" + asset->Name(), transform );
            sceneObject->AddRenderMeshRef( renderMesh );
            addedObjects.push_back( sceneObject );
        }
    }
    return addedObjects;
}