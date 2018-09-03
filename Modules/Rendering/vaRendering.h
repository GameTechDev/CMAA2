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

#include "Scene/vaCameraBase.h"

#include "Core/vaUIDObject.h"

#include "Core/Misc/vaProfiler.h"

#include "Rendering/vaRenderDevice.h"

namespace VertexAsylum
{
    class vaRenderingModule;
    class vaRenderingModuleAPIImplementation;
    //class vaRenderDevice;
    //class vaRenderDeviceContext;
    class vaRenderingGlobals;
    class vaLighting;
    class vaAssetPack;
    class vaTexture;
    class vaXMLSerializer;
    class vaPixelShader;
    class vaConstantBuffer;
    class vaVertexBuffer;
    class vaIndexBuffer;
    class vaVertexShader;
    class vaGeometryShader;
    class vaHullShader;
    class vaDomainShader;
    class vaPixelShader;
    class vaComputeShader;
    class vaShaderResource;

    // This should be renamed to something more sensible - render type? render path? 
    enum class vaDrawContextOutputType
    {
        DepthOnly       = 0,            // Z-pre-pass or shadowmap; no pixels are shaded unless alpha tested
        Forward         = 1,            // regular rendering (used for post process passes as well)
        Deferred        = 2,            // usually multiple render targets, no blending, etc
        CustomShadow    = 3,            // all non-depth-only shadow variations
        //GenericShading                // maybe for future testing/prototyping, a generic function similar to CustomShadow
    };

    enum class vaDrawContextFlags : uint32
    {
        None                                        = 0,
        DebugWireframePass                          = ( 1 << 0 ),
        SetZOffsettedProjMatrix                     = ( 1 << 1 ),
        CustomShadowmapShaderLinearDistanceToCenter = ( 1 << 2 ),   // only valid when OutputType is of vaDrawContextOutputType::CustomShadow
    };

    BITFLAG_ENUM_CLASS_HELPER( vaDrawContextFlags );

    enum class vaBlendMode
    {
        Opaque,
        Additive,
        AlphaBlend,
        PremultAlphaBlend,
        Mult,
    };

    enum class vaPrimitiveTopology
    {
        PointList,
        LineList,
        TriangleList,
        TriangleStrip,
    };

    // don't change these enum values - they are expected to be what they're set to.
    enum class vaComparisonFunc
    {
        Never           = 1,
        Less            = 2,
        Equal           = 3,
        LessEqual       = 4,
        Greater         = 5,
        NotEqual        = 6,
        GreaterEqual    = 7,
        Always          = 8
    };

    enum class vaFillMode
    {
        Wireframe       = 2,
        Solid           = 3
    };

    enum class vaDrawResultFlags
    {
        None                    = 0,            // means all ok
        UnspecifiedError        = ( 1 << 0 ),
        ShadersStillCompiling   = ( 2 << 0 ),
        AssetsStillLoading      = ( 3 << 0 )
    };
    BITFLAG_ENUM_CLASS_HELPER( vaDrawResultFlags );

    // Used for more complex rendering when there's camera, lighting, various other settings - not needed by many systems
    struct vaSceneDrawContext
    {
        vaSceneDrawContext( vaCameraBase & camera, vaRenderDeviceContext & deviceContext, vaRenderingGlobals & globals, vaDrawContextOutputType outputType, vaDrawContextFlags renderFlags = vaDrawContextFlags::None, vaLighting * lighting = nullptr )
            : Camera( camera ), APIContext( deviceContext ), Globals( globals ), OutputType( outputType ), RenderFlags( renderFlags ), Lighting( lighting ) { }

        // Mandatory
        vaRenderDeviceContext &         APIContext;         // vaRenderDeviceContext is used to get/set current render targets and access rendering API stuff like contexts, etc.
        vaCameraBase &                  Camera;             // Currently selected camera
        vaRenderingGlobals &            Globals;            // Used to set global shader constants, provide some debugging tools, etc.

        // Optional/settings
        vaDrawContextOutputType         OutputType;
        vaDrawContextFlags              RenderFlags;
        vaLighting *                    Lighting;
        vaVector4                       ViewspaceDepthOffsets   = vaVector4( 0.0f, 1.0f, 0.0f, 1.0f );      // used for depth bias for shadow maps and similar
        float                           GlobalMIPOffset         = 0.0f;                                     // global texture mip offset for those subsystems that support it
        vaVector2                       GlobalPixelScale        = vaVector2( 1.0f, 1.0f );                  // global pixel scaling for various effect that depend on pixel size - used to maintain consistency when supersampling using higher resolution (and effectively smaller pixel)
    };

