///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019, Intel Corporation
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

#include "vaApplicationWin.h"

#include "Scene/vaCameraControllers.h"
#include "Scene/vaScene.h"

#include "Rendering/vaRenderingIncludes.h"

#include "Core/vaUI.h"

#include "Rendering/vaRenderDevice.h"
#include "Rendering/Effects/vaASSAOLite.h"
#include "Rendering/Misc/vaZoomTool.h"
#include "Rendering/Misc/vaImageCompareTool.h"
#include "Rendering/Misc/vaTextureReductionTestTool.h"

#include "CMAA2/vaCMAA2.h"

#include "SMAA/vaSMAAWrapper.h"
#include "FXAA/vaFXAAWrapper.h"

namespace VertexAsylum
{
    class AutoBenchTool;

    struct CMAA2SampleConstructorParams : public vaRenderingModuleParams
    {
        vaApplicationBase &     Application;
        explicit CMAA2SampleConstructorParams( vaRenderDevice & device, vaApplicationBase & application ) : vaRenderingModuleParams(device), Application(application) { }
    };

    class CMAA2Sample : public vaRenderingModule, public std::enable_shared_from_this<CMAA2Sample>, public vaUIPanel
    {
    public:

    public:

        enum class SceneSelectionType : int32
        {
            LumberyardBistro,
            StaticImage,
            MinecraftLostEmpire,
            
            MaxValue
        };

#define MSAA_16x_SUPPORTED 0        // not testing caps for support so let's skip it for now

        enum class AAType : int32
        {
            None,
            FXAA,
            CMAA2,
            SMAA,
            MSAA2x,
            MSAA2xPlusCMAA2,
            SMAA_S2x,
            MSAA4x,
            MSAA4xPlusCMAA2,
            MSAA8x,
            MSAA8xPlusCMAA2,
#if MSAA_16x_SUPPORTED
            MSAA16x,
#endif

            SuperSampleReference,   // also used as the max value for automatic comparisons & benchmarking - options below will be ignored by the autobench

//            ExperimentalSlot1,      // at the moment tonemap+CMAA2
//            ExperimentalSlot2,

            MaxValue
        };

        struct CMAA2SampleSettings
        {
            SceneSelectionType                      SceneChoice;
            bool                                    ShowWireframe;
            float                                   CameraYFov;
            AAType                                  CurrentAAOption;
            int                                     CurrentStaticImageChoice;
            int                                     MSAADebugSampleIndex;               // -1 no debug, 0..sample count - show just selected sample
            bool                                    ZPrePass;

            CMAA2SampleSettings( )
            {
                SceneChoice                     = SceneSelectionType::LumberyardBistro;
                ShowWireframe                   = false;
                CameraYFov                      = 55.0f / 180.0f * VA_PIf;
                CurrentAAOption                 = AAType::None;
                CurrentStaticImageChoice    = 1;
                //MSAAOption                      = 0;
                MSAADebugSampleIndex            = -1;
                ZPrePass                        = true;
            }

            void Serialize( vaXMLSerializer & serializer )
            {
                serializer.Serialize( "SceneChoice"                 , (int&)SceneChoice            );
                serializer.Serialize( "ShowWireframe"               , ShowWireframe                );
                serializer.Serialize( "CameraYFov"                  , CameraYFov                   );
                serializer.Serialize( "CurrentAAOption"             , (int&)CurrentAAOption        );
                serializer.Serialize( "CurrentStaticImageChoice", CurrentStaticImageChoice );
                //serializer.SerializeValue( "MSAAOption"                  , MSAAOption                   );
                serializer.Serialize( "MSAADebugSampleIndex"        , MSAADebugSampleIndex         );
                serializer.Serialize( "ZPrePass"                    , ZPrePass                     );

                // this here is just to remind you to update serialization when changing the struct
                size_t dbgSizeOfThis = sizeof(*this); dbgSizeOfThis;
                assert( dbgSizeOfThis == 28 );
            }
        };

    protected:
        shared_ptr<vaCameraBase>                m_camera;
        shared_ptr<vaCameraControllerFreeFlight>
                                                m_cameraFreeFlightController;
        shared_ptr<vaCameraControllerFlythrough>
                                                m_flythroughCameraController;
        bool                                    m_flythroughPlay = false;

        vaApplicationBase &                     m_application;

        shared_ptr<vaSkybox>                    m_skybox;

        vaGBuffer::BufferFormats                m_GBufferFormats;
        shared_ptr<vaGBuffer>                   m_GBuffer;

