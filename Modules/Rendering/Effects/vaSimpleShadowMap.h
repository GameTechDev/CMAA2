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

// !!! SCHEDULED FOR DELETION !!!

#pragma once

#include "Core/vaCoreIncludes.h"

#include "Rendering/vaRenderingIncludes.h"

namespace VertexAsylum
{
    /*
    class vaSimpleVolumeShadowMapPlugin
    {
    public:
        virtual ~vaSimpleVolumeShadowMapPlugin( ) { }

        virtual const std::vector< std::pair< std::string, std::string > > &
            GetShaderMacros( ) = 0;

        virtual void                        StartGenerating( vaSceneDrawContext & context, vaSimpleShadowMap * ssm )   = 0;
        virtual void                        StopGenerating( vaSceneDrawContext & context, vaSimpleShadowMap * ssm )    = 0;
        virtual void                        StartUsing( vaSceneDrawContext & context, vaSimpleShadowMap * ssm )        = 0;
        virtual void                        StopUsing( vaSceneDrawContext & context, vaSimpleShadowMap * ssm )         = 0;
        
        virtual void                        SetDebugTexelLocation( int x, int y )                                   = 0;
        virtual int                         GetResolution( )                                                        = 0;
        virtual bool                        GetSupported( )                                                         = 0;    // supported on current hardware?
    };

    class vaSimpleShadowMapAPIInternalCallbacks
    {
    public:
        virtual void                    InternalResolutionOrTexelWorldSizeChanged( )                                = 0;
        virtual void                    InternalStartGenerating( const vaSceneDrawContext & context )                  = 0;
        virtual void                    InternalStopGenerating( const vaSceneDrawContext & context )                   = 0;
        virtual void                    InternalStartUsing( const vaSceneDrawContext & context )                       = 0;
        virtual void                    InternalStopUsing( const vaSceneDrawContext & context )                        = 0;
    };

    // a very simple directional shadow map
    class vaSimpleShadowMap : public VertexAsylum::vaRenderingModule, protected vaSimpleShadowMapAPIInternalCallbacks
    {
    private:

        int                             m_resolution;
        vaOrientedBoundingBox           m_volume;

        std::shared_ptr<vaTexture>      m_shadowMap;

        vaMatrix4x4                     m_view;
        vaMatrix4x4                     m_proj;
        vaMatrix4x4                     m_viewProj;
        vaVector2                       m_texelSize;

        vaSimpleVolumeShadowMapPlugin * m_volumeShadowMapPlugin;

    protected:
        vaSimpleShadowMap( );

    public:
        ~vaSimpleShadowMap( );

    public:
        void                                Initialize( int resolution );

        void                                SetVolumeShadowMapPlugin( vaSimpleVolumeShadowMapPlugin * vsmp )                            { m_volumeShadowMapPlugin = vsmp; }
        vaSimpleVolumeShadowMapPlugin *     GetVolumeShadowMapPlugin( ) const                                                           { return m_volumeShadowMapPlugin; }

        void                                UpdateArea( const vaOrientedBoundingBox & volume );

        const std::shared_ptr<vaTexture> &  GetShadowMapTexture( ) const                        { return m_shadowMap; }
        const vaMatrix4x4 &                 GetViewMatrix( ) const                              { return m_view;     }
        const vaMatrix4x4 &                 GetProjMatrix( ) const                              { return m_proj;     }
        const vaMatrix4x4 &                 GetViewProjMatrix( ) const                          { return m_viewProj; }

        int                                 GetResolution( ) const                              { return m_resolution; }
        const vaOrientedBoundingBox &       GetVolume( ) const                                  { return m_volume; }
        const vaVector2 &                   GetTexelSize( ) const                               { return m_texelSize; }

        void                                StartGenerating( vaSceneDrawContext & context )        { InternalStartGenerating( context ); assert( context.SimpleShadowMap == NULL ); context.SimpleShadowMap = this; }
        void                                StopGenerating( vaSceneDrawContext & context )         { InternalStopGenerating(  context ); assert( context.SimpleShadowMap == this ); context.SimpleShadowMap = NULL; }
        void                                StartUsing( vaSceneDrawContext & context )             { InternalStartUsing(      context ); assert( context.SimpleShadowMap == NULL ); context.SimpleShadowMap = this; }
        void                                StopUsing( vaSceneDrawContext & context )              { InternalStopUsing(       context ); assert( context.SimpleShadowMap == this ); context.SimpleShadowMap = NULL; }

    public:

    private:
        void                                SetResolution( int resolution );

    };
    */
}