    // This is a platform-independent layer for -immediate- rendering of a single draw call - it's not fully featured or designed
    // for high performance; for additional features there's a provision for API-dependent custom callbacks; for more 
    // performance/reducing overhead the alternative is to provide API-specific rendering module implementations.
    struct vaRenderItem
    {
        enum DrawType
        {
            DrawSimple,                   // Draw non-indexed, non-instanced primitives.
            // DrawAuto,                       // Draw geometry of an unknown size.
            DrawIndexed,                    // Draw indexed, non-instanced primitives.
            // DrawIndexedInstanced,           // Draw indexed, instanced primitives.
            // DrawIndexedInstancedIndirect,   // Draw indexed, instanced, GPU-generated primitives.
            // DrawInstanced,                  // Draw non-indexed, instanced primitives.
            // DrawInstancedIndirect,          //  Draw instanced, GPU-generated primitives.
        };

        DrawType                            DrawType                = DrawSimple;

        // If present, pick up various other stuff like lighting, some globals, etc.    Pointer only held between vaRenderDeviceContext::BeginItems/EndItem/ExecuteItem calls
        vaSceneDrawContext *                SceneDrawContext        = nullptr;

        // TOPOLOGY
        vaPrimitiveTopology                 Topology                = vaPrimitiveTopology::TriangleList;

        // BLENDING 
        vaBlendMode                         BlendMode               = vaBlendMode::Opaque;
        //vaVector4                           BlendFactor;
        //uint32                              BlendMask;

        // SHADER(s)
        shared_ptr<vaVertexShader>          VertexShader;
        shared_ptr<vaGeometryShader>        GeometryShader;
        shared_ptr<vaHullShader>            HullShader;
        shared_ptr<vaDomainShader>          DomainShader;
        shared_ptr<vaPixelShader>           PixelShader;

        // DEPTH
        bool                                DepthEnable             = false;
        bool                                DepthWriteEnable        = false;
        vaComparisonFunc                    DepthFunc               = vaComparisonFunc::Always;

        // STENCIL
        // <to add>

        // FILLMODE
        vaFillMode                          FillMode                = vaFillMode::Solid;

        // CULLMODE
        vaFaceCull                          CullMode                = vaFaceCull::Back;

        // MISC RASTERIZER
        bool                                FrontCounterClockwise   = false;
        // bool                                MultisampleEnable       = true;
        // bool                                ScissorEnable           = true;

        // SAMPLERS
        // not handled by API independent layer yet but default ones set with SetStandardSamplers - see SHADERGLOBAL_SHADOWCMP_SAMPLERSLOT and SHADERGLOBAL_POINTCLAMP_SAMPLERSLOT and others

        // CONSTANT BUFFERS
        shared_ptr<vaConstantBuffer>        ConstantBuffers[10];

        // SRVs
        shared_ptr<vaShaderResource>        ShaderResourceViews[20];

        // VERTICES/INDICES
        shared_ptr<vaVertexBuffer>          VertexBuffer;
        uint                                VertexBufferByteStride  = 0;    // 0 means pick up stride from vaVertexBuffer itself
        uint                                VertexBufferByteOffset  = 0;
        shared_ptr<vaIndexBuffer>           IndexBuffer;

        struct DrawSimpleParams
        {
            uint32                              VertexCount         = 0;    // (DrawSimple only) Number of vertices to draw.
            uint32                              StartVertexLocation = 0;    // (DrawSimple only) Index of the first vertex, which is usually an offset in a vertex buffer.
        }                                   DrawSimpleParams;
        struct DrawIndexedParams
        {
            uint32                              IndexCount          = 0;    // (DrawIndexed only) Number of indices to draw.
            uint32                              StartIndexLocation  = 0;    // (DrawIndexed only) The location of the first index read by the GPU from the index buffer.
            int32                               BaseVertexLocation  = 0;    // (DrawIndexed only) A value added to each index before reading a vertex from the vertex buffer.
        }                                   DrawIndexedParams;
        
