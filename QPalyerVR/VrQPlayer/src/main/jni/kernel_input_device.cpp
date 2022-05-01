#include "kernel_input_device.h"

using OVR::Axis_X;
using OVR::Axis_Y;
using OVR::Axis_Z;
using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;


namespace OVRFWQ {




    static OVRFW::GlProgram g_ProgHandSkinned;
    static const char* HandPBRSkinned1VertexShaderSrc = R"glsl(
    uniform JointMatrices
    {
     highp mat4 Joints[64];
    } jb;
    attribute highp vec4 Position;
    attribute highp vec3 Normal;
    attribute highp vec3 Tangent;
    attribute highp vec3 Binormal;
    attribute highp vec2 TexCoord;
    attribute highp vec4 JointWeights;
    attribute highp vec4 JointIndices;
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
    highp vec4 localPos1 = jb.Joints[int(JointIndices.x)] * Position;
    highp vec4 localPos2 = jb.Joints[int(JointIndices.y)] * Position;
    highp vec4 localPos3 = jb.Joints[int(JointIndices.z)] * Position;
    highp vec4 localPos4 = jb.Joints[int(JointIndices.w)] * Position;
    highp vec4 localPos = localPos1 * JointWeights.x
    + localPos2 * JointWeights.y
    + localPos3 * JointWeights.z
    + localPos4 * JointWeights.w;
    gl_Position = TransformVertex( localPos );

    vec3 eye = transposeMultiply( sm.ViewMatrix[VIEW_ID], -vec3( sm.ViewMatrix[VIEW_ID][3] ) );
    oEye = eye - vec3( ModelMatrix * Position );

    highp vec3 localNormal1 = multiply( jb.Joints[int(JointIndices.x)], Normal);
    highp vec3 localNormal2 = multiply( jb.Joints[int(JointIndices.y)], Normal);
    highp vec3 localNormal3 = multiply( jb.Joints[int(JointIndices.z)], Normal);
    highp vec3 localNormal4 = multiply( jb.Joints[int(JointIndices.w)], Normal);
    highp vec3 localNormal   = localNormal1 * JointWeights.x
    + localNormal2 * JointWeights.y
    + localNormal3 * JointWeights.z
    + localNormal4 * JointWeights.w;
    oNormal = multiply( ModelMatrix, localNormal );

    oTexCoord = TexCoord;
    }
    )glsl";
    static const char* HandPBRSkinned1FragmentShaderSrc = R"glsl(
    uniform sampler2D Texture0;
    uniform lowp vec3 g_SpecularLightDirection;
    uniform lowp vec3 g_SpecularLightColor;
    uniform lowp vec3 g_AmbientLightColor;
    uniform lowp vec3 GlowColor;

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

    lowp float pow5( float x )
    {
    float x2 = x * x;
    return x2 * x2 * x;
    }

    lowp float pow16( float x )
    {
    float x2 = x * x;
    float x4 = x2 * x2;
    float x8 = x4 * x4;
    float x16 = x8 * x8;
    return x16;
    }

    void main()
    {
    lowp vec3 eyeDir = normalize( oEye.xyz );
    lowp vec3 Normal = normalize( oNormal );

    lowp vec4 diffuse = texture2D( Texture0, oTexCoord );
    lowp vec3 ambientValue = diffuse.xyz * g_AmbientLightColor;

    lowp float nDotL = max( dot( Normal, g_SpecularLightDirection ), 0.0 );
    lowp vec3 diffuseValue = diffuse.xyz * g_SpecularLightColor * nDotL;

    lowp vec3 H = normalize( g_SpecularLightDirection + eyeDir );
    lowp float nDotH = max( dot( Normal, H ), 0.0 );

    lowp float specularPower = 1.0f - diffuse.a;
    specularPower = specularPower * specularPower;
    lowp float specularIntensity = pow16( nDotH );
    lowp vec3 specularValue = specularIntensity * g_SpecularLightColor;

    lowp float vDotN = dot( eyeDir, Normal );
    lowp float fresnel = clamp( pow5( 1.0 - vDotN ), 0.0, 1.0 );
    lowp vec3 fresnelValue = GlowColor * fresnel;
    lowp vec3 controllerColor = diffuseValue
                            + ambientValue
                            + specularValue
                            + fresnelValue
                            ;
    gl_FragColor.xyz = controllerColor;
    gl_FragColor.w = clamp( fresnel, 0.0, 1.0 );
    }
    )glsl";
