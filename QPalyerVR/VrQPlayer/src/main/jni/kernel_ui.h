/************************************************************************************


************************************************************************************/
#pragma once

#include "OVR_Math.h"
#include "OVR_FileSys.h"
#include "GUI/GuiSys.h"
#include "Locale/OVR_Locale.h"

#include <functional>
#include <unordered_map>
#include <vector>

///
#include "GUI/VRMenu.h"
namespace OVRFWQ {

    using OVR::Matrix4f;
    using OVR::Posef;
    using OVR::Quatf;
    using OVR::Vector2f;
    using OVR::Vector3f;
    using OVR::Vector4f;
class ovrQPlayerAppl;
class ovrControllerGUI : public OVRFW::VRMenu {
public:
    static char const* MENU_NAME;
    virtual ~ovrControllerGUI() {}
    static ovrControllerGUI* Create(OVRFWQ::ovrQPlayerAppl& vrControllerApp,std::string name);
    void Update(const OVRFW::ovrApplFrameIn& in);
    void Render(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out);
    OVRFW::VRMenuObject* AddItem(
            const std::string& name,
            const std::string& labelText,
            const OVR::Vector3f& position,
            const OVR::Vector2f& size = {100.0f, 100.0f},
            const OVR::Vector4f& color = {.0f, .0f,  .0f, 1.f}
    );

    OVRFW::VRMenuObject* AddButton(
            const std::string& name,
            const std::string& label,
            const OVR::Vector3f& position,
            const OVR::Vector2f& size = {100.0f, 100.0f},
            int mode = 0,
            const std::function<void( OVRFW::VRMenuObject*, float u, float v, int status_click, int mode )>& handler = {}
    );

    struct HitTestDevice {
        OVR::Vector3f pointerStart = {0.0f, 0.0f, 0.0f};
        OVR::Vector3f pointerEnd = {0.0f, 0.0f, 1.0f};
        OVRFW::VRMenuObject* hitObject = nullptr;
        int clicked = 0;
    };

    /// hit-testing
    std::vector<OVRFWQ::ovrControllerGUI::HitTestDevice>& HitTestDevices() {
        return Devices;
    }
    void AddHitTestRay(const OVR::Posef& ray, bool isClicking);
    void AddHitTestRay2( Vector3f pointerStart, Vector3f pointerEnd, int isClicking);

private:
    OVRFWQ::ovrQPlayerAppl& VrInputApp;
    std::vector<OVRFW::VRMenuObject*> AllElements;

    std::unordered_map< OVRFW::VRMenuObject*,
       std::function<void( OVRFW::VRMenuObject*, float u, float v, int status_click, int mode ) >> ButtonHandlers;

    std::vector< std::pair<OVRFW::VRMenuObject*,
        std::function<void( OVRFW::VRMenuObject*, float u, float v, int status_click, int mode )>> > LabelHandlers;


    std::vector<OVRFWQ::ovrControllerGUI::HitTestDevice> Devices;
    std::vector<OVRFWQ::ovrControllerGUI::HitTestDevice> PreviousFrameDevices;
private:
    ovrControllerGUI( OVRFWQ::ovrQPlayerAppl& vrControllerApp, char* name )
            : VRMenu(name), VrInputApp(vrControllerApp) {}

    ovrControllerGUI operator=(ovrControllerGUI&) = delete;

    virtual void OnItemEvent_Impl(
            OVRFW::OvrGuiSys& guiSys,
            OVRFW::ovrApplFrameIn const& vrFrame,
            OVRFW::VRMenuId_t const itemId,
            OVRFW::VRMenuEvent const& event) override;

    virtual bool OnKeyEvent_Impl( OVRFW::OvrGuiSys& guiSys, int const keyCode, const int repeatCount) override;

    virtual void PostInit_Impl( OVRFW::OvrGuiSys& guiSys,  OVRFW::ovrApplFrameIn const& vrFrame) override;

    virtual void Open_Impl( OVRFW::OvrGuiSys& guiSys) override;

    virtual void Frame_Impl( OVRFW::OvrGuiSys& guiSys,  OVRFW::ovrApplFrameIn const& vrFrame) override;

};

/// NOTE: this requires the app to have panel.ktx as a resource

} // namespace OVRFW
