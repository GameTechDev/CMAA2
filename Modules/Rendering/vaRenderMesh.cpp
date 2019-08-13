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

#include "vaRenderMesh.h"

#include "Rendering/vaStandardShapes.h"

#include "Core/System/vaFileTools.h"

#include "Core/vaXMLSerialization.h"

#include "IntegratedExternals/vaImguiIntegration.h"

using namespace VertexAsylum;

//vaRenderMeshManager & renderMeshManager, const vaGUID & uid

const int c_renderMeshFileVersion = 3;


vaRenderMesh::vaRenderMesh( vaRenderMeshManager & renderMeshManager, const vaGUID & uid ) : vaAssetResource(uid), m_trackee(renderMeshManager.GetRenderMeshTracker(), this), m_renderMeshManager( renderMeshManager )
{
    m_frontFaceWinding          = vaWindingOrder::CounterClockwise;
    m_boundingBox               = vaBoundingBox::Degenerate;
    //m_tangentBitangentValid     = true;
}

void vaRenderMesh::SetTriangleMesh( const shared_ptr<StandardTriangleMesh> & mesh )
{
    m_triangleMesh = mesh;
}

void vaRenderMesh::CreateTriangleMesh( vaRenderDevice & device, const vector<StandardVertex> & vertices, const vector<uint32> & indices )
{
    shared_ptr<StandardTriangleMesh> mesh = std::make_shared< vaRenderMesh::StandardTriangleMesh> ( device );

    mesh->Vertices()    = vertices;
    mesh->Indices()     = indices;

    SetTriangleMesh( mesh );

    UpdateAABB( );
}

void vaRenderMesh::UpdateAABB( ) 
{ 
    if( m_triangleMesh != nullptr ) 
        m_boundingBox = vaTriangleMeshTools::CalculateBounds( m_triangleMesh->Vertices() );
    else 
        m_boundingBox = vaBoundingBox::Degenerate; 
}

void vaRenderMesh::RebuildNormals( )
{
    m_triangleMesh->GenerateNormals( m_frontFaceWinding );
    m_triangleMesh->SetDataDirty( );
}

vaRenderMeshManager::vaRenderMeshManager( const vaRenderingModuleParams & params ) : 
    vaRenderingModule( params ), 
    vaUIPanel( "RenderMeshManager", 0, false, vaUIPanel::DockLocation::DockedLeftBottom ),
    m_constantsBuffer( params )
{
    m_isDestructing = false;
    m_renderMeshes.SetAddedCallback( std::bind( &vaRenderMeshManager::RenderMeshesTrackeeAddedCallback, this, std::placeholders::_1 ) );
    m_renderMeshes.SetBeforeRemovedCallback( std::bind( &vaRenderMeshManager::RenderMeshesTrackeeBeforeRemovedCallback, this, std::placeholders::_1, std::placeholders::_2 ) );

    //m_totalDrawListEntriesQueued = 0;
}

vaRenderMeshManager::~vaRenderMeshManager( )
{
    m_isDestructing = true;
    //m_renderMeshesMap.clear();

    // this must absolutely be true as they contain direct reference to this object
    assert( m_renderMeshes.size( ) == 0 );
}

shared_ptr<vaRenderMesh> vaRenderMeshManager::CreateRenderMesh( const vaGUID & uid ) 
{ 
    shared_ptr<vaRenderMesh> ret = shared_ptr<vaRenderMesh>( new vaRenderMesh( *this, uid ) ); 

    //if( !ret->UIDObject_IsCorrectlyTracked() )
    //{
    //    VA_LOG_ERROR_STACKINFO( "Error creating mesh; uid already used" );
    //    return nullptr;
    //}

    return ret;
}

void vaRenderMeshManager::RenderMeshesTrackeeAddedCallback( int newTrackeeIndex )
{
    newTrackeeIndex; // unreferenced
//    m_renderMeshesDrawLists.push_back( std::vector< MeshDrawListEntry >() );
//    assert( (int)m_renderMeshesDrawLists.size()-1 == newTrackeeIndex );
}

