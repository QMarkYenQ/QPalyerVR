#pragma once
#include "Appl.h"
#include <GUI/Reflection.h>
#include "Input/HandModel.h"
#include "Input/ArmModel.h"
#include "Model/SceneView.h"
#include "Render/BeamRenderer.h"
#include "Render/ParticleSystem.h"
#include "Render/Ribbon.h"
#include "GUI/GuiSys.h"
#include "GUI/VRMenu.h"

class ovrVrInput;

namespace OVRFWQ {

    static const char* MenuDefinitionFile = R"menu_definition(
    itemParms {
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
                Color = ( 0.0f, 0.0f, 0.1f, 1.0f ); // MENU_DEFAULT_COLOR
                Border = ( 16.0f, 16.0f, 16.0f, 16.0f );
                Dims = ( 100.0f, 100.0f );
            }
          }

          Text = "Panel";
          LocalPose {
              Position = ( 0.0f, 00.0f, 0.0f );
              Orientation = ( 0.0f, 0.0f, 0.0f, 1.0f );
          }
          LocalScale = ( 100.0f, 100.0f, 1.0f );
          TextLocalPose {
            Position = ( 0.0f, 0.0f, 0.0f );
            Orientation = ( 0.0f, 0.0f, 0.0f, 1.0f );
          }
          TextLocalScale = ( 1.0f, 1.0f, 1.0f );
          FontParms {
              AlignHoriz = HORIZONTAL_CENTER;
              AlignVert = VERTICAL_CENTER;
              Scale = 0.5f;
          }
          ParentId = -1;
          Id = 0;
          Name = "panel";
        }
    }
    )menu_definition";

    class SimpleTargetMenu : public OVRFW::VRMenu {
    public:
        static SimpleTargetMenu* Create(
                OVRFW::OvrGuiSys& guiSys,
                OVRFW::ovrLocale& locale,
                const std::string& menuName,
                const std::string& text) {
            return new SimpleTargetMenu(guiSys, locale, menuName, text);
        }

    private:
        SimpleTargetMenu(
                OVRFW::OvrGuiSys& guiSys,
                OVRFW::ovrLocale& locale,
                const std::string& menuName,
                const std::string& text)
                : OVRFW::VRMenu(menuName.c_str())
        {
            std::vector<uint8_t> buffer;
            std::vector<OVRFW::VRMenuObjectParms const*> itemParms;

            size_t bufferLen = OVR::OVR_strlen(MenuDefinitionFile);
            buffer.resize(bufferLen + 1);
            memcpy(buffer.data(), MenuDefinitionFile, bufferLen);
            buffer[bufferLen] = '\0';


            OVRFW::ovrParseResult parseResult = OVRFW::VRMenuObject::ParseItemParms(
                    guiSys.GetReflection(), locale, menuName.c_str(), buffer, itemParms);
            if (!parseResult) {
                DeletePointerArray(itemParms);
                ALOG("SimpleTargetMenu FAILED -> %s", parseResult.GetErrorText());
                return;
            }

            /// Hijack params
            for (auto* ip : itemParms) {
                // Find the one panel
                //
                if ((int)ip->Id.Get() == 0) {
                    const_cast<OVRFW::VRMenuObjectParms*>(ip)->Text = text;
                }
            }

            InitWithItems(
                    guiSys,
                    2.0f,
                    OVRFW::VRMenuFlags_t(OVRFW::VRMENU_FLAG_SHORT_PRESS_HANDLED_BY_APP),
                    itemParms);
        }

        virtual ~SimpleTargetMenu(){};
    };




    //------------------------------------------------------------------------
    typedef std::vector< std::pair<OVRFW::ovrParticleSystem::handle_t, OVRFW::ovrBeamRenderer::handle_t>> jointHandles_t;

    enum HapticStates {
        HAPTICS_NONE = 0,
        HAPTICS_BUFFERED = 1,
        HAPTICS_SIMPLE = 2,
        HAPTICS_SIMPLE_CLICKED = 3
    };
    struct DeviceHapticState {
        HapticStates HapticState = HapticStates::HAPTICS_NONE;
        float HapticSimpleValue = 0.0f;
    };
    struct HandSampleConfigurationParameters {
       public:
        HandSampleConfigurationParameters() : RenderAxis(false), EnableStandardDevices(false) {}

        bool RenderAxis; // render 3D axis on hands, etc
        bool EnableStandardDevices; // Render and handle input events from ovrInputDeviceStandardPointer
        // instead of per device type.
        HapticStates OnTriggerHapticsState;
    };
    //----------------------------------------------------------------------------------------------
    class ovrInputDeviceBase {
       public:
        ovrInputDeviceBase() = default;
        virtual ~ovrInputDeviceBase() = default;
        virtual const ovrInputCapabilityHeader* GetCaps() const = 0;
        virtual ovrControllerType GetType() const = 0;
        virtual ovrDeviceID GetDeviceID() const = 0;
        virtual const char* GetName() const = 0;
    };
    //----------------------------------------------------------------------------------------------
    class ovrInputDeviceHandBase : public ovrInputDeviceBase {
       public:
        std::vector<OVR::Matrix4f> g_TransformMatrices;
        ovrInputDeviceHandBase(ovrHandedness hand);
        virtual ~ovrInputDeviceHandBase() {}
        ovrHandedness GetHand() const {
            return Hand;
        }
        virtual OVR::Matrix4f GetModelMatrix(const OVR::Posef& handPose) const {
            return OVR::Matrix4f(handPose);
        }
        virtual OVR::Matrix4f GetPointerMatrix() const {
            return OVR::Matrix4f(PointerPose);
        }
        inline bool IsLeftHand() const {
            return Hand == VRAPI_HAND_LEFT;
        }
        const OVRFW::menuHandle_t& GetLastHitHandle() const {
            return HitHandle;
        }
        void SetLastHitHandle(const OVRFW::menuHandle_t& lastHitHandle) {
            HitHandle = lastHitHandle;
        }
        OVRFW::ovrHandModel& GetHandModel() {
            return HandModel;
        }
        jointHandles_t& GetFingerJointHandles() {
            return FingerJointHandles;
        }
        OVRFW::ovrBeamRenderer::handle_t& GetLaserPointerBeamHandle() {
            return LaserPointerBeamHandle;
        }
        OVRFW::ovrParticleSystem::handle_t& GetLaserPointerParticleHandle() {
            return LaserPointerParticleHandle;
        }

        bool GetIsActiveInputDevice() const {
         return IsActiveInputDevice;
        }
        void SetIsActiveInputDevice(bool isActive) {
         IsActiveInputDevice = isActive;
        }

        virtual void
        InitFromSkeletonAndMesh( ovrHandSkeleton* skeleton, ovrHandMesh* mesh);
        void UpdateSkeleton(const OVR::Posef& handPose);
        virtual bool Update(ovrMobile* ovr, const double displayTimeInSeconds, const float dt);
        virtual void Render(std::vector<OVRFW::ovrDrawSurface>& surfaceList) {
         for (auto& surface : Surfaces) {
          if (surface.surface != nullptr) {
           surfaceList.push_back(surface);
          }
         }
        }
        virtual void ResetHaptics(ovrMobile* ovr, float displayTimeInSeconds){};
        void UpdateHaptics(ovrMobile* ovr, float dt);
        virtual bool HasCapSimpleHaptics() const {
         return false;
        }
        virtual bool HasCapBufferedHaptics() const {
         return false;
        }
        virtual float GetHapticSampleDurationMS() const {
         return 0.0f;
        }
        virtual uint32_t GetHapticSamplesMax() const {
         return 0;
        }

        virtual OVR::Posef GetHandPose() const {
         return HandPose;
        }
        virtual OVR::Posef GetPointerPose() const {
         return PointerPose;
        }
        virtual bool IsPinching() const {
         return ((InputStateHand.InputStateStatus & ovrInputStateHandStatus_IndexPinching) != 0);
        }
        virtual bool IsPointerValid() const {
         return ((InputStateHand.InputStateStatus & ovrInputStateHandStatus_PointerValid) != 0);
        }
        virtual bool WasPinching() const {
         return PreviousFramePinch;
        }
        virtual bool Clicked() const {
         return WasPinching() && !IsPinching();
        }
        virtual bool IsInSystemGesture() const {
         return false;
        }

        virtual OVR::Vector3f GetRayOrigin() const {
         return IsLeftHand() ? OVR::Vector3f(0.04f, -0.05f, -0.1f)
                             : OVR::Vector3f(-0.04f, -0.05f, -0.1f);
        }

        virtual OVR::Vector3f GetRayEnd() const {
         return PointerPose.Transform(OVR::Vector3f(0.0f, 0.0f, -1.5f));
        }

        virtual ovrConfidence GetHandPoseConfidence() const {
         return ovrConfidence_HIGH;
        }
        virtual bool IsMenuPressed() const {
         return false;
        }
        virtual bool WasMenuPressed() const {
         return PreviousFrameMenu;
        }
        virtual bool MenuClicked() const {
         return WasMenuPressed() && !IsMenuPressed();
        }
        virtual const DeviceHapticState& GetRequestedHapticsState() {
         return PreviousHapticState;
        }

    protected:
        ovrHandedness Hand;
        std::vector<OVRFW::ovrDrawSurface> Surfaces;
        OVRFW::ovrHandModel HandModel;
        ovrInputStateHand InputStateHand;
        jointHandles_t FingerJointHandles;
        OVRFW::ovrBeamRenderer::handle_t LaserPointerBeamHandle;
        OVRFW::ovrParticleSystem::handle_t LaserPointerParticleHandle;
        ovrTracking Tracking;
        std::vector<OVR::Matrix4f> TransformMatrices;
        std::vector<OVR::Matrix4f> BindMatrices;
        std::vector<OVR::Matrix4f> SkinMatrices;
        OVRFW::GlBuffer SkinUniformBuffer;
        OVRFW::menuHandle_t HitHandle;
        OVRFW::ovrSurfaceDef SurfaceDef;
        OVR::Vector3f GlowColor;
        OVR::Posef HandPose;
        OVR::Posef PointerPose;
        OVR::Posef HeadPose;
        bool PreviousFramePinch;
        bool PreviousFrameMenu;
        bool IsActiveInputDevice;

        // State of Haptics after a call to UpdateHaptics
        DeviceHapticState PreviousHapticState;
    };
    class ovrInputDeviceTrackedRemoteHand : public ovrInputDeviceHandBase {
    public:
        HandSampleConfigurationParameters sampleConfiguration;

        ovrInputDeviceTrackedRemoteHand(
                const ovrInputTrackedRemoteCapabilities& caps,
                ovrHandedness hand)
                : ovrInputDeviceHandBase(hand),
                  Caps(caps),
                  ControllerModel(nullptr),
                  IsPinchingInternal(false) {
            GlowColor = OVR::Vector3f(0.0f, 0.0f, 1.0f);
        }

        virtual ~ovrInputDeviceTrackedRemoteHand() {}

        static ovrInputDeviceTrackedRemoteHand* Create(
                OVRFW::ovrAppl& app,
                const ovrInputTrackedRemoteCapabilities& capsHeader);

        virtual const ovrInputCapabilityHeader* GetCaps() const override {
            return &Caps.Header;
        }
        virtual ovrControllerType GetType() const override {
            return Caps.Header.Type;
        }
        virtual ovrDeviceID GetDeviceID() const override {
            return Caps.Header.DeviceID;
        }
        virtual const char* GetName() const override {
            return "TrackedRemoteHand";
        }
        const ovrInputTrackedRemoteCapabilities& GetTrackedRemoteCaps() const {
            return Caps;
        }

        virtual OVR::Matrix4f GetModelMatrix(const OVR::Posef& handPose) const override;

        virtual bool Update(ovrMobile* ovr, const double displayTimeInSeconds, const float dt) override;
        virtual void Render(std::vector<OVRFW::ovrDrawSurface>& surfaceList) override;

        virtual bool HasCapSimpleHaptics() const override {
            return GetTrackedRemoteCaps().ControllerCapabilities &
                   ovrControllerCaps_HasSimpleHapticVibration;
        }
        virtual bool HasCapBufferedHaptics() const override {
            return GetTrackedRemoteCaps().ControllerCapabilities &
                   ovrControllerCaps_HasBufferedHapticVibration;
        }
        virtual float GetHapticSampleDurationMS() const override {
            return GetTrackedRemoteCaps().HapticSampleDurationMS;
        }
        virtual uint32_t GetHapticSamplesMax() const override {
            return GetTrackedRemoteCaps().HapticSamplesMax;
        }

        virtual OVR::Vector3f GetRayOrigin() const override {
            return PointerPose.Transform(OVR::Vector3(0.0f, 0.0f, -0.055f));
        }
        virtual OVR::Matrix4f GetPointerMatrix() const override;

        virtual bool IsPinching() const override {
            return IsPinchingInternal;
        }
        virtual bool IsPointerValid() const override {
            return true;
        }
        virtual bool IsMenuPressed() const override {
            return !PreviousFrameMenuPressed && IsMenuPressedInternal;
        }
        virtual const DeviceHapticState& GetRequestedHapticsState() override {
            return RequestedHapticState;
        }

        void SetControllerModel(OVRFW::ModelFile* m);
        void ResetHaptics(ovrMobile* ovr, float displayTimeInSeconds) override;

    private:
        void UpdateHapticRequestedState(const ovrInputStateTrackedRemote& remoteInputState, const HandSampleConfigurationParameters& SampleConfiguration);

        DeviceHapticState RequestedHapticState;
        ovrInputTrackedRemoteCapabilities Caps;
        OVRFW::ModelFile* ControllerModel;
        bool IsPinchingInternal;
        bool PreviousFrameMenuPressed;
        bool IsMenuPressedInternal;
    };
    class ovrInputDeviceTrackedHand : public ovrInputDeviceHandBase {
    public:
        ovrInputDeviceTrackedHand(const ovrInputHandCapabilities& caps, ovrHandedness hand)
                : ovrInputDeviceHandBase(hand), Caps(caps) {
            GlowColor = OVR::Vector3f(1.0f, 0.0f, 0.0f);
        }


        virtual ~ovrInputDeviceTrackedHand() {}





        static ovrInputDeviceTrackedHand* Create(
                OVRFW::ovrAppl& app,
                const ovrInputHandCapabilities& capsHeader);

        virtual const ovrInputCapabilityHeader* GetCaps() const override {
            return &Caps.Header;
        }
        virtual ovrControllerType GetType() const override {
            return Caps.Header.Type;
        }
        virtual ovrDeviceID GetDeviceID() const override {
            return Caps.Header.DeviceID;
        }
        virtual const char* GetName() const override {
            return "TrackedHand";
        }
        const ovrInputHandCapabilities& GetHandCaps() const {
            return Caps;
        }

        virtual bool Update(ovrMobile* ovr, const double displayTimeInSeconds, const float dt) override;
        virtual void Render(std::vector<OVRFW::ovrDrawSurface>& surfaceList) override;

        virtual OVR::Vector3f GetRayOrigin() const override {
            return PointerPose.Transform(OVR::Vector3f(0.0f));
        }

        virtual void InitFromSkeletonAndMesh(
                ovrHandSkeleton* skeleton,
                ovrHandMesh* mesh) override;

        virtual ovrConfidence GetHandPoseConfidence() const override {
            return RealHandPose.HandConfidence;
        }
        virtual bool IsInSystemGesture() const override {
            return (
                    (InputStateHand.InputStateStatus & ovrInputStateHandStatus_SystemGestureProcessing) !=
                    0);
        }
        virtual bool IsMenuPressed() const override {
            return (InputStateHand.InputStateStatus & ovrInputStateHandStatus_MenuPressed) != 0;
        }

    private:
        OVR::Vector3f GetBonePosition(ovrHandBone bone) const;
        ovrInputHandCapabilities Caps;
        ovrHandPose RealHandPose;
        OVR::Vector3f PinchOffset;
    };
    class ovrInputDeviceStandardPointer : public ovrInputDeviceHandBase {
    public:
        ovrInputDeviceStandardPointer(
                const ovrInputStandardPointerCapabilities& caps,
                ovrHandedness hand)
                : ovrInputDeviceHandBase(hand), Caps(caps) {}

        virtual ~ovrInputDeviceStandardPointer() {}

        static ovrInputDeviceStandardPointer* Create(
                OVRFW::ovrAppl& app,
                const ovrInputStandardPointerCapabilities& capsHeader);

        virtual const ovrInputCapabilityHeader* GetCaps() const override {
            return &Caps.Header;
        }
        virtual ovrControllerType GetType() const override {
            return Caps.Header.Type;
        }
        virtual ovrDeviceID GetDeviceID() const override {
            return Caps.Header.DeviceID;
        }
        virtual const char* GetName() const override {
            return "StandardPointerDevice";
        }
        const ovrInputStandardPointerCapabilities& GetStandardDeviceCaps() const {
            return Caps;
        }

        bool Update(ovrMobile* ovr, const double displayTimeInSeconds, const float dt) override;

        virtual bool HasCapSimpleHaptics() const override {
            return GetStandardDeviceCaps().ControllerCapabilities &
                   ovrControllerCaps_HasSimpleHapticVibration;
        }
        virtual bool HasCapBufferedHaptics() const override {
            return GetStandardDeviceCaps().ControllerCapabilities &
                   ovrControllerCaps_HasBufferedHapticVibration;
        }
        virtual float GetHapticSampleDurationMS() const override {
            return GetStandardDeviceCaps().HapticSampleDurationMS;
        }
        virtual uint32_t GetHapticSamplesMax() const override {
            return GetStandardDeviceCaps().HapticSamplesMax;
        }

        bool IsPinching() const override {
            return InputStateStandardPointer.PointerStrength > 0.99f;
        }
        bool IsPointerValid() const override {
            return (InputStateStandardPointer.InputStateStatus &
                    ovrInputStateStandardPointerStatus_PointerValid) > 0;
        }
        bool IsMenuPressed() const override {
            return (InputStateStandardPointer.InputStateStatus &
                    ovrInputStateStandardPointerStatus_MenuPressed) > 0;
        }
        OVR::Vector3f GetRayOrigin() const override {
            return PointerPose.Transform(OVR::Vector3f(0.0f));
        }
        const DeviceHapticState& GetRequestedHapticsState() override {
            return RequestedHapticState;
        }
        void ResetHaptics(ovrMobile* ovr, float displayTimeInSeconds) override;

    protected:
        void UpdateHapticRequestedState(const ovrInputStateStandardPointer& inputState);

        DeviceHapticState RequestedHapticState;
        ovrInputStandardPointerCapabilities Caps;
        ovrInputStateStandardPointer InputStateStandardPointer;
    };
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
    class ovrInputDevice_TrackedRemote : public ovrInputDeviceBase {
    public:
        ovrInputDevice_TrackedRemote(const ovrInputTrackedRemoteCapabilities& caps)
                : ovrInputDeviceBase(), MinTrackpad(FLT_MAX), MaxTrackpad(-FLT_MAX), Caps(caps) {
            IsActiveInputDevice = false;
        }

        virtual ~ovrInputDevice_TrackedRemote() {}

        static ovrInputDeviceBase *Create(
                OVRFW::ovrAppl &app,
                OVRFW::OvrGuiSys &guiSys,
                OVRFW::VRMenu &menu,
                const ovrInputTrackedRemoteCapabilities &capsHeader);

        void UpdateHaptics(ovrMobile* ovr, const OVRFW::ovrApplFrameIn& vrFrame);
        virtual const ovrInputCapabilityHeader* GetCaps() const override {
            return &Caps.Header;
        }
        virtual ovrControllerType GetType() const override {
            return Caps.Header.Type;
        }
        virtual ovrDeviceID GetDeviceID() const override {
            return Caps.Header.DeviceID;
        }
        virtual const char* GetName() const override {
            return "TrackedRemote";
        }

        OVRFW::ovrArmModel::ovrHandedness GetHand() const {
            return (Caps.ControllerCapabilities & ovrControllerCaps_LeftHand) != 0
                   ? OVRFW::ovrArmModel::HAND_LEFT
                   : OVRFW::ovrArmModel::HAND_RIGHT;
        }

        const ovrTracking& GetTracking() const {
            return Tracking;
        }
        void SetTracking(const ovrTracking& tracking) {
            Tracking = tracking;
        }

        std::vector<OVRFW::ovrDrawSurface>& GetControllerSurfaces() {
            return Surfaces;
        }
        const ovrInputTrackedRemoteCapabilities& GetTrackedRemoteCaps() const {
            return Caps;
        }

        OVR::Vector2f MinTrackpad;
        OVR::Vector2f MaxTrackpad;
        bool IsActiveInputDevice;

    private:
        ovrInputTrackedRemoteCapabilities Caps;
        std::vector<OVRFW::ovrDrawSurface> Surfaces;
        ovrTracking Tracking;
        uint32_t HapticState;
        float HapticsSimpleValue;
    };

