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

#include "Rendering/vaRenderMesh.h"

#include "Rendering/vaAssetPack.h"

#include "Scene/vaScene.h"

namespace VertexAsylum
{
    class vaAssetImporter : public vaUIPanel
    {
    public:

        struct ImporterSettings
        {
            bool                        TextureOnlyLoadDDS                  = false;
            bool                        TextureTryLoadDDS                   = true;
            bool                        TextureAlphaMaskInColorAlpha        = true;

            bool                        AIForceGenerateNormals              = false;
            bool                        AIGenerateNormalsIfNeeded           = true;
            bool                        AIGenerateSmoothNormalsIfGenerating = true;
            float                       AIGenerateSmoothNormalsSmoothingAngle = 88.0f;        // in degrees, see AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE for more info
            //bool                        RegenerateTangents;  

            bool                        AISplitLargeMeshes                  = true;        // aiProcess_SplitLargeMeshes
            bool                        AIFindInstances                     = true;        // aiProcess_FindInstances
            bool                        AIOptimizeMeshes                    = true;        // aiProcess_OptimizeMeshes
            bool                        AIOptimizeGraph                     = true;        // aiProcess_OptimizeGraph

            bool                        EnableLogInfo                       = true;
            bool                        EnableLogWarning                    = true;
            bool                        EnableLogError                      = true;
        };

        struct ImporterContext
        {
            vaRenderDevice &            Device;

            // pack to save assets into and to search dependencies to link to
            vaAssetPack &               AssetPack;
            vaScene &                   Scene;
            ImporterSettings            Settings;

            string                      NamePrefix;             // added to loaded asset resource names (can be used to create hierarchy - "importfilename\"
            vaMatrix4x4                 BaseTransform;          // for conversion between coord systems, etc

            ImporterContext( vaRenderDevice & device, vaAssetPack & assetPack, vaScene & scene, ImporterSettings & settings, const vaMatrix4x4 & baseTransform = vaMatrix4x4::Identity ) : Device( device ), AssetPack( assetPack ), Scene( scene ), Settings( settings ), BaseTransform( baseTransform ), NamePrefix("")
            {
            }
        };

        struct UIContext
        {
            wstring             ImportingPopupSelectedFile;
            vaVector3           ImportingPopupBaseTranslation;
            vaVector3           ImportingPopupBaseScaling;
            bool                ImportingPopupRotateX90         = true;            // a.k.a. Z is up
            bool                ImportingPopupRotateX180        = false;           // a.k.a. Z+ is up when loading from Blender

            ImporterSettings    Settings;

            UIContext( )
            {
            }
        };

        struct LoadedContent
        {
            vector<shared_ptr<vaAsset>>                     LoadedAssets;
        };

        UIContext                                           m_uiContext;

        shared_ptr<vaAssetPack>                             m_tempAssetPack;
        shared_ptr<vaScene>                                 m_tempScene;

        vaRenderDevice &                                    m_device;

    public:
        vaAssetImporter( vaRenderDevice & device );
        virtual ~vaAssetImporter( );

    public:
        void                                                Clear( );

    protected:
        virtual string                                      UIPanelGetDisplayName( ) const override   { return "Asset Importer"; }
        virtual void                                        UIPanelDraw( ) override;

    public:
        static bool                                         LoadFileContents( const wstring & path, ImporterContext & parameters, LoadedContent * outContent = nullptr );

    };


}