void vaRenderMeshManager::RenderMeshesTrackeeBeforeRemovedCallback( int toBeRemovedTrackeeIndex, int toBeReplacedByTrackeeIndex )
{
    toBeRemovedTrackeeIndex; // unreferenced
    toBeReplacedByTrackeeIndex; // unreferenced
//    assert( m_renderMeshesDrawLists.size( ) == m_renderMeshes.size( ) );
//    if( toBeReplacedByTrackeeIndex != -1 )
//        m_renderMeshesDrawLists[ toBeRemovedTrackeeIndex ] = m_renderMeshesDrawLists[ toBeReplacedByTrackeeIndex ];
//    m_renderMeshesDrawLists.pop_back();

    // // if ever need be, to make this faster track whether the mesh is in the library and if it is only then do the search below 
    // if( !m_isDestructing )
    // {
    //     const wstring & name = m_renderMeshes[toBeRemovedTrackeeIndex]->GetName( );
    //     const auto iterator = m_renderMeshesMap.find( name );
    //     if( iterator != m_renderMeshesMap.end( ) )
    //     {
    //         m_renderMeshesMap.erase( iterator );
    //     }
    // }
}

// void vaRenderMeshManager::ResetDrawEntries( )
// {
//     for( size_t i = 0; i < m_renderMeshesDrawLists.size(); i++ )
//         m_renderMeshesDrawLists[i].clear( );
//     m_totalDrawListEntriesQueued = 0;
// }

void vaRenderMeshManager::UIPanelDraw( )
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    static int selected = 0;
    ImGui::BeginChild( "left pane", ImVec2( 150, 0 ), true );
    for( int i = 0; i < 7; i++ )
    {
        char label[128];
        sprintf_s( label, _countof( label ), "MyObject %d", i );
        if( ImGui::Selectable( label, selected == i ) )
            selected = i;
    }
    ImGui::EndChild( );
    ImGui::SameLine( );

    // right
    ImGui::BeginGroup( );
    ImGui::BeginChild( "item view", ImVec2( 0, -ImGui::GetFrameHeightWithSpacing( ) ) ); // Leave room for 1 line below us
    ImGui::Text( "MyObject: %d", selected );
    ImGui::Separator( );
    ImGui::TextWrapped( "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. " );
    ImGui::EndChild( );
    ImGui::BeginChild( "buttons" );
    if( ImGui::Button( "Revert" ) ) { }
    ImGui::SameLine( );
    if( ImGui::Button( "Save" ) ) { }
    ImGui::EndChild( );
    ImGui::EndGroup( );
#endif
}

bool vaRenderMesh::SaveAPACK( vaStream & outStream )
{
    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<int32>( c_renderMeshFileVersion ) );

    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<int32>( (int32)m_frontFaceWinding ) );
    //VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<bool>( m_tangentBitangentValid ) );

    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValueVector<uint32>( m_triangleMesh->Indices() ) );
    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValueVector<StandardVertex>( m_triangleMesh->Vertices() ) );

    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<int32>( 1 ) ); //(int32)m_parts.size( ) ) );

    {
        VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<int32>( m_part.IndexStart ) );
        VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<int32>( m_part.IndexCount ) );

        shared_ptr<vaRenderMaterial> material = vaUIDObjectRegistrar::GetInstance().FindCached( m_part.MaterialID, m_part.CachedMaterialRef );
        VERIFY_TRUE_RETURN_ON_FALSE( SaveUIDObjectUID( outStream, material ) );
    }

    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<vaBoundingBox>( m_boundingBox ) );

    return true;
}

bool vaRenderMesh::LoadAPACK( vaStream & inStream )
{
    int32 fileVersion = 0;
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int32>( fileVersion ) );
    if( !( (fileVersion >= 3 ) && (fileVersion <= c_renderMeshFileVersion) ) )
    {
        VA_LOG( L"vaRenderMesh::Load(): unsupported file version" );
        return false;
    }

    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int32>( reinterpret_cast<int32&>(m_frontFaceWinding) ) );

    shared_ptr<StandardTriangleMesh> triMesh = std::make_shared< vaRenderMesh::StandardTriangleMesh> ( m_renderMeshManager.GetRenderDevice() );

    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValueVector<uint32>( triMesh->Indices() ) );
    
    // std::vector<StandardVertexOld> VerticesOld;
    // VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValueVector<StandardVertexOld>( VerticesOld ) );
    // triMesh->Vertices.resize( VerticesOld.size() );
    // for( int i = 0; i < VerticesOld.size(); i++ )
    //     triMesh->Vertices[i] = VerticesOld[i];

    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValueVector<StandardVertex>( triMesh->Vertices() ) );

    SetTriangleMesh( triMesh );


    int32 partCount = 0;
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int32>( partCount ) );

    // multipart no longer supported
    assert( partCount <= 1 );

    if( partCount > 0 )
    {
        VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int32>( m_part.IndexStart ) );
        VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int32>( m_part.IndexCount ) );
        VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<vaGUID>( m_part.MaterialID ) );
    }
    else
    {
        m_part = SubPart( );
    }

    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<vaBoundingBox>( m_boundingBox ) );

    return true;
}

