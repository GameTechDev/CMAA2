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

#include "Rendering/Shaders/vaSharedTypes_HelperTools.h"

#include "Core/vaUI.h"

namespace VertexAsylum
{
    // will be moved to its own file at some point, if it ever grows into something more serious
    class vaImageCompareTool : public VertexAsylum::vaRenderingModule, public vaUIPanel
    {
    public:
        enum class VisType
        {
            None,
            ShowReference,
            ShowDifference,
            ShowDifferenceX10,
            ShowDifferenceX100
        };

    protected:
        shared_ptr<vaTexture>       m_referenceTexture;
        shared_ptr<vaTexture>       m_helperTexture;

        bool                        m_saveReferenceScheduled            = false;
        bool                        m_compareReferenceScheduled         = false;

        wstring                     m_referenceTextureStoragePath;
        wstring                     m_screenshotCapturePath;

        VisType                     m_visualizationType                 = VisType::None;

        vaAutoRMI<vaPixelShader>    m_visualizationPS;
        vaTypedConstantBufferWrapper< ImageCompareToolShaderConstants >
                                    m_constants;

        bool                        m_initialized;

    public: //protected:
        vaImageCompareTool( const vaRenderingModuleParams & params );
    public:
        ~vaImageCompareTool( );

    public:
        virtual void                RenderTick( vaRenderDeviceContext & renderContext, const shared_ptr<vaTexture> & colorInOut, shared_ptr<vaPostProcess> & postProcess );

    public:
        virtual void                SaveAsReference( vaRenderDeviceContext & renderContext, const shared_ptr<vaTexture> & colorInOut );
        virtual vaVector4           CompareWithReference( vaRenderDeviceContext & renderContext, const shared_ptr<vaTexture> & colorInOut, shared_ptr<vaPostProcess> & postProcess );

    protected:

    private:
        virtual void                UIPanelDraw( ) override;
    };


}