        // Callback to insert any API-specific overrides or additional tweaks
        std::function<bool( const vaRenderItem &, vaRenderDeviceContext & )> PreDrawHook;
        std::function<void( const vaRenderItem &, vaRenderDeviceContext & )> PostDrawHook;

        // Helpers
        void                                SetDrawSimple( int vertexCount, int startVertexLocation )                           { this->DrawType = DrawSimple; DrawSimpleParams.VertexCount = vertexCount; DrawSimpleParams.StartVertexLocation = startVertexLocation; }
        void                                SetDrawIndexed( uint indexCount, uint startIndexLocation, int baseVertexLocation )  { this->DrawType = DrawIndexed; DrawIndexedParams.IndexCount = indexCount; DrawIndexedParams.StartIndexLocation = startIndexLocation; DrawIndexedParams.BaseVertexLocation = baseVertexLocation; }
    };

    // try testing with vaZoomToolDX11 - should be a good first candidate!
    struct vaComputeItem
    {
        enum ComputeType
        {
            Dispatch,
            DispatchIndirect,
        };

        ComputeType                         ComputeType             = Dispatch;

        // If present, pick up various other stuff like lighting, some globals, etc.    Pointer only held between vaRenderDeviceContext::BeginItems/EndItem/ExecuteItem calls
        vaSceneDrawContext *                SceneDrawContext        = nullptr;

        shared_ptr<vaComputeShader>         ComputeShader;

        // CONSTANT BUFFERS
        shared_ptr<vaConstantBuffer>        ConstantBuffers[_countof(vaRenderItem::ConstantBuffers)];           // keep the same count as in for render items for convenience and debugging safety

        // SRVs
        shared_ptr<vaShaderResource>        ShaderResourceViews[_countof(vaRenderItem::ShaderResourceViews)];   // keep the same count as in for render items for convenience and debugging safety

        // UAVs
        shared_ptr<vaShaderResource>        UnorderedAccessViews[20];       // I did not add provision for hidden counter resets - if needed better use a different approach (InterlockedX on a separate untyped UAV)


        struct DispatchParams
        {
            uint32                              ThreadGroupCountX = 0;
            uint32                              ThreadGroupCountY = 0;
            uint32                              ThreadGroupCountZ = 0;
        }                                   DispatchParams;

        struct DispatchIndirectParams
        {
            shared_ptr<vaShaderResource>        BufferForArgs;
            uint32                              AlignedOffsetForArgs = 0;
        }                                   DispatchIndirectParams;

        // Callback to insert any API-specific overrides or additional tweaks
        std::function<bool( const vaComputeItem &, vaRenderDeviceContext & )> PreComputeHook;

        // Helpers
        void                                SetDispatch( uint32 threadGroupCountX, uint32 threadGroupCountY, uint32 threadGroupCountZ )     { this->ComputeType = Dispatch; DispatchParams.ThreadGroupCountX = threadGroupCountX; DispatchParams.ThreadGroupCountY = threadGroupCountY; DispatchParams.ThreadGroupCountZ = threadGroupCountZ; }
        void                                SetDispatchIndirect( shared_ptr<vaShaderResource> bufferForArgs, uint32 alignedOffsetForArgs )  { this->ComputeType = DispatchIndirect; DispatchIndirectParams.BufferForArgs = bufferForArgs; DispatchIndirectParams.AlignedOffsetForArgs = alignedOffsetForArgs; }
    };
    

    class vaShadowmap;

    enum class vaRenderSelectionCullFlags
    {
        None                    = 0,
        UseBoundingSphereFrom   = ( 1 << 0 ),
        UseBoundingSphereTo     = ( 1 << 1 ),
        UseFrustumPlanes        = ( 1 << 2 ),

        // CullTransparencies      = ( 1 << 3 ),
        // CullNonTransparencies   = ( 1 << 4 ),
        // CullShadowCasters       = ( 1 << 5 ),
        // CullNonShadowCasters    = ( 1 << 6 ),
        // CullShadowReceivers     = 
        // CullNonShadowReceivers  = 
        // CullStaticObjects                        // defined as not changing in world space or shape between frames
        // CullDynamicObjects                       // defined as changing in world space or shape between frames
    };

    struct vaRenderSelectionFilter
    {
        vaBoundingSphere                BoundingSphereFrom  = vaBoundingSphere( { 0, 0, 0 }, 0.0f );
        vaBoundingSphere                BoundingSphereTo    = vaBoundingSphere( { 0, 0, 0 }, 0.0f );
        vaPlane                         FrustumPlanes[6];

