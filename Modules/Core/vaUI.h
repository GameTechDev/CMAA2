///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018, Intel Corporation
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

#include "Core/vaEvent.h"

#include "vaXMLSerialization.h"

namespace VertexAsylum
{
    // Intended to expose ui for instances of subsystem entities such as individual materials, meshes, etc.
    // These will not show up unless you call vaUIManager::GetInstance().SelectPropertyItem( this_item )
    class vaUIPropertiesItem
    {
        static int                          s_lastID;
        string const                        m_uniqueID;
    public:
        vaUIPropertiesItem( );
        vaUIPropertiesItem( const vaUIPropertiesItem & copy );
        ~vaUIPropertiesItem( ) { }

        vaUIPropertiesItem & operator = ( const vaUIPropertiesItem & copy )             { copy; return *this; }                       // nothing to copy!
    
    public:
        const string &                      UIPropertiesItemGetUniqueID( ) const         { return m_uniqueID; }         // unique ID for the session (does not have to persist between sessions - yet)
        
        virtual string                      UIPropertiesItemGetDisplayName( ) const             { return "unnamed"; }          // name of the props item - can be anything and doesn't have to be unique
        virtual void                        UIPropertiesItemDraw( )                      = 0;

        void                                UIItemDrawCollapsable( bool showFrame = true, bool defaultOpen = false, bool indent = false );

    public:
        static void                         DrawList( const char * stringID, vaUIPropertiesItem ** objList, int objListCount, int & currentItem, float width = 0.0f, float listHeight = 0.0f, float selectedElementHeight = 0.0f );
    };

    // Intended to expose ui for a whole subsystem that usually has only one instance (i.e. gbuffer, tone mapping, material manager, ...)
    // vaUIPanel objects will be automatically tracked and DrawList() called once per frame just before the main tick
    class vaUIPanel
    {
    public:
        // Extend when needed
        enum class DockLocation : int32
        {
            NotDocked           = 0,
            DockedLeft          ,
            DockedLeftBottom    ,
            DockedRight         ,
            DockedRightBottom   ,
        };

    private:
        string const                        m_name                  = "error";
        string const                        m_familyName            = "";
        int const                           m_sortOrder             = 0;
        DockLocation const                  m_initialDock           = DockLocation::NotDocked;
        vaVector2 const                     m_initialSize           = vaVector2( 400, 400 );

        bool                                m_visible               = true;

        bool                                m_setFocusNextFrame     = false;

    protected:
        // name must be unique (if it isn't, a number gets added to it) - override UIPanelGetDisplayName to return a custom name used for the window/tab title (does not have to be unique, can change every frame, etc.)
        // sortOrder is used to determine order the windows are displayed in menus and initialized as well as initial focus (lower has priority)
        // visible determines window visibility; override UIPanelIsListed to disable it from appearing 'View' menu and to remove 'close' button (so visibility is only changeable from the program)
        // set familyName if you want multiple vaUIPanels to show under one parent 'family' panel as individual tabs
        vaUIPanel( const string & name, int sortOrder = 0, bool initialVisible = true, DockLocation initialDock = DockLocation::NotDocked, const string & familyName = "", const vaVector2 & initialSize = vaVector2( 400, 400 ) );

    public:
        virtual ~vaUIPanel();

    private:
        // // this is for ImGui persistence - to avoid overlap with other object with identically named controls
        // virtual string                      UIPanelGetID( ) const               { return m_uniqueID; }


    public:
        const string &                      UIPanelGetName( ) const                 { return m_name; }                  // name of the panel has to be unique among all panels and cannot change later; constructor will add number to the end of the name if one with the same name already exists; persistence between sessions is recommended
        const string &                      UIPanelGetFamily( ) const               { return m_familyName; }

        const vaVector2 &                   UIPanelGetInitialSize( ) const          { return m_initialSize; }
        DockLocation                        UIPanelGetInitialDock( ) const          { return m_initialDock; }
        int                                 UIPanelGetSortOrder( ) const            { return m_sortOrder; }

        virtual bool                        UIPanelIsVisible( ) const               { return m_visible; }
        virtual void                        UIPanelSetVisible( bool visible )       { m_visible = visible; }
        virtual bool                        UIPanelIsListed( ) const                { return true; }                    // is listed in the top menu under 'Windows'? if false also disables close button - UI can't change opened/closed status
        virtual void                        UIPanelDraw( )                          = 0;

        virtual string                      UIPanelGetDisplayName( ) const          { return UIPanelGetName(); }        // use to override display name if using multiple instances - can change at runtime (animate)

        virtual void                        UIPanelFileMenuItems( )                 { }                                 // use to insert items ('ImGui::MenuItem') into 'File' menu - for nicer separation add separator at the end
        virtual void                        UIPanelStandaloneMenu( )                { }                                 // use to insert standalone menus ('ImGui::BeginMenu')

    public:
        void                                UIPanelSetFocusNextFrame( bool focus = true )  { m_setFocusNextFrame = focus; }
        bool                                UIPanelGetFocusNextFrame( ) const              { return m_setFocusNextFrame; }
        void                                UIPanelDrawCollapsable( bool showFrame = true, bool defaultOpen = false, bool indent = false );

    private:
        static string                       FindUniqueName( const string & name );
    };

