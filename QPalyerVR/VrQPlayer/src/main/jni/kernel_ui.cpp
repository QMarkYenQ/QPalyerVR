/************************************************************************************

Filename    :   ui.cpp
Content     :   Componentized wrappers for GuiSys

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/
#include "kernel_ui.h"

#include <vector>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>

#include "GUI/GuiSys.h"
#include "GUI/DefaultComponent.h"
#include "GUI/ActionComponents.h"
#include "GUI/VRMenu.h"
#include "GUI/VRMenuObject.h"
#include "GUI/VRMenuMgr.h"
#include "GUI/Reflection.h"
#include "Render/DebugLines.h"

using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;
#include "kernel.h"
namespace OVRFWQ{

static const char* MenuDefinitionFile = R"menu_definition(
itemParms {
	// root
	VRMenuObjectParms {
		Type = VRMENU_CONTAINER;
		Flags = VRMENUOBJECT_RENDER_HIERARCHY_ORDER | VRMENUOBJECT_DONT_RENDER_TEXT;
		Components {
		}
		Text = "";
		LocalPose {
			Position = ( 0.0f, 0.0f, 0.0f );
			Orientation = ( 0.0f, 0.0f, 0.0f, 1.0f );
		}
		LocalScale = ( 1.0f, 1.0f, 1.0f );
		TextLocalPose {
			Position = ( 0.0f, 0.0f, 0.0f );
			Orientation = ( 0.0f, 0.0f, 0.0f, 1.0f );
		}
		TextLocalScale = ( 1.0f, 1.0f, 1.0f );
		ParentId = -1;
		Name = "root";
	}
////////////////////////////////////////////////////////////////////////////////////////////////////
	// panel
	VRMenuObjectParms {
		Type = VRMENU_STATIC;
		Flags = VRMENUOBJECT_RENDER_HIERARCHY_ORDER;
		TexelCoords = true;
		SurfaceParms {
			VRMenuSurfaceParms {
				SurfaceName = "panel";
				ImageNames {
					string[0] = "apk:///assets/panel.ktx";
				}
				TextureTypes {
					eSurfaceTextureType[0] =  SURFACE_TEXTURE_DIFFUSE;
				}
				Color = ( 0.1294f, 0.13333f, 0.14117f, 1.0f );
				Border = ( 0.0f, 0.0f, 0.0f, 0.0f );
				Dims = ( 500.0f, 300.0f );
			}
		}
		Text = "";
		LocalPose {
			Position = ( 0.0f, 0.0f, 0.0f );
			Orientation = ( 0.0f, 0.0f, 0.0f, 1.0f );
		}
		LocalScale = ( 500.0f, 300.0f, 1.0f );
		TextLocalPose {
			Position = ( 0.0f, 250.0f, 0.0f );
			Orientation = ( 0.0f, 0.0f, 0.0f, 1.0f );
		}
		TextLocalScale = ( 1.0f, 1.0f, 1.0f );
		FontParms {
			AlignHoriz = HORIZONTAL_CENTER;
			AlignVert = VERTICAL_BASELINE;
			Scale = 0.75f;
		}
		ParentName = "root";
		Name = "panel";
	}
////////////////////////////////////////////////////////////////////////////////////////////////////
// primary_input_foreground
	VRMenuObjectParms {
		Type = VRMENU_CONTAINER;
		Flags = VRMENUOBJECT_RENDER_HIERARCHY_ORDER | VRMENUOBJECT_DONT_RENDER_TEXT;
		TexelCoords = true;
		Components {
		}
		Text = "";
		LocalPose {
			Position = ( 0.0f, 0.0f, 0.05f );
			Orientation = ( 0.0f, 0.0f, 0.0f, 1.0f );
		}
		LocalScale = ( 1.0f, 1.0f, 1.0f );
		TextLocalPose {
			Position = ( 0.0f, 0.0f, 0.0f );
			Orientation = ( 0.0f, 0.0f, 0.0f, 1.0f );
		}
		TextLocalScale = ( 1.0f, 1.0f, 1.0f );
		ParentName = "panel";
		Name = "primary_input_foreground";
	}

}
)menu_definition";

static const char* MenuDefinitionFile2 = R"menu_definition(
itemParms {
////////////////////////////////////////////////////////////////////////////////////////////////////
	// primare Input trigger
	VRMenuObjectParms {
		Type = VRMENU_STATIC;
		Flags = VRMENUOBJECT_RENDER_HIERARCHY_ORDER;
		TexelCoords = true;
		SurfaceParms {
			VRMenuSurfaceParms {
				SurfaceName = "trigger_panel";
				ImageNames {
					string[0] = "apk:///assets/panel.ktx";
				}
				TextureTypes {
					eSurfaceTextureType[0] =  SURFACE_TEXTURE_DIFFUSE;
				}
				Color = ( 0.25f, 0.25f, 0.25f, 1.0f );
				Border = ( 0.0f, 0.0f, 0.0f, 0.0f );
				Dims = ( 128.0f, 128.0f );
			}
		}
        //
		LocalPose {
			Position = ( -317.0f, -10.0f, 0.0f );
			Orientation = ( 0.0f, 0.0f, 0.0f, 1.0f );
		}
		LocalScale = ( 1.0f, 1.0f, 1.0f );
        //

		TextLocalPose {
			Position = ( 0.0f, 0.0f, 0.0f );
			Orientation = ( 0.0f, 0.0f, 0.0f, 1.0f );
		}
		TextLocalScale = ( 1.0f, 1.0f, 1.0f );
		FontParms {
			AlignHoriz = HORIZONTAL_CENTER;
			AlignVert = VERTICAL_CENTER;
			Scale = 0.5f;
			WrapWidth = 120.0f;
			MultiLine = true;
		}
		ParentName = "primary_input_foreground";
		Name = "sample";
	}
}
)menu_definition";



//==================================================================================================



//==================================================================================================
std::vector<OVRFW::VRMenuObjectParms const*> g_itemParms_2;
ovrControllerGUI* ovrControllerGUI::Create( ovrQPlayerAppl& vrControllerApp, std::string name ) {

    /*
 // initially place the menu in front of the user's view on the horizon plane but do not move to
    // follow the user's gaze.
    VRMENU_FLAG_PLACE_ON_HORIZON,
    // place the menu directly in front of the user's view on the horizon plane each frame. The user
    // will sill be able to gaze track vertically.
    VRMENU_FLAG_TRACK_GAZE_HORIZONTAL,
    // place the menu directly in front of the user's view each frame -- this means gaze tracking
    // won't be available since the user can't look at different parts of the menu.
    VRMENU_FLAG_TRACK_GAZE,
    // If set, just consume the back key but do nothing with it (for warning menu that must be
    // accepted)
    VRMENU_FLAG_BACK_KEY_DOESNT_EXIT,
    // If set, a short-press of the back key will exit the app when in this menu
    VRMENU_FLAG_BACK_KEY_EXITS_APP,
    // If set, return false so short press is passed to app
    VRMENU_FLAG_SHORT_PRESS_HANDLED_BY_APP
     * */
    char const* menuFiles[] = {"apk:///assets/menu_layout.txt", nullptr};

    std::string u_ObjectName[]= {
            "primary_input_foreground",

    };

    char *namec = (char *)name.c_str();



    ovrControllerGUI* menu = new ovrControllerGUI(  vrControllerApp,  namec);


