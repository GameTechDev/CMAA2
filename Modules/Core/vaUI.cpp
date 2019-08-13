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


#include "Core/vaUI.h"

#include "Core/vaInput.h"

#include "Rendering/vaShader.h"

#include "IntegratedExternals/vaImguiIntegration.h"


namespace VertexAsylum
{
    // TODO: make this public and embeddable? no need for now (or ever?)
    class vaUIPropertiesPanel : vaUIPanel
    {
    public:
        vaUIPropertiesPanel( const string & name, int sortOrder ) : vaUIPanel( name, sortOrder, false, vaUIPanel::DockLocation::DockedRightBottom ) { }

        virtual void                        UIPanelDraw( ) override;
    };

    class vaUIFamilyPanel : public vaUIPanel
    {
        vector<vaUIPanel *>                 m_currentList;
        string                              m_currentlySelectedName;

    public:
        vaUIFamilyPanel( const string & name, int sortOrder, vaUIPanel::DockLocation initialDock ) : vaUIPanel( name, sortOrder, true, initialDock ) { assert( UIPanelGetName() == name ); }  // can't have multiple classes with the same name?

        void                                Clear( )                    { m_currentList.clear(); };
        int                                 GetCount( ) const           { return (int)m_currentList.size(); }
        void                                Add( vaUIPanel * panel )    { m_currentList.push_back( panel ); }
        const vector<vaUIPanel *> &         GetList( ) const            { return m_currentList; }
        virtual void                        UIPanelDraw( ) override;

        void                                SortAndUpdateVisibility( )  
        { 
            std::sort( m_currentList.begin(), m_currentList.end(), 
                [](const vaUIPanel * a, const vaUIPanel * b) -> bool { if( a->UIPanelGetSortOrder() == b->UIPanelGetSortOrder() ) return a->UIPanelGetName() < b->UIPanelGetName(); return a->UIPanelGetSortOrder() < b->UIPanelGetSortOrder(); }); 
            
            bool visible = false;
            for( auto panel : m_currentList ) if( panel->UIPanelIsVisible() ) visible = true;
            UIPanelSetVisible( visible );
        }
    };
}

using namespace VertexAsylum;

int vaUIPropertiesItem::s_lastID = 0;

vaUIPropertiesItem::vaUIPropertiesItem( ) : m_uniqueID( vaStringTools::Format( "vaUIPropertiesItem_%d", ++s_lastID ) )
{
}

vaUIPropertiesItem::vaUIPropertiesItem( const vaUIPropertiesItem & item ) : m_uniqueID( vaStringTools::Format( "vaUIPropertiesItem_%d", ++s_lastID ) )
{
    item; // nothing to copy
}

string vaUIPanel::FindUniqueName( const string & name )
{
    string uniqueName = name;
    int counter = 0;
    while( vaUIManager::GetInstance().m_panels.find( uniqueName ) != vaUIManager::GetInstance().m_panels.end() )
    {
        counter++;
        uniqueName = vaStringTools::Format( "%s (%d)", name.c_str(), counter );
    }
    return uniqueName;
}

vaUIPanel::vaUIPanel( const string & name, int sortOrder, bool initialVisible, DockLocation initialDock, const string & familyName, const vaVector2 & initialSize ) : m_name( FindUniqueName( name ) ), m_sortOrder( sortOrder ), m_initialDock( initialDock ), m_visible( initialVisible ), m_familyName( familyName ), m_initialSize( initialSize )
{ 
    assert( vaThreading::IsMainThread() );

    assert( vaUIManager::GetInstancePtr() != nullptr );
    if( vaUIManager::GetInstancePtr( ) != nullptr )
    {
        assert( vaUIManager::GetInstance().m_panels.find( m_name ) == vaUIManager::GetInstance().m_panels.end() );
        vaUIManager::GetInstance().m_panels.insert( std::make_pair( m_name, this) );

        // ImGui doesn't remember visibility (by design?) so we have to save/load it ourselves
        UIPanelSetVisible( vaUIManager::GetInstance().FindInitialVisibility( m_name, initialVisible ) );
    }
}  

vaUIPanel::~vaUIPanel( )
{ 
    assert( vaThreading::IsMainThread() );
    assert( vaUIManager::GetInstancePtr() != nullptr );
    if( vaUIManager::GetInstancePtr( ) != nullptr )
    {
        auto it = vaUIManager::GetInstance().m_panels.find( m_name );
        if( it != vaUIManager::GetInstance().m_panels.end() )
            vaUIManager::GetInstance().m_panels.erase( it );
        else
            { assert( false ); }
    }
}


void vaUIPanel::UIPanelDrawCollapsable( bool showFrame, bool defaultOpen, bool indent )
{
    showFrame; defaultOpen; indent;
#ifdef VA_IMGUI_INTEGRATION_ENABLED

    // needed so that any controls in obj.IHO_Draw() are unique
    ImGui::PushID( ("collapsable_"+m_name).c_str( ) );

    ImGuiTreeNodeFlags headerFlags = 0;
    headerFlags |= (showFrame)?(ImGuiTreeNodeFlags_Framed):(0);
    headerFlags |= (defaultOpen)?(ImGuiTreeNodeFlags_DefaultOpen):(0);

    if( ImGui::CollapsingHeader( UIPanelGetDisplayName().c_str( ), headerFlags ) )
    {
        if( indent )
            ImGui::Indent();
        
        UIPanelDraw( );
        
        if( indent )
            ImGui::Unindent();
    }

    ImGui::PopID();

#endif
}

void vaUIPropertiesItem::UIItemDrawCollapsable( bool showFrame, bool defaultOpen, bool indent )
{
    showFrame; defaultOpen; indent;
#ifdef VA_IMGUI_INTEGRATION_ENABLED

    // needed so that any controls in obj.IHO_Draw() are unique
    ImGui::PushID( ("collapsable_"+m_uniqueID).c_str( ) );

    ImGuiTreeNodeFlags headerFlags = 0;
    headerFlags |= (showFrame)?(ImGuiTreeNodeFlags_Framed):(0);
    headerFlags |= (defaultOpen)?(ImGuiTreeNodeFlags_DefaultOpen):(0);

    if( ImGui::CollapsingHeader( UIPropertiesItemGetDisplayName().c_str( ), headerFlags ) )
    {
        if( indent )
            ImGui::Indent();
        
        UIPropertiesItemDraw( );
        
        if( indent )
            ImGui::Unindent();
    }

    ImGui::PopID();

#endif
}