bool vaRenderMesh::SerializeUnpacked( vaXMLSerializer & serializer, const wstring & assetFolder )
{
    assetFolder;
    int32 fileVersion = c_renderMeshFileVersion;
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<int32>( "FileVersion", fileVersion ) );
    VERIFY_TRUE_RETURN_ON_FALSE( fileVersion == c_renderMeshFileVersion );


    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<int32>( "FrontFaceWinding", reinterpret_cast<int32&>(m_frontFaceWinding) ) );
    //VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<bool>( "TangentBitangentValid", m_tangentBitangentValid ) );

    if( serializer.IsReading() )
    {
        shared_ptr<StandardTriangleMesh> triMesh = std::make_shared< vaRenderMesh::StandardTriangleMesh> ( m_renderMeshManager.GetRenderDevice() );
        SetTriangleMesh( triMesh );
        triMesh->SetDataDirty();
    }

    int32 indexCount = (int32)m_triangleMesh->Indices().size();
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<int32>( "IndexCount", indexCount ) );
    if( serializer.IsReading() )
    {
        m_triangleMesh->Indices().resize( indexCount );
        vaFileTools::ReadBuffer( assetFolder + L"/Indices.bin", &m_triangleMesh->Indices()[0], sizeof(uint32) * m_triangleMesh->Indices().size() );
    }
    else if( serializer.IsWriting( ) )
    {
        vaFileTools::WriteBuffer( assetFolder + L"/Indices.bin", &m_triangleMesh->Indices()[0], sizeof(uint32) * m_triangleMesh->Indices().size() );
    }
    else { assert( false ); return false; }

    int32 vertexCount = (int32)m_triangleMesh->Vertices().size();
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<int32>( "VertexCount", vertexCount ) );
    if( serializer.IsReading() )
    {
        m_triangleMesh->Vertices().resize( vertexCount );
        vaFileTools::ReadBuffer( assetFolder + L"/Vertices.bin", &m_triangleMesh->Vertices()[0], sizeof(vaRenderMesh::StandardVertex) * m_triangleMesh->Vertices().size() );
    }
    else if( serializer.IsWriting( ) )
    {
        vaFileTools::WriteBuffer( assetFolder + L"/Vertices.bin", &m_triangleMesh->Vertices()[0], sizeof(vaRenderMesh::StandardVertex) * m_triangleMesh->Vertices().size() );
    }
    else { assert( false ); return false; }
        

    // int32 partCount = 1;
    // VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<int32>( "PartCount", partCount ) );

    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<vaVector3>( "AABBMin", m_boundingBox.Min ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<vaVector3>( "AABBSize", m_boundingBox.Size ) );

    //assert( partCount < 9999 );
    //if( serializer.IsReading() )
    //{
    //    // multipart no longer supported
    //    // assert( partCount <= 1 );
    //}
    //if( partCount > 0 )
    {
        // string partName = vaStringTools::Format( "Part%04d", partCount );
        //VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeOpenChildElement( partName.c_str() ) );
        VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<int32>( "PartIndexStart", m_part.IndexStart ) );
        VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<int32>( "PartIndexCount", m_part.IndexCount ) );
        VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<vaGUID>( "PartMaterialID", m_part.MaterialID ) );

        if( serializer.IsWriting() )
        {
            shared_ptr<vaRenderMaterial> material = vaUIDObjectRegistrar::GetInstance().FindCached( m_part.MaterialID, m_part.CachedMaterialRef );
            // serializer.WriteAttribute( "MaterialID", vaCore::GUIDToStringA( (material == nullptr)?(vaCore::GUIDNull()):(material->UIDObject_GetUID()) ) );
            assert( ( (material == nullptr)?(vaCore::GUIDNull()):(material->UIDObject_GetUID()) ) == m_part.MaterialID );
        } 

        //VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializePopToParentElement( partName.c_str() ) );
    }
    return true;
}