        shared_ptr<vaGBuffer>                   m_SSGBuffer;
        shared_ptr<vaTexture>                   m_SSScratchColor;
        int                                     m_SSBuffersLastUsed                     = 0;        // delete SS buffers when not in use for more than couple of frames
#if 1   // 2x2-res version with 3x3 jitter and 8xMSAA
        const int                               m_SSMSAASampleCount                     = 8;
        const int                               m_SSResScale                            = 2;        // draw at ResScale times higher resolution (only 1 and 2 and 4 supported due to filtering support)
        int                                     m_SSGridRes                             = 3;        // multi-tap using GridRes x GridRes samples for each pixel
        float                                   m_SSMIPBias                             = 0.95f;    // make SS sample textures from a bit lower MIPs to avoid significantly over-sharpening textures vs non-SS (for textures that have high res mip levels or at distance)
        float                                   m_SSSharpen                             = 0.12f;    // used to make texture-only view (for ex. looking at the painting) closer to non-SS (as SS adds a dose of blur due to tex sampling especially when no higher-res mip available for textures, which is the case here in most cases)
        float                                   m_SSDDXDDYBias                          = 0.20f;    // SS messes up with pixel size which messes up with specular as it is based on ddx/ddy so compensate a bit here (0.20 gave closest specular by PSNR diff from no-AA)
#else   // 4x4-res version with no jitter and 8xMSAA - sharper but the high res + MSAA totally kills perf. on my GPU making it unusable for testing :)
        const int                               m_SSMSAASampleCount                     = 8;
        const int                               m_SSResScale                            = 4;        // draw at ResScale times higher resolution (only 1 and 2 and 4 supported due to filtering support - only 4 has proper sharpening)
        int                                     m_SSGridRes                             = 1;        // multi-tap using GridRes x GridRes samples for each pixel (adds blur to textures)
        float                                   m_SSMIPBias                             = 1.85f;    // make SS sample textures from a bit lower MIPs to avoid significantly over-sharpening textures vs non-SS (for textures that have high res mip levels or at distance)
        float                                   m_SSSharpen                             = 0.06f;    // used to make texture-only view (for ex. looking at the painting) closer to non-SS (as SS adds a dose of blur due to tex sampling especially when no higher-res mip available for textures, which is the case here in most cases)
        float                                   m_SSDDXDDYBias                          = 0.22f;    // SS messes up with pixel size which messes up with specular as it is based on ddx/ddy so compensate a bit here (0.22 gave closest specular by PSNR diff from no-AA)
#endif
        shared_ptr<vaTexture>                   m_SSAccumulationColor;

        shared_ptr<vaTexture>                   m_MSTonemappedColor;
        shared_ptr<vaTexture>                   m_MSTonemappedColorComplexityMask;
        int                                     m_MSTonemappedColorLastUsed  = 0;
        shared_ptr<vaTexture>                   m_exportedLuma;

        shared_ptr<vaTexture>                   m_scratchPostProcessColor;      // in some variations a postprocess step needs to make a copy during processing (SMAA for ex.) - in that case, this will contain the final output
        shared_ptr<vaTexture>                   m_radianceTempTexture;          // for debugging visualization of individual MSAA samples

        shared_ptr<vaLighting>                  m_lighting;
        shared_ptr<vaPostProcess>               m_postProcess;
        shared_ptr<vaPostProcessTonemap>        m_postProcessTonemap;

        wstring                                 m_screenshotFolder;
        string                                  m_loadedScreenshotFullPath;
        shared_ptr<vaTexture>                   m_loadedStaticImage;

        shared_ptr<vaScene>                     m_scenes[ (int32)SceneSelectionType::MaxValue ];

        shared_ptr<vaScene>                     m_currentScene;
        vaRenderSelection                       m_currentSceneMainRenderSelection;
        vaDrawResultFlags                       m_currentDrawResults = vaDrawResultFlags::None;

        shared_ptr<vaShadowmap>                 m_queuedShadowmap;
        vaRenderSelection                       m_queuedShadowmapRenderSelection;

        shared_ptr<vaCMAA2>                     m_CMAA2;
        shared_ptr<vaSMAAWrapper>               m_SMAA;
        shared_ptr<vaFXAAWrapper>               m_FXAA;

        shared_ptr<vaZoomTool>                  m_zoomTool;
        shared_ptr<vaImageCompareTool>          m_imageCompareTool;

        float                                   m_lastDeltaTime;

        vector<string>                          m_staticImageList;
        vector<string>                          m_staticImageFullPaths;

        vaVector3                               m_mouseCursor3DWorldPosition;

        shared_ptr<vaASSAOLite>                 m_SSAOLiteEffect;

        shared_ptr<AutoBenchTool>               m_autoBench;
        int                                     m_autoBenchPerfRunCount         = 4;

        bool                                    m_requireDeterminism            = false;

        //bool                                    m_debugTexturingDisabled        = false;

    protected:
        CMAA2SampleSettings                     m_settings;

    protected:
        CMAA2Sample( const vaRenderingModuleParams & params );

    public:
        virtual ~CMAA2Sample( );

        const vaApplicationBase &               GetApplication( ) const             { return m_application; }

    public:
        shared_ptr<vaCameraBase> &              Camera( )                           { return m_camera; }
        CMAA2SampleSettings &                   Settings( )                         { return m_settings; }
        shared_ptr<vaPostProcessTonemap>  &     PostProcessTonemap( )               { return m_postProcessTonemap; }