void vaUIPropertiesItem::DrawList( const char * stringID, vaUIPropertiesItem ** objList, const int objListCount, int & currentItem, float width, float listHeight, float selectedElementHeight )
{
    stringID; objList; objListCount; currentItem; width; listHeight; selectedElementHeight;
#ifdef VA_IMGUI_INTEGRATION_ENABLED

    ImGui::PushID( stringID );

    if( ImGui::BeginChild( "PropList", ImVec2( 0.0f, listHeight ), true ) )
    {
        ImGuiTreeNodeFlags defaultFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

        int objectClickedIndex = -1;
        for( int i = 0; i < objListCount; i++ )
        {
            ImGuiTreeNodeFlags nodeFlags = defaultFlags | ((i == currentItem)?(ImGuiTreeNodeFlags_Selected):(0)) | ImGuiTreeNodeFlags_Leaf;
            bool nodeOpen = ImGui::TreeNodeEx( objList[i]->UIPropertiesItemGetUniqueID().c_str(), nodeFlags, objList[i]->UIPropertiesItemGetDisplayName().c_str() );
                
            if( ImGui::IsItemClicked() )
                objectClickedIndex = i;
 
            if( nodeOpen )
                ImGui::TreePop();
        }

        if( objectClickedIndex != -1 )
        {
            if( currentItem == objectClickedIndex )
                currentItem = -1;   // if already selected, de-select
            else
                currentItem = objectClickedIndex;
        }
    }
    ImGui::EndChild();

    if( currentItem < 0 || currentItem >= objListCount )
        selectedElementHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().WindowPadding.y;

    if( ImGui::BeginChild( "PropFrame", ImVec2( width, selectedElementHeight ), true ) )
    {
        if( currentItem >= 0 && currentItem < (int)objListCount )
        {
            vaUIPropertiesItem & obj = *objList[currentItem];
            ImGui::PushID( obj.UIPropertiesItemGetUniqueID().c_str( ) );
            obj.UIPropertiesItemDraw();
            ImGui::PopID();
        }
        else
        {
            ImGui::TextColored( ImVec4( 0.5f, 0.5f, 0.5f, 1.0f ), "Select an item to display properties" );
        }
    }
    ImGui::EndChild();
    ImGui::PopID( );

#endif // #ifdef VA_IMGUI_INTEGRATION_ENABLED
}

void vaUIPropertiesPanel::UIPanelDraw( ) 
{ 
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGui::Button( "Some button!" );
#endif
}

void vaUIFamilyPanel::UIPanelDraw( )
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGui::PushID( UIPanelGetName().c_str() );

#if 0
    vector<string> list;
    int index = -1;
    for( int i = 0; i < (int)m_currentList.size(); i++ )
    {
        list.push_back( m_currentList[i]->UIPanelGetDisplayName() );
        if( m_currentlySelectedName == m_currentList[i]->UIPanelGetName() )
            index = i;
    }
    //ImGui::Text( "Asset packs in memory:" );
    ImGui::PushItemWidth(-1);
    ImGuiEx_ListBox( "list", index, list );
    ImGui::PopItemWidth();
    if( index >= 0 && index < m_currentList.size() )
    {
        m_currentlySelectedName = m_currentList[index]->UIPanelGetName();
        m_currentList[index]->UIPanelDraw();
    }
#else

    ImGuiTabBarFlags tabBarFlags = 0;
    tabBarFlags |= ImGuiTabBarFlags_Reorderable;
    //tabBarFlags |= ImGuiTabBarFlags_AutoSelectNewTabs;
    tabBarFlags |= ImGuiTabBarFlags_NoCloseWithMiddleMouseButton;
    tabBarFlags |= ImGuiTabBarFlags_FittingPolicyScroll;    // ImGuiTabBarFlags_FittingPolicyResizeDown

    if (ImGui::BeginTabBar("MyTabBar", tabBarFlags))
    {
        for( int i = 0; i < (int)m_currentList.size(); i++ )
        {
            auto panel = m_currentList[i];
            bool isVisible = panel->UIPanelIsVisible();
            if( isVisible )
            {
                int tabItemFlags = 0;
                if( (m_currentlySelectedName == "" && i == 0) || panel->UIPanelGetFocusNextFrame() )
                {
                    tabItemFlags = ImGuiTabItemFlags_SetSelected;
                    panel->UIPanelSetFocusNextFrame( false );
                }
                string windowName = panel->UIPanelGetDisplayName( )+"###"+panel->UIPanelGetName();

                //bool isDocked = false; isDocked;
                //ImGuiWindow * imWin = ImGui::FindWindowByName( windowName.c_str() );
                //if( imWin != nullptr )
                //    isDocked = imWin->DockIsActive;
                bool isDocked = true;

                if( ImGui::BeginTabItem( windowName.c_str( ), (panel->UIPanelIsListed() && !isDocked)?(&isVisible):(nullptr), tabItemFlags ) ) //ImGuiWindowFlags_NoBackground ) )
                {
                    panel->UIPanelDraw();
                    ImGui::EndTabItem();
                    m_currentlySelectedName = panel->UIPanelGetName();
                }
                panel->UIPanelSetVisible( isVisible );
            }
        }
        ImGui::EndTabBar();
    }

#endif

    ImGui::PopID();
#endif
}

vaUIManager::vaUIManager( )
{ 
    m_propPanels.push_back( std::make_shared<vaUIPropertiesPanel>("Properties 1", 10) );
    m_propPanels.push_back( std::make_shared<vaUIPropertiesPanel>("Properties 2", 11) );
    m_propPanels.push_back( std::make_shared<vaUIPropertiesPanel>("Properties 3", 12) );
}
vaUIManager::~vaUIManager( )
{ 
}

