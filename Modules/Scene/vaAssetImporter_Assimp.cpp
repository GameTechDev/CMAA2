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
#include "Rendering/vaStandardShapes.h"

#include "IntegratedExternals/vaAssimpIntegration.h"

#include "Core/System/vaFileTools.h"

// #include <d3d11_1.h>
// #include <DirectXMath.h>
// #pragma warning(disable : 4324 4481)
//
// #include <exception>

//#include <mutex>

using namespace VertexAsylum;

#ifdef VA_ASSIMP_INTEGRATION_ENABLED

namespace 
{ 
    class myLogInfoStream : public Assimp::LogStream
    {
    public:
            myLogInfoStream()   { }
            ~myLogInfoStream()  { }

            void write(const char* message)
            {
                string messageStr = vaStringTools::Trim( message, "\n" );
                VA_LOG( "Assimp info    : %s", messageStr.c_str() );
            }
    };
    class myLogWarningStream : public Assimp::LogStream
    {
    public:
            myLogWarningStream()   { }
            ~myLogWarningStream()  { }

            void write(const char* message)
            {
                string messageStr = vaStringTools::Trim( message, "\n" );
                VA_LOG( "Assimp warning : %s", messageStr.c_str() );
            }
    };
    class myLogErrorStream : public Assimp::LogStream
    {
    public:
            myLogErrorStream()   { }
            ~myLogErrorStream()  { }

            void write(const char* message)
            {
                string messageStr = vaStringTools::Trim( message, "\n" );
                VA_LOG_ERROR( "Assimp error   : %s", messageStr.c_str() );
            }
    };
    class myLoggersRAII
    {
        Assimp::LogStream *     m_logInfoStream   ;
        Assimp::LogStream *     m_logWarningStream;
        Assimp::LogStream *     m_logErrorStream  ;

    public:
        myLoggersRAII( vaAssetImporter::ImporterContext & parameters )
        {
            Assimp::DefaultLogger::create( "AsiimpLog.txt", Assimp::Logger::NORMAL, aiDefaultLogStream_FILE );
            m_logInfoStream       = parameters.Settings.EnableLogInfo      ? (new myLogInfoStream()    ):(nullptr);
            m_logWarningStream    = parameters.Settings.EnableLogWarning   ? (new myLogWarningStream() ):(nullptr);
            m_logErrorStream      = parameters.Settings.EnableLogError     ? (new myLogErrorStream()   ):(nullptr);
            // set loggers
            if( m_logInfoStream       != nullptr )  Assimp::DefaultLogger::get()->attachStream( m_logInfoStream,        Assimp::Logger::Info ); // | Assimp::Logger::Debugging );
            if( m_logWarningStream    != nullptr )  Assimp::DefaultLogger::get()->attachStream( m_logWarningStream,     Assimp::Logger::Warn );
            if( m_logErrorStream      != nullptr )  Assimp::DefaultLogger::get()->attachStream( m_logErrorStream,       Assimp::Logger::Err );
        }

        ~myLoggersRAII( )
        {
            if( m_logInfoStream       != nullptr )  Assimp::DefaultLogger::get()->detatchStream( m_logInfoStream,        Assimp::Logger::Info ); // | Assimp::Logger::Debugging );
            if( m_logWarningStream    != nullptr )  Assimp::DefaultLogger::get()->detatchStream( m_logWarningStream,     Assimp::Logger::Warn );
            if( m_logErrorStream      != nullptr )  Assimp::DefaultLogger::get()->detatchStream( m_logErrorStream,       Assimp::Logger::Err );
            if( m_logInfoStream       != nullptr )  delete m_logInfoStream   ;
            if( m_logWarningStream    != nullptr )  delete m_logWarningStream;
            if( m_logErrorStream      != nullptr )  delete m_logErrorStream  ;
            Assimp::DefaultLogger::kill();
        }
    };


    struct LoadingTempStorage
    {
        struct LoadedTexture
        {
            const aiTexture *                   AssimpTexture;
            shared_ptr<vaAssetTexture>          Texture;
            string                              OriginalPath;
            vaTextureLoadFlags                  TextureLoadFlags;
            vaTextureContentsType               TextureContentsType;

            LoadedTexture( const aiTexture * assimpTexture, const shared_ptr<vaAssetTexture> & texture, const string & originalPath, vaTextureLoadFlags textureLoadFlags, vaTextureContentsType textureContentsType )
                : AssimpTexture( assimpTexture ), Texture(texture), OriginalPath(originalPath), TextureLoadFlags( textureLoadFlags ), TextureContentsType( textureContentsType )
            { }
        };

        struct LoadedMaterial
        {
            const aiMaterial *                  AssimpMaterial;
            shared_ptr<vaAssetRenderMaterial>   Material;

            LoadedMaterial( const aiMaterial * assimpMaterial, const shared_ptr<vaAssetRenderMaterial> & material )
                : AssimpMaterial( assimpMaterial ), Material( material ) 
            { }
        };

        struct LoadedMesh
        {
            const aiMesh *                      AssimpMesh;
            shared_ptr<vaAssetRenderMesh>       Mesh;

            LoadedMesh( const aiMesh * assimpMesh, const shared_ptr<vaAssetRenderMesh> & mesh )
                : AssimpMesh( assimpMesh ), Mesh( mesh ) 
            { }
        };

        wstring                                     ImportDirectory;
        wstring                                     ImportFileName;
        wstring                                     ImportExt;

        vector<LoadedTexture>                       LoadedTextures;
        vector<LoadedMaterial>                      LoadedMaterials;
        vector<LoadedMesh>                          LoadedMeshes;

        shared_ptr<vaAssetRenderMaterial>           FindMaterial( const aiMaterial * assimpMaterial )
        {
            for( int i = 0; i < LoadedMaterials.size(); i++ )
            {
                if( LoadedMaterials[i].AssimpMaterial == assimpMaterial )
                    return LoadedMaterials[i].Material;
            }
            return nullptr;
        }

        shared_ptr<vaAssetRenderMesh>               FindMesh( const aiMesh * assimpMesh )
        {
            for( int i = 0; i < LoadedMeshes.size(); i++ )
            {
                if( LoadedMeshes[i].AssimpMesh == assimpMesh )
                    return LoadedMeshes[i].Mesh;
            }
            return nullptr;
        }
    };
}


