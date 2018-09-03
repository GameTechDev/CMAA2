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

using namespace VertexAsylum;

//vaRenderMeshManager & renderMeshManager, const vaGUID & uid

const int c_renderMeshMaterialFileVersion = 2;


vaRenderMaterial::vaRenderMaterial( const vaRenderingModuleParams & params ) : vaRenderingModule( params ),
    vaAssetResource( vaSaferStaticCast< const vaRenderMaterialConstructorParams &, const vaRenderingModuleParams &>( params ).UID), 
    m_trackee( vaSaferStaticCast< const vaRenderMaterialConstructorParams &, const vaRenderingModuleParams &>( params ).RenderMaterialManager.GetRenderMaterialTracker( ), this ), 
    m_renderMaterialManager( vaSaferStaticCast< const vaRenderMaterialConstructorParams &, const vaRenderingModuleParams &>( params ).RenderMaterialManager ),
    m_constantsBuffer( params )
{
    m_shaderMacros.reserve( 16 );
    m_shaderMacrosDirty = true;
    m_shadersDirty = true;
    m_inputsDirty = true;

    //InitializeDefaultMaterial();
}

void vaRenderMaterial::InitializeDefaultMaterial( )
{
    m_shaderSettings.FileName               = "vaMaterialBasic.hlsl";
    //m_shaderSettings.EntryVS_PosOnly        = "VS_PosOnly";
    m_shaderSettings.EntryPS_DepthOnly      = "PS_DepthOnly";
    m_shaderSettings.EntryVS_Standard       = "VS_Standard";
    m_shaderSettings.EntryPS_Forward        = "PS_Forward";
    m_shaderSettings.EntryPS_Deferred       = "PS_Deferred";
    m_shaderSettings.EntryPS_CustomShadow   = "PS_CustomShadow";
    m_shaderSettings.BaseMacros.clear();

    m_computedTextureSlotCount      = 0; 
    m_computedVec4ConstantCount     = 0;
    m_computedScalarConstantCount   = 0;

    m_inputs = GetStandardMaterialInputs( StandardMaterialTemplateType::BasicBRDF );

    m_inputsDirty = true;
}