void vaUIManager::SerializeSettings( vaXMLSerializer & serializer )
{
    serializer.Serialize<bool>( "Visible", m_visible );
    serializer.Serialize<bool>( "MenuVisible", m_menuVisible );
    serializer.Serialize<bool>( "ConsoleVisible", m_consoleVisible );

    // ImGui doesn't remember visibility (by design?) so we have to save/load it ourselves
    if( serializer.IsWriting() )
    {
        vector<pair<string, bool>>  panelVisibility;
        for( auto it : m_panels )
        {
            assert( it.first == it.second->UIPanelGetName() );
            panelVisibility.push_back( make_pair( it.first, it.second->UIPanelIsVisible() ) );
        }
        serializer.SerializeArray<pair<string, bool>>( "PanelVisibility", "Info", panelVisibility );
    }
    else
    {
        m_initiallyPanelVisibility.clear();
        serializer.SerializeArray<pair<string, bool>>( "PanelVisibility", "Info", m_initiallyPanelVisibility );
        for( auto it : m_panels )
            it.second->UIPanelSetVisible( FindInitialVisibility( it.first, it.second->UIPanelIsVisible() ) );
    }
}

bool vaUIManager::FindInitialVisibility( const string & panelName, bool defVal )
{
    for( auto & it : m_initiallyPanelVisibility )
        if( it.first == panelName )
            return it.second;
    return defVal;
}

void vaUIManager::UpdateUI( bool appHasFocus )
{
    if( appHasFocus )
    {
        // recompile shaders if needed - not sure if this should be here...
        if( ( vaInputKeyboardBase::GetCurrent( ) != NULL ) && vaInputKeyboardBase::GetCurrent( )->IsKeyDown( KK_CONTROL ) )
        {
            if( vaInputKeyboardBase::GetCurrent( )->IsKeyClicked( ( vaKeyboardKeys )'R' ) )
                vaShader::ReloadAll( );
        }

        if( !vaInputMouseBase::GetCurrent( )->IsCaptured( ) )
        {
            // I guess this is good place as any... it must not happen between ImGuiNewFrame and Draw though!
            if( vaInputKeyboardBase::GetCurrent( )->IsKeyClicked( KK_F1 ) && !vaInputKeyboardBase::GetCurrent( )->IsKeyDownOrClicked( KK_CONTROL ) )
                m_visible = !m_visible;

            if( vaInputKeyboardBase::GetCurrent( )->IsKeyClicked( KK_F1 ) && vaInputKeyboardBase::GetCurrent( )->IsKeyDownOrClicked( KK_CONTROL ) )
                m_menuVisible = !m_menuVisible;

            if( vaInputKeyboardBase::GetCurrent( )->IsKeyClicked( KK_OEM_3 ) )
                // m_consoleVisible = !m_consoleVisible;
                vaUIConsole::GetInstance().SetOpen( !vaUIConsole::GetInstance().IsOpen() );
        }
    }
}

