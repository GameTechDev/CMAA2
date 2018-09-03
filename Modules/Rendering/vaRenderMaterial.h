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
#include "Core/Containers/vaTrackerTrackee.h"

#include "vaRendering.h"

#include "vaTriangleMesh.h"
#include "vaTexture.h"

#include "Rendering/Shaders/vaSharedTypes.h"

#include "IntegratedExternals/vaImguiIntegration.h"

#include "Rendering/vaTextureHelpers.h"


// for PBR, reading material:
// - http://blog.selfshadow.com/publications/s2015-shading-course/
// - https://www.allegorithmic.com/pbr-guide
// - http://www.marmoset.co/toolbag/learn/pbr-practice
// - http://www.marmoset.co/toolbag/learn/pbr-theory
// - http://www.marmoset.co/toolbag/learn/pbr-conversion
// - http://docs.cryengine.com/display/SDKDOC2/Creating+Textures+for+Physical+Based+Shading
// - 

namespace VertexAsylum
{
    class vaRenderMaterialManager;

    struct vaRenderMaterialConstructorParams : vaRenderingModuleParams
    {
        vaRenderMaterialManager &   RenderMaterialManager;
        const vaGUID &          UID;
        vaRenderMaterialConstructorParams( vaRenderDevice & device, vaRenderMaterialManager & renderMaterialManager, const vaGUID & uid ) : vaRenderingModuleParams(device), RenderMaterialManager( renderMaterialManager ), UID( uid ) { }
    };

    enum class vaRenderMaterialShaderType
    {
        DepthOnly       = 0,            // Z-pre-pass; no pixels are shaded unless alpha tested
        Forward         = 1,
        Deferred        = 2,
        CustomShadow    = 3,
    };

    struct vaRenderMaterialCachedShaders
    {
        struct Key
        {
            //wstring                     WStringPart;
            string                      AStringPart;

            bool                        operator == ( const Key & cmp ) const { return this->AStringPart == cmp.AStringPart; }
            bool                        operator >( const Key & cmp ) const { return this->AStringPart > cmp.AStringPart; }
            bool                        operator < ( const Key & cmp ) const { return this->AStringPart < cmp.AStringPart; }

            // alphatest is part of the key because it determines whether PS_DepthOnly is needed at all; all other shader parameters are contained in shaderMacros
            Key( const string & fileName, bool alphaTest, const string & entryPS_DepthOnly, const string & entryVS_Standard, string & entryPS_Forward, const string & entryPS_Deferred, const string & entryPS_CustomShadow, const vector< pair< string, string > > & shaderMacros )
            {
                //WStringPart = fileName;
                AStringPart = fileName;
                AStringPart += ((alphaTest)?("a&"):("b&")) + entryPS_DepthOnly + "&" + entryVS_Standard + "&" + entryPS_Forward + "&" + entryPS_Deferred + "&" + entryPS_CustomShadow;
                for( int i = 0; i < shaderMacros.size(); i++ )
                    AStringPart += shaderMacros[i].first + "&" + shaderMacros[i].second;
            }
        };

        vaRenderMaterialCachedShaders( vaRenderDevice & device ) : VS_Standard( device ), PS_DepthOnly( device ), PS_Forward( device ), PS_Deferred( device ), PS_CustomShadow( device ) { }

        vaAutoRMI<vaVertexShader>          VS_Standard;

        vaAutoRMI<vaPixelShader>           PS_DepthOnly;
        vaAutoRMI<vaPixelShader>           PS_Forward;
        vaAutoRMI<vaPixelShader>           PS_Deferred;
        vaAutoRMI<vaPixelShader>           PS_CustomShadow;
    };

    class vaRenderMaterial : public vaAssetResource, public vaRenderingModule
    {
    public:
        enum class StandardMaterialTemplateType
        {
            BasicBRDF,          // The simplest Blinn-Phong-based shading with specular mul & pow terms that can come out of the texture and support for emissive, etc.
            PrincipledBSDF      // Placeholder for future support of Blender PrincipledBSDF material (based on Disney's https://disney-animation.s3.amazonaws.com/library/s2012_pbs_disney_brdf_notes_v2.pdf)
        };

