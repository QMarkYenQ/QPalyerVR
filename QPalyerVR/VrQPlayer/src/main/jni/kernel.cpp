/************************************************************************************



*************************************************************************************/
#include "kernel.h"
#include "kernel_ui.h"
#include "kernel_input_device.h"
#include "kernel_gl_tool.h"
#include "VrApi.h"

//
#include <vector>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <string>
#include <unordered_map>
#include <functional>
//
#include "OVR_FileSys.h"
#include "OVR_Std.h"
#include "OVR_Math.h"
#include "OVR_JSON.h"
//
#include "Model/SceneView.h"
#include "Misc/Log.h"
#include "Render/SurfaceRender.h"
#include "Render/DebugLines.h"
#include "Render/TextureAtlas.h"
#include "Render/BeamRenderer.h"
#include "Render/ParticleSystem.h"
#include "Render/PanelRenderer.h"
#include "Render/SurfaceRender.h"
#include "Render/SurfaceTexture.h"
#include "Render/GlTexture.h"
#include "Render/Egl.h"
#include "Render/GlGeometry.h"
//
#include "GUI/GuiSys.h"
#include "GUI/DefaultComponent.h"
#include "GUI/ActionComponents.h"
#include "GUI/VRMenu.h"
#include "GUI/VRMenuObject.h"
#include "GUI/VRMenuMgr.h"
#include "GUI/Reflection.h"

//-----------------------------------------------------------------------
#include "JniUtils.h"
//-------------------------------------------------
#define ENABLE_KER (1)


#define ENABLE_ALGO (0)


using OVR::Axis_X;
using OVR::Axis_Y;
using OVR::Axis_Z;
using OVR::Bounds3f;


using OVR::Vector4f;
static_assert(MAX_JOINTS == 64, "MAX_JOINTS != 64");

#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#include "EGL/egl.h"
#include "EGL/eglext.h"
#include "GLES/gl.h"
#define GL_GLEXT_PROTOTYPES
#include "GLES/glext.h"
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
//--------------------------------------------------------------------------------------------------
static const char* OculusTouchVertexShaderSrc = R"glsl(
attribute highp vec4 Position;
attribute highp vec3 Normal;
attribute highp vec3 Tangent;
attribute highp vec3 Binormal;
attribute highp vec2 TexCoord;

varying lowp vec3 oEye;
varying lowp vec3 oNormal;
varying lowp vec2 oTexCoord;

vec3 multiply( mat4 m, vec3 v )
{
return vec3(
m[0].x * v.x + m[1].x * v.y + m[2].x * v.z,
m[0].y * v.x + m[1].y * v.y + m[2].y * v.z,
m[0].z * v.x + m[1].z * v.y + m[2].z * v.z );
}

vec3 transposeMultiply( mat4 m, vec3 v )
{
return vec3(
m[0].x * v.x + m[0].y * v.y + m[0].z * v.z,
m[1].x * v.x + m[1].y * v.y + m[1].z * v.z,
m[2].x * v.x + m[2].y * v.y + m[2].z * v.z );
}

void main()
{
gl_Position = TransformVertex( Position );
vec3 eye = transposeMultiply( sm.ViewMatrix[VIEW_ID], -vec3( sm.ViewMatrix[VIEW_ID][3] ) );
oEye = eye - vec3( ModelMatrix * Position );
vec3 iNormal = Normal * 100.0f;
oNormal = multiply( ModelMatrix, iNormal );
oTexCoord = TexCoord;
}
)glsl";
static const char* OculusTouchFragmentShaderSrc = R"glsl(
uniform sampler2D Texture0;
uniform lowp vec3 SpecularLightDirection;
uniform lowp vec3 SpecularLightColor;
uniform lowp vec3 AmbientLightColor;

varying lowp vec3 oEye;
varying lowp vec3 oNormal;
varying lowp vec2 oTexCoord;

lowp vec3 multiply( lowp mat3 m, lowp vec3 v )
{
return vec3(
m[0].x * v.x + m[1].x * v.y + m[2].x * v.z,
m[0].y * v.x + m[1].y * v.y + m[2].y * v.z,
m[0].z * v.x + m[1].z * v.y + m[2].z * v.z );
}

void main()
{
lowp vec3 eyeDir = normalize( oEye.xyz );
lowp vec3 Normal = normalize( oNormal );

lowp vec3 reflectionDir = dot( eyeDir, Normal ) * 2.0 * Normal - eyeDir;
lowp vec4 diffuse = texture2D( Texture0, oTexCoord );
lowp vec3 ambientValue = diffuse.xyz * AmbientLightColor;

lowp float nDotL = max( dot( Normal , SpecularLightDirection ), 0.0 );
lowp vec3 diffuseValue = diffuse.xyz * SpecularLightColor * nDotL;

lowp float specularPower = 1.0f - diffuse.a;
specularPower = specularPower * specularPower;

lowp vec3 H = normalize( SpecularLightDirection + eyeDir );
lowp float nDotH = max( dot( Normal, H ), 0.0 );
lowp float specularIntensity = pow( nDotH, 64.0f * ( specularPower ) ) * specularPower;
lowp vec3 specularValue = specularIntensity * SpecularLightColor;

lowp vec3 controllerColor = diffuseValue + ambientValue + specularValue;
gl_FragColor.xyz = controllerColor;
gl_FragColor.w = 1.0f;
}
)glsl";

//--------------------------------------------------------------------------------------------------
    static const char* ImageExternalDirectives = R"glsl(
        #extension GL_OES_EGL_image_external : enable
        #extension GL_OES_EGL_image_external_essl3 : enable
    )glsl";

    static const char* PanoramaFragmentShaderSrc = R"glsl(
        uniform samplerExternalOES Texture0;
        uniform lowp vec4 UniformColor;
        varying highp vec3 oTexCoord;

        void main()
        {

            float gain = UniformColor.r;
            float gamma = UniformColor.w;


            if( gain < 0.){
                gain = 0.;
            }
            if( abs( gamma ) < 1. ) {
                gamma = 1.;
            }
            if( gamma < 0. ) {
                gamma = 1. /  abs( gamma );
            }


            if( oTexCoord.z >0.5 ){


                if( oTexCoord.x  >0.5 &&  oTexCoord.x< 1. ){
                    gl_FragColor.rgb = vec3( gain ) * pow( texture2D( Texture0, oTexCoord.xy ).rgb,  vec3( gamma ) );
                    gl_FragColor.w =1.;

                }
                else{
                    gl_FragColor =vec4(0.,0.,0.,1.);
                }
            }else{
               if( oTexCoord.x  <0.5  &&   oTexCoord.x > 0. ){
                    gl_FragColor.rgb = vec3( gain ) * pow( texture2D( Texture0, oTexCoord.xy ).rgb,  vec3( gamma ) );
                    gl_FragColor.w =1.;
                }
                else{
                    gl_FragColor =vec4(0.,0.,0.,1.);
                }
            }

        }
    )glsl";

    static const char* PanoramaVertexShaderSrc = R"glsl(
        uniform highp mat4 Texm[NUM_VIEWS];
        attribute vec4 Position;
        attribute vec2 TexCoord;
        varying  highp vec3 oTexCoord;
        void main()
        {
        gl_Position = TransformVertex( Position );
        oTexCoord = vec3( Texm[VIEW_ID] * vec4( TexCoord, VIEW_ID, 1 ) );
        }
    )glsl";
//--------------------------------------------------------------------------------------------------

#if(ENABLE_ALGO)

#endif


//--------------------------------------------------------------------------------------------------
namespace OVRFWQ
{

    OVRFW::ovrSurfaceRender g_SurfaceRender;
    std::vector<ovrInputDeviceBase*> g_InputDevices;
//==================================================================================================
    static unsigned int MODE_VIDEO_2D =11 ;
    static unsigned int MODE_VIDEO_3D =335 ;
//==================================================================================================
//                             MENU
//==================================================================================================
    ovrControllerGUI* g_Menu_Player= nullptr;
    ovrControllerGUI* g_Menu_Folder= nullptr;
    ovrControllerGUI* g_Menu_Device= nullptr;
    ovrControllerGUI* g_Menu_Media= nullptr;
    ovrControllerGUI*  g_Menu_Screen= nullptr;
    ovrControllerGUI* g_Menu_M= nullptr;
    ovrControllerGUI* g_Menu_C= nullptr;
    ovrControllerGUI* g_Menu_ToShow= nullptr;
    static bool is_show_list = false;
//==================================================================================================
//                             GL CANVAS
//==================================================================================================
#include "android/hardware_buffer.h"
    AHardwareBuffer* g_HardwareBuf;
/////////////////////////////////////////////////////////////////////////////
    KERGL_Canvas glavmCanvasH;
    KERGL_Canvas glavmCanvasV;
/////////////////////////////////////////////////////////////////////////////
    std::unique_ptr<OVRFW::SurfaceTexture> g_MovieTexture;
    static int g_CurrentMovieWidth = 4096;
    static int g_CurrentMovieHeight = 4096;
    bool g_IsPaused = true;
    bool g_WasPausedOnUnMount =true;
/////////////////////////////////////////////////////////////////////////////
    static int g_CurrentPhotoWidth = 0;
    static int g_CurrentPhotoHeight = 0;
    int g_On = 1;
    int g_Move = 1;
    Vector3f g_move_pos( 0,0,2);
    Vector3f g_move_pos2( 0,0,2);
    Vector3f g_move_xyz( 0,3.14159/2.,2);

    Vector3f g_move_pos3( 0,0,0);
//==================================================================================================

//==================================================================================================
    OVRFW::ModelFile* g_SceneModel;
    OVRFW::OvrSceneView g_Scene;
    OVRFW::ovrTextureAtlas* g_SpriteAtlas;
    OVRFW::ovrParticleSystem* g_ParticleSystem;
    OVRFW::ovrTextureAtlas* g_BeamAtlas;
    OVRFW::ovrBeamRenderer* g_BeamRenderer;
    OVRFW::ModelFile* g_ControllerModelOculusTouchLeft;
    OVRFW::ModelFile* g_ControllerModelOculusTouchRight;
    /// Axis rendering
    OVRFW::ovrSurfaceDef g_AxisSurfaceDef;
    OVRFW::ovrDrawSurface g_AxisSurface;
    std::vector<OVR::Matrix4f> g_TransformMatrices;
    std::vector<ovrInputDeviceHandBase*> g_EnabledInputDevices;
    OVRFW::GlBuffer g_AxisUniformBuffer;
    OVRFW::GlProgram g_ProgAxis;

//==================================================================================================
//                            GL Panorama
//==================================================================================================
    OVRFW::GlProgram g_ProgOculusTouch;
    OVR::Vector3f g_SpecularLightDirection;
    OVR::Vector3f g_SpecularLightColor;
    OVR::Vector3f g_AmbientLightColor;
    //
    OVRFW::GlProgram g_PanoramaProgram;
    // panorama vars

    OVRFW::GlProgram g_TexturedMvpProgram;
    OVRFW::ovrSurfaceDef g_GlobeSurfaceDef;
    OVRFW::GlTexture g_GlobeProgramTexture;
#if(ENABLE_ALGO)

#endif
//==================================================================================================
//                             GL CANVAS
//==================================================================================================
    ovrQPlayerAppl* theApp = nullptr;
    OVRFW::ovrFileSys* g_FileSys;
    OVRFW::OvrDebugLines* g_DebugLines;
    OVRFW::OvrGuiSys::SoundEffectPlayer* g_SoundEffectPlayer;
    OVRFW::OvrGuiSys* g_GuiSys;
    OVRFW::ovrLocale* g_Locale;
    class OVRFW::OvrGuiSys& ovrQPlayerAppl::GetGuiSys(){return *g_GuiSys;}
    class OVRFW::ovrLocale& ovrQPlayerAppl::GetLocale(){return *g_Locale;}
//==================================================================================================

void ResetLaserPointer( ovrInputDeviceHandBase& trDevice );
bool IsDeviceTypeEnabled( ovrInputDeviceBase& device);
void EnumerateInputDevices( std::vector<ovrInputDeviceBase*> &devices, ovrQPlayerAppl& app);
void OnDeviceDisconnected( std::vector<ovrInputDeviceBase*> &devices, ovrQPlayerAppl& app, ovrDeviceID deviceID);
//--------------------------------------------------------------------------------------------------
static void SetObjectText(OVRFW::OvrGuiSys& guiSys, OVRFW::VRMenu* menu, char const* name, char const* fmt, ...)
{
    OVRFW::VRMenuObject* obj = menu->ObjectForName(guiSys, name);
    if (obj != nullptr) {
        char text[1024];
        va_list argPtr;
        va_start(argPtr, fmt);
        OVR::OVR_vsprintf(text, sizeof(text), fmt, argPtr);
        va_end(argPtr);
        obj->SetText(text);
    }
}
static void SetObjectColor( OVRFW::OvrGuiSys& guiSys,  OVRFW::VRMenu* menu, char const* name, Vector4f const& color)
{
    OVRFW::VRMenuObject* obj = menu->ObjectForName(guiSys, name);
    if (obj != nullptr) {
        obj->SetSurfaceColor(0, color);

    }
}
static void SetSurfaceVisible( OVRFW::OvrGuiSys& guiSys,  OVRFW::VRMenu* menu, char const* name, bool color)
{
    OVRFW::VRMenuObject* obj = menu->ObjectForName(guiSys, name);
    if (obj != nullptr) {
        obj->SetSurfaceVisible(0, color);

    }
}
static void SetObjectTexture( OVRFW::OvrGuiSys& guiSys,  OVRFW::VRMenu* menu, char const* name,OVRFW::GlTexture ProgramTexture)
{
    OVR::Vector4f color( 0.4f,0.4f,0.4f,1.0f);
    OVRFW::VRMenuObject* obj = menu->ObjectForName(guiSys, name);
    if (obj != nullptr) {
        obj->SetSurfaceColor(0,color);
        obj->SetSurfaceTexture(0,0,OVRFW::SURFACE_TEXTURE_DIFFUSE, ProgramTexture);

    }

}
static void SetObjectVisible(OVRFW::OvrGuiSys& guiSys, OVRFW::VRMenu* menu, char const* name, const bool visible)
{
    OVRFW::VRMenuObject* obj = menu->ObjectForName(guiSys, name);
    if (obj != nullptr) {
        obj->SetVisible(visible);

    }
}

//----------------------------------------------------------------------------------------------
Matrix4f VTexmForVideo(const int eye)
{
    return eye ? Matrix4f(
            1.0f,0.0f,0.0f,0.25f,
            0.0f,1.0f,0.0f,0.0f,
            0.0f,0.0f,1.0f,0.0f,
            0.0f,0.0f,0.0f,1.0f)
               : Matrix4f(
                    1.0f,0.0f,0.0f,-0.25f,
                    0.0f,1.0f,0.0f,0.0f,
                    0.0f,0.0f,1.0f,0.0f,
                    0.0f,0.0f,0.0f,1.0f);
}

float g_disparity_setting = 1;
float g_disparity_measurement= 1;
float g_gamma = 0.2;
float g_time_interval =0;
int g_frame_count =0;
bool g_show_menu = false;

float g_joystick_x =0;
float g_joystick_y =0;

Vector3f g_targetEnd (0,0,0);

Matrix4f g_center_view = OVR::Matrix4f();
Matrix4f g_center_view_real = OVR::Matrix4f();
Matrix4f g_center_view_left = OVR::Matrix4f();
Matrix4f g_center_view_right = OVR::Matrix4f();


inline Vector3f GetViewMatrixPosition(Matrix4f const& m) {
    return m.Inverted().GetTranslation();
}

inline Vector3f GetViewMatrixForward(Matrix4f const& m) {
    return Vector3f(-m.M[2][0], -m.M[2][1], -m.M[2][2]).Normalized();
}
inline Vector3f GetViewMatrixUp(Matrix4f const& m) {
    return Vector3f(-m.M[1][0], -m.M[1][1], -m.M[1][2]).Normalized();
}

inline Vector3f GetViewMatrixRight(Matrix4f const& m) {
    return Vector3f(-m.M[0][0], -m.M[0][1], -m.M[0][2]).Normalized();
}

OVRFW::ovrApplFrameOut ovrQPlayerAppl::AppFrame( const OVRFW::ovrApplFrameIn& vrFrame)
{
#if(ENABLE_KER)

    Vector3f pp = GetViewMatrixPosition(g_center_view_real);
    Vector3f fw = GetViewMatrixForward(g_center_view_real);
    const ovrJava* java = reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi());
    //==============================================================================================
    //
    //==============================================================================================