        vaRenderSelectionCullFlags      CullFlags;

        // flags: use sphere, use frustum planes, etc...

        vaVector3                       SortCenter          = vaVector3( 0, 0, 0 );
        vaSortType                      SortCenterType      = vaSortType::None;

        vaRenderSelectionFilter( ) { }

        // regular draw for a given camera
        vaRenderSelectionFilter( const vaCameraBase & camera, vaSortType sortType = vaSortType::FrontToBack )   : SortCenter( camera.GetPosition() ), SortCenterType( sortType ), CullFlags( vaRenderSelectionCullFlags::UseFrustumPlanes ) { camera.CalcFrustumPlanes(FrustumPlanes); }

        // draw for a simple cubemap
        vaRenderSelectionFilter( vaVector3 position, vaSortType sortType = vaSortType::FrontToBack )            : SortCenter( position ), SortCenterType( sortType ), CullFlags( vaRenderSelectionCullFlags::None ) { }
        
        // depends on the shadowmap type
         explicit vaRenderSelectionFilter( const vaShadowmap & shadowmap );
    };

    BITFLAG_ENUM_CLASS_HELPER( vaRenderSelectionCullFlags );

    class vaRenderMeshDrawList;

    // This contains various things that can get collected for rendering; it can accept various contents from
    // multiple scenes or from outside of scenes or from any custom code
    struct vaRenderSelection
    {
        vaRenderSelectionFilter         Filter;

        shared_ptr<vaRenderMeshDrawList> MeshList;

        void                            Reset( );
    };

    // base type for forwarding vaRenderingModule constructor parameters
    struct vaRenderingModuleParams
    {
        vaRenderDevice & RenderDevice;
        vaRenderingModuleParams( vaRenderDevice & device ) : RenderDevice( device ) { }
        virtual ~vaRenderingModuleParams( ) { }
    };

    class vaRenderingModuleRegistrar : public vaSingletonBase < vaRenderingModuleRegistrar >
    {
        friend class vaRenderDevice;
        friend class vaCore;
    private:
        vaRenderingModuleRegistrar( )   { }
        ~vaRenderingModuleRegistrar( )  { }

        struct ModuleInfo
        {
            std::function< vaRenderingModule * ( const vaRenderingModuleParams & )> 
                                                ModuleCreateFunction;

            explicit ModuleInfo( const std::function< vaRenderingModule * ( const vaRenderingModuleParams & )> & moduleCreateFunction )
                : ModuleCreateFunction( moduleCreateFunction ) { }
        };

        std::map< std::string, ModuleInfo >     m_modules;
        std::recursive_mutex                    m_mutex;

    public:
        static void                             RegisterModule( const std::string & deviceTypeName, const std::string & name, std::function< vaRenderingModule * ( const vaRenderingModuleParams & )> moduleCreateFunction );
        static vaRenderingModule *              CreateModule( const std::string & deviceTypeName, const std::string & name, const vaRenderingModuleParams & params );
        //void                                  CreateModuleArray( int inCount, vaRenderingModule * outArray[] );

        template< typename ModuleType >
        static ModuleType *                     CreateModuleTyped( const std::string & name, const vaRenderingModuleParams & params );

        template< typename ModuleType >
        static ModuleType *                     CreateModuleTyped( const std::string & name, vaRenderDevice & deviceObject );


//    private:
//        static void                             CreateSingletonIfNotCreated( );
//        static void                             DeleteSingleton( );
    };

    class vaRenderingModule
    {
        string                              m_renderingModuleTypeName;

    protected:
        friend class vaRenderingModuleRegistrar;
        friend class vaRenderingModuleAPIImplementation;

        vaRenderDevice &                    m_renderDevice;

    protected:
        vaRenderingModule( const vaRenderingModuleParams & params ) : m_renderDevice( params.RenderDevice )   {  }
                                            
//                                            vaRenderingModule( const char * renderingCounterpartID );
    public:
        virtual                             ~vaRenderingModule( )                                                       { }

    private:
        // called only by vaRenderingModuleRegistrar::CreateModule
        void                                InternalRenderingModuleSetTypeName( const string & name )                   { m_renderingModuleTypeName = name; }