void vaRenderMaterial::UpdateShaderMacros( )
{
    UpdateInputsDependencies( );

    if( !m_shaderMacrosDirty )
        return;

    vector< pair< string, string > > prevShaderMacros = m_shaderMacros;

    m_shaderMacros.clear();

    // start from base macros and add up from there

    // this can be useful if shader wants to know whether it's included from the vaRenderMaterial code
    m_shaderMacros.insert( m_shaderMacros.begin(), std::pair<string, string>( "VA_RENDER_MATERIAL", "1" ) );

    m_shaderMacros.push_back( std::pair<string, string>( "VA_RM_TRANSPARENT",               ( ( m_materialSettings.Transparent       ) ? ( "1" ) : ( "0" ) ) ) );
    m_shaderMacros.push_back( std::pair<string, string>( "VA_RM_ALPHATEST",                 ( ( m_materialSettings.AlphaTest         ) ? ( "1" ) : ( "0" ) ) ) );
    m_shaderMacros.push_back( std::pair<string, string>( "VA_RM_ALPHATEST_THRESHOLD",       vaStringTools::Format( "(%.3f)", m_materialSettings.AlphaTestThreshold ) ) );
    m_shaderMacros.push_back( std::pair<string, string>( "VA_RM_ACCEPTSHADOWS",             ( ( m_materialSettings.ReceiveShadows    ) ? ( "1" ) : ( "0" ) ) ) );
    m_shaderMacros.push_back( std::pair<string, string>( "VA_RM_WIREFRAME",                 ( ( m_materialSettings.Wireframe         ) ? ( "1" ) : ( "0" ) ) ) );
    m_shaderMacros.push_back( std::pair<string, string>( "VA_RM_ADVANCED_SPECULAR_SHADER",  ( ( m_materialSettings.AdvancedSpecularShader ) ? ( "1" ) : ( "0" ) ) ) );
    m_shaderMacros.push_back( std::pair<string, string>( "VA_RM_SPECIAL_EMISSIVE_LIGHT",    ( ( m_materialSettings.SpecialEmissiveLight ) ? ( "1" ) : ( "0" ) ) ) );
    

    string textureDeclarations = "";
    int textureSlotCount = 0;
    for( int i = 0; i < m_inputs.size(); i++ )
    {
        MaterialInput & item = m_inputs[i];

        bool hasTexture = item.ComputedSourceTextureShaderSlot != -1;

        string typeName;
        switch( item.Type )
        {
        case vaRenderMaterial::MaterialInput::InputType::Bool:      typeName = "bool";  break;
        case vaRenderMaterial::MaterialInput::InputType::Integer:   typeName = "int";   break;
        case vaRenderMaterial::MaterialInput::InputType::Scalar:    typeName = "float"; break;
        case vaRenderMaterial::MaterialInput::InputType::Color4:    typeName = "float4"; break;     // TODO: in this case need to convert sRGB to linear values!!!!
        case vaRenderMaterial::MaterialInput::InputType::Vector4:   typeName = "float4"; break;
        default: assert( false ); break;
        }

        m_shaderMacros.push_back( std::pair<string, string>( "VA_RM_INPUT_" + string(item.Name) + "_IS_TEXTURE", (hasTexture)?("1"):("0") ) );
        m_shaderMacros.push_back( std::pair<string, string>( "VA_RM_INPUT_" + string(item.Name) + "_TYPE" , typeName ) );

        string accessorFunctionName = "VA_RM_INPUT_LOAD_" + string(item.Name) + "( __interpolants )";
        if( hasTexture )
        {
            assert( item.ComputedSourceTextureShaderSlot >= 0 && item.ComputedSourceTextureShaderSlot < m_computedTextureSlotCount );

            shared_ptr<vaTexture> texture = item.GetSourceTexture( );
            vaTextureContentsType textureContentsType = vaTextureContentsType::GenericColor;

            if( texture == nullptr )
            {
                // not loaded yet probably, we have to break this whole thing
                m_shaderMacros.clear();
                m_shaderMacrosDirty = true;
                m_shadersDirty = true;
                return;
            }
            else
                textureContentsType = texture->GetContentsType();

            //item.ComputedSourceTextureShaderSlot
            string thisTextureDeclaration;
            
            // type
            thisTextureDeclaration += vaStringTools::Format( "Texture2D<%s>   ", typeName.c_str() );
            
            // global name
            thisTextureDeclaration += item.ComputedSourceTextureShaderVariableName;

            // texture register declaration
            thisTextureDeclaration += vaStringTools::Format( " : register( t%d );", item.ComputedSourceTextureShaderSlot );

            textureDeclarations += thisTextureDeclaration;

            string samplerName;
            switch( item.SourceTextureSamplerType )
            {
            case vaRenderMaterial::MaterialInput::TextureSamplerType::PointClamp:       samplerName = "g_samplerPointClamp";        break;
            case vaRenderMaterial::MaterialInput::TextureSamplerType::PointWrap:        samplerName = "g_samplerPointWrap";         break;
            case vaRenderMaterial::MaterialInput::TextureSamplerType::LinearClamp:      samplerName = "g_samplerLinearClamp";       break;
            case vaRenderMaterial::MaterialInput::TextureSamplerType::LinearWrap:       samplerName = "g_samplerLinearWrap";        break;
            case vaRenderMaterial::MaterialInput::TextureSamplerType::AnisotropicClamp: samplerName = "g_samplerAnisotropicClamp";  break;
            case vaRenderMaterial::MaterialInput::TextureSamplerType::AnisotropicWrap:  samplerName = "g_samplerAnisotropicWrap";   break;
            case vaRenderMaterial::MaterialInput::TextureSamplerType::MaxValue:
            default:
                assert( false );
                samplerName = "g_samplerPointClamp";
                break;
            }
            

            m_shaderMacros.push_back( std::pair<string, string>( "VA_RM_INPUT_" + string(item.Name) + "_TEXTURE" ,       item.ComputedSourceTextureShaderVariableName ) );
            m_shaderMacros.push_back( std::pair<string, string>( "VA_RM_INPUT_" + string(item.Name) + "_TEXTURE_UV_INDEX",    vaStringTools::Format( "%d", item.SourceTextureUVIndex ) ) );
            m_shaderMacros.push_back( std::pair<string, string>( "VA_RM_INPUT_" + string(item.Name) + "_TEXTURE_SAMPLER",     samplerName ) );

            // Accessors

            string UVAccessor;
            switch( item.SourceTextureUVIndex )
            {
            case( 0 ): UVAccessor = "Texcoord01.xy"; break;
            case( 1 ): UVAccessor = "Texcoord01.zw"; break;
            case( 2 ): UVAccessor = "Texcoord23.xy"; break;
            case( 3 ): UVAccessor = "Texcoord23.zw"; break;
            default: assert( false ); break;
            }

            // Universal accessor with default sampler
            //string accessorFunction = vaStringTools::Format( "%s.Sample( %s, __interpolants.%s )", item.ComputedSourceTextureShaderVariableName.c_str(), samplerName.c_str(), UVAccessor.c_str() );
            string accessorFunction = vaStringTools::Format( "%s.SampleBias( %s, __interpolants.%s, g_Global.GlobalMIPOffset )", item.ComputedSourceTextureShaderVariableName.c_str(), samplerName.c_str(), UVAccessor.c_str() );
            m_shaderMacros.push_back( std::pair<string, string>( accessorFunctionName, accessorFunction ) );

            m_shaderMacros.push_back( std::pair<string, string>( "VA_RM_INPUT_" + string(item.Name) + "_TEXTURE_UV( __interpolants )", vaStringTools::Format( "__interpolants.%s", UVAccessor.c_str() ) ) );

            string textureContentsTypeString;
            switch( textureContentsType )
            {
            case VertexAsylum::vaTextureContentsType::GenericColor:             textureContentsTypeString = "GENERIC_COLOR";                    break;
            case VertexAsylum::vaTextureContentsType::GenericLinear:            textureContentsTypeString = "GENERIC_LINEAR";                   break;
            case VertexAsylum::vaTextureContentsType::NormalsXYZ_UNORM:         textureContentsTypeString = "NORMAL_XYZ_UNORM";                 break;
            case VertexAsylum::vaTextureContentsType::NormalsXY_UNORM:          textureContentsTypeString = "NORMAL_XY_UNORM";                  break;
            case VertexAsylum::vaTextureContentsType::NormalsWY_UNORM:          textureContentsTypeString = "NORMAL_WY_UNORM";                  break;
            case VertexAsylum::vaTextureContentsType::NormalsXY_LAEA_ENCODED:   textureContentsTypeString = "NORMAL_XY_LAEA";                   break;
            case VertexAsylum::vaTextureContentsType::SingleChannelLinearMask:  textureContentsTypeString = "SINGLE_CHANNEL_LINEAR_MASK_UNORM"; break;
            default: assert( false ); break;
            }

            m_shaderMacros.push_back( std::pair<string, string>( "VA_RM_INPUT_" + string( item.Name ) + "_CONTENTS_TYPE_" + textureContentsTypeString, "1" ) );

            textureSlotCount++;
        }
        else
        {
            if( !item.IsValueDynamic )
            {
                string accessorFunction;
                switch( item.Type )
                {
                case vaRenderMaterial::MaterialInput::InputType::Bool:      accessorFunction = (item.Value.Bool)?"bool(true)":"bool(false)";            break;
                case vaRenderMaterial::MaterialInput::InputType::Integer:   accessorFunction = vaStringTools::Format( "int(%d)", item.Value.Integer );  break;
                case vaRenderMaterial::MaterialInput::InputType::Scalar:    accessorFunction = vaStringTools::Format( "float(%f)", item.Value.Scalar ); break;
                case vaRenderMaterial::MaterialInput::InputType::Color4:    
                case vaRenderMaterial::MaterialInput::InputType::Vector4:   accessorFunction = vaStringTools::Format( "float4(%f,%f,%f,%f)", item.Value.Vector4.x, item.Value.Vector4.y, item.Value.Vector4.z, item.Value.Vector4.w ); break;
                default: assert( false ); break;
                }
                m_shaderMacros.push_back( std::pair<string, string>( accessorFunctionName, accessorFunction ) );

                // useful for macros
                if( item.Type == vaRenderMaterial::MaterialInput::InputType::Bool )
                    m_shaderMacros.push_back( std::pair<string, string>( "VA_RM_INPUT_" + string(item.Name) + "_CONST_BOOLEAN", (item.Value.Bool)?("1"):("0") ) );
            }
            else
            {
                m_shaderMacros.push_back( std::pair<string, string>( accessorFunctionName, "0" ) );
                assert( false ); // not implemented yet
            }
        }
    }
    assert( textureSlotCount == m_computedTextureSlotCount );

    m_shaderMacros.push_back( std::pair<string, string>( "VA_RM_TEXTURE_DECLARATIONS", textureDeclarations ) );

    m_shaderMacrosDirty = false;
    m_shadersDirty = prevShaderMacros != m_shaderMacros;
}