static shared_ptr<vaAssetTexture> FindOrLoadTexture( aiTexture * assimpTexture, const string & _path, LoadingTempStorage & tempStorage, vaAssetImporter::ImporterContext & parameters, vaAssetImporter::LoadedContent * outContent, vaTextureLoadFlags textureLoadFlags, vaTextureContentsType textureContentsType )
{
    string originalPath = vaStringTools::ToLower(_path);

    wstring filePath = vaStringTools::SimpleWiden( originalPath );

    for( int i = 0; i < tempStorage.LoadedTextures.size(); i++ )
    {
        if( (originalPath == tempStorage.LoadedTextures[i].OriginalPath) && (textureLoadFlags == tempStorage.LoadedTextures[i].TextureLoadFlags) && (textureContentsType == tempStorage.LoadedTextures[i].TextureContentsType) )
        {
            assert( assimpTexture == tempStorage.LoadedTextures[i].AssimpTexture );
            return tempStorage.LoadedTextures[i].Texture;
        }
    }

    wstring outDir, outName, outExt;
    vaFileTools::SplitPath( filePath, &outDir, &outName, &outExt );

    bool foundDDS = outExt == L".dds";
    if( !foundDDS && (parameters.Settings.TextureOnlyLoadDDS || parameters.Settings.TextureTryLoadDDS) )
    {
        wstring filePathDDS = outDir + outName + L".dds";
        if( vaFileTools::FileExists( filePathDDS ) )
        {
            filePath = filePathDDS;
            foundDDS = true;
        }
        else
        {
            filePathDDS = tempStorage.ImportDirectory + outDir + outName + L".dds";
            if( vaFileTools::FileExists( filePathDDS ) )
            {
                filePath = filePathDDS;
                foundDDS = true;
            }
        }
    }

    if( !foundDDS && parameters.Settings.TextureOnlyLoadDDS )
    {
        VA_LOG( L"VaAssetImporter_Assimp : TextureOnlyLoadDDS true but no .dds texture found when looking for '%s'", filePath.c_str() );
        return nullptr;
    }

    if( !vaFileTools::FileExists( filePath ) )
    {
        filePath = tempStorage.ImportDirectory + outDir + outName + L"." + outExt;
        if( !vaFileTools::FileExists( filePath ) )
        {
            VA_LOG( "VaAssetImporter_Assimp - Unable to find texture '%s'", filePath.c_str() );
            return nullptr;
        }
    }

    shared_ptr<vaTexture> textureOut = vaTexture::CreateFromImageFile( parameters.Device, filePath, textureLoadFlags, vaResourceBindSupportFlags::ShaderResource, textureContentsType );

    if( textureOut == nullptr )
    {
        VA_LOG( "VaAssetImporter_Assimp - Error while loading '%s'", filePath.c_str( ) );
        return nullptr;
    }

    assert( vaThreading::IsMainThread() ); // remember to lock asset global mutex and switch these to 'false'
    shared_ptr<vaAssetTexture> textureAssetOut = parameters.AssetPack.Add( textureOut, parameters.AssetPack.FindSuitableAssetName( vaStringTools::SimpleNarrow(outName), true ), true );

    tempStorage.LoadedTextures.push_back( LoadingTempStorage::LoadedTexture( assimpTexture, textureAssetOut, originalPath, textureLoadFlags, textureContentsType ) );

    if( outContent != nullptr )
        outContent->LoadedAssets.push_back( textureAssetOut );

    VA_LOG_SUCCESS( L"Assimp texture '%s' loaded ok.", filePath.c_str() );

    return textureAssetOut;
}

static bool ProcessTextures( const aiScene* loadedScene, LoadingTempStorage & tempStorage, vaAssetImporter::ImporterContext & parameters, vaAssetImporter::LoadedContent * outContent )
{
    loadedScene;
    tempStorage;
    parameters;
    outContent;

    if( loadedScene->HasTextures( ) )
    {
        VA_LOG_ERROR( "Assimp error: Support for meshes with embedded textures is not implemented" );
        return false;
        // for( int i = 0; i < loadedScene->mNumTextures; i++ )
        // {
        //     aiTexture * texture = loadedScene->mTextures[i];
        // }
    }

/*
	for( unsigned int m = 0; m < loadedScene->mNumMaterials; m++ )
	{
		int texIndex = 0;
		aiReturn texFound = AI_SUCCESS;

		aiString path;	// filename

		while (texFound == AI_SUCCESS)
		{
			texFound = loadedScene->mMaterials[m]->GetTexture( aiTextureType_DIFFUSE, texIndex, &path );
			//textureIdMap[path.data] = NULL; //fill map with textures, pointers still NULL yet

            FindOrLoadTexture( path.data, tempStorage, parameters, outContent, true, false );

			texIndex++;
		}
	}
    */


    return true;
}