    if(   g_Menu_Screen!= nullptr ) g_Menu_Screen->Update(vrFrame);
    if(  g_Menu_Player!= nullptr )g_Menu_Player->Update(vrFrame);
    if(  g_Menu_C!= nullptr )g_Menu_C->Update(vrFrame);
    if(  g_Menu_M!= nullptr )g_Menu_M->Update(vrFrame);
    if(  g_Menu_Folder!= nullptr )g_Menu_Folder->Update(vrFrame);
    if(  g_Menu_Device!= nullptr )g_Menu_Device->Update(vrFrame);
    if(  g_Menu_Media!= nullptr )g_Menu_Media->Update(vrFrame);
    //==============================================================================================
    //
    //==============================================================================================
    bool hasActiveController = false;
    int iActiveInputDeviceID;
    vrapi_GetPropertyInt( java, VRAPI_ACTIVE_INPUT_DEVICE_ID, &iActiveInputDeviceID );

    OVRFWQ::EnumerateInputDevices( g_InputDevices, *theApp );


    // for each device, query its current tracking state and input state
    // it's possible for a device to be removed during this loop, so we go through it backwards
    for (int i = (int)g_InputDevices.size() - 1; i >= 0; --i) {
        ovrInputDeviceBase* device = g_InputDevices[i];
        if (device == nullptr) continue;
        if ( device->GetDeviceID() == ovrDeviceIdType_Invalid ) continue;

        bool IsActiveInputDevice = (device->GetDeviceID() ==  (uint32_t)iActiveInputDeviceID );
       if ( device->GetType() == ovrControllerType_TrackedRemote && IsActiveInputDevice)

        {
            // ovrInputDevice_TrackedRemote
            ovrInputDevice_TrackedRemote& trDevice = *static_cast<ovrInputDevice_TrackedRemote*>(device);

            if ( device->GetDeviceID() != ovrDeviceIdType_Invalid) {
                ovrTracking remoteTracking;
                ovrResult result = vrapi_GetInputTrackingState(
                        GetSessionObject(),  device->GetDeviceID(), vrFrame.PredictedDisplayTime, &remoteTracking);
                if (result != ovrSuccess) {
                    OVRFWQ::OnDeviceDisconnected(g_InputDevices, *theApp,  device->GetDeviceID());
                    continue;
                }
                // trDevice.SetTracking(remoteTracking);
                ovrDeviceID deviceID = trDevice.GetDeviceID();
                const OVRFW::ovrArmModel::ovrHandedness controllerHand = trDevice.GetHand();

                ovrInputStateTrackedRemote remoteInputState;
                remoteInputState.Header.ControllerType = trDevice.GetType();
                result = vrapi_GetCurrentInputState(GetSessionObject(), deviceID, &remoteInputState.Header);
                if (result != ovrSuccess) {
                    ALOG("ERROR %i getting remote input state!", result);
                    OVRFWQ::OnDeviceDisconnected( g_InputDevices, *theApp, deviceID);
                }else{
                    g_joystick_x =  remoteInputState.Joystick.x;
                    g_joystick_y =  remoteInputState.Joystick.y;
                  //  ALOG(" remoteInputState %f, %f", g_joystick_x, g_joystick_y);
                }
            }
        }
    }




   // g_joystick_x =  0;
    //g_joystick_y = 0;

    float eye_angle[64];
    vrFrame.HeadPose.ToArray( &eye_angle[0] );
    const float frameTime = vrFrame.DeltaSeconds;
    g_time_interval  = g_time_interval + frameTime;
    g_frame_count += 1;


    g_move_pos3.x =  pp.x;
    g_move_pos3.z = pp.y   +1.4;
    g_move_pos3.y =  pp.z;

    //g_center_view_right =
    if( g_show_menu )
    {
        if(g_Move){
            if(  abs(g_joystick_x +g_joystick_y )>0.1 ) {
                if (abs(g_joystick_x) > abs(g_joystick_y)) {
                    g_move_xyz.x += 0.03 * g_joystick_x;
                } else {
                    g_move_xyz.y += 0.03 * g_joystick_y;
                }
            }
            if( g_move_xyz.y   <0 ) g_move_xyz.y =0  ;
            if( g_move_xyz.y   >3.14 ) g_move_xyz.y =3.14;
            if( g_move_xyz.x   < -3.14/2. ) g_move_xyz.x =-3.14/2. ;
            if( g_move_xyz.x   >3.14/2. ) g_move_xyz.x =3.14/2.;

            g_move_pos.x = g_move_xyz.z *sin(g_move_xyz.y) *sin(g_move_xyz.x);
            g_move_pos.z = g_move_xyz.z *sin(g_move_xyz.y) *cos(g_move_xyz.x);
            g_move_pos.y = -g_move_xyz.z *cos(g_move_xyz.y)  + pp.y;

            float ss = -0.3;
            g_move_pos2.x = (g_move_xyz.z +ss ) *sin(g_move_xyz.y) *sin(g_move_xyz.x);
            g_move_pos2.z = (g_move_xyz.z +ss )  *sin(g_move_xyz.y) *cos(g_move_xyz.x);
            g_move_pos2.y = -(g_move_xyz.z +ss ) *cos(g_move_xyz.y)  + pp.y;

        }



        if ( !g_Move&&vrFrame.Clicked2(ovrButton_Trigger ) ) {

            g_gamma += 0.03*g_joystick_x;

            if( g_gamma >1.2) g_gamma = 1.2;
            if( g_gamma < -1.2) g_gamma = -1.2;


        }
        if (  !g_Move&&vrFrame.Clicked2(ovrButton_GripTrigger) ) {
            g_disparity_setting  -= 0.02*g_joystick_y;



          //  if( g_disparity_setting >4) g_disparity_setting = 4;
            if( g_disparity_setting <0.0) g_disparity_setting = 0.0;

        }



    }else{




        if(!vrFrame.Clicked2(ovrButton_Trigger)  && !vrFrame.Clicked2(ovrButton_GripTrigger) ) {
            if(  abs(g_joystick_x +g_joystick_y )>0.1) {
                if (abs(g_joystick_x) > abs(g_joystick_y)) {

                    Matrix4f x = Matrix4f::RotationY(g_joystick_x * 0.05);
                    g_center_view_left = g_center_view_left * x;


                } else {
                    Matrix4f x = Matrix4f::RotationX(-g_joystick_y * 0.05);
                    g_center_view_left = x * g_center_view_left;
                }
            }
        }
    }

//-------------------

    if(  vrFrame.Clicked( ovrButton_GripTrigger)  ){
        g_show_menu =! g_show_menu;

    }
//-----------------------
    if( 1 == g_On)
        g_disparity_setting = 0.1*g_disparity_measurement +0.9 *g_disparity_setting;
    /// Simple Play/Pause toggle
    if ( vrFrame.Clicked(ovrButton_A) ) {
        //g_eye_angle_shift[0] = ang_yzx0[0] /0.35;

        float ax ,ay ,az;
        g_center_view_left.ToEulerAngles<Axis_Z , Axis_Y ,Axis_X, OVR::Rotate_CCW, OVR::Handed_R>(&az,&ay,&ax);
        float ax2 ,ay2 ,az2;
        g_center_view_real.ToEulerAngles<Axis_Z , Axis_X ,Axis_Y, OVR::Rotate_CCW, OVR::Handed_R>(&az2,&ax2,&ay2);
        //g_center_view_right =  Matrix4f::RotationY(  - ay2 );

        if( ax >( 3.14/4)) ax =( 3.14/4);

        if( ax < ( - 3.14/4)) ax =(- 3.14/4);
        g_center_view_left =  Matrix4f::RotationX( ax  )  ;


        g_disparity_setting =0;
        g_On =1;

        g_move_xyz= Vector3f( 0,3.14159/2.,2);


    }

    if ( vrFrame.Clicked(ovrButton_B) ) {
        g_On =0;
       if( 0 == g_On)      g_disparity_setting =0;

    }
    /// Check for mount/unmount
    if (vrFrame.HeadsetUnMounted()) {
        g_WasPausedOnUnMount = g_IsPaused;
        PauseVideo();
    }
    if (vrFrame.HeadsetMounted() && false == g_WasPausedOnUnMount) {
        ResumeVideo();
    }
#endif

    return OVRFW::ovrApplFrameOut();
}