// void vaRenderMesh::ReconnectDependencies( )
// {
//     for( size_t i = 0; i < m_parts.size(); i++ )
//     {
//         std::shared_ptr<vaRenderMaterial> materialSharedPtr;
//         vaUIDObjectRegistrar::GetInstance( ).ReconnectDependency<vaRenderMaterial>( materialSharedPtr, m_parts[i].MaterialID );
//         m_parts[i].Material = materialSharedPtr;
//     }
// }

void vaRenderMesh::RegisterUsedAssetPacks( std::function<void( const vaAssetPack & )> registerFunction )
{
    vaAssetResource::RegisterUsedAssetPacks( registerFunction );
    //for( auto subPart : m_parts )
    {
        shared_ptr<vaRenderMaterial> material = vaUIDObjectRegistrar::GetInstance().FindCached( m_part.MaterialID, m_part.CachedMaterialRef );
        if( material != nullptr )
        {
            material->RegisterUsedAssetPacks( registerFunction );
        }
        else
        {
            // asset is missing?
            assert( m_part.MaterialID == vaCore::GUIDNull() );
        }
    }
}

shared_ptr<vaRenderMesh> vaRenderMesh::Create( vaRenderDevice & device, const vaMatrix4x4 & transform, const std::vector<vaVector3> & vertices, const std::vector<vaVector3> & normals, const std::vector<vaVector2> & texcoords0, const std::vector<vaVector2> & texcoords1, const std::vector<uint32> & indices, vaWindingOrder frontFaceWinding, const vaGUID & uid )
{
    assert( ( vertices.size( ) == normals.size( ) ) && ( vertices.size( ) == texcoords0.size( ) ) && ( vertices.size( ) == texcoords1.size( ) ) );

    vector<vaRenderMesh::StandardVertex> newVertices( vertices.size( ) );

    for( int i = 0; i < (int)vertices.size( ); i++ )
    {
        vaRenderMesh::StandardVertex & svert = newVertices[i];
        svert.Position = vaVector3::TransformCoord( vertices[i], transform );
        svert.Color = 0xFFFFFFFF;
        svert.Normal = vaVector4( vaVector3::TransformNormal( normals[i], transform ), 0.0f );
        svert.TexCoord0 = texcoords0[i];
        svert.TexCoord1 = texcoords1[i];
    }

    shared_ptr<vaRenderMesh> mesh = device.GetMeshManager().CreateRenderMesh( uid );
    if( mesh == nullptr )
    {
        assert( false );
    }
    mesh->CreateTriangleMesh( device, newVertices, indices );
    mesh->SetPart( vaRenderMesh::SubPart( 0, (int)indices.size( ), weak_ptr<vaRenderMaterial>( ) ) );
    mesh->SetFrontFaceWindingOrder( frontFaceWinding );

    mesh->UIDObject_Track(); // needed for rendering system to work

    return mesh;
}

#define RMC_DEFINE_DATA                     \
    vector<vaVector3>  vertices;            \
    vector<vaVector3>  normals;             \
    vector<vaVector2>  texcoords0;          \
    vector<vaVector2>  texcoords1;          \
    vector<uint32>     indices; 

#define RMC_RESIZE_NTTT                     \
    normals.resize( vertices.size() );      \
    texcoords0.resize( vertices.size( ) );  \
    texcoords1.resize( vertices.size( ) );  \

shared_ptr<vaRenderMesh> vaRenderMesh::CreatePlane( vaRenderDevice & device, const vaMatrix4x4 & transform, float sizeX, float sizeY, const vaGUID & uid )
{
    RMC_DEFINE_DATA;

    vaStandardShapes::CreatePlane( vertices, indices, sizeX, sizeY );
    vaWindingOrder windingOrder = vaWindingOrder::CounterClockwise;

    RMC_RESIZE_NTTT;

    vaTriangleMeshTools::GenerateNormals( normals, vertices, indices, windingOrder );

    for( size_t i = 0; i < vertices.size( ); i++ )
    {
        texcoords0[i] = vaVector2( vertices[i].x / sizeX / 2.0f + 0.5f, vertices[i].y / sizeY / 2.0f + 0.5f );
        texcoords1[i] = texcoords0[i];
    }

    return Create( device, transform, vertices, normals, texcoords0, texcoords1, indices, vaWindingOrder::CounterClockwise, uid );
}