bool vaRenderMaterial::RemoveInput( int index )
{
    if( index < 0 || index > m_inputs.size( ) )
    {
        assert( false );
        return false;
    }

    // replace with last and pop last
    if( index < ( m_inputs.size( ) - 1 ) )
        m_inputs[index] = m_inputs.back();
    m_inputs.pop_back();

    m_inputsDirty = true;
    return true;
}

bool vaRenderMaterial::SetInput( int index, const MaterialInput & newValue )
{
    if( index < 0 || index > m_inputs.size( ) )
    {
        assert( false );
        return false;
    }

    string newValueName = newValue.Name;
    assert( newValueName.length() > 0 && newValueName.length() < 32 );    // limited to 32 for UI and serch/serialization reasons
    for( int i = 0; i < newValueName.length()-1; i++ )
    {
        if( !( ( newValueName[i] >= '0' && newValueName[i] <= '9' ) || ( newValueName[i] >= 'A' && newValueName[i] <= 'z' ) || ( newValueName[i] == '_' ) || ( newValueName[i] == 0 ) ) )
        {
            // invalid character used, will be replaced with _
            assert( false );
            newValueName[i] = '_';
        }
    }
    string newValueNameTemp = newValueName;
    int newNameSearchIndex = 0;
    m_inputs[index].Name = "";
    while( FindInputByName( newValueName ) != -1 )
    {
        newNameSearchIndex++;
        newValueName = vaStringTools::Format( "%s_%d", newValueNameTemp.c_str(), newNameSearchIndex );
    }
    if( newNameSearchIndex != 0 )
    { 
        VA_LOG_WARNING( "Material input name %s already exists, using %s", newValueNameTemp.c_str(), newValueName.c_str() );
    }

    m_inputs[index] = newValue;
    m_inputs[index].Name = newValueName;
    m_inputsDirty = true;
    return true;
}

bool vaRenderMaterial::SetInputByName( const MaterialInput & newValue, bool addIfNotFound )
{
    int index = FindInputByName( newValue.Name );
    if( index == -1 )
    {
        if( !addIfNotFound )
            return false;

        m_inputs.push_back( MaterialInput() );
        index = (int)m_inputs.size()-1;
    }
    return SetInput( index, newValue );
}

bool vaRenderMaterial::SaveAPACK( vaStream & outStream )
{
    // Just using SerializeUnpacked to implement this - not a lot of binary data; can be upgraded later
    vaXMLSerializer materialSerializer;
    VERIFY_TRUE_RETURN_ON_FALSE( materialSerializer.WriterOpenElement( "Material" ) );
    SerializeUnpacked( materialSerializer, L"%there-is-no-folder@" );
    VERIFY_TRUE_RETURN_ON_FALSE( materialSerializer.WriterCloseElement( "Material" ) );

    const char * buffer = materialSerializer.GetWritePrinter( ).CStr( );
    int64 bufferSize = materialSerializer.GetWritePrinter( ).CStrSize( );

    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<int64>( bufferSize ) );
    VERIFY_TRUE_RETURN_ON_FALSE( outStream.Write( buffer, bufferSize ) );
    return true;
}

bool vaRenderMaterial::LoadAPACK( vaStream & inStream )
{
    // Just using SerializeUnpacked to implement this - not a lot of binary data; can be upgraded later
    int64 bufferSize = 0;
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int64>( bufferSize ) );

    const char * buffer = new char[bufferSize];
    if( !inStream.Read( (void*)buffer, bufferSize ) )
    {
        delete[] buffer;
        VERIFY_TRUE_RETURN_ON_FALSE( false );
    }

    vaXMLSerializer materialSerializer( buffer, bufferSize );

    bool allOk = materialSerializer.ReaderAdvanceToChildElement( "Material" );

    if( allOk )
        allOk = SerializeUnpacked( materialSerializer, L"%there-is-no-folder@" );

    if( allOk )
        allOk = materialSerializer.ReaderPopToParentElement( "Material" );

    assert( allOk );

    delete[] buffer;
    return allOk;
}