#if(0)
#else


    std::vector<uint8_t> buffer;

    size_t bufferLen;
    OVRFW::ovrParseResult parseResult;

    std::vector<OVRFW::VRMenuObjectParms const*> itemParms;
    bufferLen = OVR::OVR_strlen(MenuDefinitionFile);
    buffer.resize(bufferLen + 1);
    memcpy(buffer.data(), MenuDefinitionFile, bufferLen);
    buffer[bufferLen] = '\0';

     parseResult = OVRFW::VRMenuObject::ParseItemParms(
            vrControllerApp.GetGuiSys().GetReflection(),
            vrControllerApp.GetLocale(), u_ObjectName[0].c_str(), buffer, itemParms);
    OVR_ASSERT( parseResult );

    if (!parseResult) {
        DeletePointerArray(itemParms);
        ALOG("SimpleTargetMenu FAILED -> %s", parseResult.GetErrorText());
    }

    /// Hijack params

    for (auto* ip : itemParms) {
        // Find the one panel
        if ((int)ip->Id.Get() == 0) {
            const_cast<OVRFW::VRMenuObjectParms*>(ip)->Text = u_ObjectName[0];
        }
    }


    bufferLen = OVR::OVR_strlen(MenuDefinitionFile2);
    buffer.resize(bufferLen + 1);
    memcpy(buffer.data(), MenuDefinitionFile2, bufferLen);
    buffer[bufferLen] = '\0';

    parseResult = OVRFW::VRMenuObject::ParseItemParms(
            vrControllerApp.GetGuiSys().GetReflection(),
            vrControllerApp.GetLocale(), u_ObjectName[0].c_str(), buffer, g_itemParms_2);
    OVR_ASSERT( parseResult );





    menu->InitWithItems(
            vrControllerApp.GetGuiSys(),
            1.6f,
            OVRFW::VRMenuFlags_t(OVRFW::VRMENU_FLAG_PLACE_ON_HORIZON),
            itemParms);