shared_ptr<vaRenderMesh> vaRenderMesh::CreateGrid( vaRenderDevice & device, const vaMatrix4x4 & transform, int dimX, int dimY, float sizeX, float sizeY, const vaVector2 & uvOffsetMul, const vaVector2 & uvOffsetAdd, const vaGUID & uid )
{
    RMC_DEFINE_DATA;

    vaStandardShapes::CreateGrid( vertices, indices, dimX, dimY, sizeX, sizeY );
    vaWindingOrder windingOrder = vaWindingOrder::CounterClockwise;

    RMC_RESIZE_NTTT;

    vaTriangleMeshTools::GenerateNormals( normals, vertices, indices, windingOrder );

    for( size_t i = 0; i < vertices.size( ); i++ )
    {
        // texel-to-pixel mapping
        texcoords0[i] = vaVector2( vertices[i].x / sizeX + 0.5f, vertices[i].y / sizeY + 0.5f );
        // custom UV
        texcoords1[i] = vaVector2( vaVector2::ComponentMul( texcoords0[i], uvOffsetMul ) + uvOffsetAdd );
    }

    return Create( device, transform, vertices, normals, texcoords0, texcoords1, indices, vaWindingOrder::CounterClockwise, uid );
}

// dummy texture coords
void FillDummyTT( const vector<vaVector3> & vertices, const vector<vaVector3> & normals, vector<vaVector2> & texcoords0, vector<vaVector2> & texcoords1 )
{
    normals;
    for( size_t i = 0; i < vertices.size( ); i++ )
    {
        texcoords0[i] = vaVector2( vertices[i].x / 2.0f + 0.5f, vertices[i].y / 2.0f + 0.5f );
        texcoords1[i] = texcoords0[i];
    }
}

shared_ptr<vaRenderMesh> vaRenderMesh::CreateTetrahedron( vaRenderDevice & device, const vaMatrix4x4 & transform, bool shareVertices, const vaGUID & uid )
{
    RMC_DEFINE_DATA;

    vaStandardShapes::CreateTetrahedron( vertices, indices, shareVertices );
    vaWindingOrder windingOrder = vaWindingOrder::Clockwise;

    RMC_RESIZE_NTTT;

    vaTriangleMeshTools::GenerateNormals( normals, vertices, indices, windingOrder );

    FillDummyTT( vertices, normals, texcoords0, texcoords1 );

    return Create( device, transform, vertices, normals, texcoords0, texcoords1, indices, windingOrder, uid );
}

shared_ptr<vaRenderMesh> vaRenderMesh::CreateCube( vaRenderDevice & device, const vaMatrix4x4 & transform, bool shareVertices, float edgeHalfLength, const vaGUID & uid )
{
    RMC_DEFINE_DATA;

    vaStandardShapes::CreateCube( vertices, indices, shareVertices, edgeHalfLength );
    vaWindingOrder windingOrder = vaWindingOrder::Clockwise;

    RMC_RESIZE_NTTT;

    vaTriangleMeshTools::GenerateNormals( normals, vertices, indices, windingOrder );

    FillDummyTT( vertices, normals, texcoords0, texcoords1 );

    return Create( device, transform, vertices, normals, texcoords0, texcoords1, indices, windingOrder, uid );
}

shared_ptr<vaRenderMesh> vaRenderMesh::CreateOctahedron( vaRenderDevice & device, const vaMatrix4x4 & transform, bool shareVertices, const vaGUID & uid )
{
    RMC_DEFINE_DATA;

    vaStandardShapes::CreateOctahedron( vertices, indices, shareVertices );
    vaWindingOrder windingOrder = vaWindingOrder::Clockwise;

    RMC_RESIZE_NTTT;

    vaTriangleMeshTools::GenerateNormals( normals, vertices, indices, windingOrder );

    FillDummyTT( vertices, normals, texcoords0, texcoords1 );

    return Create( device, transform, vertices, normals, texcoords0, texcoords1, indices, windingOrder, uid );
}

