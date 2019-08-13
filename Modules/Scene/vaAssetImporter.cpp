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

#include "vaAssetImporter.h"

#include "Core/System/vaMemoryStream.h"
#include "Core/System/vaFileTools.h"

#include "IntegratedExternals/vaImguiIntegration.h"

using namespace VertexAsylum;

bool LoadFileContents_Assimp( const wstring & path, vaAssetImporter::ImporterContext & parameters, vaAssetImporter::LoadedContent * outContent );

vaAssetImporter::vaAssetImporter( vaRenderDevice & device ) : 
    vaUIPanel( "Asset Importer", 0,
#ifdef VA_ASSIMP_INTEGRATION_ENABLED
        true
#else
        false
#endif
        , vaUIPanel::DockLocation::DockedRight
        ),
    m_device( device )
{
}
vaAssetImporter::~vaAssetImporter( )
{
}


bool vaAssetImporter::LoadFileContents( const wstring & path, ImporterContext & parameters, LoadedContent * outContent )
{
    wstring filename;
    wstring ext;
    wstring textureSearchPath;
    vaFileTools::SplitPath( path.c_str( ), &textureSearchPath, &filename, &ext );
    ext = vaStringTools::ToLower( ext );
    if( ext == L".sdkmesh" )
    {
        // this path doesn't work anymore, sorry :(
        assert( false );

        // std::shared_ptr<vaMemoryStream> fileContents;
        // wstring usedPath;
        // 
        // // try asset paths
        // if( fileContents == nullptr )
        // {
        //     usedPath = path;
        //     fileContents = vaFileTools::LoadFileToMemoryStream( usedPath.c_str( ) );
        // }
        // 
        // // found? try load and return!
        // if( fileContents == nullptr )
        // {
        //     vaFileTools::EmbeddedFileData embeddedFile = vaFileTools::EmbeddedFilesFind( ( L"textures:\\" + path ).c_str( ) );
        //     if( embeddedFile.HasContents( ) )
        //         fileContents = embeddedFile.MemStream;
        //     //
        // }
        // return LoadFileContents_SDKMESH( fileContents, textureSearchPath, filename, parameters, outContent );
    }
    else
    {
        return LoadFileContents_Assimp( path, parameters, outContent );
    }

    //        VA_WARN( L"vaAssetImporter::LoadFileContents - don't know how to parse '%s' file type!", ext.c_str( ) );
    return false;
}

void vaAssetImporter::Clear( )
{
    m_tempAssetPack = nullptr;
    m_tempScene = nullptr;
}