bool vaRenderMaterial::SerializeUnpacked( vaXMLSerializer & serializer, const wstring & assetFolder )
{
    assetFolder; // unused

    int32 fileVersion = c_renderMeshMaterialFileVersion;
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "FileVersion", fileVersion ) );
    VERIFY_TRUE_RETURN_ON_FALSE( fileVersion == c_renderMeshMaterialFileVersion );

    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "FaceCull", (int32&)            m_materialSettings.FaceCull ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "AlphaTest",                    m_materialSettings.AlphaTest ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "AlphaTestThreshold",           m_materialSettings.AlphaTestThreshold ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "ReceiveShadows",               m_materialSettings.ReceiveShadows ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "Wireframe",                    m_materialSettings.Wireframe ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "Transparent",                  m_materialSettings.Transparent ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "AdvancedSpecularShader",       m_materialSettings.AdvancedSpecularShader,  m_materialSettings.AdvancedSpecularShader ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "SpecialEmissiveLight",         m_materialSettings.SpecialEmissiveLight,    m_materialSettings.SpecialEmissiveLight ) );

    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "ShaderFileName",               m_shaderSettings.FileName ) );
    //VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "ShaderEntryVS_PosOnly",        m_shaderSettings.EntryVS_PosOnly ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "ShaderEntryPS_DepthOnly",      m_shaderSettings.EntryPS_DepthOnly ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "ShaderEntryVS_Standard",       m_shaderSettings.EntryVS_Standard ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "ShaderEntryPS_Forward",        m_shaderSettings.EntryPS_Forward ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "ShaderEntryPS_Deferred",       m_shaderSettings.EntryPS_Deferred ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "ShaderEntryPS_CustomShadow",   m_shaderSettings.EntryPS_CustomShadow, string("PS_CustomShadow") ) ); // default to be removed in the future

    if( serializer.SerializeOpenChildElement( "ShaderBaseMacros" ) )
    {
        uint32 count = (uint32)m_shaderSettings.BaseMacros.size();
        VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "count", count ) );
        if( serializer.IsReading() )
            m_shaderSettings.BaseMacros.resize( count );
        for( uint32 i = 0; i < count; i++ )
        {
            string elementName = vaStringTools::Format( "a%d", i );
            VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeOpenChildElement( elementName.c_str() ) );

            VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "Name", m_shaderSettings.BaseMacros[i].first ) );
            VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeValue( "Definition", m_shaderSettings.BaseMacros[i].first ) );
        
            VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializePopToParentElement( elementName.c_str() ) );
        }
        VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializePopToParentElement( "ShaderBaseMacros" ) );
    }
    else
    {
        if( serializer.IsReading() )
        {
            m_shaderSettings.BaseMacros.clear();
        }
        else
        {
            // we couldn't open ShaderBaseMacros element for saving?
            VERIFY_TRUE_RETURN_ON_FALSE( false );
        }
    }

    serializer.SerializeObjectVector( "Inputs", m_inputs );

    if( serializer.IsReading( ) )
    {
        if( vaStringTools::CompareNoCase( m_shaderSettings.FileName, "vaMaterialPhong.hlsl" ) == 0 )
            m_shaderSettings.FileName = "vaMaterialBasic.hlsl";

        // temp corrections for old datasets
        if( FindInputByName( "EnvironmentMapMul" ) == -1 )
            SetInputByName( MaterialInput( "EnvironmentMapMul",
                            MaterialInput::InputType::Scalar,
                            MaterialInput::ValueVar( 1.0f ),
                            MaterialInput::ValueVar( 1.0f ),
                            MaterialInput::ValueVar( 0.0f ),
                            MaterialInput::ValueVar( 1e6f ),
                            false ), true );
        if( FindInputByName( "RefractionIndex" ) == -1 )
            SetInputByName( MaterialInput( "RefractionIndex",
                            MaterialInput::InputType::Scalar,
                            MaterialInput::ValueVar( 0.2f ),
                            MaterialInput::ValueVar( 0.2f ),
                            MaterialInput::ValueVar( 0.0f ),
                            MaterialInput::ValueVar( 2.0f ),
                            false ), true );
        if( FindInputByName( "Reflectivity" ) != -1 )
            RemoveInput( FindInputByName( "Reflectivity" ) );
        if( FindInputByName( "RefractI" ) != -1 )
            RemoveInput( FindInputByName( "RefractI" ) );
        if( FindInputByName( "EmissiveMul" ) == -1 )
        {
            SetInputByName( MaterialInput( "EmissiveMul",
                            1.0f,                   // value
                            1.0f,                   // value default (UI only)
                            0.0f,                   // value min (UI only)
                            1e6f ), true );         // value max (UI only)
        }

        // manual override stuff
        // MaterialInput specularInput = GetInputs( )[FindInputByName( "Specular" )];
        // {
        //     specularInput.Value.Vector4 = vaVector4( 1.0f, 1.0f, 1.0f, 1.0f );
        //     SetInput( FindInputByName( "Specular" ), specularInput );
        // }
        // 
        // MaterialInput specPowInput = GetInputs( )[FindInputByName( "SpecularPow" )];
        // if( specPowInput.Value.Scalar == 1024.0f )
        // {
        //     specPowInput.Value.Scalar = 128.0f;
        //     SetInput( FindInputByName( "SpecularPow" ), specPowInput );
        // }
        // MaterialInput specMulInput = GetInputs( )[FindInputByName( "SpecularMul" )];
        // if( specMulInput.Value.Scalar == 1024.0f )
        // {
        //     specMulInput.Value.Scalar = 0.1f;
        //     SetInput( FindInputByName( "SpecularMul" ), specMulInput );
        // }

        // conversion from old to new - not needed anymore
        // for( int i = (int)m_inputs.size()-1; i >= 0; i-- )
        // {
        //     if(     m_inputs[i].Name == "ColorAlbedo" 
        //         ||  m_inputs[i].Name == "ColorEmissive" 
        //         ||  m_inputs[i].Name == "ColorSpecular" 
        //         ||  m_inputs[i].Name == "ColorAmbient" 
        //         ||  m_inputs[i].Name == "ColorTransparent" 
        //         ||  m_inputs[i].Name == "ColorReflective" )
        //         m_inputs.erase( m_inputs.begin() + i );
        // }

        // always needed
        m_inputsDirty = true;
    }
    return true;
}

