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

#include "Rendering/DirectX/vaRenderDeviceDX12.h"

// #include "Rendering/DirectX/vaTriangleMeshDX11.h"
//#include "Rendering/DirectX/vaDirectXTools.h"


#include "Rendering/vaRenderGlobals.h"

namespace VertexAsylum
{

    class vaRenderGlobalsDX12 : public vaRenderGlobals
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    private:

    private:
        vaRenderGlobalsDX12( const vaRenderingModuleParams & params );
        ~vaRenderGlobalsDX12( );

    public:

    private:
        // vaRenderGlobals
        virtual void                    UpdateDebugOutputFloats( vaSceneDrawContext & drawContext );
    };
}