//==============================================================
// ovrInputDevice_StandardPointer
// Generic Input device for handling simple pointing and selecting interactions
    class ovrInputDevice_StandardPointer : public ovrInputDeviceBase {
    public:
        ovrInputDevice_StandardPointer(const ovrInputStandardPointerCapabilities& caps)
                : ovrInputDeviceBase(), Caps(caps) {}

        virtual ~ovrInputDevice_StandardPointer() {}

        virtual const ovrInputCapabilityHeader* GetCaps() const override {
            return &Caps.Header;
        }
        virtual ovrControllerType GetType() const override {
            return Caps.Header.Type;
        }
        virtual ovrDeviceID GetDeviceID() const override {
            return Caps.Header.DeviceID;
        }
        virtual const char* GetName() const override {
            return "StandardPointer";
        }

    private:
        ovrInputStandardPointerCapabilities Caps;
    };

//==============================================================
    class ovrControllerRibbon {
    public:



        ovrControllerRibbon() = delete;
        ovrControllerRibbon(
                const int numPoints,
                const float width,
                const float length,
                const OVR::Vector4f& color);
        ~ovrControllerRibbon();

        void Update(
                const OVR::Matrix4f& centerViewMatrix,
                const OVR::Vector3f& anchorPoint,
                const float deltaSeconds);

        OVRFW::ovrRibbon* Ribbon = nullptr;
        OVRFW::ovrPointList* Points = nullptr;
        OVRFW::ovrPointList* Velocities = nullptr;
        int NumPoints = 0;
        float Length = 1.0f;
    };

//=================================================================================================

}