        // At some point remove texture from MaterialInput and add MaterialInputTextureSampler & MaterialInputModifierFunction or something similar, but for now this will have to do it
        struct MaterialInput
        {
            enum class InputType : int32
            {
                Bool,           // one bool value; if used as a static input (IsValueDynamic == false) then it will be compiled out
                Integer,        // one integer value; can be interpreted as both uint and int in the shader
                Scalar,         // one float value
                Vector4,        // 4 float values
                Color4,         // same as Vector4 but handled differently in the UI; TODO: also .rgb components need to get linearized before using in to the shader (not yet implemented)!

                MaxValue
            };
            enum class TextureSamplerType : int32
            {
                // TODO: implement mirror but beware of messing up existing data files; also need to update global samplers...
                PointClamp,
                PointWrap,
                //PointMirror,
                LinearClamp,
                LinearWrap,
                //LinearMirror,
                AnisotropicClamp,
                AnisotropicWrap,
                //AnisotropicMirror,

                MaxValue
            };  

            union ValueVar
            {
                bool                Bool;
                int32               Integer;
                float               Scalar;
                vaVector4           Vector4;
                ValueVar( ) { }
                ValueVar( const bool      & val ) { this->Vector4 = vaVector4(0,0,0,0); this->Bool      = val; }
                ValueVar( const int32     & val ) { this->Vector4 = vaVector4(0,0,0,0); this->Integer   = val; }
                ValueVar( const float     & val ) { this->Vector4 = vaVector4(0,0,0,0); this->Scalar    = val; }
                ValueVar( const vaVector4 & val ) { this->Vector4   = val; }

                bool                operator == ( const ValueVar & other ) const { return memcmp(this, &other, sizeof(*this)) == 0; }

                static void         GetDefaultMinMax( MaterialInput::InputType type, ValueVar & outMin, ValueVar & outMax );
            };

            string                  Name                                            = "";
            
            InputType               Type                                            = InputType::MaxValue;
            ValueVar                Value                                           = ValueVar(0.0f);

            // UI ONLY VALUES
            ValueVar                ValueDefault                                    = ValueVar(0.0f);                   // ValueDefault is for UI only - no other use!!
            ValueVar                ValueMin                                        = ValueVar(0.0f);                   // ValueMin is for UI and clamping only - no other use!!
            ValueVar                ValueMax                                        = ValueVar(0.0f);                   // ValueMax is for UI and clamping only - no other use!!

            // Whether it's static (for ex. material properties, changing requires shader recompilation) vs dynamic-per-object-instance (for ex., transform, color, etc.)
            bool                    IsValueDynamic                                  = false;

            vaGUID                  SourceTextureID                                 = vaCore::GUIDNull();               // If not vaCore::GUIDNull(), value comes out of the texture, otherwise it comes out of 'value'
            int                     SourceTextureUVIndex                            = -1;                               // If using a texture, which vertex (interpolant) UV index to use
            //vaTextureTools::CommonTextureName  SourceTextureDefault                            = vaTextureTools::CommonTextureName::MaxValue; // This will be used if original texture is missing
            TextureSamplerType      SourceTextureSamplerType                        = TextureSamplerType::PointClamp;

            // Consider adding:
            // bool                 UseValueAsTextureMultiplier                     = false;                            // When SourceTexture is available, if 'true', use Value as multiplier; if 'false' use just the texture, ignore Value
            // char                 SourceTextureSwizzle[4]                         = { 'x', 'y', 'z', 'w' }            // Input texture channels swizzle 

        private:
            friend class vaRenderMaterial;
            int                     ComputedSourceTextureShaderSlot                 = -1;
            string                  ComputedSourceTextureShaderVariableName         = "";
            int                     ComputedDynamicValueConstantBufferShaderIndex   = -1;   // to get the GPU name
            int                     ComputedDynamicValueConstantBufferByteOffset    = -1;   // to get the CPU buffer offset
            weak_ptr<vaTexture>     CachedSourceTexture;
        public:
            int                     GetComputedSourceTextureShaderSlot( ) const     { return ComputedSourceTextureShaderSlot; }

        public:
            MaterialInput( ) { }
            
