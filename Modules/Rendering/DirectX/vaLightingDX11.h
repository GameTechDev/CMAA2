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

#include "Core/vaCoreIncludes.h"

#include "Rendering/vaLighting.h"

#include "Rendering/DirectX/vaDirectXIncludes.h"

#define LIGHTING_SLOT0   0
#define LIGHTING_SLOT1   1
#define LIGHTING_SLOT2   2

namespace VertexAsylum
{

    class vaLightingDX11 : public vaLighting
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    private:

    protected:
        explicit vaLightingDX11( const vaRenderingModuleParams & params );
        ~vaLightingDX11( );

    public:
        void                                    SetAPIGlobals( vaSceneDrawContext & drawContext );
        void                                    UnsetAPIGlobals( vaSceneDrawContext & drawContext );

    private:
        virtual void                            UpdateResourcesIfNeeded( vaSceneDrawContext & drawContext ) override;
        virtual void                            ApplyDirectionalAmbientLighting( vaSceneDrawContext & drawContext, vaGBuffer & GBuffer ) override;
        virtual void                            ApplyDynamicLighting( vaSceneDrawContext & drawContext, vaGBuffer & GBuffer ) override;
        //virtual void                            ApplyTonemap( vaSceneDrawContext & drawContext, vaGBuffer & GBuffer );

    public:
    };

}