void vaUIManager::DrawUI( )
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    bool visible = m_visible;
    bool menuVisible = m_menuVisible;
    bool consoleVisible = m_consoleVisible;

    if( m_visibilityOverrideCallback )
        m_visibilityOverrideCallback( visible, menuVisible, consoleVisible );

    if( !visible )
        return;

    // see ImGui::ShowDemoWindow(); / ShowExampleAppDockSpace
    if( m_showImGuiDemo )
        ImGui::ShowDemoWindow();

    if( m_panels.size() > 0 )
    {
        // // erase dead simple panels
        // {
        //     for( int i = ((int)m_simplePanels.size())-1; i>= 0; i-- )
        //         if( !m_simplePanels[i]->IsAlive() )
        //             m_simplePanels.erase( m_simplePanels.begin()+i );
        // }

        // collect panels
        vector<vaUIPanel *> panels;
        for( auto it : m_panels )
	        panels.push_back( it.second );

        // collect family panels
        {
            // reset subpanels collected by family panels
            for( int i = 0; i < m_familyPanels.size(); i++ ) 
                m_familyPanels[i]->Clear();

            // collect subpanels into corresponding family panels (and create new family panels if none exists)
            for( int i = 0; i < (int)panels.size(); i++ )
            {
                vaUIPanel * panel = panels[i];
                if( panel->UIPanelGetFamily() == "" || !panel->UIPanelIsListed() )
                    continue;

                bool found = false;

                for( auto familyPanel : m_familyPanels ) 
                    if( panel->UIPanelGetFamily() == familyPanel->UIPanelGetName() )
                    {
                        familyPanel->Add( panel );
                        found = true;
                        break;
                    }
                if( !found )
                {
                    m_familyPanels.push_back( std::make_shared<vaUIFamilyPanel>(panel->UIPanelGetFamily(), panel->UIPanelGetSortOrder(), panel->UIPanelGetInitialDock() ) );
                    m_familyPanels.back()->Add( panel );
                    panels.push_back( m_familyPanels.back().get() );    // have to update 'panels' too so we get picked up first frame
                }
            }

            // remove all panels from the main list that now belong to a family panel (since they're managed by them now, both for the menu and for the windows contents)
            for( int i = ((int)panels.size())-1; i>= 0; i-- )
                if( panels[i]->UIPanelGetFamily() != "" )
                    panels.erase( panels.begin()+i );

            // finally either remove empty family panels or let them sort their collected subpanels for later display (for correct menu and tab ordering)
            for( int i = ((int)m_familyPanels.size())-1; i>= 0; i-- )
            {
                if( m_familyPanels[i]->GetCount() == 0 )
                {
                    // first remove from the list of panels
                    for( int j = 0; j < panels.size(); j++ )
                        if( panels[j] == static_cast<vaUIPanel*>(m_familyPanels[i].get()) )
                        {
                            panels.erase( panels.begin()+j );
                            break;
                        }
                    // then remove from the list of family panels
                    m_familyPanels.erase( m_familyPanels.begin()+i );
                }
                else
                    m_familyPanels[i]->SortAndUpdateVisibility();
            }
        }

        // sort collected panels
        std::sort( panels.begin(), panels.end(), [](const vaUIPanel * a, const vaUIPanel * b) -> bool { if( a->UIPanelGetSortOrder() == b->UIPanelGetSortOrder() ) return a->UIPanelGetName() < b->UIPanelGetName(); return a->UIPanelGetSortOrder() < b->UIPanelGetSortOrder(); });

        // style stuff
        ImVec4 colorsDockspaceBg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg); //( 0.0f, 0.0f, 0.0f, 0.5f );

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        ImGuiDockNodeFlags dockNodeFlags = ImGuiDockNodeFlags_None;
        bool fullscreen = true;
    
        //dockNodeFlags |=  ImGuiDockNodeFlags_NoSplit;
        dockNodeFlags |=  ImGuiDockNodeFlags_NoDockingInCentralNode;
        //dockNodeFlags |=  ImGuiDockNodeFlags_NoResize;
        dockNodeFlags |=  ImGuiDockNodeFlags_PassthruDockspace;

        // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
        // because it would be confusing to have two docking targets within each others.
        ImGuiWindowFlags dockWindowFlags = ImGuiWindowFlags_NoDocking;
        if( menuVisible )
            dockWindowFlags |= ImGuiWindowFlags_MenuBar ;
        if( fullscreen )
        {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(viewport->Size);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            dockWindowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            dockWindowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }

        // When using ImGuiDockNodeFlags_PassthruDockspace, DockSpace() will render our background and handle the pass-thru hole, so we ask Begin() to not render a background.
        if (dockNodeFlags & ImGuiDockNodeFlags_PassthruDockspace)
            dockWindowFlags |= ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    
        ImGui::PushStyleColor(ImGuiCol_WindowBg, colorsDockspaceBg);
        ImVec4 menuCol = ImGui::GetStyleColorVec4( ImGuiCol_MenuBarBg );
        menuCol.w = 0.75f;
        ImGui::PushStyleColor( ImGuiCol_MenuBarBg, menuCol );

        ImGui::Begin("VAUIRootDockspaceWindow", nullptr, dockWindowFlags);
        ImGui::PopStyleVar();
        if( fullscreen )
            ImGui::PopStyleVar(2);

        // A programmatic initialization of docking windows when imgui.ini not available (app first run)
        // based on: https://github.com/ocornut/imgui/issues/2109#issuecomment-426204357
        {
            m_dockSpaceIDRoot = ImGui::GetID("VAUIRootDockspace");
            bool initFirstTime = false;
            if( ImGui::DockBuilderGetNode( m_dockSpaceIDRoot ) == nullptr ) // execute only first time
            {
                initFirstTime = true;

                ImGui::DockBuilderRemoveNode(m_dockSpaceIDRoot);
		        ImGui::DockBuilderAddNode(m_dockSpaceIDRoot, ImGui::GetMainViewport()->Size, dockNodeFlags);

                ImGuiID dock_main_id        = m_dockSpaceIDRoot;
                m_dockSpaceIDLeft           = ImGui::DockBuilderSplitNode(dock_main_id,         ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
                m_dockSpaceIDLeftBottom     = ImGui::DockBuilderSplitNode(m_dockSpaceIDLeft,    ImGuiDir_Down, 0.25f, nullptr, &m_dockSpaceIDLeft);
                m_dockSpaceIDRight          = ImGui::DockBuilderSplitNode(dock_main_id,         ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
                m_dockSpaceIDRightBottom    = ImGui::DockBuilderSplitNode(m_dockSpaceIDRight,   ImGuiDir_Down, 0.25f, nullptr, &m_dockSpaceIDRight);

                for( auto panel : panels )
                {
                    if( !panel->UIPanelIsListed() )
                        continue;
                    string windowName = (panel->UIPanelGetDisplayName( )+"###"+panel->UIPanelGetName());
                    switch( panel->UIPanelGetInitialDock( ) )
                    {
                    case vaUIPanel::DockLocation::NotDocked:            break;
                    case vaUIPanel::DockLocation::DockedLeft:           ImGui::DockBuilderDockWindow( windowName.c_str(), m_dockSpaceIDLeft );         break;
                    case vaUIPanel::DockLocation::DockedLeftBottom:     ImGui::DockBuilderDockWindow( windowName.c_str(), m_dockSpaceIDLeftBottom );   break;
                    case vaUIPanel::DockLocation::DockedRight:          ImGui::DockBuilderDockWindow( windowName.c_str(), m_dockSpaceIDRight );        break;
                    case vaUIPanel::DockLocation::DockedRightBottom:    ImGui::DockBuilderDockWindow( windowName.c_str(), m_dockSpaceIDRightBottom );  break;
                    default: assert( false ); break;
                    };
                }

                ImGui::DockBuilderFinish( m_dockSpaceIDRoot );
            }
        }

        ImGui::DockSpace( m_dockSpaceIDRoot, ImVec2(0.0f, 0.0f), dockNodeFlags );

        if( menuVisible )
        {
            if( ImGui::BeginMenuBar() )
            {
                if( ImGui::BeginMenu("File") )
                {
                    for( auto panel : panels )
                        panel->UIPanelFileMenuItems();

                    if( ImGui::MenuItem("Quit") )
                        vaCore::SetAppQuitFlag(true);
                    ImGui::EndMenu();
                }
                if( ImGui::BeginMenu("View") )
                {
                    for( auto panel : panels )
                    {
                        if( !panel->UIPanelIsListed() )
                            continue;

                        vaUIFamilyPanel * familyPanel = dynamic_cast<vaUIFamilyPanel *>(panel);

                        if( familyPanel == nullptr )
                        {
                            bool isVisible = panel->UIPanelIsVisible();
                            if( ImGui::MenuItem( (panel->UIPanelGetDisplayName( )+"###"+panel->UIPanelGetName()).c_str( ), "", &isVisible ) && isVisible )
                            {
                                panel->UIPanelSetFocusNextFrame( true );
                            }
                            panel->UIPanelSetVisible( isVisible );
                        }
                        else
                        {
                            if( ImGui::BeginMenu( (panel->UIPanelGetDisplayName( )+"###"+panel->UIPanelGetName()).c_str( ) ) )
                            {
                                for( auto subPanel : familyPanel->GetList() )
                                {
                                    bool isVisible = subPanel->UIPanelIsVisible();
                                    if( ImGui::MenuItem( (subPanel->UIPanelGetDisplayName( )+"###"+subPanel->UIPanelGetName()).c_str( ), "", &isVisible ) && isVisible )
                                    {
                                        subPanel->UIPanelSetFocusNextFrame(true);
                                        panel->UIPanelSetFocusNextFrame( true );
                                    }
                                    subPanel->UIPanelSetVisible( isVisible );
                                }
                                ImGui::EndMenu();
                            }
                        }
                    }

                    ImGui::EndMenu();
                }

                for( auto panel : panels )
                    panel->UIPanelStandaloneMenu();

                // if (ImGui::BeginMenu("Help"))
                // {
                //     ImGui::MenuItem("ImGui::ShowDemoWindow", "", &m_showImGuiDemo);
                //     ImGui::Separator();
                //     if( ImGui::MenuItem("Yikes!",                "" ) ) {  }
                //     ImGui::EndMenu();
                // }

                //////////////////////////////////////////////////////////////////////////
                // just example code - remove
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                    ImGui::TextUnformatted( "Use F1 to show/hide UI" );
                    ImGui::PopTextWrapPos();
                    ImGui::EndTooltip();
                }
                // just example code - remove
                //////////////////////////////////////////////////////////////////////////

                ImGui::EndMenuBar();
            }
        }
        ImGui::End();

        ImGui::PopStyleColor(); // ImGui::PushStyleColor(ImGuiCol_WindowBg, colorsDockspaceBg);

        // this is for panels when docked
        ImGui::PushStyleColor(ImGuiCol_ChildBg, (ImU32)0);
        // this is for panels when not docked
        //ImGui::PushStyleColor(ImGuiCol_WindowBg, (ImU32)0);

        for( auto it = panels.begin(); it < panels.end(); it++ )
        {
            auto panel = *it;
            bool isVisible = panel->UIPanelIsVisible();
            if( isVisible )
            {
                ImGui::SetNextWindowSize( ImFromVA( panel->UIPanelGetInitialSize() ), ImGuiCond_Once );

                string windowName = (panel->UIPanelGetDisplayName( )+"###"+panel->UIPanelGetName());

                bool isDocked = false; isDocked;
                ImGuiWindow * imWin = ImGui::FindWindowByName( windowName.c_str() );
                if( imWin != nullptr )
                    isDocked = imWin->DockIsActive;
                else
                    isDocked = panel->UIPanelGetInitialDock() != vaUIPanel::DockLocation::NotDocked;
                
                if( ImGui::Begin( windowName.c_str( ), (panel->UIPanelIsListed() && !isDocked)?(&isVisible):(nullptr), ImGuiWindowFlags_NoFocusOnAppearing ) ) //ImGuiWindowFlags_NoBackground ) )
                {
                    // panel->UIPanelSetDocked( ImGui::IsWindowDocked( ) );
                    panel->UIPanelDraw();
                }
                ImGui::End( );
                panel->UIPanelSetVisible( isVisible );
            }
        }
        // set focus to those who requested and/or those with lowest sort order during the first frame
        for( auto it = panels.rbegin(); it < panels.rend(); it++ )
        {
            auto panel = *it;
            bool isVisible = panel->UIPanelIsVisible();
            if( isVisible && (m_firstFrame || panel->UIPanelGetFocusNextFrame() ) )
            {
                ImGui::SetWindowFocus( (panel->UIPanelGetDisplayName( )+"###"+panel->UIPanelGetName()).c_str( ) );
                panel->UIPanelSetFocusNextFrame(false);
            }
        }
        m_firstFrame = false;

        ImGui::PopStyleColor();

        ImGui::PopStyleColor( ); // for ImGuiCol_MenuBarBg
    }

    // console & log at the bottom
    if( consoleVisible )
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        vaUIConsole::GetInstance().Draw( (int)viewport->Size.x, (int)viewport->Size.y );
    }

    // loading bar progress
    vaBackgroundTaskManager::GetInstance().InsertImGuiWindow();
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