//   VRMENU_FLAG_PLACE_ON_HORIZON VRMENU_FLAG_TRACK_GAZE_HORIZONTAL VRMENU_FLAG_TRACK_GAZE_VERTICAL
#if(0)
    OVR::Posef pose;
    pose.Translation = Vector3f(-317.0f, 0.0f, 0.0f);
    pose.Rotation= Quatf(0.0f, 0.0f, 0.f,1.0f);

    OVRFW::VRMenuObjectParms *xxx;


    xxx = (OVRFW::VRMenuObjectParms *) itemParms_2[0];


    xxx->SetLocalPose(pose);



    xxx->SetText("xxx");
    xxx->SetName("image_xxx");


    menu->AddItems(vrControllerApp.GetGuiSys(), itemParms_2, menu->GetRootHandle(), true);
#endif

#endif
    return menu;
}

void ovrControllerGUI::Update(const OVRFW::ovrApplFrameIn& in)
{
    for ( auto& it : LabelHandlers) {
        it.second( it.first, 0, 0, 0, 0  );
    }
    /// clear previous frame
    for (auto& device : PreviousFrameDevices) {
        if (device.hitObject) {
            auto it = ButtonHandlers.find(device.hitObject);
            if (it != ButtonHandlers.end()) {
                it->second( it->first, 0, 0, device.clicked, 1  );
            }
        }
    }

    /// hit tesst
    for (auto& device : Devices) {
        Vector3f pointerStart = device.pointerStart;
        Vector3f pointerEnd = device.pointerEnd;
        Vector3f pointerDir = (pointerEnd - pointerStart).Normalized();
        Vector3f targetEnd = pointerStart + pointerDir * 10.0f;

        OVRFW::HitTestResult hit = VrInputApp.GetGuiSys().TestRayIntersection(pointerStart, pointerDir);


        if (hit.HitHandle.IsValid()) {
            device.pointerEnd = pointerStart + hit.RayDir * hit.t - pointerDir * 0.045f;
            device.hitObject = VrInputApp.GetGuiSys().GetVRMenuMgr().ToObject(hit.HitHandle);


            if (device.hitObject) {


                auto it = ButtonHandlers.find(device.hitObject);
                if (it != ButtonHandlers.end()) {

                    it->second( it->first, hit.uv.x,hit.uv.y, device.clicked, 2  );

               }
            }
        }
    }

    /// Save these for later
    PreviousFrameDevices = Devices;
    Devices.clear();
}

void ovrControllerGUI::Render(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out)
{
    const Matrix4f& traceMat = out.FrameMatrices.CenterView.Inverted();
    VrInputApp.GetGuiSys().Frame(in, out.FrameMatrices.CenterView, traceMat);
    VrInputApp.GetGuiSys().AppendSurfaceList(out.FrameMatrices.CenterView, &out.Surfaces);
}