static bool ProcessMaterials( const aiScene* loadedScene, LoadingTempStorage & tempStorage, vaAssetImporter::ImporterContext & parameters, vaAssetImporter::LoadedContent * outContent )
{
	for( unsigned int mi = 0; mi < loadedScene->mNumMaterials; mi++ )
	{
        aiMaterial * assimpMaterial = loadedScene->mMaterials[mi];

        aiString pathTmp;

        aiString        matName("unnamed");
        aiColor3D       matColorDiffuse         = aiColor3D( 1.0f, 1.0f, 1.0f );
        aiColor3D       matColorSpecular        = aiColor3D( 1.0f, 1.0f, 1.0f );
        aiColor3D       matColorAmbient         = aiColor3D( 1.0f, 1.0f, 1.0f );
        aiColor3D       matColorEmissive        = aiColor3D( 0.0f, 0.0f, 0.0f );
        aiColor3D       matColorTransparent     = aiColor3D( 0.0f, 0.0f, 0.0f );    // for transparent materials / not sure if it is intended for subsurface scattering?
        aiColor3D       matColorReflective      = aiColor3D( 0.0f, 0.0f, 0.0f );    // not sure what this is
        int             matWireframe            = 0;                                // Specifies whether wireframe rendering must be turned on for the material. 0 for false, !0 for true.	
        int             matTwosided             = 0;                                // Specifies whether meshes using this material must be rendered without backface culling. 0 for false, !0 for true.	
        aiShadingMode   matShadingModel         = aiShadingMode_Flat;               // 
        aiBlendMode     matBlendMode            = aiBlendMode_Default;              // Blend mode - not sure if applicable at all to us.
        float           matOpacity              = 1.0f;                             // Blend alpha multiplier.
        float           matSpecularPow          = 1.0f;                             // SHININESS - Defines the shininess of a phong-shaded material. This is actually the exponent of the phong specular equation; SHININESS=0 is equivalent to SHADING_MODEL=aiShadingMode_Gouraud.
        float           matSpecularMul          = 1.0f;                             // SHININESS_STRENGTH - Scales the specular color of the material.This value is kept separate from the specular color by most modelers, and so do we.
        float           matRefractI             = 1.0f;                             // Defines the Index Of Refraction for the material. That's not supported by most file formats.	Might be of interest for raytracing.
        float           matBumpScaling          = 1.0f;
        float           matDisplacementScaling  = 1.0f;
        float           matReflectivity         = 1.0f;

        assimpMaterial->Get( AI_MATKEY_NAME, matName );

        assimpMaterial->Get( AI_MATKEY_TWOSIDED, matTwosided );
        assimpMaterial->Get( AI_MATKEY_SHADING_MODEL, *(unsigned int*)&matShadingModel );
        assimpMaterial->Get( AI_MATKEY_ENABLE_WIREFRAME, *(unsigned int*)&matWireframe );
        assimpMaterial->Get( AI_MATKEY_BLEND_FUNC, *(unsigned int*)&matBlendMode );
        assimpMaterial->Get( AI_MATKEY_OPACITY, matOpacity );
        assimpMaterial->Get( AI_MATKEY_BUMPSCALING, matBumpScaling );
        assimpMaterial->Get( "$mat.displacementscaling", 0, 0, matDisplacementScaling );
        assimpMaterial->Get( AI_MATKEY_SHININESS, matSpecularPow );
        assimpMaterial->Get( AI_MATKEY_SHININESS_STRENGTH, matSpecularMul );
        assimpMaterial->Get( AI_MATKEY_REFLECTIVITY, matReflectivity );
        assimpMaterial->Get( AI_MATKEY_REFRACTI, matRefractI );
        assimpMaterial->Get( AI_MATKEY_COLOR_DIFFUSE, matColorDiffuse );
        assimpMaterial->Get( AI_MATKEY_COLOR_AMBIENT, matColorSpecular );
        assimpMaterial->Get( AI_MATKEY_COLOR_SPECULAR, matColorAmbient );
        assimpMaterial->Get( AI_MATKEY_COLOR_EMISSIVE, matColorEmissive );
        assimpMaterial->Get( AI_MATKEY_COLOR_TRANSPARENT, matColorTransparent );
        assimpMaterial->Get( AI_MATKEY_COLOR_REFLECTIVE, matColorReflective );
        //assimpMaterial->Get( AI_MATKEY_GLOBAL_BACKGROUND_IMAGE "?bg.global",0,0

        VA_LOG( "Assimp processing material '%s'", matName.data );

        shared_ptr<vaRenderMaterial> newMaterial = parameters.Device.GetMaterialManager().CreateRenderMaterial();
        newMaterial->InitializeDefaultMaterial();

        // not sure how to support other modes?
        assert( matBlendMode == aiBlendMode_Default );
       
        vaRenderMaterial::MaterialSettings matSettings;
        {
            matSettings.AlphaTest           = false;
            // consider it as a special flag for alpha test
            if( matOpacity == 0.0f )
            {
                matOpacity = 1.0f;
                matSettings.AlphaTest       = true;
            }
            matSettings.Transparent         = matOpacity < 0.999f;
            matSettings.FaceCull            = (matTwosided==0)?(vaFaceCull::Back):(vaFaceCull::None);
            matSettings.ReceiveShadows      = true;
            matSettings.CastShadows         = true;
            matSettings.Wireframe           = matWireframe != 0;
        }

        vaVector4 colorAlbedo       = vaVector4( matColorDiffuse.r, matColorDiffuse.g, matColorDiffuse.b, 1.0f );
        vaVector4 colorEmissive     = vaVector4( matColorEmissive.r, matColorEmissive.g, matColorEmissive.b, 0.0f );
        vaVector4 colorSpecular     = vaVector4( matColorSpecular.r, matColorSpecular.g, matColorSpecular.b, 0.5f );
        vaVector4 colorAmbient      = vaVector4( matColorAmbient.r, matColorAmbient.g, matColorAmbient.b, 0.0f );
        vaVector4 colorNormalmap    = vaVector4( 0.5f, 0.5f, 1.0f, 0.0f );

        newMaterial->SetInputByName( vaRenderMaterial::MaterialInput ( "Albedo",    vaRenderMaterial::MaterialInput::InputType::Color4, 
            vaRenderMaterial::MaterialInput::ValueVar( colorAlbedo ), vaRenderMaterial::MaterialInput::ValueVar( vaVector4( 1.0f, 1.0f, 1.0f, 1.0f ) ), vaRenderMaterial::MaterialInput::ValueVar( vaVector4( 0.0f, 0.0f, 0.0f, 0.0f ) ), vaRenderMaterial::MaterialInput::ValueVar( vaVector4( 1e6f, 1e6f, 1e6f, 1.0f ) ), false ), true );

        newMaterial->SetInputByName( vaRenderMaterial::MaterialInput ( "Normalmap",    vaRenderMaterial::MaterialInput::InputType::Color4, 
            vaRenderMaterial::MaterialInput::ValueVar( colorNormalmap ), vaRenderMaterial::MaterialInput::ValueVar( vaVector4( 0.5f, 0.5f, 1.0f, 0.0f ) ), vaRenderMaterial::MaterialInput::ValueVar( vaVector4( 0.0f, 0.0f, 0.0f, 0.0f ) ), vaRenderMaterial::MaterialInput::ValueVar( vaVector4( 1.0f, 1.0f, 1.0f, 0.0f ) ), false ), true );

        newMaterial->SetInputByName( vaRenderMaterial::MaterialInput ( "Specular",    vaRenderMaterial::MaterialInput::InputType::Color4, 
            vaRenderMaterial::MaterialInput::ValueVar( colorSpecular ), vaRenderMaterial::MaterialInput::ValueVar( vaVector4( 0.0f, 0.0f, 0.0f, 0.0f ) ), vaRenderMaterial::MaterialInput::ValueVar( vaVector4( 0.0f, 0.0f, 0.0f, 0.0f ) ), vaRenderMaterial::MaterialInput::ValueVar( vaVector4( 1e6f, 1e6f, 1e6f, 1e6f ) ), false ), true );

        newMaterial->SetInputByName( vaRenderMaterial::MaterialInput ( "Ambient",    vaRenderMaterial::MaterialInput::InputType::Color4, 
            vaRenderMaterial::MaterialInput::ValueVar( colorAmbient ), vaRenderMaterial::MaterialInput::ValueVar( vaVector4( 0.0f, 0.0f, 0.0f, 0.0f ) ), vaRenderMaterial::MaterialInput::ValueVar( vaVector4( 0.0f, 0.0f, 0.0f, 0.0f ) ), vaRenderMaterial::MaterialInput::ValueVar( vaVector4( 1e6f, 1e6f, 1e6f, 1.0f ) ), false ), true );

        newMaterial->SetInputByName( vaRenderMaterial::MaterialInput ( "Emissive",  vaRenderMaterial::MaterialInput::InputType::Color4, 
            vaRenderMaterial::MaterialInput::ValueVar( colorEmissive ), vaRenderMaterial::MaterialInput::ValueVar( vaVector4( 0.0f, 0.0f, 0.0f, 0.0f ) ), vaRenderMaterial::MaterialInput::ValueVar( vaVector4( 0.0f, 0.0f, 0.0f, 0.0f ) ), vaRenderMaterial::MaterialInput::ValueVar( vaVector4( 1e6f, 1e6f, 1e6f, 1.0f ) ), false ), true );

        //newMaterial->SetInputByName( vaRenderMaterial::MaterialInput ( "ColorTransparent",
        //                                vaRenderMaterial::MaterialInput::InputType::Color4,
        //                                vaRenderMaterial::MaterialInput::ValueVar( vaVector4( matColorTransparent.r, matColorTransparent.g, matColorTransparent.b, 0.0f ) ),
        //                                vaRenderMaterial::MaterialInput::ValueVar( vaVector4( 0.0f, 0.0f, 0.0f, 0.0f ) ), vaRenderMaterial::MaterialInput::ValueVar( vaVector4( 0.0f, 0.0f, 0.0f, 0.0f ) ), vaRenderMaterial::MaterialInput::ValueVar( vaVector4( 1.0f, 1.0f, 1.0f, 1.0f ) ),
        //                                false ), true );
        //newMaterial->SetInputByName( vaRenderMaterial::MaterialInput ( "ColorReflective",
        //                                vaRenderMaterial::MaterialInput::InputType::Color4,
        //                                vaRenderMaterial::MaterialInput::ValueVar( vaVector4( matColorReflective.r, matColorReflective.g, matColorReflective.b, 0.0f ) ),
        //                                vaRenderMaterial::MaterialInput::ValueVar( vaVector4( 0.0f, 0.0f, 0.0f, 0.0f ) ), vaRenderMaterial::MaterialInput::ValueVar( vaVector4( 0.0f, 0.0f, 0.0f, 0.0f ) ), vaRenderMaterial::MaterialInput::ValueVar( vaVector4( 1.0f, 1.0f, 1.0f, 1.0f ) ),
        //                                false ), true );
        newMaterial->SetInputByName( vaRenderMaterial::MaterialInput ( "SpecularMul",
                                        vaRenderMaterial::MaterialInput::InputType::Scalar,
                                        vaRenderMaterial::MaterialInput::ValueVar( matSpecularMul ),
                                        vaRenderMaterial::MaterialInput::ValueVar( 0.5f ), vaRenderMaterial::MaterialInput::ValueVar( 0.0f ), vaRenderMaterial::MaterialInput::ValueVar( 1e6f ), 
                                        false ), true );
        newMaterial->SetInputByName( vaRenderMaterial::MaterialInput ( "SpecularPow",
                                        vaRenderMaterial::MaterialInput::InputType::Scalar,
                                        vaRenderMaterial::MaterialInput::ValueVar( matSpecularPow ),
                                        vaRenderMaterial::MaterialInput::ValueVar( 16.0f ), vaRenderMaterial::MaterialInput::ValueVar( 0.0f ), vaRenderMaterial::MaterialInput::ValueVar( 1e6f ),
                                        false ), true );
        newMaterial->SetInputByName( vaRenderMaterial::MaterialInput ( "Opacity",
                                        vaRenderMaterial::MaterialInput::InputType::Scalar,
                                        vaRenderMaterial::MaterialInput::ValueVar( matOpacity ),
                                        vaRenderMaterial::MaterialInput::ValueVar( 1.0f ), vaRenderMaterial::MaterialInput::ValueVar( 0.0f ), vaRenderMaterial::MaterialInput::ValueVar( 1e6f ),
                                        false ), true );
        newMaterial->SetInputByName( vaRenderMaterial::MaterialInput ( "RefractionIndex",
                                        vaRenderMaterial::MaterialInput::InputType::Scalar,
                                        vaRenderMaterial::MaterialInput::ValueVar( matRefractI ),
                                        vaRenderMaterial::MaterialInput::ValueVar( 1.0f ), vaRenderMaterial::MaterialInput::ValueVar( 0.0f ), vaRenderMaterial::MaterialInput::ValueVar( 1e6f ),
                                        false ), true );
        newMaterial->SetInputByName( vaRenderMaterial::MaterialInput ( "BumpScaling",
                                        vaRenderMaterial::MaterialInput::InputType::Scalar,
                                        vaRenderMaterial::MaterialInput::ValueVar( matBumpScaling ),
                                        vaRenderMaterial::MaterialInput::ValueVar( 1.0f ), vaRenderMaterial::MaterialInput::ValueVar( 0.0f ), vaRenderMaterial::MaterialInput::ValueVar( 1e6f ),
                                        false ), true );
        newMaterial->SetInputByName( vaRenderMaterial::MaterialInput ( "DisplacementScaling",
                                        vaRenderMaterial::MaterialInput::InputType::Scalar,
                                        vaRenderMaterial::MaterialInput::ValueVar( matDisplacementScaling ),
                                        vaRenderMaterial::MaterialInput::ValueVar( 1.0f ), vaRenderMaterial::MaterialInput::ValueVar( 0.0f ), vaRenderMaterial::MaterialInput::ValueVar( 1e6f ),
                                        false ), true );
        
        // not used in the shader
//        newMaterial->SetInputByName( vaRenderMaterial::MaterialInput ( "Reflectivity",
//                                        vaRenderMaterial::MaterialInput::InputType::Scalar,
//                                        vaRenderMaterial::MaterialInput::ValueVar( matReflectivity ),
//                                        vaRenderMaterial::MaterialInput::ValueVar( 1.0f ), vaRenderMaterial::MaterialInput::ValueVar( 0.0f ), vaRenderMaterial::MaterialInput::ValueVar( 1e6f ),
//                                        false ), true );


        //////////////////////////////////////////////////////////////////////////
        // textures
        //aiTextureType texTypesSupported[] = { aiTextureType_NONE, aiTextureType_DIFFUSE, aiTextureType_SPECULAR, aiTextureType_NORMALS, aiTextureType_EMISSIVE, aiTextureType_UNKNOWN };
        for( aiTextureType texType = aiTextureType_NONE; texType < AI_TEXTURE_TYPE_MAX; texType = (aiTextureType)((int)texType+1) )
        {
            const int textureCount = (int)assimpMaterial->GetTextureCount( texType );
            for( int texIndex = 0; texIndex < textureCount; texIndex++ )
            {
                aiTextureMapping    texMapping      = aiTextureMapping_UV;
                uint32              texUVIndex      = 0;
                float               texBlendFactor  = 0.0f;
                aiTextureOp         texOp           = aiTextureOp_Add;
                aiTextureMapMode    texMapModes[2]  = {aiTextureMapMode_Wrap, aiTextureMapMode_Wrap};
                aiTextureFlags      texFlags        = (aiTextureFlags)0;

                //aiReturn texFound = assimpMaterial->GetTexture( texType, texIndex, &pathTmp, &texMapping, &texUVIndex, &texBlendFactor, &texOp, &texMapMode );
                aiReturn texFound = aiGetMaterialTexture( assimpMaterial, texType, texIndex, &pathTmp, &texMapping, &texUVIndex, &texBlendFactor, &texOp, texMapModes, (unsigned int *)&texFlags );

                if( ( texFlags & aiTextureFlags_UseAlpha ) != 0 )
                {
                    int dbg = 0;
                    dbg++;
                }
                if( ( texFlags & aiTextureFlags_IgnoreAlpha ) != 0 )
                {
                    int dbg = 0;
                    dbg++;
                }

                string texCountSuffix = (texIndex==0)?(""):(vaStringTools::Format("%d", texIndex ) );

                if( texFound == aiReturn_SUCCESS )
                {
                    if( texMapping != aiTextureMapping_UV )
                    {
                        VA_LOG( "Importer warning: Texture 's' mapping mode not supported (only aiTextureMapping_UV supported), skipping", pathTmp.data );
                        continue;
                    }
                    if( texUVIndex > 1 )
                    {
                        assert( false ); // it's actually supported, but dots are not connected
                        VA_LOG( "Importer warning: Texture 's' UV index out of supported range, skipping", pathTmp.data );
                        continue;
                    }

                    vaTextureLoadFlags textureLoadFlags = vaTextureLoadFlags::Default;
                    
                    // // I guess we always want MIPs? will not work for compressed texture formats so let's not enable by default
                    // textureLoadFlags |= vaTextureLoadFlags::AutogenerateMIPsIfMissing;

                    vaTextureContentsType contentsType = vaTextureContentsType::GenericColor;

                    // TEMP HACK
                    if( vaStringTools::ToLower(string( pathTmp.C_Str( ) )).find( "paris_coasters_01_ddna" ) != string::npos )
                    {
                        texType = aiTextureType_NORMALS;
                    }

                    // We always want sRGB with diffuse/albedo, specular, ambient and reflection colors (not sure about emissive so I'll leave it in; not sure about HDR textures either)
                    // Alpha channel is always linear anyway, so for cases where we store opacity in diffuse.a (albedo.a) or 'shininess' in specular.a that's still fine
                    if( texType == aiTextureType_DIFFUSE || texType == aiTextureType_SPECULAR || texType == aiTextureType_AMBIENT || texType == aiTextureType_EMISSIVE || texType == aiTextureType_LIGHTMAP || texType == aiTextureType_REFLECTION )
                    {
                        textureLoadFlags |= vaTextureLoadFlags::PresumeDataIsSRGB;
                        contentsType = vaTextureContentsType::GenericColor;
                    }
                    // Normals
                    else if( texType == aiTextureType_NORMALS )
                    {
                        textureLoadFlags |= vaTextureLoadFlags::PresumeDataIsLinear;
                        contentsType = vaTextureContentsType::NormalsXYZ_UNORM; 

                    }
                    // For height, opacity and others it only makes sense that they were stored in linear
                    else if( texType == aiTextureType_HEIGHT || texType == aiTextureType_OPACITY || texType == aiTextureType_DISPLACEMENT || texType == aiTextureType_SHININESS )
                    {
                        textureLoadFlags |= vaTextureLoadFlags::PresumeDataIsLinear;
                        contentsType = vaTextureContentsType::SingleChannelLinearMask;
                    }
                    else
                    {
                        // not implemented
                        assert( false );
                    }

                    shared_ptr<vaAssetTexture> newTextureAsset = FindOrLoadTexture( nullptr, pathTmp.C_Str(), tempStorage, parameters, outContent, textureLoadFlags, contentsType );
                     
                    if( (newTextureAsset == nullptr) || (newTextureAsset->GetTexture() == nullptr) )
                    {
                        VA_LOG_WARNING( "Assimp warning: Texture '%s' could not be imported, skipping", pathTmp.data );
                        continue;
                    }

                    if( texMapModes[0] != texMapModes[1] )
                    {
                        VA_LOG_WARNING( "Assimp warning: Texture '%s' has mismatched U & V texMapModes (%d, %d) - using first one for both", pathTmp.data, texMapModes[0], texMapModes[1] );
                    }
                    vaRenderMaterial::MaterialInput::TextureSamplerType samplerType = vaRenderMaterial::MaterialInput::TextureSamplerType::AnisotropicWrap;
                    if( texMapModes[0] == aiTextureMapMode_Wrap )
                    {
                        samplerType = vaRenderMaterial::MaterialInput::TextureSamplerType::AnisotropicWrap;
                    }
                    else if( texMapModes[0] == aiTextureMapMode_Clamp )
                    {
                        samplerType = vaRenderMaterial::MaterialInput::TextureSamplerType::AnisotropicClamp;
                    }
                    else if( texMapModes[0] == aiTextureMapMode_Mirror )
                    {
                        VA_LOG_WARNING( "Assim warning: Texture '%s' is using 'mirror' UV sampling mode but it is not supported by the materials" );
                    }
                    else if( texMapModes[0] == aiTextureMapMode_Decal )
                    {
                        VA_LOG_WARNING( "Assim warning: Texture '%s' is using 'decal' UV sampling mode but it is not supported by the materials" );
                    }
                    
                    if( texType == aiTextureType_DIFFUSE || texType == aiTextureType_AMBIENT )
                    {
                        newMaterial->SetInputByName( vaRenderMaterial::MaterialInput ( "Albedo"+texCountSuffix,
                                        vaRenderMaterial::MaterialInput::InputType::Color4,
                                        newTextureAsset->GetTexture()->UIDObject_GetUID(),
                                        texUVIndex, samplerType, colorAlbedo ), true );
                    }
                    else
                    if( texType == aiTextureType_SPECULAR )
                    {
                        newMaterial->SetInputByName( vaRenderMaterial::MaterialInput ( "Specular"+texCountSuffix,
                                        vaRenderMaterial::MaterialInput::InputType::Color4,
                                        newTextureAsset->GetTexture()->UIDObject_GetUID(),
                                        texUVIndex, samplerType, colorSpecular ), true );
                    }
                    else
                    if( texType == aiTextureType_NORMALS )
                    {
                        newMaterial->SetInputByName( vaRenderMaterial::MaterialInput ( "Normalmap"+texCountSuffix,
                                        vaRenderMaterial::MaterialInput::InputType::Vector4,
                                        newTextureAsset->GetTexture()->UIDObject_GetUID(),
                                        texUVIndex, samplerType, colorNormalmap ), true );
                    }
                    else
                    if( texType == aiTextureType_EMISSIVE )
                    {
                        newMaterial->SetInputByName( vaRenderMaterial::MaterialInput ( "Emissive"+texCountSuffix,
                                        vaRenderMaterial::MaterialInput::InputType::Color4,
                                        newTextureAsset->GetTexture()->UIDObject_GetUID(),
                                        texUVIndex, samplerType, colorEmissive ), true );
                    }
                    else
                    if( texType == aiTextureType_OPACITY )
                    {
                        newMaterial->SetInputByName( vaRenderMaterial::MaterialInput( "Opacity",
                            vaRenderMaterial::MaterialInput::InputType::Scalar,
                            newTextureAsset->GetTexture()->UIDObject_GetUID(),
                            texUVIndex, samplerType, vaVector4( matOpacity, matOpacity, matOpacity, matOpacity ) ), true );
                        
                        // let's just say having Opacity means alpha test if transparency not set (looks like matOpacity == 0 means alpha test?)
                        if( !matSettings.Transparent )
                        {
                            matSettings.AlphaTest = true;
                            matSettings.Transparent = false;
                        }
                    }
                    else
                    if( texType == aiTextureType_DISPLACEMENT )
                    {
                        newMaterial->SetInputByName( vaRenderMaterial::MaterialInput( "Displacement",
                            vaRenderMaterial::MaterialInput::InputType::Scalar,
                            newTextureAsset->GetTexture()->UIDObject_GetUID(),
                            texUVIndex, samplerType, vaVector4( 1.0f, 1.0f, 1.0f, 1.0f ) ), true );
                    }
                    else
                    if( texType == aiTextureType_HEIGHT )
                    {
                        newMaterial->SetInputByName( vaRenderMaterial::MaterialInput( "Height",
                            vaRenderMaterial::MaterialInput::InputType::Scalar,
                            newTextureAsset->GetTexture( )->UIDObject_GetUID( ),
                            texUVIndex, samplerType, vaVector4( 0.0f, 0.0f, 0.0f, 0.0f ) ), true );
                    }
                    else
                    //if( (texType == aiTextureType_NONE) || (texType == aiTextureType_UNKNOWN) || (texType == aiTextureType_UNKNOWN) )
                    {
                        assert( false );
                        VA_LOG_WARNING( "Assimp warning: texture '%s' is of unrecognizable type - will not be connected to the material", newTextureAsset->Name().c_str() );
                    }
                }
            }
        }

        // not sure in what case this isn't the case, so go with this!
        if( matSettings.AlphaTest )
        {
            if( matSettings.Transparent )
            {
                VA_LOG_WARNING( "Assimp warning: in '%s' material, both alpha test and transparency were enabled - we'll disable transparency." );
            }
            matSettings.Transparent = false;
        }
        if( matSettings.Transparent )
        {
            matSettings.FaceCull    = vaFaceCull::None;
            matSettings.CastShadows = false;
        }

        newMaterial->SetMaterialSettings( matSettings );

        string newMaterialName = matName.data;
        assert( vaThreading::IsMainThread() ); // remember to lock asset global mutex and switch these to 'false'
        newMaterialName = parameters.AssetPack.FindSuitableAssetName( newMaterialName, true );

        assert( vaThreading::IsMainThread() ); // remember to lock asset global mutex and switch these to 'false'
        auto materialAsset = parameters.AssetPack.Add( newMaterial, newMaterialName, true );

        VA_LOG_SUCCESS( "    material '%s' added", newMaterialName.c_str() );


        tempStorage.LoadedMaterials.push_back( LoadingTempStorage::LoadedMaterial( assimpMaterial, materialAsset ) );

        if( outContent != nullptr )
            outContent->LoadedAssets.push_back( materialAsset );
	}

    return true;
}