vaUIConsole::vaUIConsole( )
{ 
    m_scrollToBottom = true;
    m_consoleOpen = false;
    m_consoleWasOpen = false;
    memset( m_textInputBuffer, 0, sizeof(m_textInputBuffer) );

    m_commands.push_back( CommandInfo( "HELP" ) );
    m_commands.push_back( CommandInfo( "HISTORY" ) );
    m_commands.push_back( CommandInfo( "CLEAR" ) );
    m_commands.push_back( CommandInfo( "QUIT" ) );
    //m_commands.push_back("CLASSIFY");  // "classify" is here to provide an example of "C"+[tab] completing to "CL" and displaying matches.
}

vaUIConsole::~vaUIConsole( )
{
}

void vaUIConsole::Draw( int windowWidth, int windowHeight )
{
    assert( vaUIManager::GetInstance().IsConsoleVisible() );

    windowHeight; windowWidth;
#ifdef VA_IMGUI_INTEGRATION_ENABLED

    // Don't allow log access to anyone else while this is drawing
    vaRecursiveMutexScopeLock lock( vaLog::GetInstance( ).Mutex() );

    const float timerSeparatorX = 80.0f;
    const int linesToShowMax = 15;

    const float secondsToShow = 8.0f;
    int showFrom = vaLog::GetInstance( ).FindNewest( secondsToShow );
    int showCount = vaMath::Min( linesToShowMax, (int)vaLog::GetInstance( ).Entries( ).size( ) - showFrom );
    if( m_consoleOpen )
    {
        showFrom = (int)vaLog::GetInstance( ).Entries( ).size( ) - linesToShowMax - 1; // vaMath::Max( 0, (int)vaLog::GetInstance( ).Entries( ).size( ) - linesToShowMax - 1 );
        showCount = linesToShowMax;
    }
    showFrom = (int)vaLog::GetInstance( ).Entries( ).size( ) - showCount;

    const float spaceToBorder = 2;
    float sizeX               = windowWidth - spaceToBorder*2;
    float sizeY               = (ImGui::GetTextLineHeightWithSpacing() * showCount + 10);
    bool opened = true;
    ImGuiWindowFlags windowFlags = 0;
    windowFlags |= ImGuiWindowFlags_NoTitleBar;
    windowFlags |= ImGuiWindowFlags_NoResize                ;
    windowFlags |= ImGuiWindowFlags_NoMove                  ;
    // windowFlags |= ImGuiWindowFlags_NoScrollbar             ;
    // windowFlags |= ImGuiWindowFlags_NoScrollWithMouse       ;
    windowFlags |= ImGuiWindowFlags_NoCollapse              ;
    windowFlags |= ImGuiWindowFlags_NoFocusOnAppearing      ;
    //windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus   ;
    windowFlags |= ImGuiWindowFlags_NoDocking      ;
    windowFlags |= ImGuiWindowFlags_NoSavedSettings      ;
    windowFlags |= ImGuiWindowFlags_NoScrollbar; //?
    windowFlags |= ImGuiWindowFlags_NoDecoration;

    const vector<vaLog::Entry> & logEntries = vaLog::GetInstance().Entries();

    float winAlpha = (m_consoleOpen)?(0.92f):(0.4f);
    if( showCount == 0 )
        winAlpha = 0.0f;
    ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( 0.0f, 0.0f, 0.0f, 1.0f ) );

    bool showConsoleWindow = (!m_consoleOpen && showCount > 0) || m_consoleOpen;

    if( !m_consoleOpen )
        windowFlags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav;

    if( showConsoleWindow )
    {
        ImGui::SetNextWindowPos( ImVec2( (float)( windowWidth/2 - sizeX/2), (float)( windowHeight - sizeY) - spaceToBorder ), ImGuiCond_Always );
        ImGui::SetNextWindowSize( ImVec2( (float)sizeX, (float)sizeY ), ImGuiCond_Always );
        ImGui::SetNextWindowCollapsed( false, ImGuiCond_Always );
        ImGui::SetNextWindowBgAlpha(winAlpha);
        
        if( !m_consoleWasOpen && m_consoleOpen )
            ImGui::SetNextWindowFocus( );

        if( ImGui::Begin( "Console", &opened, windowFlags ) )
        {
            // if console not open, show just the log
            if( !m_consoleOpen )
            {
                if( showCount > 0 )
                {
                    // for( int i = 0; i < linesToShowMax - showCount; i++ )
                    //     ImGui::Text( "" );

                    // ImGui::Columns( 2 );
                    // float scrollbarSize = 24.0f;
                    // ImGui::SetColumnOffset( 1, (float)sizeX - scrollbarSize );

                    for( int i = showFrom; i < (int)logEntries.size(); i++ )
                    {
                        if( i < 0  )
                        {
                            ImGui::Text("");
                            continue;
                        }
                        const vaLog::Entry & entry = logEntries[i];
                        float lineCursorPosY = ImGui::GetCursorPosY( );

                        char buff[64];
                        #pragma warning ( suppress: 4996 )
                        strftime( buff, sizeof(buff), "%H:%M:%S: ", localtime( &entry.LocalTime ) );
                        ImGui::TextColored( ImVec4( 0.3f, 0.3f, 0.2f, 1.0f ), buff );

                        ImGui::SetCursorPosX( timerSeparatorX );
                        ImGui::SetCursorPosY( lineCursorPosY );
                        ImGui::TextColored( ImFromVA( entry.Color ), vaStringTools::SimpleNarrow( entry.Text ).c_str( ) );
                    }

                    // ImGui::NextColumn();
                    // 
                    // int smax = vaMath::Max( 1, (int)vaLog::GetInstance().Entries().size() - showFrom );
                    // int smin = 0;
                    // int currentPos = smax - smax; // inverted? todo, this is weird
                    // ImGui::VSliderInt( "", ImVec2( scrollbarSize-16, (float)sizeY - 16 ), &currentPos, smin, smax, " " );
                }
            }
            else
            {

                //ImGui::TextWrapped("This example implements a console with basic coloring, completion and history. A more elaborate implementation may want to store entries along with extra data such as timestamp, emitter, etc.");
                ImGui::TextWrapped("Enter 'HELP' for help, press TAB to use text completion.");

                // TODO: display items starting from the bottom

                // if (ImGui::SmallButton("Add Dummy Text")) { vaLog::GetInstance().Add("%d some text", (int)vaLog::GetInstance().Entries().size()); vaLog::GetInstance().Add("some more text"); vaLog::GetInstance().Add("display very important message here!"); } ImGui::SameLine();
                // if (ImGui::SmallButton("Add Dummy Error")) vaLog::GetInstance().Add("[error] something went wrong"); ImGui::SameLine();
                // if (ImGui::SmallButton("Clear")) vaLog::GetInstance().Clear(); ImGui::SameLine();
                // bool copy_to_clipboard = ImGui::SmallButton("Copy"); ImGui::SameLine();
                // if (ImGui::SmallButton("Scroll to bottom")) m_scrollToBottom = true;
                bool copy_to_clipboard = false;

                ImGui::Separator();

                // ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
                // static ImGuiTextFilter filter;
                // filter.Draw("Filter (\"incl,-excl\") (\"error\")", 180);
                // ImGui::PopStyleVar();
                // ImGui::Separator();

                ImGui::BeginChild("ScrollingRegion", ImVec2(0,-ImGui::GetFrameHeightWithSpacing()), false, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoSavedSettings );
                if (ImGui::BeginPopupContextWindow())
                {
                    if (ImGui::Selectable("Clear")) vaLog::GetInstance().Clear();
                    ImGui::EndPopup();
                }

                // if( m_scrollByAddedLineCount )
                // {
                //     int lineDiff = vaMath::Max( 0, (int)logEntries.size() - m_lastDrawnLogLineCount );
                //     if( lineDiff > 0 )
                //     {
                //         ImGuiWindow * window = ImGui::GetCurrentWindow();
                //         if( window != nullptr )
                //         {
                //             float newScrollY = window->Scroll.y + ImGui::GetTextLineHeightWithSpacing() * lineDiff;
                // 
                //             window->DC.CursorMaxPos.y += window->Scroll.y; // SizeContents is generally computed based on CursorMaxPos which is affected by scroll position, so we need to apply our change to it.
                //             window->Scroll.y = newScrollY;
                //             window->DC.CursorMaxPos.y -= window->Scroll.y;
                //         }
                //     }
                //     m_scrollByAddedLineCount = false;
                // }


                // Display every line as a separate entry so we can change their color or add custom widgets. If you only want raw text you can use ImGui::TextUnformatted(log.begin(), log.end());
                // NB- if you have thousands of entries this approach may be too inefficient and may require user-side clipping to only process visible items.
                // You can seek and display only the lines that are visible using the ImGuiListClipper helper, if your elements are evenly spaced and you have cheap random access to the elements.
                // To use the clipper we could replace the 'for (int i = 0; i < Items.Size; i++)' loop with:
                //     ImGuiListClipper clipper(Items.Size);
                //     while (clipper.Step())
                //         for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                // However take note that you can not use this code as is if a filter is active because it breaks the 'cheap random-access' property. We would need random-access on the post-filtered list.
                // A typical application wanting coarse clipping and filtering may want to pre-compute an array of indices that passed the filtering test, recomputing this array when user changes the filter,
                // and appending newly elements as they are inserted. This is left as a task to the user until we can manage to improve this example code!
                // If your items are of variable size you may want to implement code similar to what ImGuiListClipper does. Or split your data into fixed height items to allow random-seeking into your list.
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4,1)); // Tighten spacing
                // for (int i = 0; i < Items.Size; i++)
                // {
                //     const char* item = Items[i];
                //     if (!filter.PassFilter(item))
                //         continue;
                //     ImVec4 col = ImVec4(1.0f,1.0f,1.0f,1.0f); // A better implementation may store a type per-item. For the sample let's just parse the text.
                //     if (strstr(item, "[error]")) col = ImColor(1.0f,0.4f,0.4f,1.0f);
                //     else if (strncmp(item, "# ", 2) == 0) col = ImColor(1.0f,0.78f,0.58f,1.0f);
                //     ImGui::PushStyleColor(ImGuiCol_Text, col);
                //     ImGui::TextUnformatted(item);
                //     ImGui::PopStyleColor();
                // }
                float scrollY = ImGui::GetScrollY();
                float windowStartY = ImGui::GetCursorPosY();
                windowStartY;

                float availableDrawArea = (linesToShowMax+1) * ImGui::GetTextLineHeightWithSpacing( );
                int drawFromLine = vaMath::Clamp( (int)(scrollY / ImGui::GetTextLineHeightWithSpacing( )), 0, vaMath::Max( 0, (int)logEntries.size()-1) );
                int drawToLine = vaMath::Clamp( (int)((scrollY+availableDrawArea)/ ImGui::GetTextLineHeightWithSpacing( )), 0, (int)logEntries.size() );

                if( copy_to_clipboard )
                {
                    // below was never tested but it might work
                    assert( false );
                    drawFromLine = 0;
                    drawToLine = (int)logEntries.size();
                    ImGui::LogToClipboard();
                }

                // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                // these (and same below) are the only place that remains to be optimized for huge LOG buffers; it's not that complicated probably 
                // but requires going into NewLine and figuring out how to correctly make a MultiNewLine instead
                for( int i = 0; i < drawFromLine; i++ )
                    ImGui::NewLine();
                // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

                for( int i = drawFromLine; i < drawToLine; i++ )
                {
                    const vaLog::Entry & entry = logEntries[i];
                    //float lineCursorPosY = ImGui::GetCursorPosY( );

                    char buff[64];
                    #pragma warning ( suppress: 4996 )
                    strftime( buff, sizeof(buff), "%H:%M:%S: ", localtime( &entry.LocalTime ) );
                    ImGui::TextColored( ImVec4( 0.3f, 0.3f, 0.2f, 1.0f ), buff );

                    ImGui::SameLine( timerSeparatorX );
                    ImGui::TextColored( ImFromVA( entry.Color ), vaStringTools::SimpleNarrow( entry.Text ).c_str( ) );
                }

                // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                // see comments above
                for( int i = drawToLine; i < (int)logEntries.size( ); i++ )
                    ImGui::NewLine();
                // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

                if( copy_to_clipboard )
                    ImGui::LogFinish();

                int addedLineCount = (int)logEntries.size() - m_lastDrawnLogLineCount;
                m_lastDrawnLogLineCount = (int)logEntries.size();

                // keep scrolling to bottom if we're at bottom and new lines were added (also reverse scroll if lines removed)
                if( addedLineCount != 0 && (drawToLine+vaMath::Abs(addedLineCount)) >= (int)logEntries.size( ) )
                    m_scrollToBottom = true;
           

                if( m_scrollToBottom )
                    ImGui::SetScrollHereY();
                m_scrollToBottom = false;

                ImGui::PopStyleVar();
                ImGui::EndChild();
                ImGui::Separator();

                if( !m_consoleWasOpen )
                    ImGui::SetKeyboardFocusHere( 0 );

                // messes with the scrollbar for some reason
                // // Keep auto focus on the input box
                // if( ImGui::IsItemHovered( ) || ( ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive( ) && !ImGui::IsMouseClicked( 0 ) ) )
                //     ImGui::SetKeyboardFocusHere( 0 ); // Auto focus previous widget

                // Command-line
                if (ImGui::InputText("Input", m_textInputBuffer, _countof(m_textInputBuffer), ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_CallbackCompletion|ImGuiInputTextFlags_CallbackHistory, (ImGuiInputTextCallback)&TextEditCallbackStub, (void*)this))
                {
                    char* input_end = m_textInputBuffer+strlen(m_textInputBuffer);
                    while (input_end > m_textInputBuffer && input_end[-1] == ' ') input_end--; *input_end = 0;
                    if (m_textInputBuffer[0])
                    {
                        ExecuteCommand( m_textInputBuffer );
                    }
                    strcpy_s(m_textInputBuffer, "");
                }
            }
        }
        ImGui::End( );
    }
    ImGui::PopStyleColor( 1 );