            // Texture input initializer
            MaterialInput( const string & name, InputType type, const vaGUID & sourceTextureID, int sourceTextureUVIndex, TextureSamplerType sourceTextureSamplerType, const vaVector4 & valueIfNoTexture = vaVector4( 1.0f, 1.0f, 1.0f, 1.0f ), const vaVector4 & valueDefault = vaVector4( 1.0f, 1.0f, 1.0f, 1.0f ), const vaVector4 & valueMin = vaVector4( 0.0f, 0.0f, 0.0f, 0.0f ), const vaVector4 & valueMax  = vaVector4( 1e6f, 1e6f, 1e6f, 1e6f ) )
                :
                Name                    ( name ),
                Type                    ( type ),
                Value                   ( valueIfNoTexture ),
                ValueDefault            ( valueDefault ),
                ValueMin                ( valueMin ),
                ValueMax                ( valueMax ),
                IsValueDynamic          ( false ),
                SourceTextureID         ( sourceTextureID ),
                SourceTextureUVIndex    ( sourceTextureUVIndex ),
                SourceTextureSamplerType( sourceTextureSamplerType )
            {
                assert( type != InputType::Bool );      // doesn't make sense for a texture
                assert( type != InputType::Integer );   // not supported by this constructor (construct a value and then set texture ID)
            }

            // Non-texture value initializer
            MaterialInput( const string & name, InputType type, const ValueVar & value, const ValueVar & valueDefault, const ValueVar & valueMin, const ValueVar & valueMax, bool isValueDynamic = false )
                :
                Name                    ( name ),
                Type                    ( type ),
                Value                   ( value ),
                ValueDefault            ( valueDefault ),
                ValueMin                ( valueMin ),
                ValueMax                ( valueMax ),
                IsValueDynamic          ( isValueDynamic )
            {
            }

            // TODO: do all of the specific versions for different types to make it simpler to use
            //MaterialInput( const string & name, bool value, bool valueDefault, bool isValueDynamic = false );
            //MaterialInput( const string & name, int value, int valueDefault, int valueMin, int valueMax, bool isValueDynamic = false );
            MaterialInput( const string & name, float value, float valueDefault, float valueMin = VA_FLOAT_LOWEST, float valueMax = VA_FLOAT_HIGHEST, bool isValueDynamic = false )
                :
                Name                    ( name ),
                Type                    ( InputType::Scalar ),
                Value                   ( value ),
                ValueDefault            ( valueDefault ),
                ValueMin                ( valueMin ),
                ValueMax                ( valueMax ),
                IsValueDynamic          ( isValueDynamic )
            {
            }
            //MaterialInput( const string & name, bool isColor, const vaVector4 & value, const vaVector4 & valueDefault, const vaVector4 & valueMin, const vaVector4 & valueMax, bool isValueDynamic = false );

            bool                    operator == ( const MaterialInput & other ) const 
            {
                return
                (this->Name                     == other.Name                    ) &&
                (this->Type                     == other.Type                    ) &&
                (this->Value                    == other.Value                   ) &&
                (this->ValueDefault             == other.ValueDefault            ) &&
                (this->ValueMin                 == other.ValueMin                ) &&
                (this->ValueMax                 == other.ValueMax                ) &&
                (this->IsValueDynamic           == other.IsValueDynamic          ) &&
                (this->SourceTextureID          == other.SourceTextureID         ) &&
                (this->SourceTextureUVIndex     == other.SourceTextureUVIndex    ) &&
                (this->SourceTextureSamplerType == other.SourceTextureSamplerType);
            }
            bool                    operator != ( const MaterialInput & other ) const { return !(*this == other); }

            shared_ptr<vaTexture>   GetSourceTexture( );

            // bool                    SaveAPACK( vaStream & outStream );
            // bool                    LoadAPACK( vaStream & inStream );
            bool                    Serialize( vaXMLSerializer & serializer );

            bool                    IsValid( ) const        { return Name.size() > 0; }

            bool                    IsTexture( ) const      { return SourceTextureID != vaCore::GUIDNull(); }

            void                    ResetTemps( )
            {
                ComputedSourceTextureShaderSlot                 = -1;
                ComputedSourceTextureShaderVariableName         = "";
                ComputedDynamicValueConstantBufferShaderIndex   = -1;
                ComputedDynamicValueConstantBufferByteOffset    = -1;
                CachedSourceTexture.reset();
            }

            // for the future....
            // void                    SetValueTexture( const shared_ptr<vaTexture> texture );
            // void                    SetValueInteger( const int32 & value, const int32 & valueMin = INT_MIN, const int32 & valueMax = INT_MAX );
            // void                    SetValueScalar( const float & value, const float & valueMin = -1e6f, const float & valueMax = 1e6f );
            // void                    SetValueFloat4( const vaVector4 & value, const vaVector4 & valueMin = vaVector4( -1e6f, -1e6f, -1e6f, -1e6f ), const vaVector4 & valueMax = vaVector4( 1e6f, 1e6f, 1e6f, 1e6f ) );
            // void                    SetValueColor( const vaVector4 & value, const vaVector4 & valueMin = vaVector4( -1e6f, -1e6f, -1e6f, -1e6f ), const vaVector4 & valueMax = vaVector4( 1e6f, 1e6f, 1e6f, 1e6f ) );
        };