static bool ProcessMeshes( const aiScene* loadedScene, LoadingTempStorage & tempStorage, vaAssetImporter::ImporterContext & parameters, vaAssetImporter::LoadedContent * outContent )
{
	for( unsigned int mi = 0; mi < loadedScene->mNumMeshes; mi++ )
	{
        aiMesh * assimpMesh = loadedScene->mMeshes[mi];

        bool hasTangentBitangents = assimpMesh->HasTangentsAndBitangents();

        VA_LOG( "Assimp processing mesh '%s'", assimpMesh->mName.data );

        if( !assimpMesh->HasFaces() )
        { 
            assert( false );
            VA_LOG_ERROR( "Assimp error: mesh '%s' has no faces, skipping.", assimpMesh->mName.data );
            continue;
        }

        if( assimpMesh->mPrimitiveTypes != aiPrimitiveType_TRIANGLE )
        { 
            VA_LOG_WARNING( "Assimp warning: mesh '%s' reports non-triangle primitive types - those will be skipped during import.", assimpMesh->mName.data );
        }

        if( !assimpMesh->HasPositions() )
        { 
            assert( false );
            VA_LOG_ERROR( "Assimp error: mesh '%s' does not have positions, skipping.", assimpMesh->mName.data );
            continue;
        }
        if( !assimpMesh->HasNormals() )
        { 
            //assert( false );
            VA_LOG_ERROR( "Assimp error: mesh '%s' does not have normals, skipping.", assimpMesh->mName.data );
            continue;
        }

        vector<vaVector3>   vertices;
        vector<uint32>      colors;
        vector<vaVector3>   normals;
        //vector<vaVector4>   tangents;  // .w holds handedness
        vector<vaVector2>   texcoords0;
        vector<vaVector2>   texcoords1;

        vertices.resize( assimpMesh->mNumVertices );
        colors.resize( vertices.size( ) );
        normals.resize( vertices.size( ) );
        //tangents.resize( vertices.size( ) );
        texcoords0.resize( vertices.size( ) );
        texcoords1.resize( vertices.size( ) );


        for( int i = 0; i < (int)vertices.size(); i++ )
            vertices[i] = vaVector3( assimpMesh->mVertices[i].x, assimpMesh->mVertices[i].y, assimpMesh->mVertices[i].z );

        if( assimpMesh->HasVertexColors( 0 ) )
        {
            for( int i = 0; i < (int)colors.size(); i++ )
                colors[i] = vaVector4::ToRGBA( vaVector4( assimpMesh->mColors[0][i].r, assimpMesh->mColors[0][i].g, assimpMesh->mColors[0][i].b, assimpMesh->mColors[0][i].a ) );
        }
        else
        {
            for( int i = 0; i < (int)colors.size(); i++ )
                colors[i] = vaVector4::ToRGBA( vaVector4( 1.0f, 1.0f, 1.0f, 1.0f ) );
        }

        for( int i = 0; i < (int)vertices.size(); i++ )
            normals[i] = vaVector3( assimpMesh->mNormals[i].x, assimpMesh->mNormals[i].y, assimpMesh->mNormals[i].z );

        if( hasTangentBitangents )
        {
            VA_LOG_WARNING( "Assimp importer warning: mesh '%s' has (co)tangent space in the vertices but these are not supported by VA (generated in the pixel shader)" );
            // for( int i = 0; i < (int)tangents.size(); i++ )
            // {
            //     vaVector3 tangent   = vaVector3( assimpMesh->mTangents[i].x, assimpMesh->mTangents[i].y, assimpMesh->mTangents[i].z );
            //     vaVector3 bitangent = vaVector3( assimpMesh->mBitangents[i].x, assimpMesh->mBitangents[i].y, assimpMesh->mBitangents[i].z );
            //     float handedness = vaVector3::Dot( bitangent, vaVector3::Cross( normals[i], tangent ).Normalized() );
            //     tangents[i] = vaVector4( tangent, handedness );
            // }
        }
        else
        {
            // // VA_LOG_WARNING( "Assimp importer warning: mesh '%s' has no tangent space - ok if no normal maps used", assimpMesh->mName.data );
            // 
            // for( size_t i = 0; i < tangents.size( ); i++ )
            // {
            //     vaVector3 bitangent = ( vertices[i] + vaVector3( 0.0f, 0.0f, -5.0f ) ).Normalized( );
            //     if( vaMath::Abs( vaVector3::Dot( bitangent, normals[i] ) ) > 0.9f )
            //         bitangent = ( vertices[i] + vaVector3( -5.0f, 0.0f, 0.0f ) ).Normalized( );
            //     tangents[i] = vaVector4( vaVector3::Cross( bitangent, normals[i] ).Normalized( ), 1.0f );
            // }
        }

        vector<vaVector2> * uvsOut[] = { &texcoords0, &texcoords1 };

        for( int uvi = 0; uvi < 2; uvi++ )
        {
            vector<vaVector2> & texcoords = *uvsOut[uvi];

            if( assimpMesh->HasTextureCoords( uvi ) )
            {
                for( int i = 0; i < (int)texcoords0.size(); i++ )
                    texcoords[i] = vaVector2( assimpMesh->mTextureCoords[uvi][i].x, assimpMesh->mTextureCoords[uvi][i].y );
            }
            else
            {
                for( int i = 0; i < (int)texcoords0.size(); i++ )
                    texcoords[i] = vaVector2( 0.0f, 0.0f );
            }
        }

        bool indicesOk = true;
        vector<uint32>      indices;
        indices.reserve( assimpMesh->mNumFaces * 3 );
        for( int i = 0; i < (int)assimpMesh->mNumFaces; i ++ )
        {
            if( assimpMesh->mFaces[i].mNumIndices != 3 )
            {
                continue;
                //assert( false );
                //VA_LOG_ERROR( "Assimp error: mesh '%s' face has incorrect number of indices (3), skipping.", assimpMesh->mName.data );
                //indicesOk = false;
                //break;
            }
            indices.push_back( assimpMesh->mFaces[i].mIndices[0] );
            indices.push_back( assimpMesh->mFaces[i].mIndices[1] );
            indices.push_back( assimpMesh->mFaces[i].mIndices[2] );
        }
        if( !indicesOk )
            continue;

        auto materialAsset = tempStorage.FindMaterial( loadedScene->mMaterials[assimpMesh->mMaterialIndex] );
        auto material = materialAsset->GetRenderMaterial();

        // vector<vaRenderMesh::SubPart> parts;
        //parts.resize( 1 );
        //vaRenderMesh::SubPart & part = parts[0];
        vaRenderMesh::SubPart part;
        part.CachedMaterialRef = material;
        part.MaterialID = material->UIDObject_GetUID();
        part.IndexStart = 0;
        part.IndexCount = (int)indices.size();
        
        //shared_ptr<vaRenderMesh> newMesh = vaRenderMesh::Create( parameters.BaseTransform, vertices, normals, tangents, texcoords0, texcoords1, indices, vaWindingOrder::Clockwise );
        shared_ptr<vaRenderMesh> newMesh = vaRenderMesh::Create( parameters.Device, vaMatrix4x4::Identity, vertices, normals, texcoords0, texcoords1, indices, vaWindingOrder::Clockwise );
        newMesh->SetPart( part );
        //newMesh->SetTangentBitangentValid( hasTangentBitangents );

        string newMeshName = assimpMesh->mName.data;
        if( newMeshName == "" )
            newMeshName = "_" + materialAsset->Name();
        assert( vaThreading::IsMainThread() ); // remember to lock asset global mutex and switch these to 'false'
        newMeshName = parameters.AssetPack.FindSuitableAssetName( newMeshName, true );

        assert( vaThreading::IsMainThread() ); // remember to lock asset global mutex and switch these to 'false'
        shared_ptr<vaAssetRenderMesh> newAsset = parameters.AssetPack.Add( newMesh, newMeshName, true );

        VA_LOG_SUCCESS( "    mesh '%s' added", newMeshName.c_str() );

        tempStorage.LoadedMeshes.push_back( LoadingTempStorage::LoadedMesh( assimpMesh, newAsset ) );

        if( outContent != nullptr )
            outContent->LoadedAssets.push_back( newAsset );
    }

    return true;
}