//void vaRenderMaterial::ReconnectDependencies( )
//{
//    m_textureAlbedo     = nullptr;
//    m_textureNormalmap  = nullptr;
//    m_textureSpecular   = nullptr;
//
//    vaUIDObjectRegistrar::GetInstance( ).ReconnectDependency<vaTexture>( m_textureAlbedo, m_textureAlbedoUID );
//    vaUIDObjectRegistrar::GetInstance( ).ReconnectDependency<vaTexture>( m_textureNormalmap, m_textureNormalmapUID );
//    vaUIDObjectRegistrar::GetInstance( ).ReconnectDependency<vaTexture>( m_textureSpecular, m_textureSpecularUID );
//    vaUIDObjectRegistrar::GetInstance( ).ReconnectDependency<vaTexture>( m_textureEmissive, m_textureEmissiveUID );
//}

void vaRenderMaterial::RegisterUsedAssetPacks( std::function<void( const vaAssetPack & )> registerFunction )
{
    vaAssetResource::RegisterUsedAssetPacks( registerFunction );
    for( auto input : m_inputs )
    {
        if( input.SourceTextureID == vaCore::GUIDNull() )
            continue;
        auto texture = input.GetSourceTexture();
        if( texture != nullptr )
        {
            texture->RegisterUsedAssetPacks( registerFunction );
        }
        else
        {
            // Either ReconnectDependencies( ) was not called, or the asset is missing?
            assert( false );
        }
    }
}

void vaRenderMaterial::MaterialInput::ValueVar::GetDefaultMinMax( vaRenderMaterial::MaterialInput::InputType type, ValueVar & outMin, ValueVar & outMax )
{
    switch( type )
    {
    case vaRenderMaterial::MaterialInput::InputType::Bool:
        outMin.Bool = false; outMax.Bool = true;
        break;
    case vaRenderMaterial::MaterialInput::InputType::Integer:
        outMin.Integer = INT_MIN; outMax.Integer = INT_MAX;
        break;
    case vaRenderMaterial::MaterialInput::InputType::Scalar:
        outMin.Scalar = -1e6f; outMax.Scalar = 1e6f;
        break;
    case vaRenderMaterial::MaterialInput::InputType::Vector4:
        outMin.Vector4.x = -1e6f; outMax.Vector4.x = 1e6f;
        outMin.Vector4.y = -1e6f; outMax.Vector4.y = 1e6f;
        outMin.Vector4.z = -1e6f; outMax.Vector4.z = 1e6f;
        outMin.Vector4.w = -1e6f; outMax.Vector4.w = 1e6f;
        break;
    case vaRenderMaterial::MaterialInput::InputType::Color4:
        outMin.Vector4.x = 0.0f; outMax.Vector4.x = 1e6f;
        outMin.Vector4.y = 0.0f; outMax.Vector4.y = 1e6f;
        outMin.Vector4.z = 0.0f; outMax.Vector4.z = 1e6f;
        outMin.Vector4.w = 0.0f; outMax.Vector4.w = 1e6f;
        break;
    default:
        assert( false );
        break;
    }
}

shared_ptr<vaTexture> vaRenderMaterial::MaterialInput::GetSourceTexture( )
{
    if( SourceTextureID == vaCore::GUIDNull() )
    {
        CachedSourceTexture.reset();
        return nullptr;
    }

    return vaUIDObjectRegistrar::GetInstance().FindCached<vaTexture>( SourceTextureID, CachedSourceTexture );
}

// bool vaRenderMaterial::MaterialInput::SaveAPACK( vaStream & outStream )
// {
//     // codepath not implemented (the whole APACK Save/Load path for materials is handled using the SerializeUnpacked path - no need for specialized APACK one as the data is relatively small)
//     assert( false );
//     outStream;
//     return false;
// }
// 
// bool vaRenderMaterial::MaterialInput::LoadAPACK( vaStream & inStream )
// {
//     // codepath not implemented (the whole APACK Save/Load path for materials is handled using the SerializeUnpacked path - no need for specialized APACK one as the data is relatively small)
//     assert( false );
//     inStream;
//     return false;
// }

bool vaRenderMaterial::MaterialInput::Serialize( vaXMLSerializer & serializer )
{
    if( serializer.IsReading( ) )
    {
        // clear object
        *this = vaRenderMaterial::MaterialInput();
    }

    serializer.SerializeValue( "Name", Name );
    serializer.SerializeValue( "Type", (int32&)Type );

    // conversion from old to new - not needed anymore
    // if( serializer.IsReading( ) )
    // {
    //     if( Name == "TextureAlbedo" )
    //         Name = "Albedo";
    //     if( Name == "TextureEmissive" )
    //         Name = "Emissive";
    //     if( Name == "TextureNormalmap" )
    //         Name = "Normalmap";
    //     if( Name == "TextureSpecular" )
    //         Name = "Specular";
    // }

    switch( this->Type )
    {
    case( InputType::Bool ):
        serializer.SerializeValue( "Value", Value.Bool );
        serializer.SerializeValue( "ValueDefault", ValueDefault.Bool );
        serializer.SerializeValue( "ValueMin", ValueMin.Bool );
        serializer.SerializeValue( "ValueMax", ValueMax.Bool );
        break;
    case( InputType::Integer ):
        serializer.SerializeValue( "Value", Value.Integer );
        serializer.SerializeValue( "ValueDefault", ValueDefault.Integer );
        serializer.SerializeValue( "ValueMin", ValueMin.Integer );
        serializer.SerializeValue( "ValueMax", ValueMax.Integer );
        break;
    case( InputType::Scalar ):
        serializer.SerializeValue( "Value", Value.Scalar );
        serializer.SerializeValue( "ValueDefault", ValueDefault.Scalar );
        serializer.SerializeValue( "ValueMin", ValueMin.Scalar );
        serializer.SerializeValue( "ValueMax", ValueMax.Scalar );
        break;
    case( InputType::Vector4 ) : // Fall through
    case( InputType::Color4 )  :
        serializer.SerializeValue( "Value", Value.Vector4 );
        serializer.SerializeValue( "ValueDefault", ValueDefault.Vector4 );
        serializer.SerializeValue( "ValueMin", ValueMin.Vector4 );
        serializer.SerializeValue( "ValueMax", ValueMax.Vector4 );
        break;
    default:
        assert( false );
        break;
    }

    serializer.SerializeValue( "IsValueDynamic",            IsValueDynamic );
    serializer.SerializeValue( "SourceTextureID",           SourceTextureID );
    serializer.SerializeValue( "SourceTextureUVIndex",      SourceTextureUVIndex );
    serializer.SerializeValue( "SourceTextureSamplerType",  (int32&)SourceTextureSamplerType );

    if( serializer.IsReading( ) )
    {
        if( Name == "Normalmap" )
        {
            Value.Vector4 = vaVector4( 0.5f, 0.5f, 1.0f, 0.0f );
            ValueDefault.Vector4 = vaVector4( 0.5f, 0.5f, 1.0f, 0.0f );
            ValueMin.Vector4 = vaVector4( 0.0f, 0.0f, 0.0f, 0.0f );
            ValueMax.Vector4 = vaVector4( 1.0f, 1.0f, 1.0f, 0.0f );
        }
    }


    if( serializer.IsReading() )
        ResetTemps( );

    return false;
}