#endif // #ifdef VA_IMGUI_INTEGRATION_ENABLED

    m_consoleWasOpen = m_consoleOpen;
}

int vaUIConsole::TextEditCallbackStub( void * _data )
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGuiInputTextCallbackData * data = (ImGuiInputTextCallbackData *)_data;
    vaUIConsole* console = (vaUIConsole*)data->UserData;
    return console->TextEditCallback(data);
#else
    _data;
    return 0;
#endif
}

#ifdef VA_IMGUI_INTEGRATION_ENABLED
// Portable helpers
//static int   Stricmp(const char* str1, const char* str2)         { int d; while ((d = toupper(*str2) - toupper(*str1)) == 0 && *str1) { str1++; str2++; } return d; }
static int   Strnicmp(const char* str1, const char* str2, int n) { int d = 0; while (n > 0 && (d = toupper(*str2) - toupper(*str1)) == 0 && *str1) { str1++; str2++; n--; } return d; }
//static char* Strdup(const char *str)                             { size_t len = strlen(str) + 1; void* buff = malloc(len); return (char*)memcpy(buff, (const void*)str, len); }
#endif

int vaUIConsole::TextEditCallback( void * _data )
{
    _data;
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGuiInputTextCallbackData * data = (ImGuiInputTextCallbackData *)_data;
    //AddLog("cursor: %d, selection: %d-%d", data->CursorPos, data->SelectionStart, data->SelectionEnd);
    switch (data->EventFlag)
    {
    case ImGuiInputTextFlags_CallbackCompletion:
        {
            // Example of TEXT COMPLETION

            // Locate beginning of current word
            const char* word_end = data->Buf + data->CursorPos;
            const char* word_start = word_end;
            while (word_start > data->Buf)
            {
                const char c = word_start[-1];
                if (c == ' ' || c == '\t' || c == ',' || c == ';')
                    break;
                word_start--;
            }

            // Build a list of candidates
            ImVector<const char*> candidates;
            for (int i = 0; i < (int)m_commands.size(); i++)
                if (Strnicmp( m_commands[i].Name.c_str(), word_start, (int)(word_end-word_start)) == 0)
                    candidates.push_back( m_commands[i].Name.c_str() );

            if (candidates.Size == 0)
            {
                // No match
                vaLog::GetInstance().Add("No match for \"%.*s\"!\n", (int)(word_end-word_start), word_start);
            }
            else if (candidates.Size == 1)
            {
                // Single match. Delete the beginning of the word and replace it entirely so we've got nice casing
                data->DeleteChars((int)(word_start-data->Buf), (int)(word_end-word_start));
                data->InsertChars(data->CursorPos, candidates[0]);
                data->InsertChars(data->CursorPos, " ");
            }
            else
            {
                // Multiple matches. Complete as much as we can, so inputing "C" will complete to "CL" and display "CLEAR" and "CLASSIFY"
                int match_len = (int)(word_end - word_start);
                for (;;)
                {
                    int c = 0;
                    bool all_candidates_matches = true;
                    for (int i = 0; i < candidates.Size && all_candidates_matches; i++)
                        if (i == 0)
                            c = toupper(candidates[i][match_len]);
                        else if (c == 0 || c != toupper(candidates[i][match_len]))
                            all_candidates_matches = false;
                    if (!all_candidates_matches)
                        break;
                    match_len++;
                }

                if (match_len > 0)
                {
                    data->DeleteChars((int)(word_start - data->Buf), (int)(word_end-word_start));
                    data->InsertChars(data->CursorPos, candidates[0], candidates[0] + match_len);
                }

                // List matches
                vaLog::GetInstance().Add("Possible matches:\n");
                for (int i = 0; i < candidates.Size; i++)
                    vaLog::GetInstance().Add("- %s\n", candidates[i]);
            }

            break;
        }
    case ImGuiInputTextFlags_CallbackHistory:
        {
            // Example of HISTORY
            const int prev_history_pos = m_commandHistoryPos;
            if (data->EventKey == ImGuiKey_UpArrow)
            {
                if (m_commandHistoryPos == -1)
                    m_commandHistoryPos = (int)m_commandHistory.size() - 1;
                else if (m_commandHistoryPos > 0)
                    m_commandHistoryPos--;
            }
            else if (data->EventKey == ImGuiKey_DownArrow)
            {
                if (m_commandHistoryPos != -1)
                    if (++m_commandHistoryPos >= m_commandHistory.size())
                        m_commandHistoryPos = -1;
            }

            // A better implementation would preserve the data on the current input line along with cursor position.
            if (prev_history_pos != m_commandHistoryPos)
            {
                data->CursorPos = data->SelectionStart = data->SelectionEnd = data->BufTextLen = (int)snprintf(data->Buf, (size_t)data->BufSize, "%s", (m_commandHistoryPos >= 0) ? m_commandHistory[m_commandHistoryPos].c_str() : "");
                data->BufDirty = true;
            }
        }
    }
#endif
    return 0;
}
void vaUIConsole::ExecuteCommand( const string & commandLine )
{
    vaLog::GetInstance().Add( "# %s\n", commandLine.c_str() );

    // Insert into history. First find match and delete it so it can be pushed to the back. This isn't trying to be smart or optimal.
    m_commandHistoryPos = -1;
    for( int i = (int)m_commandHistory.size() - 1; i >= 0; i-- )
        if( vaStringTools::CompareNoCase( m_commandHistory[i], commandLine ) == 0 )
        {
            //free( History[i] );
            //History.erase( History.begin( ) + i );
            m_commandHistory.erase( m_commandHistory.begin() + i );
            break;
        }
    m_commandHistory.push_back( commandLine );

    // Process command
    if( vaStringTools::CompareNoCase( commandLine, "CLEAR" ) == 0 )
    {
        vaLog::GetInstance().Clear();
    }
    else if( vaStringTools::CompareNoCase( commandLine, "HELP" ) == 0 )
    {
        vaLog::GetInstance().Add( "Commands:" );
        for( int i = 0; i < (int)m_commands.size(); i++ )
            vaLog::GetInstance().Add( "- %s", m_commands[i].Name.c_str() );
    }
    else if( vaStringTools::CompareNoCase( commandLine, "HISTORY" ) == 0 )
    {
        for( int i = (int)m_commandHistory.size() >= 10 ? (int)m_commandHistory.size() - 10 : 0; i < (int)m_commandHistory.size(); i++ )
            vaLog::GetInstance().Add( "%3d: %s\n", i, m_commandHistory[i].c_str() );
    }
    else if( vaStringTools::CompareNoCase( commandLine, "QUIT" ) == 0 )
    {
        vaCore::SetAppQuitFlag( true );
    }
    else
    {
        vaLog::GetInstance().Add( LOG_COLORS_ERROR, "Unknown command: '%s'\n", commandLine.c_str() );
    }
    m_scrollToBottom = true;
}