shared_ptr<vaRenderMesh> vaRenderMesh::CreateIcosahedron( vaRenderDevice & device, const vaMatrix4x4 & transform, bool shareVertices, const vaGUID & uid )
{
    RMC_DEFINE_DATA;

    vaStandardShapes::CreateIcosahedron( vertices, indices, shareVertices );
    vaWindingOrder windingOrder = vaWindingOrder::Clockwise;

    RMC_RESIZE_NTTT;

    vaTriangleMeshTools::GenerateNormals( normals, vertices, indices, windingOrder );

    FillDummyTT( vertices, normals, texcoords0, texcoords1 );

    return Create( device, transform, vertices, normals, texcoords0, texcoords1, indices, windingOrder, uid );
}

shared_ptr<vaRenderMesh> vaRenderMesh::CreateDodecahedron( vaRenderDevice & device, const vaMatrix4x4 & transform, bool shareVertices, const vaGUID & uid )
{
    RMC_DEFINE_DATA;

    vaStandardShapes::CreateDodecahedron( vertices, indices, shareVertices );
    vaWindingOrder windingOrder = vaWindingOrder::Clockwise;

    RMC_RESIZE_NTTT;

    vaTriangleMeshTools::GenerateNormals( normals, vertices, indices, windingOrder );

    FillDummyTT( vertices, normals, texcoords0, texcoords1 );

    return Create( device, transform, vertices, normals, texcoords0, texcoords1, indices, windingOrder, uid );
}

shared_ptr<vaRenderMesh> vaRenderMesh::CreateSphere( vaRenderDevice & device, const vaMatrix4x4 & transform, int tessellationLevel, bool shareVertices, const vaGUID & uid )
{
    RMC_DEFINE_DATA;

    vaStandardShapes::CreateSphere( vertices, indices, tessellationLevel, shareVertices );
    vaWindingOrder windingOrder = vaWindingOrder::Clockwise;

    RMC_RESIZE_NTTT;

    vaTriangleMeshTools::GenerateNormals( normals, vertices, indices, windingOrder );

    FillDummyTT( vertices, normals, texcoords0, texcoords1 );

    return Create( device, transform, vertices, normals, texcoords0, texcoords1, indices, windingOrder, uid );
}

shared_ptr<vaRenderMesh> vaRenderMesh::CreateCylinder( vaRenderDevice & device, const vaMatrix4x4 & transform, float height, float radiusBottom, float radiusTop, int tessellation, bool openTopBottom, bool shareVertices, const vaGUID & uid )
{
    RMC_DEFINE_DATA;

    vaStandardShapes::CreateCylinder( vertices, indices, height, radiusBottom, radiusTop, tessellation, openTopBottom, shareVertices );
    vaWindingOrder windingOrder = vaWindingOrder::Clockwise;

    RMC_RESIZE_NTTT;

    vaTriangleMeshTools::GenerateNormals( normals, vertices, indices, windingOrder );

    FillDummyTT( vertices, normals, texcoords0, texcoords1 );

    return Create( device, transform, vertices, normals, texcoords0, texcoords1, indices, windingOrder, uid );
}

shared_ptr<vaRenderMesh> vaRenderMesh::CreateTeapot( vaRenderDevice & device, const vaMatrix4x4 & transform, const vaGUID & uid )
{
    RMC_DEFINE_DATA;

    vaStandardShapes::CreateTeapot( vertices, indices );
    vaWindingOrder windingOrder = vaWindingOrder::Clockwise;

    RMC_RESIZE_NTTT;

    vaTriangleMeshTools::GenerateNormals( normals, vertices, indices, windingOrder );

    FillDummyTT( vertices, normals, texcoords0, texcoords1 );

    return Create( device, transform, vertices, normals, texcoords0, texcoords1, indices, windingOrder, uid );
}