static inline vaVector3     VAFromAI( const aiVector3D & val )     { return vaVector3( val.x, val.y, val.z ); }
static inline vaVector3     VAFromAI( const aiColor3D & val )      { return vaVector3( val.r, val.g, val.b ); }
static inline vaMatrix4x4   VAFromAI( const aiMatrix4x4 & val )    { return vaMatrix4x4( val.a1, val.a2, val.a3, val.a4, val.b1, val.b2, val.b3, val.b4, val.c1, val.c2, val.c3, val.c4, val.d1, val.d2, val.d3, val.d4 ); }

static bool ProcessNodesRecursive( const aiScene* loadedScene, aiNode* mNode, LoadingTempStorage & tempStorage, vaAssetImporter::ImporterContext & parameters, vaAssetImporter::LoadedContent * outContent, const shared_ptr<vaSceneObject> & parentSceneObject )
{
    string      name        = mNode->mName.C_Str();
    vaMatrix4x4 transform   = VAFromAI( mNode->mTransformation );

    if( parentSceneObject == nullptr )
        transform = parameters.BaseTransform * transform;

    shared_ptr<vaSceneObject> & newSceneObject = parameters.Scene.CreateObject( name, transform, parentSceneObject );
    
    for( int i = 0; i < (int)mNode->mNumMeshes; i++ )
    {
        aiMesh * meshPtr = loadedScene->mMeshes[ mNode->mMeshes[i] ];
        shared_ptr<vaAssetRenderMesh> meshAsset = tempStorage.FindMesh( meshPtr );
        if( meshAsset == nullptr )
        {
            VA_LOG_WARNING( "Node %s can't find mesh %d that was supposed to be loaded", name.c_str(), i );
        }
        else
        {
            newSceneObject->AddRenderMeshRef( meshAsset->GetRenderMesh() );
        }
    }

    for( int i = 0; i < (int)mNode->mNumChildren; i++ )
    {
        bool ret = ProcessNodesRecursive( loadedScene, mNode->mChildren[i], tempStorage, parameters, outContent, newSceneObject );
        if( !ret )
        {
            VA_LOG_ERROR( "Node %s child %d fatal processing error", name.c_str(), i );
            return false;
        }
    }
    return true;
}