    public:
        const char *                        GetRenderingModuleTypeName( )                                               { return m_renderingModuleTypeName.c_str(); }
        vaRenderDevice &                    GetRenderDevice( )                                                          { return m_renderDevice; }
        vaRenderDevice &                    GetRenderDevice( ) const                                                    { return m_renderDevice; }

    public:
        template< typename CastToType >
        CastToType                          SafeCast( )                                                                 
        { 
           CastToType ret = dynamic_cast< CastToType >( this );
           assert( ret != NULL );
           return ret;
        }

    };

    template< typename DeviceType, typename ModuleType, typename ModuleAPIImplementationType >
    class vaRenderingModuleAutoRegister
    {
    public:
        explicit vaRenderingModuleAutoRegister( )
        {
            vaRenderingModuleRegistrar::RegisterModule( typeid(DeviceType).name(), typeid(ModuleType).name(), &CreateNew );
        }
        ~vaRenderingModuleAutoRegister( ) { }

    private:
        static inline vaRenderingModule *                   CreateNew( const vaRenderingModuleParams & params )
        { 
            return static_cast<vaRenderingModule*>( new ModuleAPIImplementationType( params ) ); 
        }
    };

    // to be placed at the end of the API counterpart module C++ file
    // example: 
    //      VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, Tree, TreeDX11 );
#define VA_RENDERING_MODULE_REGISTER( DeviceType, ModuleType, ModuleAPIImplementationType )                                                    \
        vaRenderingModuleAutoRegister< DeviceType, ModuleType, ModuleAPIImplementationType > autoReg##ModuleType_##ModuleAPIImplementationType;     \

// A helper for implementing vaRenderingModule and vaRenderingModuleAPIImplementation 
#define     VA_RENDERING_MODULE_MAKE_FRIENDS( )                                                                               \
    template< typename DeviceType, typename ModuleType, typename ModuleAPIImplementationType > friend class vaRenderingModuleAutoRegister;       \
    friend class vaRenderingModuleRegistrar;                                                                                            

    // A helper used to create rendering module and its counterpart at runtime
    // use example: 
    //      Tree * newTree = VA_RENDERING_MODULE_CREATE( Tree );
    // in addition, parameters can be passed through a pointer to vaRenderingModuleParams object; for example:
    //      vaTextureConstructorParams params( vaCore::GUIDCreate( ) );
    //      vaTexture * texture = VA_RENDERING_MODULE_CREATE( vaTexture, &params );
#define VA_RENDERING_MODULE_CREATE( ModuleType, Param )         vaRenderingModuleRegistrar::CreateModuleTyped<ModuleType>( typeid(ModuleType).name(), Param )
#define VA_RENDERING_MODULE_CREATE_SHARED( ModuleType, Param )  std::shared_ptr< ModuleType >( vaRenderingModuleRegistrar::CreateModuleTyped<ModuleType>( typeid(ModuleType).name(), Param ) )
#define VA_RENDERING_MODULE_CREATE_UNIQUE( ModuleType, Param )  std::unique_ptr< ModuleType >( vaRenderingModuleRegistrar::CreateModuleTyped<ModuleType>( typeid(ModuleType).name(), Param ) )

    template< typename ModuleType >
    inline ModuleType * vaRenderingModuleRegistrar::CreateModuleTyped( const std::string & name, const vaRenderingModuleParams & params )
    {
        ModuleType * ret = NULL;
        vaRenderingModule * createdModule;
        createdModule = CreateModule( typeid(params.RenderDevice).name(), name, params );

        ret = dynamic_cast<ModuleType*>( createdModule );
        if( ret == NULL )
        {
            wstring wname = vaStringTools::SimpleWiden( name );
            VA_ERROR( L"vaRenderingModuleRegistrar::CreateModuleTyped failed for '%s'; have you done VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaSomeClass, vaSomeClassDX11 )?; is vaSomeClass inheriting vaRenderingModule with 'public'? ", wname.c_str() );
        }

        return ret;
    }

    template< typename ModuleType >
    inline ModuleType * vaRenderingModuleRegistrar::CreateModuleTyped( const std::string & name, vaRenderDevice & deviceObject )
    {
        ModuleType * ret = NULL;
        vaRenderingModule * createdModule;
        vaRenderingModuleParams tempParams( deviceObject );
        createdModule = CreateModule( typeid(tempParams.RenderDevice).name(), name, tempParams );

        ret = dynamic_cast<ModuleType*>( createdModule );
        if( ret == NULL )
        {
            wstring wname = vaStringTools::SimpleWiden( name );
            VA_ERROR( L"vaRenderingModuleRegistrar::CreateModuleTyped failed for '%s'; have you done VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaSomeClass, vaSomeClassDX11 )?; is vaSomeClass inheriting vaRenderingModule with 'public'? ", wname.c_str() );
        }

        return ret;
    }