vaDrawResultFlags vaRenderMeshManager::Draw( vaSceneDrawContext & drawContext, const vaRenderMeshDrawList & list, vaBlendMode blendMode, vaRenderMeshDrawFlags drawFlags, std::function< void( const vaRenderMeshDrawList::Entry & entry, const vaRenderMaterial & material, vaGraphicsItem & renderItem ) > globalCustomizer )
{
    vaDrawResultFlags drawResults = vaDrawResultFlags::None;

    vaRenderMaterialShaderType shaderType;
    switch( drawContext.OutputType )
    {
    case( vaDrawContextOutputType::DepthOnly ):     shaderType = vaRenderMaterialShaderType::DepthOnly;     break;
    case( vaDrawContextOutputType::Forward   ):     shaderType = vaRenderMaterialShaderType::Forward;       break;
    case( vaDrawContextOutputType::Deferred  ):     shaderType = vaRenderMaterialShaderType::Deferred;      break;
    case( vaDrawContextOutputType::CustomShadow ):  shaderType = vaRenderMaterialShaderType::CustomShadow;  break;
    default:
        assert( false );
        return vaDrawResultFlags::UnspecifiedError;
        break;
    }

    drawContext.RenderDeviceContext.BeginItems( vaRenderTypeFlags::Graphics );


    bool reverseOrder           = false; //(drawFlags & vaRenderMeshDrawFlags::ReverseOrder          ) != 0;
    bool skipTransparencies     = (drawFlags & vaRenderMeshDrawFlags::SkipTransparencies    ) != 0;
    bool skipNonTransparencies  = (drawFlags & vaRenderMeshDrawFlags::SkipNonTransparencies ) != 0;
    bool skipNonShadowCasters   = (drawFlags & vaRenderMeshDrawFlags::SkipNonShadowCasters  ) != 0;
    
    bool enableDepthTest        = (drawFlags & vaRenderMeshDrawFlags::EnableDepthTest       ) != 0;
    bool invertDepthTest        = (drawFlags & vaRenderMeshDrawFlags::InvertDepthTest       ) != 0;
    bool enableDepthWrite       = (drawFlags & vaRenderMeshDrawFlags::EnableDepthWrite      ) != 0;
    bool depthTestIncludesEqual = (drawFlags & vaRenderMeshDrawFlags::DepthTestIncludesEqual) != 0;

    bool depthEnable            = enableDepthTest || enableDepthWrite;
    bool useReversedZ           = (invertDepthTest)?(!drawContext.Camera.GetUseReversedZ()):(drawContext.Camera.GetUseReversedZ());

    vaComparisonFunc depthFunc  = vaComparisonFunc::Always;
    if( enableDepthTest )
        depthFunc               = ( depthTestIncludesEqual ) ? ( ( useReversedZ )?( vaComparisonFunc::GreaterEqual ):( vaComparisonFunc::LessEqual ) ):( ( useReversedZ )?( vaComparisonFunc::Greater ):( vaComparisonFunc::Less ) );

    for( int i = (reverseOrder)?(list.Count()-1):(0); (reverseOrder)?(i>=0):(i < list.Count()); (reverseOrder)?(i--):(i++) )
    {
        vaGraphicsItem renderItem;
        renderItem.SceneDrawContext = &drawContext;

        // set our main constant buffer
        renderItem.ConstantBuffers[ RENDERMESH_CONSTANTSBUFFERSLOT ] = m_constantsBuffer;

        // these could be set outside of the loop but we'll keep them here in case of any overrides (RenderItemOverrides) messing up the states
        renderItem.BlendMode        = blendMode;
        renderItem.DepthFunc        = depthFunc;
        renderItem.Topology         = vaPrimitiveTopology::TriangleList;
        renderItem.DepthEnable      = depthEnable;
        renderItem.DepthWriteEnable = enableDepthWrite;

        const vaRenderMeshDrawList::Entry & entry = list[i];
        if( entry.Mesh == nullptr ) 
        { VA_WARN( "vaRenderMeshManagerDX11::Draw - drawing empty mesh" ); continue; }

        const vaRenderMesh & mesh = *entry.Mesh.get();
        if( mesh.GetTriangleMesh( ) == nullptr )
            { assert( false ); continue; }

        // we can only render our own meshes
        assert( &mesh.GetManager() == this );

        if( mesh.GetTriangleMesh( )->Indices().size() == 0 )
            continue;   // should this assert? I guess empty mesh is valid?

        // mesh data
        const shared_ptr<vaRenderMesh::StandardTriangleMesh> & triangleMesh = mesh.GetTriangleMesh( );

        triangleMesh->UpdateAndSetToRenderItem( drawContext.RenderDeviceContext, renderItem );

        // update per-instance constants
        RenderMeshConstants instanceConsts;
        {
            // instanceConsts.World = entry.Transform;
            //consts.Color = entry.Color;

            // this means 'do not override'
            instanceConsts.ColorOverride = vaVector4( 0.0f, 0.0f, 0.0f, 1.0f );

            // since we now support non-uniform scale, we need the 'normal matrix' to keep normals correct 
            // (for more info see : https://www.scratchapixel.com/lessons/mathematics-physics-for-computer-graphics/geometry/transforming-normals or http://www.lighthouse3d.com/tutorials/glsl-12-tutorial/the-normal-matrix/ )
            vaMatrix4x4 normalWorld = entry.Transform.InverseD( nullptr, false ).Transpose( );
            normalWorld.r0.w = 0.0f; normalWorld.r1.w = 0.0f; normalWorld.r2.w = 0.0f;
            normalWorld.r3.x = 0.0f; normalWorld.r3.y = 0.0f; normalWorld.r3.z = 0.0f; normalWorld.r3.w = 1.0f;
            // instanceConsts.NormalWorld = normalWorld;

            //if( drawType != vaDrawType::ShadowmapGenerate )
            {
                instanceConsts.WorldView = entry.Transform * drawContext.Camera.GetViewMatrix( );
                instanceConsts.ShadowWorldViewProj = vaMatrix4x4::Identity;

                instanceConsts.NormalWorldView = normalWorld * drawContext.Camera.GetViewMatrix( );
            }
        }

        const vaRenderMesh::SubPart & subPart = mesh.GetPart(); // parts[subPartIndex];

        if( subPart.IndexCount == 0 )
            continue;

        assert( (subPart.IndexStart + subPart.IndexCount) <= (int)triangleMesh->Indices().size() );
        
        shared_ptr<vaRenderMaterial> material = vaUIDObjectRegistrar::GetInstance().FindCached( subPart.MaterialID, subPart.CachedMaterialRef );
            
        if( material != nullptr && !material->SetToRenderItem( renderItem, shaderType ) )
        {
            // VA_WARN( "material->SetToRenderItem returns false, using default material instead" );
            material = nullptr;
            drawResults |= vaDrawResultFlags::AssetsStillLoading;
        }

        // no material found? should we warn? it might be ok sometimes too, just relying on default?
        if( material == nullptr  )
        {
            material = GetRenderDevice().GetMaterialManager().GetDefaultMaterial( );
            if( material != nullptr )
                material->SetToRenderItem( renderItem, shaderType );
            drawResults |= vaDrawResultFlags::AssetsStillLoading;
        }

        const vaRenderMaterial::MaterialSettings & materialSettings = material->GetMaterialSettings( );

        bool isWireframe = ( ( drawContext.RenderFlags & vaDrawContextFlags::DebugWireframePass ) != 0 ) || material->GetMaterialSettings( ).Wireframe;

        bool showMaterialSelected = mesh.GetUIShowSelected( ) || material->GetUIShowSelected();
        if( isWireframe )
        {
            instanceConsts.ColorOverride = vaVector4( 1.0f, 0.0f, 0.0f, 0.0f );
        }
        if( showMaterialSelected )
        {
            float highlight = 0.5f * (float)vaMath::Sin( drawContext.RenderDeviceContext.GetRenderDevice().GetTotalTime( ) * VA_PI * 2.0 ) + 0.5f;
            instanceConsts.ColorOverride = vaVector4( highlight, highlight, highlight, highlight );
        }

        m_constantsBuffer.Update( drawContext.RenderDeviceContext, instanceConsts );

        if( skipTransparencies )
        {
            assert( !skipNonTransparencies );
            if( material->GetMaterialSettings().Transparent )
                continue;
        }
        if( skipNonTransparencies )
        {
            assert( !skipTransparencies );
            if( !material->GetMaterialSettings( ).Transparent )
                continue;
        }
        if( skipNonShadowCasters )
        {
            if( !material->GetMaterialSettings().CastShadows )
                continue;
        }

        renderItem.FillMode                 = (isWireframe)?(vaFillMode::Wireframe):(vaFillMode::Solid);
        renderItem.CullMode                 = materialSettings.FaceCull;
        renderItem.FrontCounterClockwise    = mesh.GetFrontFaceWindingOrder() == vaWindingOrder::CounterClockwise;

        material->SetToRenderItem( renderItem, shaderType );

        renderItem.SetDrawIndexed( subPart.IndexCount, subPart.IndexStart, 0 );

        // apply overrides, if any
        if( entry.CustomHandler != nullptr )
            entry.CustomHandler->ApplyMeshCustomization( drawContext, entry, *material, renderItem );
        if( globalCustomizer )
            globalCustomizer( entry, *material, renderItem );

        drawResults |= drawContext.RenderDeviceContext.ExecuteItem( renderItem );
    }

    drawContext.RenderDeviceContext.EndItems();
    return drawResults;
}