OVRFW::VRMenuObject* ovrControllerGUI::AddItem(
        const std::string& name,
        const std::string& labelText,
        const OVR::Vector3f& position,
        const OVR::Vector2f& size,
        const OVR::Vector4f& color ) {


#if(1)
    OVR::Posef pose;
    pose.Translation = position;
    pose.Rotation= Quatf(0.0f, 0.0f, 0.f,1.0f);
    OVRFW::VRMenuObjectParms *xxx;
    xxx = (OVRFW::VRMenuObjectParms *) g_itemParms_2[0];
    xxx->SetLocalPose( pose );
    xxx->SetText( labelText );
    xxx->SetName( name  );

    this->AddItems(VrInputApp.GetGuiSys(), g_itemParms_2, this->GetRootHandle(), true);

    OVRFW::VRMenuObject* obj = this->ObjectForName(VrInputApp.GetGuiSys(), name.c_str());
  //  obj->GetSurface(0).SetAnchors(Vector2f(0.,0.5));

    OVRFW::VRMenuObject* mo;
    // GetVRMenuMgr
    OVRFW::OvrVRMenuMgr& menuMgr = VrInputApp.GetGuiSys().GetVRMenuMgr();

    obj->SetSurfaceDims(0, size);
    Vector3f sc(1,1,1);

    obj->SetLocalScale(sc);
    obj->RegenerateSurfaceGeometry(0, false);
    obj->SetSurfaceColor(0,color);

     OVR::Vector3f border (0,0,0);
   // obj->SetSurfaceBorder(0,border);
//obj->SetLocalBoundsExpand(border,border);
 //obj->SetCullBounds(border);
#endif

    return obj;


}

/*

    lb->SetSurfaceTexture(0,0,1);
    void SetSurfaceTexture(
            int const surfaceIndex,
            int const textureIndex,
            eSurfaceTextureType const type,
            GLuint const texId,
            int const width,
            int const height);
 * */
    OVRFW::VRMenuObject* ovrControllerGUI::AddButton(
        const std::string& name, const std::string& label, const OVR::Vector3f& position,
        const OVR::Vector2f& size,  int mode,
        const std::function<void(OVRFW::VRMenuObject*, float u, float v, int status_click, int mode )>& handler )
{
    OVRFW::VRMenuObject* obj  = AddItem(name, label, position, size );


    if( obj && handler ){

        if( 0 == mode ){
            LabelHandlers.emplace_back( obj, handler );

        }else if ( 1 == mode ) {

            ButtonHandlers[obj] = handler;
        }else if ( 2 == mode ) {
            ButtonHandlers[obj] = handler;
            LabelHandlers.emplace_back( obj, handler );
        }
    }

    return obj;

}


void ovrControllerGUI::AddHitTestRay(const OVR::Posef& ray, bool isClicking) {
    OVRFWQ::ovrControllerGUI::HitTestDevice device;
    device.pointerStart = ray.Transform({0.0f, 0.0f, 0.0f});
    device.pointerEnd = ray.Transform({0.0f, 0.0f, -1.0f});
    device.clicked = isClicking;
    Devices.push_back(device);
}

void ovrControllerGUI::AddHitTestRay2( Vector3f pointerStart, Vector3f pointerEnd, int isClicking) {
    OVRFWQ::ovrControllerGUI::HitTestDevice device;
    device.pointerStart = pointerStart;
    device.pointerEnd = pointerEnd;
    device.clicked = isClicking;
    Devices.push_back(device);
}

void ovrControllerGUI::OnItemEvent_Impl(
        OVRFW::OvrGuiSys& guiSys,
        OVRFW::ovrApplFrameIn const& vrFrame,
        OVRFW::VRMenuId_t const itemId,
        OVRFW::VRMenuEvent const& event)
{


}

bool ovrControllerGUI::OnKeyEvent_Impl(
        OVRFW::OvrGuiSys& guiSys, int const keyCode, const int repeatCount)
{
    return (keyCode == AKEYCODE_BACK);
}

void ovrControllerGUI::PostInit_Impl(OVRFW::OvrGuiSys& guiSys, OVRFW::ovrApplFrameIn const& vrFrame)
{

}

void ovrControllerGUI::Open_Impl(OVRFW::OvrGuiSys& guiSys)
{

}

void ovrControllerGUI::Frame_Impl(OVRFW::OvrGuiSys& guiSys, OVRFW::ovrApplFrameIn const& vrFrame) {
    OVR_UNUSED(VrInputApp);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace OVRFW