//     std::vector<OVR::Matrix4f> g_TransformMatrices;
    static OVRFW::ovrProgramParm HandSkinnedUniformParms[] = {
            {"Texture0", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            {"g_SpecularLightDirection", OVRFW::ovrProgramParmType::FLOAT_VECTOR3},
            {"g_SpecularLightColor", OVRFW::ovrProgramParmType::FLOAT_VECTOR3},
            {"g_AmbientLightColor", OVRFW::ovrProgramParmType::FLOAT_VECTOR3},
            {"JointMatrices", OVRFW::ovrProgramParmType::BUFFER_UNIFORM},
            {"GlowColor", OVRFW::ovrProgramParmType::FLOAT_VECTOR3},
    };



ovrInputDeviceHandBase::ovrInputDeviceHandBase(ovrHandedness hand) : ovrInputDeviceBase(),
    Hand(hand),
    TransformMatrices(MAX_JOINTS, OVR::Matrix4f::Identity()),
    BindMatrices(MAX_JOINTS, OVR::Matrix4f::Identity()),
    SkinMatrices(MAX_JOINTS, OVR::Matrix4f::Identity()),
    GlowColor(1.0f, 1.0f, 1.0f),
    PreviousFramePinch(false),
    IsActiveInputDevice(false) {

    if( !g_ProgHandSkinned.IsValid()){



      g_ProgHandSkinned = OVRFW::GlProgram::Build(
                HandPBRSkinned1VertexShaderSrc,
                HandPBRSkinned1FragmentShaderSrc,
                HandSkinnedUniformParms,
                sizeof(HandSkinnedUniformParms) / sizeof(OVRFW::ovrProgramParm));
    }

}


// ovrQPlayerAppl::ovrInputDeviceHandBase
    void ovrInputDeviceHandBase::InitFromSkeletonAndMesh( ovrHandSkeleton* skeleton, ovrHandMesh* mesh) {
        if (mesh == nullptr) {
            ALOGW("InitFromSkeletonAndMesh - mesh == nullptr");
            return;
        }
        ALOG(
                "InitFromSkeletonAndMesh - mesh=%p NumVertices=%u NumIndices=%u",
                mesh,
                mesh->NumVertices,
                mesh->NumIndices);

        auto g_SpecularLightDirection = Vector3f(1.0f, 1.0f, 0.0f);
        auto g_SpecularLightColor = Vector3f(1.0f, 0.95f, 0.8f) * 0.75f;
        auto g_AmbientLightColor = Vector3f(1.0f, 1.0f, 1.0f) * 0.15f;



        /// Ensure all identity
        g_TransformMatrices.resize(MAX_JOINTS, OVR::Matrix4f::Identity());
        BindMatrices.resize(MAX_JOINTS, OVR::Matrix4f::Identity());
        SkinMatrices.resize(MAX_JOINTS, OVR::Matrix4f::Identity());
        SkinUniformBuffer.Create(
                OVRFW::GLBUFFER_TYPE_UNIFORM, MAX_JOINTS * sizeof(Matrix4f), SkinMatrices.data());

        HandModel.Init(*skeleton);
        FingerJointHandles.resize(HandModel.GetSkeleton().GetJoints().size());

        /// Walk the transform hierarchy and store the wolrd space transforms in g_TransformMatrices
        const std::vector<OVR::Posef>& poses = HandModel.GetSkeleton().GetWorldSpacePoses();
        for (size_t j = 0; j < poses.size(); ++j) {
            g_TransformMatrices[j] = Matrix4f(poses[j]);
        }

        for (size_t j = 0; j < BindMatrices.size(); ++j) {
            BindMatrices[j] = g_TransformMatrices[j].Inverted();
        }

        /// Init skinned rendering
        /// Build geometry from mesh
        OVRFW::VertexAttribs attribs;
        std::vector<OVRFW::TriangleIndex> indices;

        attribs.position.resize(mesh->NumVertices);
        memcpy(
                attribs.position.data(),
                &mesh->VertexPositions[0],
                mesh->NumVertices * sizeof(ovrVector3f));
        attribs.normal.resize(mesh->NumVertices);
        memcpy(attribs.normal.data(), &mesh->VertexNormals[0], mesh->NumVertices * sizeof(ovrVector3f));
        attribs.uv0.resize(mesh->NumVertices);
        memcpy(attribs.uv0.data(), &mesh->VertexUV0[0], mesh->NumVertices * sizeof(ovrVector2f));
        attribs.jointIndices.resize(mesh->NumVertices);
        /// We can't do a straight copy heere since the sizes don't match
        for (std::uint32_t i = 0; i < mesh->NumVertices; ++i) {
            const ovrVector4s& blendIndices = mesh->BlendIndices[i];
            attribs.jointIndices[i].x = blendIndices.x;
            attribs.jointIndices[i].y = blendIndices.y;
            attribs.jointIndices[i].z = blendIndices.z;
            attribs.jointIndices[i].w = blendIndices.w;
        }
        attribs.jointWeights.resize(mesh->NumVertices);
        memcpy(
                attribs.jointWeights.data(),
                &mesh->BlendWeights[0],
                mesh->NumVertices * sizeof(ovrVector4f));

        static_assert(
                sizeof(ovrVertexIndex) == sizeof(OVRFW::TriangleIndex),
                "sizeof(ovrVertexIndex) == sizeof(TriangleIndex) don't match!");
        indices.resize(mesh->NumIndices);
        memcpy(indices.data(), mesh->Indices, mesh->NumIndices * sizeof(ovrVertexIndex));

        ALOG(
                "InitFromSkeletonAndMesh - attribs.position=%u indices=%u",
                (uint)attribs.position.size(),
                (uint)indices.size());

        SurfaceDef.surfaceName = "HandSurface";
        SurfaceDef.geo.Create(attribs, indices);

        /// Build the graphics command
        OVRFW::ovrGraphicsCommand& gc = SurfaceDef.graphicsCommand;
        gc.Program = g_ProgHandSkinned;
        gc.UniformData[0].Data = &gc.Textures[0];
        gc.UniformData[1].Data = &g_SpecularLightDirection;
        gc.UniformData[2].Data = &g_SpecularLightColor;
        gc.UniformData[3].Data = &g_AmbientLightColor;
        /// bind the data matrix
        assert(MAX_JOINTS == SkinMatrices.size());
        gc.UniformData[4].Count = MAX_JOINTS;
        gc.UniformData[4].Data = &SkinUniformBuffer;
        /// bind the glow color
        gc.UniformData[5].Data = &GlowColor;
        /// gpu state needs alpha blending
        gc.GpuState.depthEnable = gc.GpuState.depthMaskEnable = true;
        gc.GpuState.blendEnable = OVRFW::ovrGpuState::BLEND_ENABLE;
        gc.GpuState.blendSrc = GL_SRC_ALPHA;
        gc.GpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;

        /// add the surface
        Surfaces.clear();
        OVRFW::ovrDrawSurface handSurface;
        handSurface.surface = &(SurfaceDef);
        Surfaces.push_back(handSurface);

        UpdateSkeleton(OVR::Posef::Identity());
    }
    void ovrInputDeviceHandBase::UpdateSkeleton(const OVR::Posef& handPose)
    {
        const std::vector<OVR::Posef>& poses = HandModel.GetSkeleton().GetWorldSpacePoses();
        for (size_t j = 0; j < poses.size(); ++j) {
            /// Compute transform
            g_TransformMatrices[j] = Matrix4f(poses[j]);
            Matrix4f m = g_TransformMatrices[j] * BindMatrices[j];
            SkinMatrices[j] = m.Transposed();
        }

        /// Update the shader uniform parameters
        SkinUniformBuffer.Update(SkinMatrices.size() * sizeof(Matrix4f), SkinMatrices.data());

        Matrix4f matDeviceModel = GetModelMatrix(handPose);

        /// Ensure the surface is using this uniform parameters
        for (auto& surface : Surfaces) {
            OVRFW::ovrSurfaceDef* sd = const_cast<OVRFW::ovrSurfaceDef*>(surface.surface);
            sd->graphicsCommand.UniformData[4].Count = MAX_JOINTS;
            sd->graphicsCommand.UniformData[4].Data = &SkinUniformBuffer;
            surface.modelMatrix = matDeviceModel;
        }
    }
    bool ovrInputDeviceHandBase::Update(
            ovrMobile* ovr, const double displayTimeInSeconds, const float dt ) {
        const ovrTracking2 headTracking = vrapi_GetPredictedTracking2(ovr, displayTimeInSeconds);
        HeadPose = headTracking.HeadPose.Pose;

        /// Save Pinch state from last frame
        PreviousFramePinch = IsPinching();
        PreviousFrameMenu = IsMenuPressed();

        ovrResult result =
                vrapi_GetInputTrackingState(ovr, GetDeviceID(), displayTimeInSeconds, &Tracking);
        if (result != ovrSuccess) {
            return false;
        } else {
            HandPose = Tracking.HeadPose.Pose;
        }

        return true;
    }
    void ovrInputDeviceHandBase::UpdateHaptics(ovrMobile* ovr, float displayTimeInSeconds)
    {
        if (!HasCapSimpleHaptics() && !HasCapBufferedHaptics()) {
            return;
        }

        const DeviceHapticState& desiredState = GetRequestedHapticsState();
        const auto hapticMaxSamples = GetHapticSamplesMax();
        const auto hapticSampleDurationMs = GetHapticSampleDurationMS();

        if (desiredState.HapticState == HapticStates::HAPTICS_BUFFERED) {
            if (HasCapBufferedHaptics()) {
                // buffered haptics
                float intensity = 0.0f;
                intensity = fmodf(displayTimeInSeconds, 1.0f);

                ovrHapticBuffer hapticBuffer;
                uint8_t dataBuffer[hapticMaxSamples];
                hapticBuffer.BufferTime = displayTimeInSeconds;
                hapticBuffer.NumSamples = hapticMaxSamples;
                hapticBuffer.HapticBuffer = dataBuffer;
                hapticBuffer.Terminated = false;

                for (uint32_t i = 0; i < hapticMaxSamples; i++) {
                    dataBuffer[i] = intensity * 255;
                    intensity += hapticSampleDurationMs * 0.001f;
                    intensity = fmodf(intensity, 1.0f);
                }

                vrapi_SetHapticVibrationBuffer(ovr, GetDeviceID(), &hapticBuffer);
                PreviousHapticState.HapticState = HAPTICS_BUFFERED;
            } else {
                ALOG("Device does not support buffered haptics?");
            }
        } else if (desiredState.HapticState == HapticStates::HAPTICS_SIMPLE_CLICKED) {
            // simple haptics
            if (HasCapSimpleHaptics()) {
                if (PreviousHapticState.HapticState != HAPTICS_SIMPLE_CLICKED) {
                    vrapi_SetHapticVibrationSimple(ovr, GetDeviceID(), 1.0f);
                    PreviousHapticState = {HAPTICS_SIMPLE_CLICKED, 1.0f};
                }
            } else {
                ALOG("Device does not support simple haptics?");
            }
        } else if (desiredState.HapticState == HapticStates::HAPTICS_SIMPLE) {
            // huge epsilon value since there is so much noise in the grip trigger
            // and currently a problem with sending too many haptics values.
            if (PreviousHapticState.HapticSimpleValue < (desiredState.HapticSimpleValue - 0.05f) ||
                                                        PreviousHapticState.HapticSimpleValue > (desiredState.HapticSimpleValue + 0.05f)) {
                vrapi_SetHapticVibrationSimple(ovr, GetDeviceID(), desiredState.HapticSimpleValue);
                PreviousHapticState = desiredState;
            }
        } else {
            if (PreviousHapticState.HapticState == HAPTICS_BUFFERED) {
                ovrHapticBuffer hapticBuffer;
                uint8_t dataBuffer[hapticMaxSamples];
                hapticBuffer.BufferTime = displayTimeInSeconds;
                hapticBuffer.NumSamples = hapticMaxSamples;
                hapticBuffer.HapticBuffer = dataBuffer;
                hapticBuffer.Terminated = true;

                for (uint32_t i = 0; i < hapticMaxSamples; i++) {
                    dataBuffer[i] = (((float)i) / (float)hapticMaxSamples) * 255;
                }

                vrapi_SetHapticVibrationBuffer(ovr, GetDeviceID(), &hapticBuffer);
                PreviousHapticState = {};
            } else if (
                    PreviousHapticState.HapticState == HAPTICS_SIMPLE ||
                    PreviousHapticState.HapticState == HAPTICS_SIMPLE_CLICKED) {
                vrapi_SetHapticVibrationSimple(ovr, GetDeviceID(), 0.0f);
                PreviousHapticState = {};
            }
        }
    }




    //==============================
// ovrQPlayerAppl::ovrInputDeviceTrackedRemoteHand
ovrInputDeviceTrackedRemoteHand* ovrInputDeviceTrackedRemoteHand::Create(
        OVRFW::ovrAppl& app, const ovrInputTrackedRemoteCapabilities& remoteCapabilities) {
    ALOG("VrInputStandard - ovrInputDeviceTrackedRemoteHand::Create");



    ovrInputStateTrackedRemote remoteInputState;
    remoteInputState.Header.ControllerType = remoteCapabilities.Header.Type;
    remoteInputState.Header.TimeInSeconds = 0.0f;
    ovrResult result = vrapi_GetCurrentInputState(
            app.GetSessionObject(), remoteCapabilities.Header.DeviceID, &remoteInputState.Header);
    if (result == ovrSuccess) {
        ovrHandedness controllerHand =
                (remoteCapabilities.ControllerCapabilities & ovrControllerCaps_LeftHand) != 0
                ? VRAPI_HAND_LEFT
                : VRAPI_HAND_RIGHT;
        ovrInputDeviceTrackedRemoteHand* device =
                new ovrInputDeviceTrackedRemoteHand(remoteCapabilities, controllerHand);
        return device;
    } else {
        ALOG("VrInputStandard - vrapi_GetCurrentInputState: Error %i", result);
    }

    return nullptr;
}
    // Matrix to get from tracking pose to the OpenXR compatible 'grip' pose
    static const OVR::Matrix4f xfTrackedFromBinding = OVR::Matrix4f(
            OVR::Posef{
                    OVR::Quatf{
                            OVR::Vector3f{1, 0, 0},
                            OVR::DegreeToRad(60.0f)},
                    OVR::Vector3f{0, -0.03, 0.04}
            }
    );

    static const OVR::Matrix4f xfTrackedFromBindingInv = xfTrackedFromBinding.Inverted();

OVR::Matrix4f ovrInputDeviceTrackedRemoteHand::GetModelMatrix(const OVR::Posef& handPose) const {
    OVR::Matrix4f mat(handPose);

    mat = mat * xfTrackedFromBindingInv;

    const float controllerPitch = IsLeftHand() ? OVR::DegreeToRad(180.0f) : 0.0f;
    const float controllerYaw = IsLeftHand() ? OVR::DegreeToRad(90.0f) : OVR::DegreeToRad(-90.0f);
    return mat * Matrix4f::RotationY(controllerYaw) * Matrix4f::RotationX(controllerPitch);
}
    static const OVR::Matrix4f xfPointerFromBinding =
            OVR::Matrix4f::Translation(OVR::Vector3(0.0f, 0.0f, -0.055f));

bool ovrInputDeviceTrackedRemoteHand::Update( ovrMobile* ovr,
              const double displayTimeInSeconds, const float dt)
{
    bool ret = ovrInputDeviceHandBase::Update(ovr, displayTimeInSeconds, dt);
    if (ret) {
        PointerPose = HandPose;
        HandPose = OVR::Posef(OVR::Matrix4f(HandPose) * xfTrackedFromBinding);
        /// Pointer is at hand for controller

        ovrInputStateTrackedRemote remoteInputState;
        remoteInputState.Header.ControllerType = GetType();
        remoteInputState.Header.TimeInSeconds = 0.0f;
        ovrResult r = vrapi_GetCurrentInputState(ovr, GetDeviceID(), &remoteInputState.Header);
        if (r == ovrSuccess) {
            IsPinchingInternal = remoteInputState.IndexTrigger > 0.99f;

            PreviousFrameMenuPressed = IsMenuPressedInternal;
            IsMenuPressedInternal = (remoteInputState.Buttons & ovrButton_Enter) != 0;

            UpdateHapticRequestedState(remoteInputState, sampleConfiguration);
        }
    }
    return ret;
}
void ovrInputDeviceTrackedRemoteHand::Render(std::vector<OVRFW::ovrDrawSurface>& surfaceList) {
    // We have controller models
    if (nullptr != ControllerModel && Surfaces.size() > 1u) {
        // Render controller
        if (Surfaces[0].surface != nullptr) {
            Surfaces[0].modelMatrix = Matrix4f(HandPose) * xfTrackedFromBindingInv *
                                      Matrix4f::RotationY(OVR::DegreeToRad(180.0f)) *
                                      Matrix4f::RotationX(OVR::DegreeToRad(-90.0f));
            surfaceList.push_back(Surfaces[0]);
        }
    }
}
OVR::Matrix4f ovrInputDeviceTrackedRemoteHand::GetPointerMatrix() const {
    return OVR::Matrix4f(PointerPose) * xfPointerFromBinding;
}
    static const float kHapticsGripThreashold = 0.1f;

void ovrInputDeviceTrackedRemoteHand::SetControllerModel(OVRFW::ModelFile* m) {
    /// we always want to keep rendering the hand
    if (m == nullptr) {
        // stop rendering controller - ensure that we only have the one surface
        Surfaces.resize(1);
    } else {
        // start rendering controller
        ControllerModel = m;
        // Add a surface for it
        Surfaces.resize(2);
        // Ensure we render controller ( Surfaces[0] ) before hand ( Surfaces[1] )
        Surfaces[1] = Surfaces[0];
        // The current model only has one surface, but this will ensure we don't overflow
        for (auto& model : ControllerModel->Models) {
            OVRFW::ovrDrawSurface controllerSurface;
            controllerSurface.surface = &(model.surfaces[0].surfaceDef);
            Surfaces[0] = controllerSurface;
        }
    }
}
void ovrInputDeviceTrackedRemoteHand::UpdateHapticRequestedState(
        const ovrInputStateTrackedRemote& remoteInputState, const HandSampleConfigurationParameters& sampleConfiguration)  {
    if (remoteInputState.IndexTrigger > kHapticsGripThreashold) {
        RequestedHapticState = {
                sampleConfiguration.OnTriggerHapticsState, remoteInputState.IndexTrigger};
    } else {
        RequestedHapticState = {};
    }
}
void ovrInputDeviceTrackedRemoteHand::ResetHaptics(ovrMobile* ovr, float displayTimeInSeconds) {
    RequestedHapticState = {};
    UpdateHaptics(ovr, displayTimeInSeconds);
}


//==============================
// ovrInputDeviceTrackedHand::Create
    ovrInputDeviceTrackedHand* ovrInputDeviceTrackedHand::Create(
            OVRFW::ovrAppl& app, const ovrInputHandCapabilities& capsHeader) {
        ALOG("VrInputStandard - ovrInputDeviceTrackedHand::Create");

        ovrInputStateHand handInputState;
        handInputState.Header.ControllerType = capsHeader.Header.Type;

        ovrResult result = vrapi_GetCurrentInputState(
                app.GetSessionObject(), capsHeader.Header.DeviceID, &handInputState.Header);
        if (result == ovrSuccess) {
            ovrHandedness controllerHand = (capsHeader.HandCapabilities & ovrHandCaps_LeftHand) != 0
                                           ? VRAPI_HAND_LEFT
                                           : VRAPI_HAND_RIGHT;
            ovrInputDeviceTrackedHand* device =
                    new ovrInputDeviceTrackedHand(capsHeader, controllerHand);
            return device;
        } else {
            ALOG("VrInputStandard - vrapi_GetCurrentInputState: Error %i", result);
        }

        return nullptr;
    }
    void ovrInputDeviceTrackedHand::InitFromSkeletonAndMesh(
            ovrHandSkeleton* skeleton, ovrHandMesh* mesh) {
        /// Base
        ovrInputDeviceHandBase::InitFromSkeletonAndMesh( skeleton, mesh);
    }
    bool ovrInputDeviceTrackedHand::Update(
            ovrMobile* ovr, const double displayTimeInSeconds, const float dt)
    {
        bool ret = ovrInputDeviceHandBase::Update(ovr, displayTimeInSeconds, dt);
        if (ret) {
            ovrResult r = ovrSuccess;
            InputStateHand.Header.ControllerType = GetType();
            InputStateHand.Header.TimeInSeconds = 0.0f;
            r = vrapi_GetCurrentInputState(ovr, GetDeviceID(), &InputStateHand.Header);
            if (r != ovrSuccess) {
                ALOG("VrInputStandard - failed to get hand input state.");
                return false;
            }

            RealHandPose.Header.Version = ovrHandVersion_1;
            r = vrapi_GetHandPose(ovr, GetDeviceID(), displayTimeInSeconds, &(RealHandPose.Header));
            if (r != ovrSuccess) {
                ALOG("VrInputStandard - failed to get hand pose");
                return false;
            } else {
                /// Get the root pose from the API
                HandPose = RealHandPose.RootPose;
                /// Pointer poses
                PointerPose = InputStateHand.PointerPose;
                /// update based on hand pose
                HandModel.Update(RealHandPose);
                UpdateSkeleton(HandPose);
            }

            if (IsPinching() != PreviousFramePinch)
            ALOG("HAND IsPinching = %s", (IsPinching() ? "Y" : "N"));
        }
        return ret;
    }
    void ovrInputDeviceTrackedHand::Render(std::vector<OVRFW::ovrDrawSurface>& surfaceList) {
        GlowColor = OVR::Vector3f(0.75f);

        if (IsInSystemGesture()) {
            // make it more blue if we are in the system gesture
            GlowColor.z = 1.0f;
        }

        ovrInputDeviceHandBase::Render(surfaceList);
    }
    OVR::Vector3f ovrInputDeviceTrackedHand::GetBonePosition(ovrHandBone bone) const {
        return HandModel.GetSkeleton().GetWorldSpacePoses()[bone].Translation;
    }
    //==============================
// ovrInputDeviceStandardPointer::Create
    ovrInputDeviceStandardPointer* ovrInputDeviceStandardPointer::Create(
            OVRFW::ovrAppl& app, const ovrInputStandardPointerCapabilities& capsHeader) {
        ALOG("VrInputStandard - ovrInputDeviceStandardPointer::Create");

        ovrInputStateStandardPointer inputState;
        inputState.Header.ControllerType = capsHeader.Header.Type;
        inputState.Header.TimeInSeconds = 0.0f;

        ovrResult result = vrapi_GetCurrentInputState(
                app.GetSessionObject(), capsHeader.Header.DeviceID, &inputState.Header);

        if (result == ovrSuccess) {
            ovrHandedness controllerHand =
                    (capsHeader.ControllerCapabilities & ovrControllerCaps_LeftHand) != 0
                    ? VRAPI_HAND_LEFT
                    : VRAPI_HAND_RIGHT;
            return new ovrInputDeviceStandardPointer(capsHeader, controllerHand);
        } else {
            ALOG("VrInputStandard - vrapi_GetCurrentInputState: Error %i", result);
        }

        return nullptr;
    }
    bool ovrInputDeviceStandardPointer::Update(
            ovrMobile* ovr, const double displayTimeInSeconds, const float dt) {
        auto ret = ovrInputDeviceHandBase::Update(ovr, displayTimeInSeconds, dt);

        if (ret) {
            /// Pointer is at hand for controller
            PointerPose = GetHandPose();

            memset(&InputStateStandardPointer, 0, sizeof(InputStateStandardPointer));
            InputStateStandardPointer.Header.ControllerType = GetType();
            InputStateStandardPointer.Header.TimeInSeconds = displayTimeInSeconds;
            ovrResult r =
                    vrapi_GetCurrentInputState(ovr, GetDeviceID(), &InputStateStandardPointer.Header);
            if (r == ovrSuccess) {
                HandPose = InputStateStandardPointer.GripPose;

                PointerPose = InputStateStandardPointer.PointerPose;
                UpdateHapticRequestedState(InputStateStandardPointer);
            } else {
                ALOG("Failed to read standard pointer state: %u", r);
            }
        }

        return ret;
    }
    void ovrInputDeviceStandardPointer::ResetHaptics(ovrMobile* ovr, float displayTimeInSeconds) {
        RequestedHapticState = {};
        UpdateHaptics(ovr, displayTimeInSeconds);
    }

    void ovrInputDeviceStandardPointer::UpdateHapticRequestedState(
            const ovrInputStateStandardPointer& inputState) {
        if (inputState.PointerStrength > kHapticsGripThreashold) {
            //  RequestedHapticState = { g_SampleConfiguration.OnTriggerHapticsState, inputState.PointerStrength};
        } else {
            RequestedHapticState = {};
        }
    }
//==================================================================================================

//GUI

//==================================================================================================

//==================================================================================================
//======================================================================
// ovrControllerRibbon

//==============================

#if !defined(PERSISTENT_RIBBONS)
    static void UpdateRibbon(
            OVRFW::ovrPointList& points,
            OVRFW::ovrPointList& velocities,
            const Vector3f& anchorPos,
            const float maxDist,
            const float deltaSeconds) {
        const Vector3f g(0.0f, -9.8f, 0.0f);
        const float invDeltaSeconds = 1.0f / deltaSeconds;

        int count = 0;
        int i = points.GetFirst();
        Vector3f& firstPoint = points.Get(i);

        Vector3f delta = anchorPos - firstPoint;

        firstPoint = anchorPos; // move the first point
        // translate( firstPoint, Vector3f( 0.0f, -1.0f, 0.0f ), deltaSeconds );

        Vector3f prevPoint = firstPoint;

        // move and accelerate all subsequent points
        for (;;) {
            i = points.GetNext(i);
            if (i < 0) {
                break;
            }

            count++;

            Vector3f& curPoint = points.Get(i);
            Vector3f& curVel = velocities.Get(i);
            curVel += g * deltaSeconds;
            curPoint = curPoint + curVel * deltaSeconds;

            delta = curPoint - prevPoint;
            const float dist = delta.Length();
            Vector3f dir = delta * 1.0f / dist;
            if (dist > maxDist) {
                Vector3f newPoint = prevPoint + dir * maxDist;
                Vector3f dragDelta = newPoint - curPoint;
                Vector3f dragVel = dragDelta * invDeltaSeconds;
                curVel = dragVel * 0.1f;
                curPoint = newPoint;
            } else {
                // damping
                curVel = curVel * 0.995f;
            }

            prevPoint = curPoint;
        }

        //	ALOG( "Ribbon: Updated %i points", count );
    }
#endif

// ovrControllerRibbon::ovrControllerRibbon
    ovrControllerRibbon::ovrControllerRibbon(
            const int numPoints,
            const float width,
            const float length,
            const Vector4f& color)
            : NumPoints(numPoints), Length(length) {
#if defined(PERSISTENT_RIBBONS)
        Points = new ovrPointList_Circular(numPoints);
#else
        Points = new OVRFW::ovrPointList_Vector(numPoints);
        Velocities = new OVRFW::ovrPointList_Vector(numPoints);

        for (int i = 0; i < numPoints; ++i) {
            Points->AddToTail(Vector3f(0.0f, i * (length / numPoints), 0.0f));
            Velocities->AddToTail(Vector3f(0.0f));
        }
#endif

        Ribbon = new OVRFW::ovrRibbon(*Points, width, color);
    }

    ovrControllerRibbon::~ovrControllerRibbon() {
        delete Points;
        Points = nullptr;
        delete Velocities;
        Velocities = nullptr;
        delete Ribbon;
        Ribbon = nullptr;
    }

//==============================
// ovrControllerRibbon::Update
    void ovrControllerRibbon::Update(
            const Matrix4f& centerViewMatrix,
            const Vector3f& anchorPos,
            const float deltaSeconds) {
        assert(Points != nullptr);
#if defined(PERSISTENT_RIBBONS)
        if (Points->GetCurPoints() == 0) {
        Points->AddToTail(anchorPos);
    } else {
        Vector3f delta = anchorPos - Points->Get(Points->GetLast());
        if (delta.Length() > 0.01f) {
            if (Points->IsFull()) {
                Points->RemoveHead();
            }
            Points->AddToTail(anchorPos);
        }
    }
#else
        assert(Velocities != nullptr);
        UpdateRibbon(*Points, *Velocities, anchorPos, Length / Points->GetMaxPoints(), deltaSeconds);
#endif
        Ribbon->Update(*Points, centerViewMatrix, true);
    }



}