    // if you just want to create a panel with a lambda callback without inheriting vaUIPanel, here's a simple wrapper
    class vaUISimplePanel : vaUIPanel
    {
        std::function< void() > m_callback;
    public:
        vaUISimplePanel( std::function< void() > callback, const string & name, int sortOrder = 0, bool initialVisible = true, DockLocation initialDock = DockLocation::NotDocked, const string & familyName = "", const vaVector2 & initialSize = vaVector2( 400, 400 ) ) : vaUIPanel( name, sortOrder, initialVisible, initialDock, familyName, initialSize ), m_callback(callback) { }

        virtual void                        UIPanelDraw( ) override { m_callback(); }
    };

    class vaUIPropertiesPanel;
    class vaUIFamilyPanel;

    class vaUIManager final : public vaSingletonBase<vaUIManager>
    {
        friend class vaUIPanel;
        map< string, vaUIPanel *>           m_panels;

        // ImGui doesn't remember visibility (by design?) so we have to save/load it ourselves
        // this is loaded during serialization so all new panels can check if they were hidden when the app closed last time (& serialized settings)
        vector< pair<string, bool>>         m_initiallyPanelVisibility;

        vector<shared_ptr<vaUIPropertiesPanel>>  
                                            m_propPanels;
        vector<shared_ptr<vaUIFamilyPanel>> m_familyPanels;

        //
        std::function< void( bool & uiVisible, bool & menuVisible, bool & consoleVisible ) > 
                                            m_visibilityOverrideCallback;

        // various settings
        bool                                m_visible           = true;
        bool                                m_menuVisible       = true;
        bool                                m_consoleVisible    = true;

        // some additional initialization required first frame
        bool                                m_firstFrame        = true;

        // ImGui demo - useful for ImGui development
        bool                                m_showImGuiDemo     = false;

        // ImGui docking stuff
        uint32 /*ImGuiID*/                  m_dockSpaceIDRoot           = 0;
        uint32 /*ImGuiID*/                  m_dockSpaceIDLeft           = 0;
        uint32 /*ImGuiID*/                  m_dockSpaceIDLeftBottom     = 0;
        uint32 /*ImGuiID*/                  m_dockSpaceIDRight          = 0;
        uint32 /*ImGuiID*/                  m_dockSpaceIDRightBottom    = 0;

    private:
        friend class vaCore;
        vaUIManager( );
        ~vaUIManager( );

    private:
        friend class vaApplicationBase;
        void                                SerializeSettings( vaXMLSerializer & serializer );
        void                                UpdateUI( bool appHasFocus );
        void                                DrawUI( );

        void                                SelectPropertyItem( const shared_ptr<vaUIPropertiesItem> & item, int preferredPropPanel = -1 ) { item; preferredPropPanel; }
        void                                UnselectPropertyItem( const shared_ptr<vaUIPropertiesItem> & item ) { item; };

        // ImGui doesn't remember visibility (by design?) so we have to save/load it ourselves
        bool                                FindInitialVisibility( const string & panelName, bool defVal );

    public:

        bool                                IsVisible( ) const                          { return m_visible; }
        void                                SetVisible( bool visible )                  { m_visible = visible; }

        bool                                IsMenuVisible( ) const                      { return m_menuVisible; }
        void                                SetMenuVisible( bool menuVisible )          { m_menuVisible = menuVisible; }

        bool                                IsConsoleVisible( ) const                   { return m_consoleVisible; }
        void                                SetConsoleVisible( bool consoleVisible )    { m_consoleVisible = consoleVisible; }

        // use this to override 
        void                                SetVisibilityOverrideCallback( std::function< void(bool & uiVisible, bool & menuVisible, bool & consoleVisible) > callback )       { m_visibilityOverrideCallback = callback; }
    };

    // This is a half-baked helper class for displaying log in a form of a console, with additional support for 
    // commands and etc. Initially based on ImGui console examples.
    // !!! Requires additional work to be functional. !!!
    class vaUIConsole : public vaSingletonBase< vaUIConsole >
    {
        struct CommandInfo
        {
            string      Name;
            //some-kindo-of-callback

            CommandInfo( ) { }
            explicit CommandInfo( const string & name ) : Name(name) { }
        };

    private:
        char                    m_textInputBuffer[256];
        bool                    m_scrollToBottom;
        bool                    m_scrollByAddedLineCount = false;      // if already at bottom, scroll next frame to stay at bottom
        int                     m_lastDrawnLogLineCount = 0;
        
        vector<CommandInfo>     m_commands;

        vector<string>          m_commandHistory;
        int                     m_commandHistoryPos;    // -1: new line, 0..History.Size-1 browsing history.

        // show console with input text (otherwise just show log)
        bool                    m_consoleOpen;
        bool                    m_consoleWasOpen;

    private:
        friend class vaCore;
        vaUIConsole( ); 
        ~vaUIConsole( );

    public:
        // if open - shows full console and input box; if closed - shows only log for the messages not older than (some time)
        bool                    IsOpen( ) const                     { return m_consoleOpen; }
        void                    SetOpen( bool consoleOpen )         { m_consoleOpen = consoleOpen; }

        bool                    IsVisible( ) const                  { return vaUIManager::GetInstance().IsConsoleVisible(); }
        void                    SetVisible( bool visible    )       { vaUIManager::GetInstance().SetConsoleVisible( visible ); }
        // Todo:
        //void                    AddCommand( const string & name ... )
        //void                    RemoveCommand( const string & name )

        void                    Draw( int windowWidth, int windowHeight );

    private:
        void                    ExecuteCommand( const string & commandLine );

    private:
        static int              TextEditCallbackStub( void * data );
        int                     TextEditCallback( void * data );
    };

}