        struct MaterialSettings
        {
            vaFaceCull                                      FaceCull                = vaFaceCull::Back;
            bool                                            AlphaTest               = false;
            float                                           AlphaTestThreshold      = 0.5f;             // to be moved to Inputs if needed
            bool                                            ReceiveShadows          = true;
            bool                                            CastShadows             = true;
            bool                                            Wireframe               = false;
            bool                                            Transparent             = false;            // if true, "CastShadows" will be ignored
            bool                                            AdvancedSpecularShader  = false;
            bool                                            SpecialEmissiveLight    = false;            // only add emissive within light sphere, and then scale with light itself; this is to allow emissive materials to be 'controlled' by the light - useful for models that represent light emitters (lamps, etc.)

            MaterialSettings( ) { }

            inline bool operator == ( const MaterialSettings & mat ) const
            {
                return 0 == memcmp( this, &mat, sizeof( MaterialSettings ) );
            }

            inline bool operator != ( const MaterialSettings & mat ) const
            {
                return 0 != memcmp( this, &mat, sizeof( MaterialSettings ) );
            }
        };

        struct ShaderSettings
        {
            string                              FileName;
            //string                              EntryVS_PosOnly;
            string                              EntryVS_Standard;
            string                              EntryPS_DepthOnly;
            string                              EntryPS_Forward;
            string                              EntryPS_Deferred;
            string                              EntryPS_CustomShadow;
            vector< pair< string, string > >    BaseMacros;
        };

    private:
        //wstring const                                   m_name;                 // unique (within renderMeshManager) name
        vaTT_Trackee< vaRenderMaterial * >              m_trackee;

    protected:
        vaRenderMaterialManager &                       m_renderMaterialManager;

        // Primary material data
        MaterialSettings                             m_materialSettings;
        ShaderSettings                                  m_shaderSettings;
        std::vector<MaterialInput>                      m_inputs;

        // Secondary / runtime material data
        // for future needs
        //std::map<string, int>                           m_inputsMap;        // map to m_inputs indices with Name used as a key

        // shader linkage (generated after inputs loading)
        int                                             m_computedTextureSlotCount;
        int                                             m_computedVec4ConstantCount;
        int                                             m_computedScalarConstantCount;

        vector< pair< string, string > >                m_shaderMacros;
        bool                                            m_shaderMacrosDirty;
        bool                                            m_shadersDirty;

        bool                                            m_inputsDirty;

        shared_ptr<vaRenderMaterialCachedShaders>       m_shaders;

        vaTypedConstantBufferWrapper< RenderMeshMaterialConstants >
                                                        m_constantsBuffer;


    protected:
        friend class vaRenderMeshManager;
        vaRenderMaterial( const vaRenderingModuleParams & params );
    public:
        virtual ~vaRenderMaterial( )                { }

    public:
        //const wstring &                                 GetName( ) const                                                { return m_name; };

        const MaterialSettings &                        GetMaterialSettings( )                                          { return m_materialSettings; }
        void                                            SetMaterialSettings( const MaterialSettings & settings )        { if( m_materialSettings != settings ) m_shaderMacrosDirty = true; m_materialSettings = settings; }

        const ShaderSettings &                          GetShaderSettings( )                                            { return m_shaderSettings; }
        void                                            SetShaderSettings( const ShaderSettings & settings )            { /*if( m_shaderSettings != settings )*/ { m_shaderMacrosDirty = true; } m_shaderSettings = settings; }

        void                                            SetSettingsDirty( )                                             { m_shaderMacrosDirty = true; }

        vaRenderMaterialManager &                       GetManager( ) const                                             { m_renderMaterialManager; }
        int                                             GetListIndex( ) const                                           { return m_trackee.GetIndex( ); }

        const std::vector<MaterialInput> &              GetInputs( ) const                                              { return m_inputs; }
        int                                             FindInputByName( const string & name ) const                    { for( int i = 0; i < m_inputs.size(); i++ ) if( vaStringTools::ToLower( m_inputs[i].Name ) == vaStringTools::ToLower( name ) ) return i; return -1; }