std::vector<vaRenderMaterial::MaterialInput> vaRenderMaterial::GetStandardMaterialInputs( vaRenderMaterial::StandardMaterialTemplateType type )
{
    std::vector<vaRenderMaterial::MaterialInput> retVal;
    switch( type )
    {
    case vaRenderMaterial::StandardMaterialTemplateType::BasicBRDF:
    {
        retVal.push_back( MaterialInput ( "Albedo",
                                        MaterialInput::InputType::Color4,
                                        vaCore::GUIDNull(),
                                        0,
                                        MaterialInput::TextureSamplerType::AnisotropicWrap,
                                        vaVector4( 1.0f, 1.0f, 1.0f, 1.0f ) ) );
        retVal.push_back( MaterialInput ( "Emissive",
                                        MaterialInput::InputType::Color4,
                                        vaCore::GUIDNull(),
                                        0,
                                        MaterialInput::TextureSamplerType::AnisotropicWrap,
                                        vaVector4( 0.0f, 0.0f, 0.0f, 0.0f ) ) );
        retVal.push_back( MaterialInput ( "Normalmap",
                                        MaterialInput::InputType::Vector4,
                                        vaCore::GUIDNull(),
                                        0,
                                        MaterialInput::TextureSamplerType::AnisotropicWrap,
                                        vaVector4( 0.5f, 0.5f, 1.0f, 0.0f ) ) );
        retVal.push_back( MaterialInput ( "Specular",               // reflected color modifier in .rgb, multiplier that is multiplied with SpecularMul in .a
                                        MaterialInput::InputType::Color4,
                                        vaCore::GUIDNull(),
                                        0,
                                        MaterialInput::TextureSamplerType::AnisotropicWrap,
                                        vaVector4( 1.0f, 1.0f, 1.0f, 1.0f ) ) );
        retVal.push_back( MaterialInput ( "EmissiveMul",
                                        1.0f,                   // value
                                        1.0f,                   // value default (UI only)
                                        0.0f,                   // value min (UI only)
                                        1e6f ) );               // value max (UI only)
        retVal.push_back( MaterialInput ( "SpecularMul",
                                        0.5f,                   // value
                                        0.5f,                   // value default (UI only)
                                        0.0f,                   // value min (UI only)
                                        1e6f ) );               // value max (UI only)
        retVal.push_back( MaterialInput ( "SpecularPow",        
                                        16.0f,                  // value
                                        16.0f,                  // value default (UI only)
                                        0.0f,                   // value min (UI only)
                                        1e6f ) );               // value max (UI only)
        retVal.push_back( MaterialInput ( "EnvironmentMapMul",
                                        1.0f,                   // value
                                        1.0f,                   // value default (UI only)
                                        0.0f,                   // value min (UI only)
                                        1e6f ) );               // value max (UI only)
        retVal.push_back( MaterialInput ( "RefractionIndex",
                                        0.2f,                   // value
                                        0.2f,                   // value default (UI only)
                                        0.0f,                   // value min (UI only)
                                        2.0f ) );               // value max (UI only)
    }
        break;
    case vaRenderMaterial::StandardMaterialTemplateType::PrincipledBSDF:
        assert( false );
        break;
    default:
        assert( false );
        break;
    }
    return retVal;
};

void vaRenderMaterial::VerifyNames( )
{
    for( int i = 0; i < m_inputs.size(); i++ )
    {
        // must find itself but must have no other inputs with the same name
        assert( FindInputByName( m_inputs[i].Name ) == i );
    }
}

