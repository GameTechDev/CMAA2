///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017, Intel Corporation
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

#include "Rendering/vaRenderingIncludes.h"

namespace VertexAsylum
{
    class vaInputMouseBase;

    class vaZoomTool : public VertexAsylum::vaRenderingModule, public vaImguiHierarchyObject
    {
    public:
        struct Settings
        {
            bool                    Enabled     = false;
            int                     ZoomFactor  = 6;
            vaVector2i              BoxPos      = vaVector2i( 400, 300 );
            vaVector2i              BoxSize     = vaVector2i( 96, 64 );

            Settings( )            { }
        };

    protected:
        shared_ptr<vaTexture>       m_edgesTexture;

        Settings                    m_settings;

        vaTypedConstantBufferWrapper<ZoomToolShaderConstants>
                                    m_constantsBuffer;

        vaAutoRMI<vaComputeShader>      m_CSZoomToolFloat;
        vaAutoRMI<vaComputeShader>      m_CSZoomToolUnorm;

    protected:
        vaZoomTool( const vaRenderingModuleParams & params );
    public:
        ~vaZoomTool( );

    public:
        Settings &                  Settings( )                                                             { return m_settings; }

        void                        HandleMouseInputs( vaInputMouseBase & mouseInput );

        virtual void                Draw( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & colorInOut );

    protected:
        virtual void                UpdateConstants( vaRenderDeviceContext & apiContext );

    private:
        virtual string              IHO_GetInstanceName( ) const { return "ZoomTool"; }
        virtual void                IHO_Draw( );
    };
    
}