        bool                                            RemoveInput( int index );
        bool                                            SetInput( int index, const MaterialInput & newValue );
        bool                                            SetInputByName( const MaterialInput & newValue, bool addIfNotFound = false );

        bool                                            GetNeedsPSForShadowGenerate( ) const                            { return false; }

        vaFaceCull                                      GetFaceCull( ) const                                            { return m_materialSettings.FaceCull; }

        // disabled for now (for simplicity reasons - and it's small anyway) but the vaAssetRenderMesh will use SerializeUnpacked to/from memory stream to support APACK! :)
        bool                                            SaveAPACK( vaStream & outStream ) override;
        bool                                            LoadAPACK( vaStream & inStream ) override;
        bool                                            SerializeUnpacked( vaXMLSerializer & serializer, const wstring & assetFolder ) override;

        //virtual void                                    ReconnectDependencies( );
        virtual void                                    RegisterUsedAssetPacks( std::function<void( const vaAssetPack & )> registerFunction ) override;

        // // sets vertex shader, pixel shader and textures
        // virtual void                                    SetToAPIContext( vaSceneDrawContext & drawContext, vaRenderMaterialShaderType shaderType, bool assertOnOverwrite = true )  = 0;
        // virtual void                                    UnsetFromAPIContext( vaSceneDrawContext & drawContext, vaRenderMaterialShaderType shaderType, bool assertOnNotSet = true ) = 0;
        
        void                                            InitializeDefaultMaterial( );

        bool                                            SetToRenderItem( vaRenderItem & renderItem, vaRenderMaterialShaderType shaderType );

    protected:
        shared_ptr<vaVertexShader>                      GetVS( vaRenderMaterialShaderType shaderType );
        shared_ptr<vaPixelShader>                       GetPS( vaRenderMaterialShaderType shaderType );

        void                                            VerifyNames( );

        void                                            UpdateShaderMacros( );

        static std::vector<MaterialInput>               GetStandardMaterialInputs( StandardMaterialTemplateType type );

        // called after m_inputs are loaded
        void                                            UpdateInputsDependencies( );
    };

    class vaRenderMaterialManager : public vaRenderingModule, public vaImguiHierarchyObject
    {
        //VA_RENDERING_MODULE_MAKE_FRIENDS( );
    protected:

        vaTT_Tracker< vaRenderMaterial * >              m_renderMaterials;
        // map<wstring, shared_ptr<vaRenderMaterial>>  m_renderMaterialsMap;

        shared_ptr< vaRenderMaterial >                  m_defaultMaterial;
        bool                                            m_isDestructing;

        bool                                            m_texturingDisabled;

        map< vaRenderMaterialCachedShaders::Key, weak_ptr<vaRenderMaterialCachedShaders> >
                                                        m_cachedShaders;

    public:
        //friend class vaRenderingCore;
        vaRenderMaterialManager( const vaRenderingModuleParams & params );
        virtual ~vaRenderMaterialManager( );

    private:
        void                                            RenderMaterialsTrackeeAddedCallback( int newTrackeeIndex );
        void                                            RenderMaterialsTrackeeBeforeRemovedCallback( int removedTrackeeIndex, int replacedByTrackeeIndex );

    public:
        shared_ptr<vaRenderMaterial>                    GetDefaultMaterial( ) const     { return m_defaultMaterial; }

    public:
        shared_ptr<vaRenderMaterial>                    CreateRenderMaterial( const vaGUID & uid = vaCore::GUIDCreate( ) );
        vaTT_Tracker< vaRenderMaterial * > *            GetRenderMaterialTracker( ) { return &m_renderMaterials; }

        bool                                            GetTexturingDisabled( ) const   { return m_texturingDisabled; }
        void                                            SetTexturingDisabled( bool texturingDisabled );

    public:
        shared_ptr<vaRenderMaterialCachedShaders>       FindOrCreateShaders( const string & fileName, bool alphaTest, const string & entryPS_DepthOnly, const string & entryVS_Standard, string & entryPS_Forward, const string & entryPS_Deferred, const string & entryPS_CustomShadow, const vector< pair< string, string > > & shaderMacros );

    protected:
        virtual string                                  IHO_GetInstanceName( ) const { return vaStringTools::Format( "vaRenderMaterialManager (%d meshes)", m_renderMaterials.size( ) ); }
        virtual void                                    IHO_Draw( );
    };


}