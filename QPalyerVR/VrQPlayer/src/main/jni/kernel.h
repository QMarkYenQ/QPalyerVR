/************************************************************************************



************************************************************************************/
#pragma once
#include "Appl.h"
namespace OVRFW {
    class ovrLocale;
    class OvrGuiSys;

};

namespace OVRFWQ {
    class ovrInputDevice_TrackedRemote;



class ovrQPlayerAppl : public OVRFW::ovrAppl {
   public:
    ovrQPlayerAppl( const int32_t mainThreadTid, const int32_t renderThreadTid,
                    const int cpuLevel, const int gpuLevel);
    ~ovrQPlayerAppl();
    //==============================================================================================
    bool AppInit(const OVRFW::ovrAppContext* contet) ;
    //
    void AppShutdown(const OVRFW::ovrAppContext* contet) ;
    //
    void AppResumed(const OVRFW::ovrAppContext* contet) ;
    //
    void AppPaused(const OVRFW::ovrAppContext* contet) ;
    //==============================================================================================
    //----------------------------------------------------------------------------------------------
    //
    //----------------------------------------------------------------------------------------------
    OVRFW::ovrApplFrameOut AppFrame(const OVRFW::ovrApplFrameIn& in) ;
    //----------------------------------------------------------------------------------------------
    //
    //----------------------------------------------------------------------------------------------
    void AppRenderFrame(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out) ;
    //----------------------------------------------------------------------------------------------
    class OVRFW::OvrGuiSys& GetGuiSys();

    class OVRFW::ovrLocale& GetLocale();
    //----------------------------------------------------------------------------------------------
    // Called from JAVA layer
    void SetVideoSize(int width, int height);
    void GetScreenSurface(jobject& surfaceTexture);
    void VideoEnded();
    //----------------------------------------------------------------------------------------------
    // Call to JAVA layer
    void StartVideo();
    void PauseVideo();
    void ResumeVideo();
    void NextVideo();
    void PreviousVideo();
    void SeekVideo(int);
    long CurrentPosition();
    long Duration();
    // Folder
    void pickFolderList(int);
    long numFolderList();
    jstring nameFolderList(int pos);
    // DLNA
    void pickDLNAList(int);
    long numDLNAList();
    jstring nameDLNAList(int pos);
    // Media
    void pickMediaList(int);
    long numMediaList();
    jstring nameMediaList(int pos);
    //----------------------------------------------------------------------------------------------
   private:

//--------------------------
    void RenderRunningMenu(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out);
    void RenderRunningControl(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out);
    void RenderRunningVideo(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out);

//---------------------------
    void SubmitCompositorLayers(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out);
    void AppRenderEye(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out, int eye) ;
    void AppEyeGLStateSetup(const OVRFW::ovrApplFrameIn& in, const ovrFramebuffer* fb, int eye) ;
//---------------------------
    void Init_Folder();
    void Init_Device();
    void Init_Media();
    //----------------------
    void InitPlayer();
    void InitScreen();  //show 2d
   private:
    ovrRenderState mRenderState;
    unsigned int mRenderMode;
    float RandomFloat();
 };
} // namespace OVRFW