        void                                    SetRequireDeterminism( bool enable ){ m_requireDeterminism = enable; }

        const char *                            GetAAName( AAType aaType );
        int                                     GetSSResScale( ) const              { return m_SSResScale; }
        int                                     GetSSGridRes( ) const               { return m_SSGridRes; }
        int                                     GetSSMSAASampleCount( ) const       { return m_SSMSAASampleCount; }

        const shared_ptr<vaCameraControllerFlythrough> & 
                                                GetFlythroughCameraController()     { return m_flythroughCameraController; }
        bool                                    GetFlythroughCameraEnabled() const  { return m_flythroughPlay; }
        void                                    SetFlythroughCameraEnabled( bool enabled ) { m_flythroughPlay = enabled; }

    public:
        // events/callbacks:
        void                                    OnBeforeStopped( );
        void                                    OnTick( float deltaTime );
        void                                    OnSerializeSettings( vaXMLSerializer & serializer );

    protected:
        vaDrawResultFlags                       RenderTick( );
        void                                    LoadAssetsAndScenes( );
        int                                     GetMSAACountForAAType( CMAA2Sample::AAType aaType );


        bool                                    LoadCamera( int index = -1 );
        void                                    SaveCamera( int index = -1 );

        vaDrawResultFlags                       DrawScene( vaCameraBase & camera, vaGBuffer & gbufferOutput, const shared_ptr<vaTexture> & colorScratch, bool & colorScratchContainsFinal, const vaViewport & mainViewport, float globalMIPOffset, const vaVector2 & globalPixelScale = vaVector2( 1.0, 1.0 ), bool skipTonemapTick = false );

        virtual void                            UIPanelDraw( ) override;

    private:
        //void                                    RandomizeCurrentPoissonDisk( int count = SSAO_MAX_SAMPLES );
    };

    class AutoBenchTool;
    class AutoBenchToolWorkItem
    {
    protected:
        CMAA2Sample &                           m_parent;
        bool                                    m_isDone                        = false;

        AutoBenchToolWorkItem( CMAA2Sample & parent ) : m_parent( parent )      { }
        virtual ~AutoBenchToolWorkItem()        { }

    public:
        virtual void                            Tick( AutoBenchTool & abTool, float deltaTime )         = 0;
        virtual void                            OnRender( AutoBenchTool & abTool )                      = 0;
        virtual void                            OnRenderComparePoint( AutoBenchTool & , vaImageCompareTool & , vaRenderDeviceContext & , const shared_ptr<vaTexture> & , shared_ptr<vaPostProcess> &  ) { }
        virtual bool                            IsDone( AutoBenchTool & abTool ) const                  = 0;
        virtual float                           GetProgress( ) const                                    = 0;
    };

    class AutoBenchTool
    {
        CMAA2Sample &                           m_parent;
        vector<shared_ptr<AutoBenchToolWorkItem>>
                                                m_tasks;
        shared_ptr<AutoBenchToolWorkItem>       m_currentTask;

        vaPostProcessTonemap::TMSettings        m_backupTonemapSettings;
        vaCameraBase                            m_backupCamera;
        CMAA2Sample::CMAA2SampleSettings        m_backupSettings;

        float                                   m_backupFlythroughCameraTime;
        float                                   m_backupFlythroughCameraSpeed;
        bool                                    m_backupFlythroughCameraEnabled;

        wstring                                 m_reportDir;
        wstring                                 m_reportName;
        vector<vector<string>>                  m_reportCSV;
        string                                  m_reportTXT;

    public:
        AutoBenchTool( CMAA2Sample & parent ) : m_parent( parent ), m_backupCamera( false ) { }
        ~AutoBenchTool( )                       { assert( !IsActive() ); }

        void                                    AddTask( shared_ptr<AutoBenchToolWorkItem> task )   { m_tasks.insert( m_tasks.begin(), task ); }
        void                                    Tick( float deltaTime );
        void                                    OnRender( );
        void                                    OnRenderComparePoint( vaImageCompareTool & imageCompareTool, vaRenderDeviceContext & renderContext, const shared_ptr<vaTexture> & colorInOut, shared_ptr<vaPostProcess> & postProcess );
        bool                                    IsActive( ) const                                   { return m_currentTask != nullptr || m_tasks.size() > 0; }
        float                                   GetProgress( ) const                                { if( IsActive() && m_currentTask != nullptr ) return m_currentTask->GetProgress(); else return 0.5f; }
        int                                     GetQueuedTaskCount( ) const                         { return (int)m_tasks.size(); }

        void                                    ReportStart( );
        void                                    ReportAddRowValues( const vector<string> & row )    { m_reportCSV.push_back(row); FlushRowValues(); }
        void                                    ReportAddText( const string & text )                { m_reportTXT += text; }
        wstring                                 ReportGetDir( )                                     { return m_reportDir; }
        void                                    ReportFinish( );
    
    private:
        void                                    FlushRowValues( );
    };


}