void vaRenderMaterial::UpdateInputsDependencies( )
{
    // there is no real reason why this would happen at runtime except during editing - so if you ever find this bit of code to be slow,
    // the initial simplest solution would be to, instead of going through each input every time, have a counter that you increment
    // every UpdateInputsDependencies() and check just one m_inputs[ counter % m_inputs.size() ] or even at lower frequency!
    for( int i = 0; i < m_inputs.size( ); i++ )
    {
        MaterialInput & item = m_inputs[i];
        if( !item.IsTexture() )
            continue;
        shared_ptr<vaTexture> prevTexture = item.CachedSourceTexture.lock();
        if( prevTexture != item.GetSourceTexture() )
        {
            // texture changed? need to re-dirtify the flag :)
            m_inputsDirty = true;
        }
    }

    if( !m_inputsDirty )
        return;

    //assert( m_inputsMap.size() == 0 );  // this should only be called at load time, once
    //
    //for( int i = 0; i < m_inputs.size(); i++ )
    //{
    //    MaterialInput & item = m_inputs[i];
    //    if( m_inputsMap.find( item.Name ) != m_inputsMap.end( ) )
    //    {
    //        assert( false ); // two inputs with the same name? that's a bug, second one will be ignored
    //        // VA_WARN(  )
    //        continue;
    //    }
    //    m_inputsMap.insert( make_pair( string(item.Name), i ) );
    //}

    m_computedTextureSlotCount      = 0; 
    m_computedVec4ConstantCount     = 0;
    int constantBufferByteOffset    = 0;
    for( int i = 0; i < m_inputs.size(); i++ )
    {
        MaterialInput & item = m_inputs[i];
        if( item.SourceTextureID != vaCore::GUIDNull( ) )
        {
            item.ComputedSourceTextureShaderSlot            = m_computedTextureSlotCount;
            item.ComputedSourceTextureShaderVariableName    = vaStringTools::Format( "g_RMTexture%02d", item.ComputedSourceTextureShaderSlot );
            m_computedTextureSlotCount++;
        }
        else
        {
            item.ComputedSourceTextureShaderSlot            = -1;
            item.ComputedSourceTextureShaderVariableName    = "";
        }

        if( item.IsValueDynamic && (item.Type == MaterialInput::InputType::Bool || item.Type == MaterialInput::InputType::Integer || item.Type == MaterialInput::InputType::Scalar ) )
        {
            item.ComputedDynamicValueConstantBufferShaderIndex  = m_computedVec4ConstantCount;
            item.ComputedDynamicValueConstantBufferByteOffset   = constantBufferByteOffset;
            m_computedVec4ConstantCount++;
            constantBufferByteOffset += 4*4;
        }
    }
    m_computedScalarConstantCount   = 0;
    for( int i = 0; i < m_inputs.size(); i++ )
    {
        MaterialInput & item = m_inputs[i];
        if( item.IsValueDynamic && (item.Type == MaterialInput::InputType::Bool || item.Type == MaterialInput::InputType::Integer || item.Type == MaterialInput::InputType::Scalar ) )
        {
            item.ComputedDynamicValueConstantBufferShaderIndex  = m_computedVec4ConstantCount+m_computedScalarConstantCount;
            item.ComputedDynamicValueConstantBufferByteOffset   = constantBufferByteOffset;
            m_computedScalarConstantCount++;
            constantBufferByteOffset += 4;
        }
    }

    VerifyNames();
    m_inputsDirty = false;
    m_shaderMacrosDirty = true;
    m_shadersDirty = true;
}


shared_ptr<vaVertexShader> vaRenderMaterial::GetVS( vaRenderMaterialShaderType shaderType )
{
    shaderType;

    // never use VS_PosOnly for now
    return m_shaders->VS_Standard.get();
}

shared_ptr<vaPixelShader> vaRenderMaterial::GetPS( vaRenderMaterialShaderType shaderType )
{
    if( shaderType == vaRenderMaterialShaderType::Deferred )
        return m_shaders->PS_Deferred.get();
    else if( shaderType == vaRenderMaterialShaderType::Forward )
        return m_shaders->PS_Forward.get();
    else if( shaderType == vaRenderMaterialShaderType::DepthOnly )
        return m_shaders->PS_DepthOnly.get();
    else if( shaderType == vaRenderMaterialShaderType::CustomShadow )
        return m_shaders->PS_CustomShadow.get();
    else
    {
        assert( false );
        return nullptr;
    }
}