void vaAssetImporter::UIPanelDraw( )
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
#ifdef VA_ASSIMP_INTEGRATION_ENABLED
    // importing assets UI
    {
        const char * importAssetsPopup = "   IMPORT NEW ASSETS   ";
        if( ImGui::Button( importAssetsPopup ) )
        {
            wstring fileName = vaFileTools::OpenFileDialog( L"", vaCore::GetExecutableDirectory( ) );
            if( vaFileTools::FileExists( fileName ) )
            {
                bool isFBX = vaStringTools::ToLower( vaFileTools::SplitPathExt( fileName ) ) == L".fbx";

                //strcpy_s( m_uiContext.ImportingModeNewFileNameBuff, sizeof(m_uiContext.ImportingModeNewFileNameBuff), "" );
                m_uiContext.ImportingPopupSelectedFile      = fileName;
                m_uiContext.ImportingPopupBaseTranslation   = vaVector3( 0.0f, 0.0f, 0.0f );
                m_uiContext.ImportingPopupBaseScaling       = vaVector3( 1.0f, 1.0f, 1.0f );
                m_uiContext.ImportingPopupRotateX90         = isFBX;
                m_uiContext.ImportingPopupRotateX180        = !isFBX;
                m_uiContext.Settings                        = ImporterSettings( );
                ImGui::OpenPopup( importAssetsPopup );
            }
        }

        if( ImGui::BeginPopupModal( importAssetsPopup ) )
        {
            string fileNameA = vaStringTools::SimpleNarrow( m_uiContext.ImportingPopupSelectedFile );
            ImGui::Text( "Importing file:" );
            ImGui::Text( "(idea: store these import settings default next to the importing file in .va_lastimport)" );
            ImGui::Text( " '%s'", fileNameA.c_str( ) );
            ImGui::Separator( );
            ImGui::Text( "Base transformation (applied to everything):" );
            if( ImGui::Checkbox( "Base rotate around X axis by 90 degrees  (for Maya assets)", &m_uiContext.ImportingPopupRotateX90 ) )
                if( m_uiContext.ImportingPopupRotateX90 )
                    m_uiContext.ImportingPopupRotateX180 = false;
            //if( ImGui::Checkbox( "Base rotate around X axis by 180 degrees (for Blender assets)", &m_uiContext.ImportingPopupRotateX180 ) )
            //    if( m_uiContext.ImportingPopupRotateX180 )
            //        m_uiContext.ImportingPopupRotateX90 = false;
            //ImGui::InputFloat3( "Base scaling", &m_uiContext.ImportingPopupBaseScaling.x );
            //ImGui::InputFloat3( "Base translation", &m_uiContext.ImportingPopupBaseTranslation.x );
            ImGui::Separator( );
            ImGui::Text( "Import options:" );
            ImGui::Checkbox( "Assimp: force generate normals", &m_uiContext.Settings.AIForceGenerateNormals );
            // ImGui::Checkbox( "Assimp: generate normals (if missing)", &m_uiContext.Settings.AIGenerateNormalsIfNeeded );
            // ImGui::Checkbox( "Assimp: generate smooth normals (if generating)", &m_uiContext.Settings.AIGenerateSmoothNormalsIfGenerating );
            ImGui::Checkbox( "Assimp: SplitLargeMeshes",  &m_uiContext.Settings.AISplitLargeMeshes );
            ImGui::Checkbox( "Assimp: FindInstances",  &m_uiContext.Settings.AIFindInstances );
            ImGui::Checkbox( "Assimp: OptimizeMeshes", &m_uiContext.Settings.AIOptimizeMeshes );
            ImGui::Checkbox( "Assimp: OptimizeGraph",  &m_uiContext.Settings.AIOptimizeGraph );
            //ImGui::Checkbox( "Regenerate tangents/bitangents",      &m_uiContext.ImportingRegenerateTangents );
            ImGui::Separator( );
            if( ImGui::Button( "          Start importing (can take a while, no progress bar)      " ) )
            {
                wstring fileName;
                vaFileTools::SplitPath( m_uiContext.ImportingPopupSelectedFile, nullptr, &fileName, nullptr );

                m_tempAssetPack = m_device.GetAssetPackManager().CreatePack( vaStringTools::SimpleNarrow(fileName) + "_AssetPack" );
                m_tempScene = std::make_shared<vaScene>( vaStringTools::SimpleNarrow(fileName) + "_Scene" );

                vaMatrix4x4 baseTransform;
                float rotationX = ( m_uiContext.ImportingPopupRotateX90 ) ? ( -VA_PIf * 0.5f ) : ( ( m_uiContext.ImportingPopupRotateX180 ) ? ( VA_PIf * 1.0f ) : ( 0.0f ) );
                baseTransform = vaMatrix4x4::Scaling( m_uiContext.ImportingPopupBaseScaling ) * vaMatrix4x4::RotationX( rotationX ) * vaMatrix4x4::Translation( m_uiContext.ImportingPopupBaseTranslation );

                vaAssetImporter::ImporterContext loadParams( m_device, *m_tempAssetPack, *m_tempScene, m_uiContext.Settings, baseTransform );

                vaAssetImporter::LoadFileContents( m_uiContext.ImportingPopupSelectedFile, loadParams );

                ImGui::CloseCurrentPopup( );
            }
            ImGui::SameLine( );
            if( ImGui::Button( "Cancel" ) )
            {
                ImGui::CloseCurrentPopup( );
            }

            ImGui::EndPopup( );
        }
    }

    if( ImGui::BeginChild( "ImporterDataStuff", ImVec2( 0.0f, 0.0f ), true ) )
    {
        ImGui::Text( "Imported data:" );
        if( ImGui::Button( "Clear all" ) )
        {
            Clear( );
        }
        if( m_tempScene == nullptr )
        {
            ImGui::Text( "Scene and assets will appear here after importing" );
        }
        else
        {
            m_tempAssetPack->DrawCollapsable( false, true );
            m_tempScene->DrawCollapsable( false, true );
        }
    }
    ImGui::End( );
#else
    ImGui::Text("VA_ASSIMP_INTEGRATION_ENABLED not defined!");
#endif
#endif // #ifdef VA_IMGUI_INTEGRATION_ENABLED
}
