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
#include "Core/vaSingleton.h"

#include "Rendering/vaRenderingIncludes.h"

#include "Rendering/Effects/vaPostProcessTonemap.h"

#include "vaImageCompareTool.h"

#include "Core/System/vaMemoryStream.h"


namespace VertexAsylum
{
    struct vaAssetTexture;

    class vaTextureReductionTestTool : public vaSingletonBase<vaTextureReductionTestTool>
    {
    public:
        typedef pair< shared_ptr<vaTexture>, string > TestItemType;

    protected:
        vector<TestItemType>                m_textures;
        vector<shared_ptr<vaAssetTexture>>  m_textureAssets;
        vector<int>                         m_texturesMaxFoundReduction;
        vector<int>                         m_texturesSorted;

        // vector<vector<float>>               m_resultsPSNR;
        // vector<vector<float>>               m_resultsMSR;
        // vector<float>                       m_avgPSNR;
        // vector<float>                       m_avgMSR;

        bool                                m_runningTests                  = false;
        int                                 m_currentTexture                = -1;
        int                                 m_currentCamera                 = -1;
        int                                 m_currentSearchReductionCount   = -1;
        bool                                m_restoreTonemappingAfterTests  = false;

        bool                                m_enabled                       = true;
        bool                                m_popupJustOpened               = true;

        shared_ptr<vaTexture>               m_referenceTexture;

        shared_ptr<vaTexture>               m_currentlyOverriddenTexture;
        shared_ptr<vaTexture>               m_currentOverrideView;

        static const int                    c_cameraSlotCount               = 20;
        shared_ptr<vaMemoryStream>          m_cameraSlots[c_cameraSlotCount];
        shared_ptr<vaMemoryStream>          m_userCameraBackup;
        float                               m_targetPSNRThreshold           = 72.0f;
        int                                 m_maxLevelsToDrop               = 5;
        int                                 m_cameraSlotSelectedIndex       = -1;

        int                                 m_downscaleTextureButtonClicks  = 2;

        vaPostProcessTonemap::TMSettings
                                            m_userTonemapSettings;

        bool                                m_overrideAll                   = false;

        static bool                         s_supportedByApp;

    public:
        // this could be made to work without asset links - except then no automatic reduction could be done, just the report
        vaTextureReductionTestTool( const vector<TestItemType> & textures, vector<shared_ptr<vaAssetTexture>> textureAssets );
        ~vaTextureReductionTestTool( );

    public:
        // if had control of camera, revert to before captured
        void                        ResetCamera( const shared_ptr<vaCameraBase> & camera );

        void                        TickCPU( const shared_ptr<vaCameraBase> & camera, const shared_ptr<vaPostProcessTonemap> & tonemapper );
        void                        TickGPU( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & colorBuffer, shared_ptr<vaPostProcess> & postProcess );

        bool                        IsEnabled( ) const              { return m_enabled; }

        // ideally you want to automatically stop any scene movement during this
        bool                        IsRunningTests( ) const         { return m_runningTests; }

        void                        DrawUI( vaRenderDevice & device, const shared_ptr<vaCameraBase> & camera );

        void                        OverrideAllWithCurrentStates( );

        void                        DownscaleAll( vaRenderDeviceContext & apiContext );

    private:
        void                        SaveAsReference( vaRenderDeviceContext & apiContext, const shared_ptr<vaTexture> & colorBuffer );

        void                        ResetData( );
        void                        ResetTextureOverrides( );
        //void                        ComputeAveragesAndSort( );

    public:
        static void                 SetSupportedByApp( )            { s_supportedByApp = true; }
        static bool                 GetSupportedByApp( )            { return s_supportedByApp; }
    };

}