bool vaRenderMaterial::SetToRenderItem( vaRenderItem & renderItem, vaRenderMaterialShaderType shaderType )
{
    UpdateShaderMacros( );

    if( m_shaderMacrosDirty )
    {
        // still dirty? there's non-loaded textures or something similar - need to bail out
        return false;
    }

    if( m_shadersDirty || (m_shaders == nullptr) )
    {
        m_shaders = m_renderMaterialManager.FindOrCreateShaders( m_shaderSettings.FileName, m_materialSettings.AlphaTest, m_shaderSettings.EntryPS_DepthOnly, m_shaderSettings.EntryVS_Standard, m_shaderSettings.EntryPS_Forward, m_shaderSettings.EntryPS_Deferred, m_shaderSettings.EntryPS_CustomShadow, m_shaderMacros );
        m_shadersDirty = false;
    }

    renderItem.VertexShader = GetVS( shaderType );
    renderItem.PixelShader  = GetPS( shaderType );

    // int texOffset = RENDERMESH_TEXTURE_SLOT0;
    assert( RENDERMESH_TEXTURE_SLOT0 == 0 ); // 0 required for now - can be modified here below, just make sure to properly adjust range checking and assert

    for( int i = 0; i < m_inputs.size(); i++ )
    {
        MaterialInput & item = m_inputs[i];

        if( item.GetComputedSourceTextureShaderSlot() != -1 )
        {
            shared_ptr<vaTexture> texture = item.GetSourceTexture();
            if( item.GetComputedSourceTextureShaderSlot() >= 0 && item.GetComputedSourceTextureShaderSlot() < _countof( renderItem.ShaderResourceViews ) && texture != nullptr )
            {
                if( m_renderMaterialManager.GetTexturingDisabled() )
                {
                    if( item.Type == vaRenderMaterial::MaterialInput::InputType::Vector4 )          // Vector4? probably a normalmap, use black as default (will get converted to float3( 0, 0, 1 ) in most shaders)
                        renderItem.ShaderResourceViews[item.GetComputedSourceTextureShaderSlot()] = GetRenderDevice().GetTextureTools( ).GetCommonTexture( vaTextureTools::CommonTextureName::Black1x1 );
                    else //if( item.Type == vaRenderMaterial::MaterialInput::InputType::Color4 )    // regular color? any other case?
                        renderItem.ShaderResourceViews[item.GetComputedSourceTextureShaderSlot()] = GetRenderDevice().GetTextureTools( ).GetCommonTexture( vaTextureTools::CommonTextureName::White1x1 );
                }
                else
                {
                    renderItem.ShaderResourceViews[item.GetComputedSourceTextureShaderSlot()] = texture;
                }
            }
            else
            {
                assert( false );
            }
        }
    }
    return true;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// vaRenderMaterialManager
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


vaRenderMaterialManager::vaRenderMaterialManager( const vaRenderingModuleParams & params ) : vaRenderingModule( params )
{
    m_isDestructing = false;
    
    m_renderMaterials.SetAddedCallback( std::bind( &vaRenderMaterialManager::RenderMaterialsTrackeeAddedCallback, this, std::placeholders::_1 ) );
    m_renderMaterials.SetBeforeRemovedCallback( std::bind( &vaRenderMaterialManager::RenderMaterialsTrackeeBeforeRemovedCallback, this, std::placeholders::_1, std::placeholders::_2 ) );

    m_defaultMaterial = VA_RENDERING_MODULE_CREATE_SHARED( vaRenderMaterial, vaRenderMaterialConstructorParams( params.RenderDevice, *this, vaCore::GUIDFromString( L"11523d65-09ea-4342-9bad-8dab7a4dc1e0" ) ) );

    m_defaultMaterial->InitializeDefaultMaterial();

    m_texturingDisabled = false;
}
vaRenderMaterialManager::~vaRenderMaterialManager( )
{
    m_isDestructing = true;
    //m_renderMeshesMap.clear();

    m_defaultMaterial = nullptr;
    // this must absolutely be true as they contain direct reference to this object
    assert( m_renderMaterials.size( ) == 0 );
}

void vaRenderMaterialManager::SetTexturingDisabled( bool texturingDisabled )
{
    if( m_texturingDisabled == texturingDisabled )
        return;

    m_texturingDisabled = texturingDisabled;

    for( int i = 0; i < (int)m_renderMaterials.size(); i++ )
    {
        m_renderMaterials[i]->SetSettingsDirty();
    }
}

void vaRenderMaterialManager::RenderMaterialsTrackeeAddedCallback( int newTrackeeIndex )
{
    newTrackeeIndex; // unreferenced
}

void vaRenderMaterialManager::RenderMaterialsTrackeeBeforeRemovedCallback( int removedTrackeeIndex, int replacedByTrackeeIndex )
{
    removedTrackeeIndex; // unreferenced
    replacedByTrackeeIndex; // unreferenced
    //    assert( m_renderMaterialsMap.size() == 0 ); // removal not implemented!
}

shared_ptr<vaRenderMaterial> vaRenderMaterialManager::CreateRenderMaterial( const vaGUID & uid )
{
    auto ret = VA_RENDERING_MODULE_CREATE_SHARED( vaRenderMaterial, vaRenderMaterialConstructorParams( GetRenderDevice(), *this, uid ) );
    ret->UIDObject_Track(); // needed to work with rendering system - no harm in doing it here
    return ret;
}

void vaRenderMaterialManager::IHO_Draw( )
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

shared_ptr<vaRenderMaterialCachedShaders>
vaRenderMaterialManager::FindOrCreateShaders( const string & fileName, bool alphaTest, const string & entryPS_DepthOnly, const string & entryVS_Standard, string & entryPS_Forward, const string & entryPS_Deferred, const string & entryPS_CustomShadow, const vector< pair< string, string > > & shaderMacros )
{
    vaRenderMaterialCachedShaders::Key cacheKey( fileName, alphaTest, entryPS_DepthOnly, entryVS_Standard, entryPS_Forward, entryPS_Deferred, entryPS_CustomShadow, shaderMacros );

    auto it = m_cachedShaders.find( cacheKey );
    
    // in cache but no longer used by anyone so it was destroyed
    if( (it != m_cachedShaders.end()) && it->second.expired() )
    {
        m_cachedShaders.erase( it );
        it = m_cachedShaders.end();
    }

    // not in cache
    if( it == m_cachedShaders.end() )
    {
        shared_ptr<vaRenderMaterialCachedShaders> newShaders( new vaRenderMaterialCachedShaders( GetRenderDevice() ) );

        // vertex input layout is here!
        std::vector<vaVertexInputElementDesc> inputElements;
        inputElements.push_back( { "SV_Position", 0,    vaResourceFormat::R32G32B32_FLOAT,       0, vaVertexInputElementDesc::AppendAlignedElement, vaVertexInputElementDesc::InputClassification::PerVertexData, 0 } );
        inputElements.push_back( { "COLOR", 0,          vaResourceFormat::R8G8B8A8_UNORM,        0, vaVertexInputElementDesc::AppendAlignedElement, vaVertexInputElementDesc::InputClassification::PerVertexData, 0 } );
        inputElements.push_back( { "NORMAL", 0,         vaResourceFormat::R32G32B32A32_FLOAT,    0, vaVertexInputElementDesc::AppendAlignedElement, vaVertexInputElementDesc::InputClassification::PerVertexData, 0 } );
        inputElements.push_back( { "TEXCOORD", 0,       vaResourceFormat::R32G32_FLOAT,          0, vaVertexInputElementDesc::AppendAlignedElement, vaVertexInputElementDesc::InputClassification::PerVertexData, 0 } );
        inputElements.push_back( { "TEXCOORD", 1,       vaResourceFormat::R32G32_FLOAT,          0, vaVertexInputElementDesc::AppendAlignedElement, vaVertexInputElementDesc::InputClassification::PerVertexData, 0 } );

        newShaders->VS_Standard->CreateShaderAndILFromFile( vaStringTools::SimpleWiden(fileName), "vs_5_0", entryVS_Standard.c_str( ), inputElements, shaderMacros );
        if( alphaTest )
            newShaders->PS_DepthOnly->CreateShaderFromFile( vaStringTools::SimpleWiden(fileName), "ps_5_0", entryPS_DepthOnly.c_str( ), shaderMacros );
        else
            newShaders->PS_DepthOnly->Clear( );
        newShaders->PS_Forward->CreateShaderFromFile( vaStringTools::SimpleWiden(fileName), "ps_5_0", entryPS_Forward.c_str( ), shaderMacros );
        newShaders->PS_Deferred->CreateShaderFromFile( vaStringTools::SimpleWiden(fileName), "ps_5_0", entryPS_Deferred.c_str( ), shaderMacros );
        newShaders->PS_CustomShadow->CreateShaderFromFile( vaStringTools::SimpleWiden(fileName), "ps_5_0", entryPS_CustomShadow.c_str( ), shaderMacros );
        
        m_cachedShaders.insert( std::make_pair( cacheKey, newShaders ) );

        return newShaders;
    }
    else
    {
        return it->second.lock();
    }
}