static bool ProcessSceneNodes( const aiScene* loadedScene, LoadingTempStorage & tempStorage, vaAssetImporter::ImporterContext & parameters, vaAssetImporter::LoadedContent * outContent )
{
    vaScene & outScene = parameters.Scene;
    
    for( int i = 0; i < (int)loadedScene->mNumLights; i++ )
    {
        aiLight & light = *loadedScene->mLights[i];

        assert( false ); // never tested
        
        switch( light.mType )
        {
        case( aiLightSource_DIRECTIONAL ):  outScene.Lights().push_back( std::make_shared<vaLight>( vaLight::MakeDirectional(  light.mName.C_Str(), VAFromAI( light.mColorDiffuse ), VAFromAI( light.mDirection ) ) ) ); break;
        case( aiLightSource_POINT ):        outScene.Lights().push_back( std::make_shared<vaLight>( vaLight::MakePoint(        light.mName.C_Str(), light.mSize.Length(), VAFromAI( light.mColorDiffuse ), VAFromAI( light.mPosition ) ) ) ); break;
        case( aiLightSource_SPOT ):         outScene.Lights().push_back( std::make_shared<vaLight>( vaLight::MakeSpot(         light.mName.C_Str(), light.mSize.Length(), VAFromAI( light.mColorDiffuse ), VAFromAI( light.mPosition ), VAFromAI( light.mDirection ), light.mAngleInnerCone, light.mAngleOuterCone ) ) ); break;
        case( aiLightSource_AMBIENT ):      outScene.Lights().push_back( std::make_shared<vaLight>( vaLight::MakeAmbient(      light.mName.C_Str(), VAFromAI( light.mColorDiffuse ) ) ) ); break;

        case( aiLightSource_UNDEFINED ): 
        case( aiLightSource_AREA ): 
        default:
            VA_WARN( "Unrecognized or unsupported light type for light '%s'", light.mName.C_Str() ); 
            break;
        }
    }

    return ProcessNodesRecursive( loadedScene, loadedScene->mRootNode, tempStorage, parameters, outContent, nullptr );
}