void ovrQPlayerAppl::RenderRunningControl( const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out)
{
    //----------------------------------------------------------------------------------------------
    const ovrJava* java = reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi());
    bool hasActiveController = false;
    int iActiveInputDeviceID;
    vrapi_GetPropertyInt( java, VRAPI_ACTIVE_INPUT_DEVICE_ID, &iActiveInputDeviceID );
    //==============================================================================================
    OVRFWQ::EnumerateInputDevices( g_InputDevices, *theApp );

    // for each device, query its current tracking state and input state
    // it's possible for a device to be removed during this loop, so we go through it backwards
    for (int i = (int)g_InputDevices.size() - 1; i >= 0; --i) {
        ovrInputDeviceBase* device = g_InputDevices[i];
        if (device == nullptr) continue;
        if ( device->GetDeviceID() == ovrDeviceIdType_Invalid ) continue;
        auto deviceType = device->GetType();
        if (device->GetType() == ovrControllerType_TrackedRemote)
        {
            // ovrInputDevice_TrackedRemote
            ovrInputDevice_TrackedRemote& trDevice =
                    *static_cast<ovrInputDevice_TrackedRemote*>(device);

            if ( device->GetDeviceID() != ovrDeviceIdType_Invalid) {
                ovrTracking remoteTracking;
                ovrResult result = vrapi_GetInputTrackingState(
                        GetSessionObject(),  device->GetDeviceID(), in.PredictedDisplayTime, &remoteTracking);
                if (result != ovrSuccess) {
                    OVRFWQ::OnDeviceDisconnected(g_InputDevices, *theApp,  device->GetDeviceID());
                    continue;
                }
                trDevice.SetTracking(remoteTracking);

            }
        }
        //------------------------------------------------------------------------------------------
        if (deviceType == ovrControllerType_TrackedRemote || deviceType == ovrControllerType_Hand ||
            deviceType == ovrControllerType_StandardPointer)
        {
            ovrInputDeviceHandBase& trDevice =
                    *static_cast<ovrInputDeviceHandBase*>(device);
            if ( device->GetDeviceID() != ovrDeviceIdType_Invalid) {
                if ( false ==
                     trDevice.Update(GetSessionObject(), in.PredictedDisplayTime, in.DeltaSeconds)) {
                    OVRFWQ::OnDeviceDisconnected( g_InputDevices, *theApp,  device->GetDeviceID());
                    continue;
                }
            }
        }
    }
    //==============================================================================================
    // Establish which devices are active before we iterate; since the button action handlers
    // may change this (resulting in the button being clicked twice)
    g_EnabledInputDevices.clear();
    for (int i = (int)g_InputDevices.size() - 1; i >= 0; --i) {
        ovrInputDeviceBase* device = g_InputDevices[i];
        if (device == nullptr) {
            ALOGW("RenderRunningFrame - device == nullptr ");
            assert(false); // this should never happen!
            continue;
        }
        ovrDeviceID deviceID = device->GetDeviceID();
        if (deviceID == ovrDeviceIdType_Invalid) {
            ALOGW("RenderRunningFrame - deviceID == ovrDeviceIdType_Invalid ");
            assert(deviceID != ovrDeviceIdType_Invalid);
            continue;
        }

        if (device->GetType() != ovrControllerType_TrackedRemote &&
            device->GetType() != ovrControllerType_Hand &&
            device->GetType() != ovrControllerType_StandardPointer) {
            continue;
        }
        ovrInputDeviceHandBase* trDevice = static_cast<ovrInputDeviceHandBase*>(device);

        if (!OVRFWQ::IsDeviceTypeEnabled(*device)) {
            ResetLaserPointer(*trDevice);
            trDevice->ResetHaptics(GetSessionObject(), in.PredictedDisplayTime);
            continue;
        }
        g_EnabledInputDevices.push_back(trDevice);
    }
    //==============================================================================================
    // Clean all hit

    //==============================================================================================
    // loop through all devices to update controller arm models and place the pointer for the
    // dominant hand
    for (auto devIter = g_EnabledInputDevices.begin(); devIter != g_EnabledInputDevices.end(); devIter++) {
        auto& trDevice = **devIter;

        bool updateLaser = trDevice.IsPinching();
        bool renderLaser = trDevice.IsPointerValid();

        if (trDevice.IsMenuPressed() && trDevice.GetType() == ovrControllerType_StandardPointer) {

        }

        trDevice.UpdateHaptics(GetSessionObject(), in.PredictedDisplayTime);

        if (renderLaser) {
            Vector3f pointerStart(0.0f);
            Vector3f pointerEnd(0.0f);
            bool LaserHit = false;
            pointerStart = trDevice.GetRayOrigin();
            pointerEnd = trDevice.GetRayEnd();

            Vector3f const pointerDir = (pointerEnd - pointerStart).Normalized();
            Vector3f targetEnd = pointerStart + pointerDir * (updateLaser ? 10.0f : 0.075f);

            OVRFW::HitTestResult hit = g_GuiSys->TestRayIntersection(pointerStart, pointerDir);
            LaserHit = hit.HitHandle.IsValid() && (trDevice.GetHandPoseConfidence() == ovrConfidence_HIGH);
            trDevice.SetLastHitHandle(hit.HitHandle);



            //     int iActiveInputDeviceID;
            vrapi_GetPropertyInt(java, VRAPI_ACTIVE_INPUT_DEVICE_ID, &iActiveInputDeviceID);
           uint32_t ActiveInputDeviceID = (uint32_t)iActiveInputDeviceID;
            bool IsActiveInputDevice = (trDevice.GetDeviceID() ==  ActiveInputDeviceID);


            bool IsDominantHand =  trDevice.IsLeftHand()  == ( vrapi_GetSystemPropertyInt(java, VRAPI_SYS_PROP_DOMINANT_HAND) == VRAPI_HAND_LEFT );

            if (LaserHit)
            {
                g_targetEnd.x = hit.uv.x;
                g_targetEnd.y= hit.uv.y;


                targetEnd = pointerStart + hit.RayDir * hit.t - pointerDir * 0.025f;

                //   g_targetEnd = targetEnd;
                OVRFW::VRMenuObject* hitObject = g_GuiSys->GetVRMenuMgr().ToObject(hit.HitHandle);
                if (hitObject != nullptr) {
                    if (updateLaser) {
                        //   hitObject->SetSurfaceColor(0, MENU_HIGHLIGHT_COLOR);
                        pointerEnd = targetEnd;


                    }

                }
            }
// IsPinching


            int status_click = 0;

            if( trDevice.Clicked() )
                status_click  = 2;
            else if( trDevice.IsPinching() )
                status_click  = 1;
            else
                status_click  = 0;

            if ( g_Menu_Screen!= nullptr)   g_Menu_Screen->AddHitTestRay2(pointerStart, pointerEnd, status_click );

            if (g_Menu_C != nullptr)  g_Menu_C->AddHitTestRay2(pointerStart, pointerEnd, status_click );

            if (g_Menu_Player != nullptr)  g_Menu_Player->AddHitTestRay2(pointerStart, pointerEnd, status_click );
            if (g_Menu_Folder != nullptr)  g_Menu_Folder->AddHitTestRay2(pointerStart, pointerEnd, status_click );
            if (g_Menu_Device != nullptr)  g_Menu_Device->AddHitTestRay2(pointerStart, pointerEnd, status_click );
            if (g_Menu_Media != nullptr)  g_Menu_Media->AddHitTestRay2(pointerStart, pointerEnd, status_click );
            //--------------------------------------------------------------------------------------
            Vector4f ConfidenceLaserColor_0, ConfidenceLaserColor_1;
            float confidenceAlpha = trDevice.GetHandPoseConfidence() == ovrConfidence_HIGH ? 1.0f : 0.1f;
            ConfidenceLaserColor_0.x = 1.0f - confidenceAlpha;
            ConfidenceLaserColor_0.y = confidenceAlpha;
            ConfidenceLaserColor_0.z = 0.0f;
            ConfidenceLaserColor_0.w = confidenceAlpha;


            ConfidenceLaserColor_1.x = 1.0f - confidenceAlpha;
            ConfidenceLaserColor_1.y =  1.0f;
            ConfidenceLaserColor_1.z = confidenceAlpha;
            ConfidenceLaserColor_1.w = confidenceAlpha;



            OVRFW::ovrBeamRenderer::handle_t& LaserPointerBeamHandle = trDevice.GetLaserPointerBeamHandle();
            OVRFW::ovrParticleSystem::handle_t& LaserPointerParticleHandle = trDevice.GetLaserPointerParticleHandle();
            if (!LaserPointerBeamHandle.IsValid()) {
                // initi
                LaserPointerBeamHandle = g_BeamRenderer->AddBeam(
                        in,*g_BeamAtlas,0,0.032f,
                        pointerStart, pointerEnd, ConfidenceLaserColor_0,
                        OVRFW::ovrBeamRenderer::LIFETIME_INFINITE
                );
            } else {
                float  width_v =0;
                Vector4f ConfidenceLaserColor_v;



                if( IsActiveInputDevice ){

                    ConfidenceLaserColor_v.x = 1.0f - confidenceAlpha;
                    ConfidenceLaserColor_v.y =  confidenceAlpha;
                    ConfidenceLaserColor_v.z = 0.3f;
                    ConfidenceLaserColor_v.w = confidenceAlpha;

                }else{

                    ConfidenceLaserColor_v.x = 1.0f - confidenceAlpha;
                    ConfidenceLaserColor_v.y =  confidenceAlpha;
                    ConfidenceLaserColor_v.z  = 0.2f;
                    ConfidenceLaserColor_v.w = confidenceAlpha;

                }
                if( IsDominantHand ) {

                    width_v = 0.024f;
                }else{

                    width_v = 0.018f;
                }


                g_BeamRenderer->UpdateBeam(
                        in, LaserPointerBeamHandle, *g_BeamAtlas,
                        0,
                        width_v,pointerStart, pointerEnd, ConfidenceLaserColor_v
                );

            }

            if (!LaserPointerParticleHandle.IsValid()) {
                if (LaserHit) {
                    LaserPointerParticleHandle = g_ParticleSystem->AddParticle(
                            in,targetEnd,
                            0.0f,
                            Vector3f(0.0f),
                            Vector3f(0.0f),
                            ConfidenceLaserColor_0,
                            OVRFW::ovrEaseFunc::NONE,0.0f,0.1f,0.1f,0);
                }
            } else {

                if (LaserHit) {

                    Vector4f ConfidenceLaserColor_v;
                    if( IsActiveInputDevice ){

                        ConfidenceLaserColor_v.x = 1.0f - confidenceAlpha;
                        ConfidenceLaserColor_v.y =  confidenceAlpha;
                        ConfidenceLaserColor_v.z = 0.3f;
                        ConfidenceLaserColor_v.w = confidenceAlpha;

                    }else{

                        ConfidenceLaserColor_v.x = 1.0f - confidenceAlpha;
                        ConfidenceLaserColor_v.y =  confidenceAlpha;
                        ConfidenceLaserColor_v.z  = 0.2f;
                        ConfidenceLaserColor_v.w = confidenceAlpha;

                    }


                    g_ParticleSystem->UpdateParticle(
                            in, LaserPointerParticleHandle, targetEnd,
                            0.0f,Vector3f(0.0f),Vector3f(0.0f),
                            ConfidenceLaserColor_v,OVRFW::ovrEaseFunc::NONE,
                            0.0f,0.1f,0.1f,0);
                } else {
                        g_ParticleSystem->RemoveParticle(LaserPointerParticleHandle);
                        LaserPointerParticleHandle.Release();
                }

            }
        } else {
            ResetLaserPointer(trDevice);
        }
    }
    //==============================================================================================

    g_BeamRenderer->Frame(in, out.FrameMatrices.CenterView, *g_BeamAtlas);
    g_ParticleSystem->Frame(in, g_SpriteAtlas, out.FrameMatrices.CenterView);

    // render bones first
    const Matrix4f projectionMatrix;
    g_ParticleSystem->RenderEyeView( projectionMatrix, projectionMatrix, out.Surfaces);
    g_BeamRenderer->RenderEyeView( projectionMatrix, projectionMatrix, out.Surfaces);
    int axisSurfaces = 0;
    // add the controller model surfaces to the list of surfaces to render
    for (int i = 0; i < (int)g_InputDevices.size(); ++i) {
        ovrInputDeviceBase* device = g_InputDevices[i];
        if (device == nullptr) {
            assert(false); // this should never happen!
            continue;
        }
        if (device->GetType() != ovrControllerType_TrackedRemote &&
            device->GetType() != ovrControllerType_Hand &&
            device->GetType() != ovrControllerType_StandardPointer) {
            continue;
        }
        if (!OVRFWQ::IsDeviceTypeEnabled(*device)) {
            continue;
        }
        ovrInputDeviceHandBase& trDevice = *static_cast<ovrInputDeviceHandBase*>(device);

        //     const Posef& handPose = trDevice.GetHandPose();
        //    const Matrix4f matDeviceModel = trDevice.GetModelMatrix(handPose);
        //    g_TransformMatrices[axisSurfaces++] = OVR::Matrix4f(handPose);
        //   g_TransformMatrices[axisSurfaces++] = matDeviceModel;
        //  g_TransformMatrices[axisSurfaces++] = trDevice.GetPointerMatrix();

        bool renderHand = (trDevice.GetHandPoseConfidence() == ovrConfidence_HIGH);
        if (renderHand) {
            trDevice.Render(out.Surfaces);
        }
    }

}

//--------------------------------------------------------------------------------------------------
Posef CalcMenuPositionMenu(Matrix4f const& viewMatrix , Vector3f pos)
{
    const Matrix4f invViewMatrix = viewMatrix.Inverted();
    const Vector3f viewPos(GetViewMatrixPosition(viewMatrix));
    const Vector3f viewFwd(GetViewMatrixForward(viewMatrix));
    const Vector3f viewRight(GetViewMatrixRight(viewMatrix));
    const Vector3f viewUp(GetViewMatrixUp(viewMatrix));
    //Matrix4f ak = Matrix4f::RotationZ(az)*Matrix4f::RotationX(ax)*Matrix4f::RotationY(ay);
    float MenuDistance = pos.Length();
    Vector3f up(0.0f, 1.0f, 0.0f);
    Vector3f fwd(0.0f, 0.0f, -1.0f);
    Vector3f rgh(1.0f, 0.0f, 0.0f);
    pos = pos.Normalized();
    Vector3f pos2 = pos;
    pos2.x = 0;
    // spawn directly in front
    Quatf rotation_phi(rgh, atan2(pos.y, pos.z)  );
    Quatf rotation_thea(up, atan2(-pos.x, pos2.Length()));
    Quatf rotation_rot;
    pos = pos * MenuDistance;
    Vector3f position(fwd * pos.z + up * pos.y + rgh * pos.x);

    Matrix4f horizontalViewMatrix;
    Quatf rotation;
    if (abs( pos.x ) < 0.3) {
        horizontalViewMatrix = Matrix4f::LookAtRH(viewPos, position, rgh);
        rotation =Quatf (-fwd, 3.14f / 2.);
    }
    else {
        rotation = Quatf (-fwd, 0);
        horizontalViewMatrix = Matrix4f::LookAtRH(viewPos, position, up);
    }

    horizontalViewMatrix.Transpose(); // transpose because we want the rotation opposite of where we're looking

    Quatf viewRot(horizontalViewMatrix);
    Quatf fullRotation =  viewRot * rotation;

    return Posef(fullRotation, position);
}

Posef CalcMenuPosition2D(Matrix4f const& viewMatrix , Vector3f pos)
    {
        const Matrix4f invViewMatrix = viewMatrix.Inverted();
        const Vector3f viewPos(GetViewMatrixPosition(viewMatrix));
        const Vector3f viewFwd(GetViewMatrixForward(viewMatrix));
        const Vector3f viewRight(GetViewMatrixRight(viewMatrix));
        const Vector3f viewUp(GetViewMatrixUp(viewMatrix));
        //Matrix4f ak = Matrix4f::RotationZ(az)*Matrix4f::RotationX(ax)*Matrix4f::RotationY(ay);
        float MenuDistance = pos.Length();
        Vector3f up(0.0f, 1.0f, 0.0f);
        Vector3f fwd(0.0f, 0.0f, -1.0f);
        Vector3f rgh(1.0f, 0.0f, 0.0f);
        pos = pos.Normalized();
        Vector3f pos2 = pos;
        pos2.x = 0;
        // spawn directly in front
        Quatf rotation_phi(rgh, atan2(pos.y, pos.z)  );
        Quatf rotation_thea(up, atan2(-pos.x, pos2.Length()));
        Quatf rotation_rot;
        // if(atan2(pos.y, pos.z ) >0)
        //     rotation_rot = Quatf  (fwd, ( atan2(- pos.x,  pos2.Length())  ));
        // else
        //     rotation_rot =  Quatf  (fwd,- ( atan2(- pos.x,  pos2.Length())));
        // Quatf viewRot(invViewMatrix);
        //  Quatf fullRotation = rotation * viewRot;


        pos = pos * MenuDistance;
        Vector3f position(fwd * pos.z + up * pos.y + rgh * pos.x);


        Matrix4f horizontalViewMatrix;
        Quatf rotation;
        if (abs( pos.x ) < 0.3) {
            horizontalViewMatrix = Matrix4f::LookAtRH(viewPos, position, rgh);
            rotation =Quatf (-fwd, 3.14f / 2.);
        }
        else {
            rotation = Quatf (-fwd, 0);
            horizontalViewMatrix = Matrix4f::LookAtRH(viewPos, position, up);
        }

        horizontalViewMatrix
                .Transpose(); // transpose because we want the rotation opposite of where we're looking


        // this was only here to test rotation about the local axis
        //Quatf rotation( -fwd, 3.14f/2. );
        Quatf viewRot(horizontalViewMatrix);
        Quatf fullRotation =  viewRot * rotation;

        // Quatf fullRotation = rotation_phi * rotation_thea   ;
        //fullRotation.Normalize();


        //     Vector3f position(viewPos + viewFwd * MenuDistance);
        return Posef(fullRotation, position);
    }
