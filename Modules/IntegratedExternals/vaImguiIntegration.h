#pragma once



#include "Core\vaCore.h"
#include "Core\vaStringTools.h"
#include "Core\vaGeometry.h"

#ifdef VA_IMGUI_INTEGRATION_ENABLED
#include "IntegratedExternals\imgui\imgui.h"
#include "IntegratedExternals\imgui\imgui_internal.h"
#endif

namespace VertexAsylum
{
    class vaImguiHierarchyObject
    {
    private:
        static int                                              s_lastID;
        string                                                  m_persistentObjectID;

    protected:
        vaImguiHierarchyObject( )                               { m_persistentObjectID = vaStringTools::Format( "IHO%d", s_lastID ); s_lastID++; }  // create unique string id for each new object
        virtual ~vaImguiHierarchyObject( )                      { }

    public:
        // this is just so that the title of the collapsing header can be the same for multiple different objects, as ImGui tracks them by string id which defaults to the title
        virtual string                                          IHO_GetPersistentObjectID( ) const  { return m_persistentObjectID; }
        // this string can change at runtime!
        virtual string                                          IHO_GetInstanceName( ) const        { return "Unnamed"; }
        // draw your own IMGUI stuff here, and call IHO_DrawIfOpen on sub-elements
        virtual void                                            IHO_Draw( )                         { }

    public:
        static void                                             DrawCollapsable( vaImguiHierarchyObject & obj, bool display_frame = true, bool default_open = false, bool indent = false );

        static void                                             DrawList( const char * stringID, vaImguiHierarchyObject ** objList, int objCount, int & currentItem, float width = 0.0f, float listHeight = 0.0f, float selectedElementHeight = 0.0f );
    };

#ifdef VA_IMGUI_INTEGRATION_ENABLED
    inline ImVec4       ImFromVA( const vaVector4 &  v )            { return ImVec4( v.x, v.y, v.z, v.w); }
    inline ImVec4       ImFromVA( const vaVector3 &  v )            { return ImVec4( v.x, v.y, v.z, 0.0f); }
    inline vaVector4    VAFromIm( const ImVec4 &  v )               { return vaVector4( v.x, v.y, v.z, v.w); }

    bool                ImguiEx_VectorOfStringGetter( void * strVectorPtr, int n, const char** out_text );

    inline bool         ImGuiEx_ListBox( const char * label, int & itemIndex, const std::vector<std::string> & elements, int heightInItems = -1 )
    {
        ImGui::ListBox( label, &itemIndex, ImguiEx_VectorOfStringGetter, (void*)&elements, (int)elements.size(), heightInItems );
    }

    inline bool         ImGuiEx_Combo( const char * label, int & itemIndex, const std::vector<std::string> & elements )
    {
        char buffer[4096]; buffer[0] = NULL;
        int currentLoc = 0;
        for( auto & element : elements ) 
        {
            if( (currentLoc + element.size() + 2) >= sizeof(buffer) )
                break;
            memcpy( &buffer[currentLoc], element.c_str(), element.size() );
            currentLoc += (int)element.size();
            buffer[currentLoc] = 0;
            currentLoc++;
        }
        buffer[currentLoc] = 0;
        itemIndex = vaMath::Clamp( itemIndex, 0, (int)elements.size() );
        return ImGui::Combo( label, &itemIndex, buffer );
    }

    // Two big TTF fonts for big.. things? Created in vaApplicationBase
    ImFont *                                                    ImGetBigClearSansRegular( );
    ImFont *                                                    ImGetBigClearSansBold( );

    bool                ImGuiEx_PopupInputStringBegin( const string & label, const string & initialValue );
    bool                ImGuiEx_PopupInputStringTick( string & outValue );
#endif
}

