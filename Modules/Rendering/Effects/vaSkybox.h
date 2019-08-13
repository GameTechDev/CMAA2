/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// VertexAsylum Codebase, Copyright (c) Filip Strugar.
// Contents of this file are distributed under MIT license (https://en.wikipedia.org/wiki/MIT_License)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Core/vaCoreIncludes.h"

#include "Rendering/vaRenderingIncludes.h"

#ifndef __INTELLISENSE__
#include "Rendering/Shaders/vaSkybox.hlsl"
#endif

namespace VertexAsylum
{
    class vaSkybox : public VertexAsylum::vaRenderingModule
    {
    public:
        struct Settings
        {
            vaMatrix3x3             Rotation        = vaMatrix3x3::Identity;
            float                   ColorMultiplier = 1.0f;

            Settings( )            { }
        };

    protected:
        shared_ptr<vaTexture>       m_cubemap;

        Settings                    m_settings;

        vaTypedConstantBufferWrapper<SkyboxConstants>
                                    m_constantsBuffer;

        vaAutoRMI<vaVertexShader>   m_vertexShader;
        vaAutoRMI<vaPixelShader>    m_pixelShader;

        vaTypedVertexBufferWrapper < vaVector3 > m_screenTriangleVertexBuffer;
        vaTypedVertexBufferWrapper < vaVector3 > m_screenTriangleVertexBufferReversedZ;

    protected:
        vaSkybox( const vaRenderingModuleParams & params );
    public:
        ~vaSkybox( );

    public:
        Settings &                  Settings( )                                         { return m_settings; }

        shared_ptr<vaTexture>       GetCubemap( ) const                                 { return m_cubemap;     }
        void                        SetCubemap( const shared_ptr<vaTexture> & cubemap ) { m_cubemap = cubemap;  }

    protected:
        virtual void                UpdateConstants( vaSceneDrawContext & drawContext );

    public:
        virtual vaDrawResultFlags   Draw( vaSceneDrawContext & drawContext );
    };

}