static bool ProcessScene( /*const wstring & path, */ const aiScene* loadedScene, LoadingTempStorage & tempStorage, vaAssetImporter::ImporterContext & parameters, vaAssetImporter::LoadedContent * outContent )
{
    if( !ProcessTextures( loadedScene, tempStorage, parameters, outContent ) )
        return false;

    if( !ProcessMaterials( loadedScene, tempStorage, parameters, outContent ) )
        return false;

    if( !ProcessMeshes( loadedScene, tempStorage, parameters, outContent ) )
        return false;

    if( !ProcessSceneNodes( loadedScene, tempStorage, parameters, outContent ) )
        return false;

    return true;
}

bool LoadFileContents_Assimp( const wstring & path, vaAssetImporter::ImporterContext & parameters, vaAssetImporter::LoadedContent * outContent )
{
    // setup our loggers
    myLoggersRAII loggersRAII( parameters );

    // construct global importer and exporter instances
	Assimp::Importer importer;
	
	
	// aiString s;
    // imp.GetExtensionList(s);
    // vaLog::GetInstance().Add( "Assimp extension list: %s", s.data );

    const aiScene* loadedScene = nullptr;

    LoadingTempStorage tempStorage;
    vaFileTools::SplitPath( vaStringTools::ToLower( path ), &tempStorage.ImportDirectory, &tempStorage.ImportFileName, &tempStorage.ImportExt );

    {
        vaSimpleScopeTimerLog timerLog( vaStringTools::Format( L"Assimp parsing '%s'", path.c_str( ) ) );

        string apath = vaStringTools::SimpleNarrow( path );

        unsigned int flags = 0;
        //flags |= aiProcess_CalcTangentSpace;          // switching to shader-based (co)tangent compute
        flags |= aiProcess_JoinIdenticalVertices;
        flags |= aiProcess_ImproveCacheLocality;
        flags |= aiProcess_LimitBoneWeights;
        flags |= aiProcess_RemoveRedundantMaterials;
        flags |= aiProcess_Triangulate;
        flags |= aiProcess_GenUVCoords;
        flags |= aiProcess_SortByPType;                 // to more easily exclude (or separately handle) lines & points
        // flags |= aiProcess_FindDegenerates;          // causes issues
        // flags |= aiProcess_FixInfacingNormals;       // causes issues

        flags |= aiProcess_FindInvalidData;
        flags |= aiProcess_ValidateDataStructure;
        // flags |= aiProcess_TransformUVCoords;        // not yet needed

        if( parameters.Settings.AISplitLargeMeshes )
            flags |= aiProcess_SplitLargeMeshes;
        if( parameters.Settings.AIFindInstances )
            flags |= aiProcess_FindInstances;
        if( parameters.Settings.AIOptimizeMeshes )
            flags |= aiProcess_OptimizeMeshes;
        if( parameters.Settings.AIOptimizeGraph )
            flags |= aiProcess_OptimizeGraph;

        flags |= aiProcess_ConvertToLeftHanded;
        
        if( parameters.Settings.AIForceGenerateNormals )
            parameters.Settings.AIGenerateNormalsIfNeeded = true;

        // flags |= aiProcess_RemoveComponent;
        int removeComponentFlags = 0; //aiComponent_CAMERAS | aiComponent_LIGHTS; // aiComponent_COLORS

        if( parameters.Settings.AIGenerateNormalsIfNeeded )
        {
            if( parameters.Settings.AIGenerateSmoothNormalsIfGenerating )
            {
                flags |= aiProcess_GenSmoothNormals; 
                importer.SetPropertyFloat( AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, parameters.Settings.AIGenerateSmoothNormalsSmoothingAngle );
            }
            else
            {
                flags |= aiProcess_GenNormals; 
            }
            if( parameters.Settings.AIForceGenerateNormals )
            {
                flags |= aiProcess_RemoveComponent;
                removeComponentFlags |= aiComponent_NORMALS;
            }
       }

        importer.SetPropertyInteger( AI_CONFIG_PP_RVC_FLAGS, removeComponentFlags );

        importer.SetPropertyBool( "GLOB_MEASURE_TIME", true );

        //importer.SetProgressHandler()

        loadedScene = importer.ReadFile( apath.c_str(), flags );
        if( loadedScene == nullptr )
        {
            VA_LOG_ERROR( importer.GetErrorString() );
            return false; 
        }
    }

    {
        vaSimpleScopeTimerLog timerLog( vaStringTools::Format( L"Importing Assimp scene...", path.c_str( ) ) );

        bool ret = ProcessScene( loadedScene, tempStorage, parameters, outContent );
        parameters.Scene.ApplyDeferredObjectActions();
        return ret;
    }

   
//    importer.

    return false;
}

#else

bool LoadFileContents_Assimp( const wstring & path, vaAssetImporter::ImporterContext & parameters, vaAssetImporter::LoadedContent * outContent )
{
    path; parameters; outContent;
    assert( false );
    return false;
}

#endif