//--------------------------------------------------------------------------------------------------
void ovrQPlayerAppl::RenderRunningMenu( const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out)
{
    //g_center_view
    Matrix4f traceMat(out.FrameMatrices.CenterView.Inverted());
    g_GuiSys->Frame(in, out.FrameMatrices.CenterView, traceMat);
    // g_GuiSys->ResetMenuOrientations()

    if (&out.Surfaces != nullptr)
        g_GuiSys->AppendSurfaceList(out.FrameMatrices.CenterView, &out.Surfaces);

    if ( g_Menu_Screen != nullptr) {
        Vector3f pp = GetViewMatrixPosition(g_center_view_real);
        pp.z = pp.z + 2.5;
         g_Menu_Screen->SetMenuPose(CalcMenuPosition2D(out.FrameMatrices.CenterView, pp ));
    }

    if (g_Menu_Player != nullptr) {
        g_Menu_Player->SetMenuPose( CalcMenuPositionMenu( out.FrameMatrices.CenterView, g_move_pos ));
        bool to_show = g_show_menu;
        SetObjectVisible(GetGuiSys(), g_Menu_Player, "panel", to_show);
    }
    if (g_Menu_ToShow != nullptr){
        g_Menu_ToShow->SetMenuPose( CalcMenuPositionMenu( out.FrameMatrices.CenterView, g_move_pos2 ));
        bool to_show = g_show_menu && is_show_list;
        SetObjectVisible(GetGuiSys(), g_Menu_ToShow, "panel", to_show);
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////
static KERGL_Canvas g_Canvas_Screen;
static KERGL_Canvas g_Canvas_Daul_Left;
static KERGL_Canvas g_Canvas_Daul_Right;
static KERGL_Canvas g_Canvas_Depth;
static KERGL_Canvas g_Canvas_Depth_Left;
static KERGL_Canvas g_Canvas_Depth_Right;
static KERGL_Canvas g_Canvas_Depth_Left_INV;
static KERGL_Canvas g_Canvas_Depth_Right_INV;

static KERGL_Canvas g_Canvas_Dummp;
//--------------------------------------
Vector4f g_GlobeProgramColor;
Vector4f g_GlobeProgramD;
OVR::Matrix4f g_GlobeProgramMatrices[2];
void ovrQPlayerAppl::RenderRunningVideo(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out)
{
    Matrix4f center_view_submit;
    static OVRFW::GlTexture ProgramTexture;
    static OVRFW::GlTexture ProgramTexture_photo;
    Vector3f pp = GetViewMatrixPosition(g_center_view_real);
    float ax ,ay ,az;
    g_center_view.ToEulerAngles<Axis_Z , Axis_X ,Axis_Y, OVR::Rotate_CCW, OVR::Handed_R>(&az,&ax,&ay);
    //==============================================================================================
     bool drawScreen = (g_MovieTexture != nullptr) ;

    if ( drawScreen ) {
        g_MovieTexture->Update();
        Tool_Func_Copy(& g_Canvas_Screen, g_MovieTexture->GetTextureId() );
        //==========================================================================================

        if(mRenderMode == MODE_VIDEO_3D )
        {
            if (  g_Menu_Screen != nullptr ) {
                SetObjectVisible(GetGuiSys(),  g_Menu_Screen, "panel", 0 );
            }
#if(ENABLE_ALGO)


#else
            g_GlobeProgramColor = Vector4f( 0.6, 1, 1,  g_gamma );
            g_GlobeProgramTexture = OVRFW::GlTexture( g_MovieTexture->GetTextureId() , GL_TEXTURE_EXTERNAL_OES, 0, 0 );
            g_GlobeProgramMatrices[0] = VTexmForVideo(0);
            g_GlobeProgramMatrices[1] = VTexmForVideo(1);
            g_GlobeSurfaceDef.graphicsCommand.Program =  g_PanoramaProgram;
            g_GlobeSurfaceDef.graphicsCommand.UniformData[0].Data = &g_GlobeProgramMatrices[0];
            g_GlobeSurfaceDef.graphicsCommand.UniformData[0].Count = 3;
            g_GlobeSurfaceDef.graphicsCommand.UniformData[1].Data = &g_GlobeProgramColor;
            g_GlobeSurfaceDef.graphicsCommand.UniformData[2].Data = &g_GlobeProgramTexture;
            Matrix4f modelMatrix = g_center_view *  g_center_view_real.Inverted();
            out.Surfaces.push_back(  OVRFW::ovrDrawSurface( modelMatrix, &g_GlobeSurfaceDef));
#endif


        }else{ // 2D


            if ( g_Menu_Screen != nullptr) {
                SetObjectVisible(GetGuiSys(),  g_Menu_Screen, "panel", 1);
            }
            g_GlobeProgramTexture = OVRFW::GlTexture( g_Canvas_Screen.handle_tx , GL_TEXTURE_2D, 0, 0);
            SetObjectTexture( GetGuiSys(),   g_Menu_Screen, "show", g_GlobeProgramTexture );


        }
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////
ovrQPlayerAppl::ovrQPlayerAppl( const int32_t mainId, const int32_t renderId, const int cpuLv, const int gpuLv)
        : ovrAppl(mainId, renderId, cpuLv, gpuLv, true )
{
    mRenderMode = MODE_VIDEO_3D;
    mRenderState=RENDER_STATE_LOADING;
    g_FileSys=nullptr;

    g_SoundEffectPlayer=nullptr;

    g_GuiSys=nullptr;
    g_Locale=nullptr;

    g_MovieTexture = nullptr;

#if( ENABLE_KER)

    g_DebugLines=nullptr;
    g_SceneModel=nullptr;
    g_SpriteAtlas=nullptr;
    g_ParticleSystem=nullptr;
    g_BeamAtlas=nullptr;
    g_BeamRenderer=nullptr;
    g_ControllerModelOculusTouchLeft=nullptr;
    g_ControllerModelOculusTouchRight=nullptr;
#endif

    theApp = this;

}
//
ovrQPlayerAppl::~ovrQPlayerAppl()
{
    delete g_SoundEffectPlayer;
    g_SoundEffectPlayer = nullptr;

    OVRFW::OvrGuiSys::Destroy(g_GuiSys);


    g_MovieTexture = nullptr;

#if( ENABLE_KER)

    delete g_ControllerModelOculusTouchLeft;
    g_ControllerModelOculusTouchLeft = nullptr;
    delete g_ControllerModelOculusTouchRight;
    g_ControllerModelOculusTouchRight = nullptr;
    delete g_BeamRenderer;
    g_BeamRenderer = nullptr;
    delete g_ParticleSystem;
    g_ParticleSystem = nullptr;
    delete g_SpriteAtlas;
    g_SpriteAtlas = nullptr;
    if (g_SceneModel != nullptr) {
        delete g_SceneModel;
        g_SceneModel =nullptr;
    }
#endif



}
////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void ovrQPlayerAppl::SetVideoSize(int width, int height)
{
    ALOG("SetVideoSize width=%i height=%i ", width, height);
#if( ENABLE_KER)
    g_CurrentMovieWidth = width;
    g_CurrentMovieHeight = height;
#endif
}
void ovrQPlayerAppl::GetScreenSurface(jobject& surfaceTexture)
{
    ALOG("GetScreenSurface");
#if( ENABLE_KER)

    if(g_MovieTexture != nullptr)
        surfaceTexture = (jobject) g_MovieTexture->GetJavaObject();
#endif
}
void ovrQPlayerAppl::VideoEnded()
{
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////
#define CALL_TO_JAVA (1)
// Call to JAVA layer
void ovrQPlayerAppl::StartVideo()
{
#if(CALL_TO_JAVA)
    const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
    JNIEnv* env;
    ctx.Vm->AttachCurrentThread(&env, NULL);
    jobject me = ctx.ActivityObject;
    jclass acl = env->GetObjectClass(me); // class pointer of NativeActivity
    jmethodID startMovieMethodId =
            env->GetMethodID(acl, "startMovieFromNative", "(Ljava/lang/String;)V");
    jstring jstrMovieName = env->NewStringUTF("");
    ALOGV("CallVoidMethod - enter");
    env->CallVoidMethod(ctx.ActivityObject, startMovieMethodId, jstrMovieName);
    ALOGV("CallVoidMethod - exit");
    env->DeleteLocalRef(jstrMovieName);
    g_IsPaused = false;
#endif
};
void ovrQPlayerAppl::PauseVideo()
{
#if(CALL_TO_JAVA)
    const char method[] = "pauseMovie";
    const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
    JNIEnv* env;
    ctx.Vm->AttachCurrentThread(&env, NULL);
    jobject me = ctx.ActivityObject;
    jclass acl = env->GetObjectClass(me); // class pointer of NativeActivity
    jmethodID pauseMovieMethodId = env->GetMethodID(acl, method, "()V");
    env->CallVoidMethod(ctx.ActivityObject, pauseMovieMethodId);
    g_IsPaused = true;
#endif
};
long ovrQPlayerAppl::CurrentPosition()
{
    long r =0;
#if(CALL_TO_JAVA)
    const char method[] = "CurrentPosition";
    const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
    JNIEnv* env;
    ctx.Vm->AttachCurrentThread(&env, NULL);
    jobject me = ctx.ActivityObject;
    jclass acl = env->GetObjectClass(me); // class pointer of NativeActivity
    jmethodID pauseMovieMethodId = env->GetMethodID(acl, method, "()J");
     r = env->CallLongMethod(ctx.ActivityObject, pauseMovieMethodId);
#endif
    return r;
};
long ovrQPlayerAppl::Duration()
{
    long r =0;
#if(CALL_TO_JAVA)
    const char method[] = "Duration";
    const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
    JNIEnv* env;
    ctx.Vm->AttachCurrentThread(&env, NULL);
    jobject me = ctx.ActivityObject;
    jclass acl = env->GetObjectClass(me); // class pointer of NativeActivity
    jmethodID pauseMovieMethodId = env->GetMethodID(acl, method, "()J");
     r = env->CallLongMethod(ctx.ActivityObject, pauseMovieMethodId);
#endif
    return r;
};
void ovrQPlayerAppl::ResumeVideo()
{
#if(CALL_TO_JAVA)
    const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
    JNIEnv* env;
    ctx.Vm->AttachCurrentThread(&env, NULL);
    jobject me = ctx.ActivityObject;
    jclass acl = env->GetObjectClass(me); // class pointer of NativeActivity
    jmethodID resumeMovieMethodId = env->GetMethodID(acl, "resumeMovie", "()V");
    env->CallVoidMethod(ctx.ActivityObject, resumeMovieMethodId);
    g_IsPaused = false;
#endif
};
void ovrQPlayerAppl::NextVideo()
{
#if(CALL_TO_JAVA)
    const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
    JNIEnv* env;
    ctx.Vm->AttachCurrentThread(&env, NULL);
    jobject me = ctx.ActivityObject;
    jclass acl = env->GetObjectClass(me); // class pointer of NativeActivity
    jmethodID resumeMovieMethodId = env->GetMethodID(acl, "nextMovie", "()V");
    env->CallVoidMethod(ctx.ActivityObject, resumeMovieMethodId);
    g_IsPaused = false;
#endif
};
void ovrQPlayerAppl::PreviousVideo()
{
#if(CALL_TO_JAVA)
    const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
    JNIEnv* env;
    ctx.Vm->AttachCurrentThread(&env, NULL);
    jobject me = ctx.ActivityObject;
    jclass acl = env->GetObjectClass(me); // class pointer of NativeActivity
    jmethodID resumeMovieMethodId = env->GetMethodID(acl, "previousMovie", "()V");
    env->CallVoidMethod(ctx.ActivityObject, resumeMovieMethodId);
    g_IsPaused = false;
#endif
};
void ovrQPlayerAppl::SeekVideo( int offset )
{
#if(CALL_TO_JAVA)
    const char method[] = "seekMovie";
    const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
    JNIEnv* env;
    ctx.Vm->AttachCurrentThread(&env, NULL);
    jobject me = ctx.ActivityObject;
    jclass acl = env->GetObjectClass(me); // class pointer of NativeActivity
    jmethodID pauseMovieMethodId = env->GetMethodID(acl, method, "(I)V");
    env->CallVoidMethod(ctx.ActivityObject, pauseMovieMethodId , offset);
#endif
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////
long ovrQPlayerAppl::numMediaList()
{
    long r =0;
#if(CALL_TO_JAVA)
        const char method[] = "numPlayList";
        const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
        JNIEnv* env;
        ctx.Vm->AttachCurrentThread(&env, NULL);
        jobject me = ctx.ActivityObject;
        jclass acl = env->GetObjectClass(me); // class pointer of NativeActivity
        jmethodID pauseMovieMethodId = env->GetMethodID(acl, method, "()J");
     r = env->CallLongMethod(ctx.ActivityObject, pauseMovieMethodId);
#endif
        return r;

}
jstring ovrQPlayerAppl::nameMediaList(int pos){

    jstring res;
#if(CALL_TO_JAVA)
        const char method[] = "namePlayList";
        const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
        JNIEnv* env;
        ctx.Vm->AttachCurrentThread(&env, NULL);
        jobject me = ctx.ActivityObject;
        jclass acl = env->GetObjectClass(me); // class pointer of NativeActivity
        jmethodID pauseMovieMethodId = env->GetMethodID(acl, method, "(I)Ljava/lang/String;");
     res =  static_cast<jstring> (env->CallObjectMethod(ctx.ActivityObject, pauseMovieMethodId,pos));
#endif
        return res;
}
void ovrQPlayerAppl::pickMediaList(int pos)
{
#if(CALL_TO_JAVA)
        const char method[] = "pickPlayList";
        const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
        JNIEnv* env;
        ctx.Vm->AttachCurrentThread(&env, NULL);
        jobject me = ctx.ActivityObject;
        jclass acl = env->GetObjectClass(me); // class pointer of NativeActivity
        jmethodID pauseMovieMethodId = env->GetMethodID(acl, method, "(I)V");
        env->CallVoidMethod(ctx.ActivityObject, pauseMovieMethodId , pos);
#endif
}
//--------------------------------------------------------------------------------------------------
long ovrQPlayerAppl::numFolderList()
{
    long r =0;
#if(CALL_TO_JAVA)
    const char method[] = "numFolderList";
    const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
    JNIEnv* env;
    ctx.Vm->AttachCurrentThread(&env, NULL);
    jobject me = ctx.ActivityObject;
    jclass acl = env->GetObjectClass(me); // class pointer of NativeActivity
    jmethodID pauseMovieMethodId = env->GetMethodID(acl, method, "()J");
     r = env->CallLongMethod(ctx.ActivityObject, pauseMovieMethodId);
#endif
    return r;

}
jstring ovrQPlayerAppl::nameFolderList(int pos)
{
    jstring res;
#if(CALL_TO_JAVA)
    const char method[] = "nameFolderList";
    const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
    JNIEnv* env;
    ctx.Vm->AttachCurrentThread(&env, NULL);
    jobject me = ctx.ActivityObject;
    jclass acl = env->GetObjectClass(me); // class pointer of NativeActivity
    jmethodID pauseMovieMethodId = env->GetMethodID(acl, method, "(I)Ljava/lang/String;");
    res =  static_cast<jstring> (env->CallObjectMethod(ctx.ActivityObject, pauseMovieMethodId,pos));
#endif
    return res;
}
void ovrQPlayerAppl::pickFolderList(int pos)
{
#if(CALL_TO_JAVA)
    const char method[] = "pickFolderList";
    const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
    JNIEnv* env;
    ctx.Vm->AttachCurrentThread(&env, NULL);
    jobject me = ctx.ActivityObject;
    jclass acl = env->GetObjectClass(me); // class pointer of NativeActivity
    jmethodID pauseMovieMethodId = env->GetMethodID(acl, method, "(I)V");
    env->CallVoidMethod(ctx.ActivityObject, pauseMovieMethodId , pos);
#endif
}
//--------------------------------------------------------------------------------------------------
long ovrQPlayerAppl::numDLNAList()
{
    long r =0;
#if(CALL_TO_JAVA)
    const char method[] = "numDLNAList";
    const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
    JNIEnv* env;
    ctx.Vm->AttachCurrentThread(&env, NULL);
    jobject me = ctx.ActivityObject;
    jclass acl = env->GetObjectClass(me); // class pointer of NativeActivity
    jmethodID pauseMovieMethodId = env->GetMethodID(acl, method, "()J");
     r = env->CallLongMethod(ctx.ActivityObject, pauseMovieMethodId);
#endif
    return r;

}
jstring ovrQPlayerAppl::nameDLNAList(int pos)
{
    jstring res;
#if(CALL_TO_JAVA)
    const char method[] = "nameDLNAList";
    const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
    JNIEnv* env;
    ctx.Vm->AttachCurrentThread(&env, NULL);
    jobject me = ctx.ActivityObject;
    jclass acl = env->GetObjectClass(me); // class pointer of NativeActivity
    jmethodID pauseMovieMethodId = env->GetMethodID(acl, method, "(I)Ljava/lang/String;");
     res =  static_cast<jstring> (env->CallObjectMethod(ctx.ActivityObject, pauseMovieMethodId,pos));
#endif
    return res;
}
void ovrQPlayerAppl::pickDLNAList(int pos)
{
#if(CALL_TO_JAVA)
    const char method[] = "pickDLNAList";
    const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
    JNIEnv* env;
    ctx.Vm->AttachCurrentThread(&env, NULL);
    jobject me = ctx.ActivityObject;
    jclass acl = env->GetObjectClass(me); // class pointer of NativeActivity
    jmethodID pauseMovieMethodId = env->GetMethodID(acl, method, "(I)V");
    env->CallVoidMethod(ctx.ActivityObject, pauseMovieMethodId , pos);
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////
void ovrQPlayerAppl::SubmitCompositorLayers(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out)
{
    // set up layers
    int& layerCount = NumLayers;
    layerCount = 0;

    /// Add content layer
    ovrLayerProjection2& layer = Layers[layerCount].Projection;
    layer = vrapi_DefaultLayerProjection2();
    layer.HeadPose = Tracking.HeadPose;
    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; ++eye)
    {
        ovrFramebuffer* framebuffer = GetFrameBuffer(GetNumFramebuffers() == 1 ? 0 : eye);
        layer.Textures[eye].ColorSwapChain = framebuffer->ColorTextureSwapChain;
        layer.Textures[eye].SwapChainIndex = framebuffer->TextureSwapChainIndex;
        layer.Textures[eye].TexCoordsFromTanAngles = ovrMatrix4f_TanAngleMatrixFromProjection(
                (ovrMatrix4f*)&out.FrameMatrices.EyeProjection[eye]);
    }
    layer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;
    layer.Header.SrcBlend = VRAPI_FRAME_LAYER_BLEND_SRC_ALPHA;
    layer.Header.DstBlend = VRAPI_FRAME_LAYER_BLEND_ONE_MINUS_SRC_ALPHA;
    layerCount++;

    // render images for each eye
    for (int eye = 0; eye < GetNumFramebuffers(); ++eye) {
        ovrFramebuffer* framebuffer = GetFrameBuffer(eye);
        ovrFramebuffer_SetCurrent(framebuffer);

        AppEyeGLStateSetup(in, framebuffer, eye);
        AppRenderEye(in, out, eye);

        ovrFramebuffer_Resolve(framebuffer);
        ovrFramebuffer_Advance(framebuffer);
    }

    ovrFramebuffer_SetNone();
}
void ovrQPlayerAppl::AppRenderEye( const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out, int eye)
{
    // Render the surfaces returned by Frame.
    g_SurfaceRender.RenderSurfaceList(
            out.Surfaces,
            out.FrameMatrices.EyeView[0], // always use 0 as it assumes an array
            out.FrameMatrices.EyeProjection[0], // always use 0 as it assumes an array
            eye);
}
void ovrQPlayerAppl::AppEyeGLStateSetup(const OVRFW::ovrApplFrameIn&, const ovrFramebuffer* fb, int)
{
    GL(glDisable(GL_SCISSOR_TEST));
    GL(glDepthMask(GL_TRUE));
    GL(glEnable(GL_DEPTH_TEST));
    GL(glDepthFunc(GL_LEQUAL));
    GL(glEnable(GL_CULL_FACE));
    GL(glViewport(0, 0, fb->Width, fb->Height));
  //  GL(glScissor(0, 0, fb->Width, fb->Height));
    GL(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    // This app was originally written with the presumption that
    // its swapchains and compositor front buffer were RGB.
    // In order to have the colors the same now that its compositing
    // to an sRGB front buffer, we have to write to an sRGB swapchain
    // but with the linear->sRGB conversion disabled on write.
    GL(glDisable(GL_FRAMEBUFFER_SRGB_EXT));
}
//--------------------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////
void ovrQPlayerAppl::AppRenderFrame( const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out)
    {
        switch (mRenderState) {
            case RENDER_STATE_LOADING: {
                DefaultRenderFrame_Loading(in, out);
            } break;
            case RENDER_STATE_RUNNING: {

                // disallow player movement
                OVRFW::ovrApplFrameIn vrFrameWithoutMove = in;
                // Player movement
                vrFrameWithoutMove.LeftRemoteJoystick.x = 0.0f;
                vrFrameWithoutMove.LeftRemoteJoystick.y = 0.0f;
                // Force ignoring motion
                vrFrameWithoutMove.LeftRemoteTracked = false;
                vrFrameWithoutMove.RightRemoteTracked = false;
                // Player movement.
                g_Scene.SetFreeMove(false);
                g_Scene.Frame(vrFrameWithoutMove);
                g_Scene.GetFrameMatrices( SuggestedEyeFovDegreesX, SuggestedEyeFovDegreesY, out.FrameMatrices);
                g_Scene.GenerateFrameSurfaceList(out.FrameMatrices, out.Surfaces);
                g_center_view_real = out.FrameMatrices.CenterView;
                g_center_view = g_center_view_left * g_center_view_real ;
                //==================================================================================
               RenderRunningVideo(in, out);
                if ( g_show_menu ) {
                    RenderRunningMenu(in, out);
                    RenderRunningControl(in, out);
                }
                SubmitCompositorLayers(in, out);
            } break;
            case RENDER_STATE_ENDING: {
                DefaultRenderFrame_Ending(in, out);
            } break;
        }
    }
////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////
bool ovrQPlayerAppl::AppInit(const OVRFW::ovrAppContext* context)
{
    ALOG("====================="
         "                     "
         "      AppInit    "
         "                     "
         "====================="
    );
    const ovrJava& java = *(reinterpret_cast<const ovrJava*>(context->ContextForVrApi()));
    const xrJava ctx = JavaContextConvert(java);

    ////////////////////////////////////////////////////////////////////////////////////////////////

    g_FileSys = OVRFW::ovrFileSys::Create(ctx);
    if (nullptr == g_FileSys) {
        ALOGE("Couldn't create g_FileSys");
        return false;
    }
    g_Locale = OVRFW::ovrLocale::Create(*ctx.Env, ctx.ActivityObject, "default");
    if (nullptr == g_Locale) {
        ALOGE("Couldn't create g_Locale");
        return false;
    }
    g_DebugLines = OVRFW::OvrDebugLines::Create();
    if (nullptr == g_DebugLines) {
        ALOGE("Couldn't create g_DebugLines");
        return false;
    }
    g_DebugLines->Init();
    g_SoundEffectPlayer = new OVRFW::OvrGuiSys::ovrDummySoundEffectPlayer();
    if (nullptr == g_SoundEffectPlayer) {
        ALOGE("Couldn't create g_SoundEffectPlayer");
        return false;
    }
    g_GuiSys = OVRFW::OvrGuiSys::Create(&ctx);
    if (nullptr == g_GuiSys) {
        ALOGE("Couldn't create GUI");
        return false;
    }
    std::string fontName;
    //efigs.fnt
    GetLocale().GetLocalizedString("@string/font_name", "japanese.fnt", fontName);
    GetGuiSys().Init(g_FileSys, *g_SoundEffectPlayer, fontName.c_str(), g_DebugLines);
    ALOGV("AppInit - fontName %s", fontName.c_str());

    //==============================================================================================
    //----------------------------------------------------------------------------------------------
    //==============================================================================================
    ALOG("Init Rendering" );
    g_SurfaceRender.Init();
    ////////////////////////////////////////////////////////////////////////////////////////////////



    //==============================================================================================
    //----------------------------------------------------------------------------------------------
    //==============================================================================================
    ALOG("movie texture" );
    /// Build movie Texture
    g_MovieTexture = std::unique_ptr<OVRFW::SurfaceTexture>( new OVRFW::SurfaceTexture(ctx.Env) );
    /// Use a globe mesh for the video surface
    g_GlobeSurfaceDef.surfaceName = "Globe";
    g_GlobeSurfaceDef.geo = OVRFW::BuildGlobe();
    g_GlobeSurfaceDef.graphicsCommand.GpuState.depthEnable = false;
    g_GlobeSurfaceDef.graphicsCommand.GpuState.cullEnable = false;


    /// Build Shader and required uniforms
    static OVRFW::ovrProgramParm PanoramaUniformParms[] = {
            {"Texm", OVRFW::ovrProgramParmType::FLOAT_MATRIX4},
            {"UniformColor", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
            {"Texture0", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
    };
#if(1)
         g_PanoramaProgram = OVRFW::GlProgram::Build(
            NULL,
            PanoramaVertexShaderSrc,
            ImageExternalDirectives,
            PanoramaFragmentShaderSrc,
            PanoramaUniformParms,
            sizeof(PanoramaUniformParms) / sizeof(OVRFW::ovrProgramParm));

#endif


    //---------------------------------------------------------------------

#if(ENABLE_ALGO)

#endif

    //----------------------------------------------------------------------------------------------
    //
    //
    //
    //----------------------------------------------------------------------------------------------
    /// For movie players, best to set the display to 60fps if available
    {
        // Query supported frame rates
        int numSupportedRates =
                vrapi_GetSystemPropertyInt( &java, VRAPI_SYS_PROP_NUM_SUPPORTED_DISPLAY_REFRESH_RATES);
        std::vector<float> refreshRates(numSupportedRates);
        int numValues = vrapi_GetSystemPropertyFloatArray(
                &java,
                VRAPI_SYS_PROP_SUPPORTED_DISPLAY_REFRESH_RATES,
                (float*)refreshRates.data(),
                numSupportedRates);
        float max_rate =0.f;
        if (numValues > 0) {
            // See if we have one close to 60fps
            for (const float& rate : refreshRates) {
                ALOGV("AppInit - available refresh rate of %.2f Hz", rate);

                if (fabs(rate - 60.0f) < 1.f) {
                    ALOGV("AppInit - setting refresh rate to %.2f Hz", rate);
                    vrapi_SetDisplayRefreshRate(GetSessionObject(), rate);
                    break;
                }
            }
        }
    }
    //----------------------------------------------------------------------------------------------
    //
    //
    //
    //----------------------------------------------------------------------------------------------

#if(1)
        {

            static OVRFW::ovrProgramParm OculusTouchUniformParms[] = {
                    {"Texture0", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
                    {"SpecularLightDirection", OVRFW::ovrProgramParmType::FLOAT_VECTOR3},
                    {"SpecularLightColor", OVRFW::ovrProgramParmType::FLOAT_VECTOR3},
                    {"AmbientLightColor", OVRFW::ovrProgramParmType::FLOAT_VECTOR3},
            };
            g_SpecularLightDirection = Vector3f(1.0f, 1.0f, 0.0f);
            g_SpecularLightColor = Vector3f(1.0f, 0.95f, 0.8f) * 0.75f;
            g_AmbientLightColor = Vector3f(1.0f, 1.0f, 1.0f) * 0.15f;


            g_ProgOculusTouch = OVRFW::GlProgram::Build(
                    OculusTouchVertexShaderSrc,
                    OculusTouchFragmentShaderSrc,
                    OculusTouchUniformParms,
                    sizeof(OculusTouchUniformParms) / sizeof(OVRFW::ovrProgramParm));


            OVRFW::ModelGlPrograms programs;
            programs.ProgSingleTexture = &g_ProgOculusTouch;
            programs.ProgBaseColorPBR = &g_ProgOculusTouch;
            programs.ProgSkinnedBaseColorPBR = &g_ProgOculusTouch;
            programs.ProgLightMapped = &g_ProgOculusTouch;
            programs.ProgBaseColorEmissivePBR = &g_ProgOculusTouch;
            programs.ProgSkinnedBaseColorEmissivePBR = &g_ProgOculusTouch;
            programs.ProgSimplePBR = &g_ProgOculusTouch;
            programs.ProgSkinnedSimplePBR = &g_ProgOculusTouch;

            OVRFW::MaterialParms materials;
            {
                const char* controllerModelFile =
                        "apk:///assets/oculusQuest_oculusTouch_Right.gltf.ovrscene";
                g_ControllerModelOculusTouchRight =
                        LoadModelFile(g_GuiSys->GetFileSys(), controllerModelFile, programs, materials);
                if (g_ControllerModelOculusTouchRight == NULL ||
                    static_cast<int>(g_ControllerModelOculusTouchRight->Models.size()) < 1) {
                    ALOGE_FAIL(
                            "Couldn't load Oculus Touch for Oculus Quest Controller Controller right model");
                }

                for (auto& model : g_ControllerModelOculusTouchRight->Models) {
                    auto& gc = model.surfaces[0].surfaceDef.graphicsCommand;
                    gc.UniformData[0].Data = &gc.Textures[0];
                    gc.UniformData[1].Data = &g_SpecularLightDirection;
                    gc.UniformData[2].Data = &g_SpecularLightColor;
                    gc.UniformData[3].Data = &g_AmbientLightColor;
                    gc.UniformData[4].Data = &gc.Textures[1];
                }
            }
            {
                const char* controllerModelFile =
                        "apk:///assets/oculusQuest_oculusTouch_Left.gltf.ovrscene";
                g_ControllerModelOculusTouchLeft =
                        LoadModelFile(g_GuiSys->GetFileSys(), controllerModelFile, programs, materials);
                if (g_ControllerModelOculusTouchLeft == NULL ||
                    static_cast<int>(g_ControllerModelOculusTouchLeft->Models.size()) < 1) {
                    ALOGE_FAIL(
                            "Couldn't load Oculus Touch for Oculus Quest Controller Controller left model");
                }

                for (auto& model : g_ControllerModelOculusTouchLeft->Models) {
                    auto& gc = model.surfaces[0].surfaceDef.graphicsCommand;
                    gc.UniformData[0].Data = &gc.Textures[0];
                    gc.UniformData[1].Data = &g_SpecularLightDirection;
                    gc.UniformData[2].Data = &g_SpecularLightColor;
                    gc.UniformData[3].Data = &g_AmbientLightColor;
                    gc.UniformData[4].Data = &gc.Textures[1];
                }
            }
        }
#endif
        //----------------------------------------------------------------------------------------------
        {

            OVRFW::MaterialParms materialParms;
            materialParms.UseSrgbTextureFormats = false;
            const char* sceneUri = "apk:///assets/box.ovrscene";
#if(1)
            g_SceneModel = LoadModelFile(
                    g_GuiSys->GetFileSys(), sceneUri,
                    g_Scene.GetDefaultGLPrograms(), materialParms);
#else

            g_SceneModel = std::unique_ptr<OVRFWQ::OVRFW::ModelFile>(new ::OVRFWQ::ModelFile("Void"));
#endif

#if(1)
            if (g_SceneModel != nullptr)
            {
                g_Scene.SetWorldModel(*g_SceneModel);
                Vector3f modelOffset;
                modelOffset.x = 0.0f;
                modelOffset.y = 0.0f;
                modelOffset.z = -2.25f;
                g_Scene.GetWorldModel()->State.SetMatrix(
                        Matrix4f::Scaling(2.5f, 2.5f, 2.5f) * Matrix4f::Translation(modelOffset));
            }
#endif
        }

#if(1)
        g_SpriteAtlas = new OVRFW::ovrTextureAtlas();
        g_SpriteAtlas->Init(g_GuiSys->GetFileSys(), "apk:///assets/particles2.ktx");
        g_SpriteAtlas->BuildSpritesFromGrid(4, 2, 8);

        g_ParticleSystem = new OVRFW::ovrParticleSystem();
        auto particleGPUstate = OVRFW::ovrParticleSystem::GetDefaultGpuState();
        g_ParticleSystem->Init(2048, g_SpriteAtlas, particleGPUstate, false);

        g_BeamAtlas = new OVRFW::ovrTextureAtlas();
        g_BeamAtlas->Init(g_GuiSys->GetFileSys(), "apk:///assets/beams.ktx");
        g_BeamAtlas->BuildSpritesFromGrid(2, 1, 2);
        //
        g_BeamRenderer = new OVRFW::ovrBeamRenderer();
        g_BeamRenderer->Init(256, true);
#endif


    static EGLImageKHR g_imageEGL;

    memset(&g_Canvas_Dummp, 0, sizeof(g_Canvas_Dummp));

    g_Canvas_Dummp.size_w = 64;
    g_Canvas_Dummp.size_h = 64;

    // OUR parameters that we will set and give it to AHardwareBuffer
    AHardwareBuffer_Desc usage;
    // filling in the usage for HardwareBuffer
    usage.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
    usage.height =  g_Canvas_Dummp.size_h ;
    usage.width = g_Canvas_Dummp.size_w ;
    usage.layers = 1;
    usage.rfu0 = 0;
    usage.rfu1 = 0;
    usage.stride =  usage.width ;
    usage.usage = AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN | AHARDWAREBUFFER_USAGE_CPU_WRITE_NEVER
                  | AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT;
    // create g_HardwareBuffer
    AHardwareBuffer_allocate(&usage, &g_HardwareBuf); // it's worth to check the return code

    // ACTUAL parameters of the AHardwareBuffer which it reports
    AHardwareBuffer_Desc usage1;
    // for stride, see below
    AHardwareBuffer_describe(g_HardwareBuf, &usage1);
    //
    g_Canvas_Dummp.stride = 4 * usage1.stride;
    //
    // get the native buffer
    EGLClientBuffer clientBuf = eglGetNativeClientBufferANDROID(g_HardwareBuf);
    // obtaining the EGL display
    EGLDisplay disp = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    // specifying the image attributes
    EGLint eglImageAttributes[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
    // creating an EGL image
    g_imageEGL = eglCreateImageKHR(disp, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, clientBuf, eglImageAttributes);
    if( !glIsTexture( g_Canvas_Dummp.handle_tx )) {
        ALOG("glIsTexture\n");
        glGenFramebuffers(1, &g_Canvas_Dummp.handle_fb );
        glGenTextures(1, &g_Canvas_Dummp.handle_tx );
        // GL_TEXTURE_EXTERNAL_OES
        glBindTexture(GL_TEXTURE_2D, g_Canvas_Dummp.handle_tx);
        // attaching an EGLImage to OUTPUT texture
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, g_imageEGL);

        glBindFramebuffer(GL_FRAMEBUFFER, g_Canvas_Dummp.handle_fb);
        // attach texture to FBO (create texture my_texture first!)
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_Canvas_Dummp.handle_tx, 0);
        // check for framebuffer complete
        int succxx = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (succxx != GL_FRAMEBUFFER_COMPLETE) {
            ALOG("GL_FRAMEBUFFER_COMPLETE Error\n");
        }else{
            ALOG("GL_FRAMEBUFFER_COMPLETE Ok\n");
        }
    }

    ALOG("stride %d \n", usage1.stride ); ////unit : length of pixels

    ////////////////////////////////////////////////////////////////////////////////////////////////
    int succ = 0;
    succ = KERGL_TOOL();
    assert (succ == 1);

    #define SC_SZ (1920)
    succ = KERGL_Canvas_Create(&glavmCanvasH, 1000, 2);
    succ = KERGL_Canvas_Create(&glavmCanvasV, 2, 1000);
    KERGL_Canvas_Create(&g_Canvas_Screen, SC_SZ, SC_SZ);



    #define OV_SZ (420)

    //    memset(&g_Canvas_Daul_Left, 0, sizeof(g_Canvas_Daul_Left));
    succ = KERGL_Canvas_Create(&g_Canvas_Daul_Left, OV_SZ, OV_SZ);
    assert (succ == GL_FRAMEBUFFER_COMPLETE);
    //   memset(&g_Canvas_Daul_Right, 0, sizeof(g_Canvas_Daul_Right));
    succ = KERGL_Canvas_Create(&g_Canvas_Daul_Right, OV_SZ, OV_SZ);
    assert (succ == GL_FRAMEBUFFER_COMPLETE);
    //  memset(&g_Canvas_Depth_Left, 0, sizeof(g_Canvas_Depth_Left));
    KERGL_Canvas_Create(&g_Canvas_Depth_Left, OV_SZ, OV_SZ);
    //   memset(&g_Canvas_Depth_Right, 0, sizeof(g_Canvas_Depth_Right));
    KERGL_Canvas_Create(&g_Canvas_Depth_Right, OV_SZ, OV_SZ);


 //----------------------------------------------------------------------------------------------


#if(1)

    g_Menu_Player = ovrControllerGUI::Create(*this, "player");
    if (g_Menu_Player != nullptr) {
        ALOGV("Menu Name %s",g_Menu_Player->GetName());
        GetGuiSys().AddMenu(g_Menu_Player);
        GetGuiSys().OpenMenu(g_Menu_Player->GetName());
         InitPlayer();
    }

    g_Menu_Screen = ovrControllerGUI::Create(*this, "screen");
    if ( g_Menu_Screen != nullptr) {
        ALOGV("Menu Name %s", g_Menu_Screen->GetName());
        GetGuiSys().AddMenu( g_Menu_Screen);
        GetGuiSys().OpenMenu( g_Menu_Screen->GetName());

        InitScreen();
    }


        
    g_Menu_Media= ovrControllerGUI::Create(*this, "list_media");
    if (g_Menu_Media != nullptr) {
        {
            ALOGV("Menu Name %s",g_Menu_Media->GetName());
            GetGuiSys().AddMenu(g_Menu_Media);
            GetGuiSys().OpenMenu(g_Menu_Media->GetName());
            Vector3f  offset(0,0.0,1.8);
            g_Menu_Media->SetMenuDistance(offset);
            Init_Media();
            SetObjectVisible(GetGuiSys(), g_Menu_Media, "panel", false);
        }
    }

    g_Menu_Device = ovrControllerGUI::Create(*this, "list_device");
    if (g_Menu_Device != nullptr) {
        ALOGV("Menu Name %s",g_Menu_Device->GetName());
        GetGuiSys().AddMenu(g_Menu_Device);
        GetGuiSys().OpenMenu(g_Menu_Device->GetName());
        Vector3f  offset(0,0.0,1.8);
        g_Menu_Device->SetMenuDistance(offset);
        Init_Device();
        SetObjectVisible(GetGuiSys(), g_Menu_Device, "panel", false);
    }

     g_Menu_Folder = ovrControllerGUI::Create(*this, "list_folder");
     if (g_Menu_Folder != nullptr) {
        ALOGV("Menu Name %s",g_Menu_Folder->GetName());
        GetGuiSys().AddMenu(g_Menu_Folder);
        GetGuiSys().OpenMenu(g_Menu_Folder->GetName());
        Vector3f  offset(0,0.0,1.8);
        g_Menu_Folder->SetMenuDistance(offset);
        Init_Folder();
        SetObjectVisible(GetGuiSys(), g_Menu_Folder, "panel", false);
     }

    g_Menu_ToShow =  g_Menu_Folder;



#endif


 //----------------------------------------------------------------------------------------------

 //----------------------------------------------------------------------------------------------
 /// Start movie on Java side
 StartVideo();
 /// All done
 ALOGV("AppInit - exit");
 return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////
void ovrQPlayerAppl::AppShutdown(const OVRFW::ovrAppContext* context)
{
    ALOG("====================="
         "                     "
         "      AppShutdown    "
         "                     "
         "====================="
    );
    mRenderState = RENDER_STATE_ENDING;
#if(ENABLE_KER)

    //=======================================

    g_GlobeSurfaceDef.geo.Free();
    OVRFW::GlProgram::Free(g_TexturedMvpProgram);
    //=======================================
    g_SurfaceRender.Shutdown();
    g_MovieTexture.release();
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////
void ovrQPlayerAppl::AppResumed(const OVRFW::ovrAppContext* /* context */)
{
    ALOG("====================="
         "                     "
         "      App Resumed    "
         "                     "
         "====================="
    );

    mRenderState = RENDER_STATE_RUNNING;
#if(ENABLE_KER)
    ResumeVideo();
#endif


}
////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////
void ovrQPlayerAppl::AppPaused(const OVRFW::ovrAppContext* /* context */)
{
    ALOG("====================="
         "                     "
         "      App Paused     "
         "                     "
         "====================="
    );
#if(ENABLE_KER)
    if (mRenderState == RENDER_STATE_RUNNING) {
        PauseVideo();
    }
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////

    static float calculation(int i, float scale, float offset)
    {
        return ( scale *( -i+offset) );
    }
    void ovrQPlayerAppl:: InitPlayer()
    {
        float  oy = 87.;
        ovrControllerGUI* ref = g_Menu_Player;
        SetObjectColor(GetGuiSys(), ref, "panel", Vector4f( 1.0f, 1.0f, 1.0f,1.0f  ));
        SetSurfaceVisible(GetGuiSys(), ref, "panel", false);
        //----------------------------------------------------------------------------------------------
        ref->AddButton("folder", "Folder",
                       Vector3f( -110.0f + 172.f, oy-169.0f, 0.0f  ),
                       Vector2f(90.f,  56.f),
                       2,[=](   OVRFW::VRMenuObject* obj , float u, float v, int status_click, int mode )
                       {
                           if ( 0 == mode ) {

                           }else if( 1== mode){

                           }else if( 2== mode){
                               if( 2 == status_click ) {
                                   is_show_list = !is_show_list;
                                   g_Menu_ToShow = g_Menu_Folder;
                               }

                           }
                       }
        );
        ref->AddButton("device", "Device",
                       Vector3f( -110.0f + 296.f, oy-169.0f, 0.0f  ),
                       Vector2f(90.f,  56.f),
                       2,[=](   OVRFW::VRMenuObject* obj , float u, float v, int status_click, int mode )
                       {
                           if ( 0 == mode ) {

                           }else if( 1== mode){

                           }else if( 2== mode){
                               if( 2 == status_click ) {
                                   is_show_list = !is_show_list;
                                   g_Menu_ToShow = g_Menu_Device;
                               }

                           }
                       }
        );

        ref->AddButton("media", "Media",
                       Vector3f( -110.0f + 172.f, oy-87.0f, 0.0f  ),
                       Vector2f(90.f,  56.f),
                       2,[=](   OVRFW::VRMenuObject* obj , float u, float v, int status_click, int mode )
                       {
                           if ( 0 == mode ) {

                           }else if( 1== mode){

                           }else if( 2== mode){
                               if( 2 == status_click ) {
                                   is_show_list = !is_show_list;
                                   g_Menu_ToShow = g_Menu_Media;
                               }

                           }
                       }
        );

        ref->AddButton("video_position", "",
                       Vector3f( 182.0f, oy-87.0f, 0.0f  ),
                       Vector2f(154.0f,  56.f),
                       2,[=](   OVRFW::VRMenuObject* obj , float u, float v, int status_click, int mode )
                       {
                           if ( 0 == mode ) {

                               static int ixf = 1000;
                               if( ixf++ > 30) ixf =0;
                               if( 0 ==ixf ){
#if(1)
                                   char text[1024];
                                   long c = CurrentPosition();
                                   long d = Duration();
                                   time_t t = (time_t) ((d -c)/1000);
                                   struct tm *local_time = gmtime(&t);
                                   char buf[1024];
                                   buf [ strftime(buf, sizeof(buf), "%H:%M:%S", local_time) ] = '\0';
                                   obj->SetText(buf);
#endif
                               }
                           }else if( 1== mode){
                               Vector4f color;

                               color = Vector4f ( 0.5f, 0.5f, 0.5f ,1.f );
                               obj->SetSurfaceColor(0, color);



                           }else if( 2== mode){
                               Vector4f color;
                               color = Vector4f ( 1.f, 0.f, 0.f ,1.f );
                               obj->SetSurfaceColor(0, color);

                               char text[1024];
                               long d = Duration();
                               time_t t = (time_t) ((d) / 1000);
                               struct tm *local_time = gmtime(&t);
                               char buf[1024];
                               buf[strftime(buf, sizeof(buf), "%H:%M:%S", local_time)] = '\0';
                               obj->SetText(buf);


                           }
                       }
        );
        ref->AddButton("video_progress", "",
                       Vector3f( 0.0f, oy-128.0f, 0.0f  ),
                       Vector2f(512.0f,  16.f),
                       2,[=](   OVRFW::VRMenuObject* obj , float u, float v, int status_click, int mode )
                       {
                           if(0 == mode){

                               static int ixf =1000;
                               if( ixf++ > 30) ixf =0;
                               if( 0 ==ixf ) {
#if(1)
                                   long c = CurrentPosition();
                                   long d = Duration();

                                   glBindFramebuffer(GL_FRAMEBUFFER, glavmCanvasH.handle_fb);
                                   glClearColor(0.9, 0.9, 0.9, 1.);
                                   glDisable(GL_SCISSOR_TEST);
                                   glClear(GL_COLOR_BUFFER_BIT);
                                   glEnable(GL_SCISSOR_TEST);
                                   glScissor(0, 0,  glavmCanvasH.size_w *((float)c/d), 2 );
                                   glClearColor(0.9, 0.0, 0.0, 1.);
                                   glClear(GL_COLOR_BUFFER_BIT);
                                   glDisable(GL_SCISSOR_TEST);

                                   OVRFW::GlTexture ProgramTexture = OVRFW::GlTexture( glavmCanvasH.handle_tx, GL_TEXTURE_2D, 0, 0 );

                                   Vector4f color( 1.f, 1.f, 1.f ,1.f );
                                   obj->SetSurfaceColor(0,color);
                                   obj->SetSurfaceTexture(0,0,OVRFW::SURFACE_TEXTURE_DIFFUSE, ProgramTexture);

#endif

                               }

                           }else if( 1== mode){

                           }else if( 2== mode){
                               if(1 == status_click ){

                                   SeekVideo(  u  * Duration() );
                               }
                           }

                       }
        );
        ref->AddButton("next", "+",
                       Vector3f( -110.0f -53.f, oy-87.0f, 0.0f  ),
                       Vector2f(56.0f,  56.f),
                       1,[=](   OVRFW::VRMenuObject* obj , float u, float v, int status_click, int mode )
                       {
                           if ( 0 == mode ) {

                           }else if( 1== mode){
                               Vector4f color;
                               color = Vector4f ( 0.5f, 0.5f, 0.5f ,1.f );
                               obj->SetSurfaceColor(0, color);
                               obj->SetColor(color);

                           }else if( 2== mode){
                               Vector4f color;
                               color = Vector4f ( 1.f, 0.f, 0.f ,1.f );
                               obj->SetSurfaceColor(0, color);
                               obj->SetColor(color);
                               if(2 == status_click ){

                                   NextVideo();
                               }
                           }
                       }
        );
        ref->AddButton("previous", "-",
                       Vector3f( -110.0f -106.f, oy-87.0f, 0.0f  ),
                       Vector2f(56.0f,  56.f),
                       1,[=](   OVRFW::VRMenuObject* obj , float u, float v, int status_click, int mode )
                       {
                           if ( 0 == mode ) {

                           }else if( 1== mode){
                               Vector4f color;
                               color = Vector4f ( 0.5f, 0.5f, 0.5f ,1.f );
                               obj->SetSurfaceColor(0, color);

                           }else if( 2== mode){
                               Vector4f color;
                               color = Vector4f ( 1.f, 0.f, 0.f ,1.f );
                               obj->SetSurfaceColor(0, color);
                               if(2 == status_click ){

                                   PreviousVideo();
                               }
                           }
                       }
        );
        ref->AddButton("Pause", "Pause",
                       Vector3f( -110.0f +24, oy-87.0f, 0.0f  ),
                       Vector2f(106.f,  56.f),
                       1,[=](   OVRFW::VRMenuObject* obj , float u, float v, int status_click, int mode )
                       {
                           if ( 0 == mode ) {

                           }else if( 1== mode){
                               Vector4f color;

                               color = Vector4f ( 0.5f, 0.5f, 0.5f ,1.f );
                               obj->SetSurfaceColor(0, color);

                           }else if( 2== mode){
                               Vector4f color;
                               color = Vector4f ( 1.f, 0.f, 0.f ,1.f );
                               obj->SetSurfaceColor(0, color);
                               if(2 == status_click ) {



                                   if( g_IsPaused){

                                       obj->SetText("Pause");
                                       ResumeVideo();

                                   }else{
                                       obj->SetText("Play");

                                       PauseVideo();
                                   }

                               }
                           }

                       }
        );
        ref->AddButton("on", "on",
                       Vector3f( -110.0f -106.f, oy-87.0f -82, 0.0f  ),
                       Vector2f(96.f,  56.f),
                       2,[=](   OVRFW::VRMenuObject* obj , float u, float v, int status_click, int mode )
                       {
                           if ( 0 == mode ) {
                               static int ixf =1000;
                               if( ixf++ > 30) ixf =0;
                               if( 0 ==ixf ) {
                                   if (g_On == 1) {
                                       obj->SetText("AUTO");
                                   } else {
                                       obj->SetText("MAN");
                                   }
                               }
                           }

                       }
        );

        ref->AddButton("mode", "on",
                       Vector3f( -110.0f -106.f, oy-87.0f -182, 0.0f  ),
                       Vector2f(96.f,  56.f),
                       2,[=](   OVRFW::VRMenuObject* obj , float u, float v, int status_click, int mode )
                       {
                           if ( 0 == mode ) {
                               if ( mRenderMode == MODE_VIDEO_3D){
                                   obj->SetText("2D");
                               }else {
                                   obj->SetText("3D");
                               }
                           }else if( 1== mode){
                               Vector4f color;

                               color = Vector4f ( 0.5f, 0.5f, 0.5f ,1.f );
                               obj->SetSurfaceColor(0, color);

                           }else if( 2== mode){
                               Vector4f color;
                               color = Vector4f ( 1.f, 0.f, 0.f ,1.f );
                               obj->SetSurfaceColor(0, color);
                               if(2 == status_click ) {
                                   if( mRenderMode  == MODE_VIDEO_3D)
                                       mRenderMode =MODE_VIDEO_2D;
                                   else
                                       mRenderMode = MODE_VIDEO_3D;
                               }
                           }

                       }
        );




    }
    void ovrQPlayerAppl:: InitScreen()
    {
        ovrControllerGUI* ref =  g_Menu_Screen;
        SetObjectColor(GetGuiSys(), ref, "panel", Vector4f( 0.0f, 1.0f, 1.0f,1.0f  ));
        SetSurfaceVisible(GetGuiSys(), ref, "panel", false);

        ref->AddButton("show", "",
                       Vector3f( 0.0f ,0.0f , 0.0f  ),
                       Vector2f(640.0f,  640.f),
                       0,[=](   OVRFW::VRMenuObject* obj , float u, float v, int status_click, int mode )
                       {
                           static int first =1;
                           if ( 0 == mode ) {
                               if(first){
                                   first =0;

                                   obj->SetSurfaceBorder(0,Vector4f(0));
                                   obj->RegenerateSurfaceGeometry(0,true);
                               }
                               float rate = 1.*g_CurrentMovieWidth/g_CurrentMovieHeight;
                               obj->SetLocalScale(Vector3f(rate,1,1));
                           }else if( 1== mode){
                           }else if( 2== mode){
                           }
                       }
        );




    }


    void ovrQPlayerAppl::Init_Device()
        {
            ovrControllerGUI* ref = g_Menu_Device;
            //----------------------------------------------------------------------------------------------
            static float offset =0;
            static long total = 1;
            static long see =7;
            static float pos =0;
            ref->AddButton("scllor", "",Vector3f(400.f, .0f, 0.0f),Vector2f(20.0f, 260.f),
                           2,[=]( OVRFW::VRMenuObject* obj , float u, float v, int status_click, int mode )
                           {

                               if ( 0 == mode ) {
                                   if(g_Menu_ToShow ==  ref ){
                                       int total_n= numDLNAList();
                                       pos = pos +(total - total_n  );
                                       if(pos< 0 ) pos =0;
                                       if(pos>= total_n- see ) pos =total_n -see;
                                       total =total_n;
                                       //
                                       glBindFramebuffer(GL_FRAMEBUFFER, glavmCanvasV.handle_fb);
                                       glClearColor(0.9, 0.9, 0.9, 1.);
                                       glDisable(GL_SCISSOR_TEST);
                                       glClear(GL_COLOR_BUFFER_BIT);
                                       glEnable(GL_SCISSOR_TEST);
                                       glScissor(0, glavmCanvasH.size_w *((float)pos/total), 2, glavmCanvasH.size_w *((float)see/total) );

                                       glClearColor(0.9, 0.0, 0.0, 1.);
                                       glClear(GL_COLOR_BUFFER_BIT);
                                       glDisable(GL_SCISSOR_TEST);
                                       OVRFW::GlTexture ProgramTexture = OVRFW::GlTexture( glavmCanvasV.handle_tx, GL_TEXTURE_2D, 0, 0 );
                                       Vector4f color( 1.f, 1.f, 1.f ,1.f );
                                       obj->SetSurfaceColor(0,color);
                                       obj->SetSurfaceTexture(0,0,OVRFW::SURFACE_TEXTURE_DIFFUSE, ProgramTexture);
                                   }

                               }else if( 1== mode){



                               }else if( 2== mode){

                                   if(1 == status_click ){

                                       float x = v * total - 0.5 *see;

                                       long  y = total - see;

                                       x = x < 0 ? 0: x;

                                       x = x > y ? y :x;

                                       pos = x;


                                   }
                               }
                           }
            );

            ref->AddButton("cancal", "ESC",
                           Vector3f( 320.0f, -200.f, 0.0f  ),
                           Vector2f(56.0f,  56.f),
                           1,[=](   OVRFW::VRMenuObject* obj , float u, float v, int status_click, int mode ){

                        if ( 0 == mode ) {

                        }else if( 1== mode){
                            Vector4f color;

                            color = Vector4f ( 0.0f, 0.0f, 0.0f ,1.f );
                            obj->SetSurfaceColor(0, color);

                        }else if( 2== mode){
                            Vector4f color;
                            color = Vector4f ( 0.2f, 0.9f, 0.2f ,1.f );
                            obj->SetSurfaceColor(0, color);

                            if(2 == status_click ){

                                is_show_list =false;
                            }
                        }
                    });

            //----------------------------------------------------------------------------------------------
            for( int a=0;a<64;a++) {
                std::stringstream ss;
                ss << a;
                std::string str = ss.str();
                ref->AddButton(
                        str, str,Vector3f(0.f, 0.f, 0.0f),Vector2f(720.0f, 42.f),
                        2,[=](   OVRFW::VRMenuObject* obj , float u, float v, int status_click, int mode )
                        {

                            if ( 0 == mode ) {
                                int x= atoi( obj->GetName().c_str() );
                                OVR::Posef pose = obj->GetLocalPose();
                                pose.Translation = Vector3f(0, calculation(x,0.08,pos+3),0);
                                obj->SetLocalPose( pose );
                                if(  x-pos >= see|| x-pos <0  || x >= total || x <0){
                                    obj->SetVisible(false);
                                }else{

                                    OVRFW::VRMenuFontParms f = obj->GetFontParms();

                                    f.MultiLine =false;
                                    //  f.AlignHoriz =OVRFW::HORIZONTAL_LEFT;
                                    // f.Outline = true;
                                    // f.AlignVert = OVRFW::VERTICAL_CENTER_FIXEDHEIGHT ;
                                    f.WrapWidth =14;
    /*
     *
     enum HorizontalJustification { HORIZONTAL_LEFT, HORIZONTAL_CENTER, HORIZONTAL_RIGHT };

    enum VerticalJustification {
        VERTICAL_BASELINE, // align text by baseline of first row
        VERTICAL_CENTER,
        VERTICAL_CENTER_FIXEDHEIGHT, // ignores ascenders/descenders
        VERTICAL_TOP
    };

     * */
                                    obj->SetFontParms(f);
                                    const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
                                    JNIEnv* env;
                                    ctx.Vm->AttachCurrentThread(&env, NULL);
                                    //=============================================================
                                    jstring name = nameDLNAList(x);
                                    //=============================================================
                                    const char *nativeString = env->GetStringUTFChars (name, 0);
                                    size_t length = strlen(reinterpret_cast<const char *const>(nativeString));
                                    char subbuff[3000];
                                    strcpy( subbuff, &nativeString[0] );

                                    subbuff[64] = '\0';


                                    obj->SetText(subbuff);


                                    env->ReleaseStringUTFChars(name, nativeString);
                                    obj->SetVisible(true);
                                }

                            }else if( 1== mode){
                                obj->SetSurfaceColor(0, Vector4f(0.0f, 0.0f, 0.0f, 1.f));
                            }else if( 2== mode){
                                obj->SetSurfaceColor(0, Vector4f(0.2f, 0.9f, 0.2f, 1.f));

                                if(2 == status_click ) {
                                    int x= atoi( obj->GetName().c_str() );
                                    obj->SetSurfaceColor(0, Vector4f(0.9f, 0.2f, 0.2f, 1.f));

                                    pickDLNAList(x);
                                    SetObjectVisible(GetGuiSys(), g_Menu_ToShow, "panel", false);


                                    g_Menu_ToShow =  g_Menu_Folder;
                                    SetObjectVisible(GetGuiSys(), g_Menu_ToShow, "panel", true);

                                    //   int id = panel;
                                    //  SetObjectVisible(GetGuiSys(), g_Menu_Folder, u_ObjectName[id].c_str(), false);
                                }
                            }
                        }
                );
            }
            //----------------------------------------------------------------------------------------------
        }

    void ovrQPlayerAppl::Init_Media() {

        ovrControllerGUI* ref = g_Menu_Media;
        //----------------------------------------------------------------------------------------------
        static float offset =0;
        static long total = 1;
        static long see =7;
        static float pos =0;
        ref->AddButton("scllor", "",Vector3f(400.f, .0f, 0.0f),Vector2f(20.0f, 260.f),
                       2,[=]( OVRFW::VRMenuObject* obj , float u, float v, int status_click, int mode )
                       {

                           if ( 0 == mode ) {
                               if( g_Menu_ToShow ==  ref ) {
                                   int total_n = numMediaList();
                                   pos = pos + (total - total_n);
                                   if (pos < 0) pos = 0;
                                   if (pos >= total_n - see) pos = total_n - see;
                                   total = total_n;
                                   //
                                   glBindFramebuffer(GL_FRAMEBUFFER, glavmCanvasV.handle_fb);
                                   glClearColor(0.9, 0.9, 0.9, 1.);
                                   glDisable(GL_SCISSOR_TEST);
                                   glClear(GL_COLOR_BUFFER_BIT);
                                   glEnable(GL_SCISSOR_TEST);
                                   glScissor(0, glavmCanvasH.size_w * ((float) pos / total),
                                             2, glavmCanvasH.size_w * ((float) see / total));

                                   glClearColor(0.9, 0.0, 0.0, 1.);
                                   glClear(GL_COLOR_BUFFER_BIT);
                                   glDisable(GL_SCISSOR_TEST);
                                   OVRFW::GlTexture ProgramTexture = OVRFW::GlTexture(glavmCanvasV.handle_tx,
                                                                                      GL_TEXTURE_2D, 0, 0);
                                   Vector4f color(1.f, 1.f, 1.f, 1.f);
                                   obj->SetSurfaceColor(0, color);
                                   obj->SetSurfaceTexture(0, 0, OVRFW::SURFACE_TEXTURE_DIFFUSE, ProgramTexture);
                               }
                           }else if( 1== mode){



                           }else if( 2== mode){

                               if(1 == status_click ){

                                   float x = v * total - 0.5 *see;

                                   long  y = total - see;

                                   x = x < 0 ? 0: x;

                                   x = x > y ? y :x;

                                   pos = x;


                               }
                           }
                       }
        );

        ref->AddButton("cancal", "ESC",
                       Vector3f( 320.0f, -200.f, 0.0f  ),
                       Vector2f(56.0f,  56.f),
                       1,[=](   OVRFW::VRMenuObject* obj , float u, float v, int status_click, int mode ){

                    if ( 0 == mode ) {

                    }else if( 1== mode){
                        Vector4f color;

                        color = Vector4f ( 0.0f, 0.0f, 0.0f ,1.f );
                        obj->SetSurfaceColor(0, color);
                        //  obj->SetSurfaceDims(0,Vector2f(42.0f, 42.f));
                        obj->SetLocalScale(Vector3f(1.f));
                    }else if( 2== mode){
                        Vector4f color;
                        color = Vector4f ( 0.2f, 0.9f, 0.2f ,1.f );
                        obj->SetSurfaceColor(0, color);
                        // obj->SetSurfaceDims(0,Vector2f(26.0f, 26.f));
                        obj->SetLocalScale(Vector3f(1.2f));
                        if(2 == status_click ){

                            is_show_list =false;
                        }
                    }
                });

        //----------------------------------------------------------------------------------------------
        for(int a=0;a<64;a++) {
            std::stringstream ss;
            ss << a;
            std::string str = ss.str();
            ref->AddButton(
                    str, str,Vector3f(0.f, 0.f, 0.0f),Vector2f(720.0f, 42.f),
                    2,[=](   OVRFW::VRMenuObject* obj , float u, float v, int status_click, int mode )
                    {

                        if ( 0 == mode ) {
                            int x= atoi( obj->GetName().c_str() );
                            obj->SetLocalPosition( Vector3f(0, calculation(x,0.08,pos+3),0) );
                            if(  x-pos >= see|| x-pos <0  || x >= total || x <0){
                                obj->SetVisible(false);
                            }else{
                                OVRFW::VRMenuFontParms f = obj->GetFontParms();
                                f.MultiLine =false;
                                f.WrapWidth =14;
                                obj->SetFontParms(f);
                                const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
                                JNIEnv* env;
                                ctx.Vm->AttachCurrentThread(&env, NULL);
                                //=============================================================
                                jstring name = nameMediaList( x);
                                //=============================================================
                                const char *nativeString = env->GetStringUTFChars (name, 0);
                                size_t length = strlen(reinterpret_cast<const char *const>(nativeString));
                                char subbuff[3000];
                                strcpy( subbuff, &nativeString[0] );
                                subbuff[64] = '\0';
                                obj->SetText(subbuff);
                                env->ReleaseStringUTFChars(name, nativeString);
                                obj->SetVisible(true);
                            }

                        }else if( 1== mode){
                            obj->SetSurfaceColor(0, Vector4f(0.0f, 0.0f, 0.0f, 1.f));
                            obj->SetLocalScale(Vector3f(1.0f));

                        }else if( 2== mode){
                            obj->SetSurfaceColor(0, Vector4f(0.2f, 0.9f, 0.2f, 1.f));
                            obj->SetLocalScale(Vector3f(1.1f, 1.4f, 1.f));
                            int x= atoi( obj->GetName().c_str() );
                            obj->SetLocalPosition( Vector3f(0, calculation(x,0.08,pos+3),0.05) );
                            if(2 == status_click )
                            {

                                obj->SetSurfaceColor(0, Vector4f(0.9f, 0.2f, 0.2f, 1.f));

                                pickMediaList( x);

                            }
                        }
                    }
            );
        }
        //----------------------------------------------------------------------------------------------


    }

    void ovrQPlayerAppl::Init_Folder()
    {
        ovrControllerGUI* ref = g_Menu_Folder;
        //----------------------------------------------------------------------------------------------
        static float offset =0;
        static long total = 1;
        static long see =7;
        static float pos =0;
        ref->AddButton("scllor", "",Vector3f(400.f, .0f, 0.0f),Vector2f(20.0f, 260.f),
                       2,[=]( OVRFW::VRMenuObject* obj , float u, float v, int status_click, int mode )
                       {

                           if ( 0 == mode ) {
                               if( g_Menu_ToShow ==  ref ) {
                                   int total_n = numFolderList();
                                   pos = pos + (total - total_n);
                                   if (pos < 0) pos = 0;
                                   if (pos >= total_n - see) pos = total_n - see;
                                   total = total_n;
                                   //
                                   glBindFramebuffer(GL_FRAMEBUFFER, glavmCanvasV.handle_fb);
                                   glClearColor(0.9, 0.9, 0.9, 1.);
                                   glDisable(GL_SCISSOR_TEST);
                                   glClear(GL_COLOR_BUFFER_BIT);
                                   glEnable(GL_SCISSOR_TEST);
                                   glScissor(0, glavmCanvasH.size_w * ((float) pos / total),
                                             2, glavmCanvasH.size_w * ((float) see / total));

                                   glClearColor(0.9, 0.0, 0.0, 1.);
                                   glClear(GL_COLOR_BUFFER_BIT);
                                   glDisable(GL_SCISSOR_TEST);
                                   OVRFW::GlTexture ProgramTexture = OVRFW::GlTexture(glavmCanvasV.handle_tx,
                                                                                      GL_TEXTURE_2D, 0, 0);
                                   Vector4f color(1.f, 1.f, 1.f, 1.f);
                                   obj->SetSurfaceColor(0, color);
                                   obj->SetSurfaceTexture(0, 0, OVRFW::SURFACE_TEXTURE_DIFFUSE, ProgramTexture);
                               }
                           }else if( 1== mode){



                           }else if( 2== mode){

                               if(1 == status_click ){

                                   float x = v * total - 0.5 *see;

                                   long  y = total - see;

                                   x = x < 0 ? 0: x;

                                   x = x > y ? y :x;

                                   pos = x;


                               }
                           }
                       }
        );

        ref->AddButton("cancal", "ESC",
                       Vector3f( 320.0f, -200.f, 0.0f  ),
                       Vector2f(56.0f,  56.f),
                       1,[=](   OVRFW::VRMenuObject* obj , float u, float v, int status_click, int mode ){

                    if ( 0 == mode ) {

                    }else if( 1== mode){
                        Vector4f color;

                        color = Vector4f ( 0.0f, 0.0f, 0.0f ,1.f );
                        obj->SetSurfaceColor(0, color);
                        //  obj->SetSurfaceDims(0,Vector2f(42.0f, 42.f));
                        obj->SetLocalScale(Vector3f(1.f));
                    }else if( 2== mode){
                        Vector4f color;
                        color = Vector4f ( 0.2f, 0.9f, 0.2f ,1.f );
                        obj->SetSurfaceColor(0, color);
                        // obj->SetSurfaceDims(0,Vector2f(26.0f, 26.f));
                        obj->SetLocalScale(Vector3f(1.2f));
                        if(2 == status_click ){

                            is_show_list =false;
                        }
                    }
                });

        //----------------------------------------------------------------------------------------------
        for(int a=0;a<128;a++) {
            std::stringstream ss;
            ss << a;
            std::string str = ss.str();
            ref->AddButton(
                    str, str,Vector3f(0.f, 0.f, 0.0f),Vector2f(720.0f, 42.f),
                    2,[=](   OVRFW::VRMenuObject* obj , float u, float v, int status_click, int mode )
                    {

                        if ( 0 == mode ) {
                            int x= atoi( obj->GetName().c_str() );
                            obj->SetLocalPosition( Vector3f(0, calculation(x,0.08,pos+3),0) );
                            if(  x-pos >= see|| x-pos <0  || x >= total || x <0){
                                obj->SetVisible(false);
                            }else{

                                OVRFW::VRMenuFontParms f = obj->GetFontParms();

                                f.MultiLine =false;
                                // f.Outline = true;
                                // f.AlignVert = OVRFW::VERTICAL_CENTER_FIXEDHEIGHT ;
                                f.WrapWidth =14;
                                //   f.AlignHoriz =OVRFW::HORIZONTAL_LEFT;



                                obj->SetFontParms(f);
                                //OVRFW::VRMenuSurface =
                                //


                                const ovrJava& ctx = *(reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
                                JNIEnv* env;
                                ctx.Vm->AttachCurrentThread(&env, NULL);
                                //=============================================================
                                jstring name = nameFolderList(x);
                                //=============================================================
                                const char *nativeString = env->GetStringUTFChars (name, 0);
                                size_t length = strlen(reinterpret_cast<const char *const>(nativeString));
                                char subbuff[3000];
                                strcpy( subbuff, &nativeString[0] );

                                subbuff[64] = '\0';


                                obj->SetText(subbuff);


                                env->ReleaseStringUTFChars(name, nativeString);
                                obj->SetVisible(true);
                            }

                        }else if( 1== mode){
                            obj->SetSurfaceColor(0, Vector4f(0.0f, 0.0f, 0.0f, 1.f));
                            obj->SetLocalScale(Vector3f(1.0f));

                        }else if( 2== mode){
                            obj->SetSurfaceColor(0, Vector4f(0.2f, 0.9f, 0.2f, 1.f));
                            obj->SetLocalScale(Vector3f(1.1f, 1.4f, 1.f));
                            int x= atoi( obj->GetName().c_str() );
                            obj->SetLocalPosition( Vector3f(0, calculation(x,0.08,pos+3),0.05) );
                            if(2 == status_click ) {

                                //obj->GetSurface(0).SetAnchors(Vector2f(0.45,0.5));
                                obj->SetSurfaceColor(0, Vector4f(0.9f, 0.2f, 0.2f, 1.f));

    // requires call to CreateFromSurfaceParms or RegenerateSurfaceGeometry() to take effect

                                // obj->SetSurfaceDims()
                                pickFolderList(x);

                                //  is_show_list = !is_show_list;

                                // g_Menu_ToShow =  g_Menu_Media;
                                //   is_show_list =false;
                                //   g_Menu_ToShow =  g_Menu_C;
                                //   int id = panel;
                                //  SetObjectVisible(GetGuiSys(), g_Menu_Folder, u_ObjectName[id].c_str(), false);
                            }
                        }
                    }
            );
        }
        //----------------------------------------------------------------------------------------------


    }

////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////
int FindInputDevice( std::vector<ovrInputDeviceBase*> &devices, ovrDeviceID deviceID )
{
    for (int i = 0; i < (int)devices.size(); ++i) {
        if (devices[i]->GetDeviceID() == deviceID) {
            return i;
        }
    }
    return -1;
}
bool IsDeviceTracked( std::vector<ovrInputDeviceBase*> &devices, ovrDeviceID deviceID)
{
    return FindInputDevice(devices, deviceID) >= 0;
}
void RemoveDevice( std::vector<ovrInputDeviceBase*> &devices, ovrDeviceID deviceID)
{
    int index = FindInputDevice(devices, deviceID);
    if (index < 0) {
        return;
    }
    ovrInputDeviceBase* device = devices[index];
    delete device;
    devices[index] = devices.back();
    devices[devices.size() - 1] = nullptr;
    devices.pop_back();
}
void OnDeviceConnected(  const ovrInputCapabilityHeader& capsHeader,
                         std::vector<ovrInputDeviceBase*> &devices, ovrQPlayerAppl& app )
{
    ovrInputDeviceBase* device = nullptr;
    ovrResult result = ovrError_NotInitialized;
    switch (capsHeader.Type) {
        case ovrControllerType_TrackedRemote: {
            ALOG("VrInputStandard - Controller connected, ID = %u", capsHeader.DeviceID);
            ovrInputTrackedRemoteCapabilities remoteCapabilities;
            remoteCapabilities.Header = capsHeader;
            result = vrapi_GetInputDeviceCapabilities(app.GetSessionObject(), &remoteCapabilities.Header);
            if (result == ovrSuccess) {
                ovrInputDeviceTrackedRemoteHand* remoteHandDevice =
                        ovrInputDeviceTrackedRemoteHand::Create(app, remoteCapabilities);
                if (remoteHandDevice != nullptr) {
                    device = remoteHandDevice;
                    ovrHandMesh mesh;
                    mesh.Header.Version = ovrHandVersion_1;
                    ovrHandedness handedness =
                            remoteHandDevice->IsLeftHand() ? VRAPI_HAND_LEFT : VRAPI_HAND_RIGHT;
                    if (vrapi_GetHandMesh(app.GetSessionObject(), handedness, &mesh.Header) !=
                        ovrSuccess) {
                        ALOG("VrInputStandard - failed to get hand mesh");
                    }
                    ovrHandSkeleton skeleton;
                    skeleton.Header.Version = ovrHandVersion_1;
                    if (vrapi_GetHandSkeleton(app.GetSessionObject(), handedness, &skeleton.Header) !=
                        ovrSuccess) {
                        ALOG("VrInputStandard - failed to get hand skeleton");
                    }
                    remoteHandDevice->InitFromSkeletonAndMesh(  &skeleton, &mesh);

                    if (remoteHandDevice->IsLeftHand()) {
                        remoteHandDevice->SetControllerModel(g_ControllerModelOculusTouchLeft);
                    } else {
                        remoteHandDevice->SetControllerModel(g_ControllerModelOculusTouchRight);
                    }
                }
            }
            break;
        }
        case ovrControllerType_Hand: {
            ALOG("VrInputStandard - Hand connected, ID = %u", capsHeader.DeviceID);

            ovrInputHandCapabilities handCapabilities;
            handCapabilities.Header = capsHeader;
            ALOG("VrInputStandard - calling get device caps");
            result = vrapi_GetInputDeviceCapabilities(app.GetSessionObject(), &handCapabilities.Header);
            ALOG("VrInputStandard - post calling get device caps %d", result);
            if (result == ovrSuccess) {
                ovrInputDeviceTrackedHand* handDevice = ovrInputDeviceTrackedHand::Create( app, handCapabilities);
                if (handDevice != nullptr) {
                    device = handDevice;
                    ovrHandedness handedness = handDevice->IsLeftHand() ? VRAPI_HAND_LEFT : VRAPI_HAND_RIGHT;

                    ovrHandSkeleton skeleton;
                    skeleton.Header.Version = ovrHandVersion_1;
                    if (vrapi_GetHandSkeleton(app.GetSessionObject(), handedness, &skeleton.Header) !=
                        ovrSuccess) {
                        ALOG("VrInputStandard - failed to get hand skeleton");
                    } else {
                        ALOG("VrInputStandard - got a skeleton ... NumBones:%u", skeleton.NumBones);
                        for (uint32_t i = 0; i < skeleton.NumBones; ++i) {
                            Posef pose = skeleton.BonePoses[i];
                            ALOG( "Posef{ Quatf{ %.6f, %.6f, %.6f, %.6f }, Vector3f{ %.6f, %.6f, %.6f } }, // bone=%u parent=%d",
                                  pose.Rotation.x, pose.Rotation.y, pose.Rotation.z, pose.Rotation.w,
                                  pose.Translation.x,pose.Translation.y,
                                  pose.Translation.z,
                                  i,
                                  (int)skeleton.BoneParentIndices[i]);
                        }
                    }

                    ovrHandMesh mesh;
                    mesh.Header.Version = ovrHandVersion_1;
                    if (vrapi_GetHandMesh(app.GetSessionObject(), handedness, &mesh.Header) !=
                        ovrSuccess) {
                        ALOG("VrInputStandard - failed to get hand mesh");
                    }

                    handDevice->InitFromSkeletonAndMesh(&skeleton, &mesh);
                }
            }
            break;
        }
        case ovrControllerType_StandardPointer: {
            ALOG("VrInputStandard - StandardPointer connected, ID = %u", capsHeader.DeviceID);

            ovrInputStandardPointerCapabilities pointerCaps;
            pointerCaps.Header = capsHeader;
            ALOG("VrInputStandard - StandardPointer calling get device caps");
            result = vrapi_GetInputDeviceCapabilities(app.GetSessionObject(), &pointerCaps.Header);
            ALOG("VrInputStandard - StandardPointer post calling get device caps %d", result);

            if (result == ovrSuccess) {
                device = ovrInputDeviceStandardPointer::Create(app, pointerCaps);
                ALOG("VrInputStandard - StandardPointer created device");
            }

            break;
        }

        default:
        ALOG("VrInputStandard - Unknown device connected!");
            return;
    }

    if (result != ovrSuccess) {
        ALOG("VrInputStandard - vrapi_GetInputDeviceCapabilities: Error %i", result);
    }
    if (device != nullptr) {
        ALOG(
                "VrInputStandard - Added device '%s', id = %u", device->GetName(), capsHeader.DeviceID);
        devices.push_back(device);
    } else {
        ALOG("VrInputStandard - Device creation failed for id = %u", capsHeader.DeviceID);
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////
void ResetLaserPointer( ovrInputDeviceHandBase& trDevice )
{
    OVRFW::ovrBeamRenderer::handle_t& LaserPointerBeamHandle =
            trDevice.GetLaserPointerBeamHandle();
    OVRFW::ovrParticleSystem::handle_t& LaserPointerParticleHandle =
            trDevice.GetLaserPointerParticleHandle();
    if (LaserPointerBeamHandle.IsValid()) {
        g_BeamRenderer->RemoveBeam(LaserPointerBeamHandle);
        LaserPointerBeamHandle.Release();
    }
    if (LaserPointerParticleHandle.IsValid()) {
        g_ParticleSystem->RemoveParticle(LaserPointerParticleHandle);
        LaserPointerParticleHandle.Release();
    }
}
bool IsDeviceTypeEnabled( ovrInputDeviceBase& device)
{
    auto deviceType = device.GetType();
    if (deviceType == ovrControllerType_StandardPointer) {
        return 1;
    } else {
        return 0;
    }
}
void EnumerateInputDevices( std::vector<ovrInputDeviceBase*> &devices, ovrQPlayerAppl& app)
{
    for (uint32_t deviceIndex = 0;; deviceIndex++) {
        ovrInputCapabilityHeader curCaps;
        if (vrapi_EnumerateInputDevices(app.GetSessionObject(), deviceIndex, &curCaps) < 0) {
            break; // no more devices
        }
        if (!IsDeviceTracked(devices,curCaps.DeviceID)) {
            ALOG("Input -      tracked");
            OnDeviceConnected(curCaps, devices, app );
        }
    }
}
void OnDeviceDisconnected( std::vector<ovrInputDeviceBase*> &devices, ovrQPlayerAppl& app, ovrDeviceID deviceID)
{
    ALOG("VrInputStandard - Controller disconnected, ID = %i", deviceID);
    int deviceIndex = FindInputDevice(devices, deviceID);
    if (deviceIndex >= 0) {
        ovrInputDeviceBase* device = g_InputDevices[deviceIndex];
        if (device != nullptr) {
            auto deviceType = device->GetType();
            if (deviceType == ovrControllerType_TrackedRemote ||
                deviceType == ovrControllerType_Hand ||
                deviceType == ovrControllerType_StandardPointer) {
                ovrInputDeviceHandBase& trDevice = *static_cast<ovrInputDeviceHandBase*>(device);
                ResetLaserPointer(trDevice);
            }
        }
    }
    OVRFWQ::RemoveDevice(devices, deviceID);
}


} // namespace OVRFW

// g_MovieTexture