    // TODO: upgrade to variadic template when you figure out how :P
    template< typename T >
    class vaAutoRenderingModuleInstance
    {
        shared_ptr<T> const   m_instance;

    public:
        vaAutoRenderingModuleInstance( const vaRenderingModuleParams & params ) : m_instance( shared_ptr<T>( vaRenderingModuleRegistrar::CreateModuleTyped<T>( typeid(T).name(), params ) ) ) { }
        vaAutoRenderingModuleInstance( vaRenderDevice & device )                : m_instance( shared_ptr<T>( vaRenderingModuleRegistrar::CreateModuleTyped<T>( typeid(T).name(), device ) ) ) { }
        ~vaAutoRenderingModuleInstance()    { }

        T & operator*( ) const
        {	// return reference to object
            return ( *m_instance );
        }

        const shared_ptr<T> & operator->( ) const
        {	// return pointer to class object
            return m_instance;
        }

        const shared_ptr<T> & get( ) const
        {	// return pointer to class object
            return m_instance;
        }

        operator const shared_ptr<T> & ( ) const
        {	// return pointer to class object
            return m_instance;
        }
    };

    template< class _Ty >
    using vaAutoRMI = vaAutoRenderingModuleInstance< _Ty >;

    // this should go into a separate header (vaAsset.h?)
    struct vaAsset;
    class vaAssetResource : public std::enable_shared_from_this<vaAssetResource>, public vaUIDObject
    {
    private:
        const vaAsset *     m_parentAsset;

        bool                m_UI_ShowSelected   = false;
    
    protected:
        vaAssetResource( const vaGUID & uid ) : vaUIDObject( uid )          { m_parentAsset = nullptr; }
        virtual ~vaAssetResource( )                                         { assert( m_parentAsset == nullptr ); }
    
    public:
        const vaAsset *     GetParentAsset( )                               { return m_parentAsset; }

        virtual bool        LoadAPACK( vaStream & inStream )                               = 0;
        virtual bool        SaveAPACK( vaStream & outStream )                                                       = 0;
        virtual bool        SerializeUnpacked( vaXMLSerializer & serializer, const wstring & assetFolder )          = 0;

        // just for UI display
        bool                GetUIShowSelected( ) const                      { return m_UI_ShowSelected; }
        void                SetUIShowSelected( bool showSelected )          { m_UI_ShowSelected = showSelected; }

    private:
        friend struct vaAsset;
        void                SetParentAsset( const vaAsset * asset )         
        { 
            // there can be only one asset resource linked to one asset; this is one (of the) way to verify it:
            if( asset == nullptr )
            {
                assert( m_parentAsset != nullptr );
            }
            else
            {
                assert( m_parentAsset == nullptr );
            }
            m_parentAsset = asset; 
        }

//    protected:
//        virtual void        ReconnectDependencies( )                        { }

    public:
        virtual void        RegisterUsedAssetPacks( std::function<void( const vaAssetPack & )> registerFunction );
    };
}


#ifdef VA_SCOPE_PROFILER_ENABLED

#if defined( VA_REMOTERY_INTEGRATION_ENABLED )

    #if defined( VA_REMOTERY_INTEGRATION_USE_D3D11 )
        #define VA_SCOPE_CPUGPU_TIMER( name, apiContext )                                       vaScopeTimer scope_##name( #name, &apiContext ); rmt_ScopedCPUSample( name, 0 ); rmt_ScopedD3D11Sample( name )
    #else
        #define VA_SCOPE_CPUGPU_TIMER( name, apiContext )                                       vaScopeTimer scope_##name( #name, &apiContext ); rmt_ScopedCPUSample( name, 0 )
    #endif

#else

    #define VA_SCOPE_CPUGPU_TIMER( name, apiContext )                                       vaScopeTimer scope_##name( #name, &apiContext )

#endif

#else
    #define VA_SCOPE_CPUGPU_TIMER( name, apiContext )                                       
#endif