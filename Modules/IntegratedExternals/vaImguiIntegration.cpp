#include "vaImguiIntegration.h"

using namespace VertexAsylum;

int vaImguiHierarchyObject::s_lastID = 0;

#ifdef VA_IMGUI_INTEGRATION_ENABLED

bool VertexAsylum::ImguiEx_VectorOfStringGetter( void * strVectorPtr, int n, const char** out_text )
{
    *out_text = (*(const std::vector<std::string> *)strVectorPtr)[n].c_str();
    return true;
}


void vaImguiHierarchyObject::DrawCollapsable( vaImguiHierarchyObject & obj, bool display_frame, bool default_open, bool indent )
{
    // needed so that any controls in obj.IHO_Draw() are unique
    ImGui::PushID( obj.IHO_GetPersistentObjectID( ).c_str( ) );

    ImGuiTreeNodeFlags headerFlags = 0;
    headerFlags |= (display_frame)?(ImGuiTreeNodeFlags_Framed):(0);
    headerFlags |= (default_open)?(ImGuiTreeNodeFlags_DefaultOpen):(0);

    if( ImGui::CollapsingHeader( obj.IHO_GetInstanceName( ).c_str( ), headerFlags ) )
    {
        if( indent )
            ImGui::Indent();
        
        obj.IHO_Draw();
        
        if( indent )
            ImGui::Unindent();
    }

    ImGui::PopID();
}

void vaImguiHierarchyObject::DrawList( const char * stringID, vaImguiHierarchyObject ** objList, int objCount, int & currentItem, float width, float listHeight, float selectedElementHeight )
{
    ImGui::PushID( stringID );
#if 0
    ImGui::ListBoxHeader( label, ImVec2( width, listHeight ) );

    bool value_changed = false;
    ImGuiListClipper clipper( objCount, ImGui::GetTextLineHeightWithSpacing() );

    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        {
            const bool item_selected = (i == currentItem);
            vaImguiHierarchyObject & obj = *(objList[i]);
            string itemText = obj.IHO_GetInstanceName().c_str();

            ImGui::PushID( obj.IHO_GetPersistentObjectID( ).c_str( ) );
            
            if( ImGui::Selectable( itemText.c_str(), item_selected ) )
            {
                if( currentItem != i )
                    currentItem = i;
                else
                    currentItem = -1;
                value_changed = true;
            }
            ImGui::PopID();
        }
    }
    ImGui::ListBoxFooter();
#else
    if( ImGui::BeginChild( "PropList", ImVec2( 0.0f, listHeight ), true ) )
    {
        ImGuiTreeNodeFlags defaultFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

        int objectClickedIndex = -1;
        for( int i = 0; i < objCount; i++ )
        {
            ImGuiTreeNodeFlags nodeFlags = defaultFlags | ((i == currentItem)?(ImGuiTreeNodeFlags_Selected):(0)) | ImGuiTreeNodeFlags_Leaf;
            bool nodeOpen = ImGui::TreeNodeEx( objList[i]->IHO_GetPersistentObjectID().c_str(), nodeFlags, objList[i]->IHO_GetInstanceName().c_str() );
                
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
#endif

    if( currentItem < 0 || currentItem >= objCount )
        selectedElementHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().WindowPadding.y;

    if( ImGui::BeginChild( "PropFrame", ImVec2( width, selectedElementHeight ), true ) )
    {
        if( currentItem >= 0 && currentItem < objCount )
        {
            vaImguiHierarchyObject & obj = *(objList[currentItem]);
            ImGui::PushID( obj.IHO_GetPersistentObjectID( ).c_str( ) );
            obj.IHO_Draw();
            ImGui::PopID();
        }
        else
        {
            ImGui::TextColored( ImVec4( 0.5f, 0.5f, 0.5f, 1.0f ), "Select an item to display properties" );
        }
    }
    ImGui::EndChild();
    ImGui::PopID( );
}


namespace VertexAsylum
{
    static char     c_popup_InputString_Label[128] = { 0 };
    static char     c_popup_InputString_Value[128] = { 0 };
    static bool     c_popup_InputString_JustOpened = false;

    bool                ImGuiEx_PopupInputStringBegin( const string & label, const string & initialValue )
    {
        if( c_popup_InputString_JustOpened )
        {
            assert( false );
            return false;
        }
        strncpy_s( c_popup_InputString_Label, label.c_str(), sizeof( c_popup_InputString_Label ) - 1 );
        strncpy_s( c_popup_InputString_Value, initialValue.c_str(), sizeof( c_popup_InputString_Value ) - 1 );

        ImGui::OpenPopup( c_popup_InputString_Label );
        c_popup_InputString_JustOpened = true;
        return true;
    }

    bool                ImGuiEx_PopupInputStringTick( string & outValue )
    {
        ImGui::SetNextWindowContentSize(ImVec2(300.0f, 0.0f));
        if( ImGui::BeginPopupModal( c_popup_InputString_Label ) )
        {
            if( c_popup_InputString_JustOpened )
            {
                ImGui::SetKeyboardFocusHere();
                c_popup_InputString_JustOpened = false;
            }

            bool enterClicked = ImGui::InputText( "New name", c_popup_InputString_Value, sizeof( c_popup_InputString_Value ), ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue );
            string newName = c_popup_InputString_Value;
        
            if( enterClicked || ImGui::Button( "Accept" ) )
            {
                if( newName != "" )
                {
                    outValue = newName;
                    ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                    return true;
                }
            }
            ImGui::SameLine();
            if( ImGui::Button( "Cancel" ) )
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
        return false;
    }
}

#else
void vaImguiHierarchyObject::DrawCollapsable( vaImguiHierarchyObject &, bool, bool, bool )
{
}
#endif

#ifdef VA_IMGUI_INTEGRATION_ENABLED

namespace VertexAsylum
{

    static ImFont *     s_bigClearSansRegular   = nullptr;
    static ImFont *     s_bigClearSansBold      = nullptr;

    ImFont *            ImGetBigClearSansRegular( )                 { return s_bigClearSansRegular; }
    ImFont *            ImGetBigClearSansBold( )                    { return s_bigClearSansBold;    }

    void                ImSetBigClearSansRegular( ImFont * font )   { s_bigClearSansRegular = font; }
    void                ImSetBigClearSansBold(    ImFont * font )   { s_bigClearSansBold    = font; }

}

#endif