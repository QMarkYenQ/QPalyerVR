// Copyright (c) YenHouse Technologies, LLC and its affiliates. All Rights reserved.
package com.yen.vrqplayer;
/**
 * When using NativeActivity, we currently need to handle loading of dependent shared libraries
 * manually before a shared library that depends on them is loaded, since there is not currently a
 * way to spemFolderfy a shared library dependency for NativeActivity via the manifest meta-data.
 *
 * <p>The simplest method for doing so is to subclass NativeActivity with an empty activity that
 * calls System.loadLibrary on the dependent libraries, which is unfortunate when the goal is to
 * write a pure native C/C++ only Android activity.
 *
 * <p>A native-code only solution is to load the dependent libraries dynamically using dlopen().
 * However, there are a few considerations, see:
 * https://groups.google.com/forum/#!msg/android-ndk/l2E2qh17Q6I/wj6s_6HSjaYJ
 *
 * <p>1. Only
 *
 *
 * call dlopen() if you're sure it will succeed as the bionic dynamic linker will
 * remember if dlopen failed and will not re-try a dlopen on the same lib a second time.
 *
 * <p>2. Must remember what libraries have already been loaded to avoid infinitely looping when
 * libraries have mFolderrcular dependenmFolderes.
 */

import android.content.Context;
import android.content.Intent;
import android.graphics.SurfaceTexture;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.os.Bundle;
import android.os.Environment;
import androidx.annotation.Nullable;
import androidx.core.app.ActivityCompat;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.Log;
import android.view.Surface;
import java.io.File;
import java.util.ArrayList;
import java.util.List;

import static com.yen.dlna.bean.ContainerInfo.TYPE_Contain;
import static com.yen.dlna.bean.ContainerInfo.TYPE_dd;
import static com.yen.dlna.bean.ContainerInfo.TYPE_ee;

import android.net.Uri;
import com.google.android.exoplayer2.MediaItem;

import com.google.android.exoplayer2.Player;
import com.google.android.exoplayer2.SeekParameters;
import com.google.android.exoplayer2.SimpleExoPlayer;

import com.google.android.exoplayer2.analytics.AnalyticsListener;

import com.google.common.collect.ImmutableList;
import com.yen.dlna.DLNAManager;
import com.yen.dlna.DLNAPlayer;
import com.yen.dlna.bean.ContainerInfo;

import com.yen.dlna.listener.ContainerCallback;
import com.yen.dlna.listener.DLNAControlCallback;
import com.yen.dlna.listener.DLNADeviceConnectListener;
import com.yen.dlna.listener.DLNARegistryListener;
import com.yen.dlna.listener.DLNAStateCallback;

import org.fourthline.cling.model.action.ActionInvocation;
import org.fourthline.cling.support.model.Res;
import org.fourthline.cling.support.model.container.Container;


public class MainActivity extends android.app.NativeActivity
        implements ActivityCompat.OnRequestPermissionsResultCallback,
        DLNADeviceConnectListener
{
  public static final String TAG = "VrQPlayer";
  //===============================================================
  private com.yen.dlna.bean.DeviceInfo mDeviceInfo;
  private DevicesAdapter mDevicesAdapter;
  private List<ContainerInfo> mFolder;
  private Handler mHandler;
  private ContainerAdapter mContainerAdapter;
  //===============================================================
  private static final int UPDATE_VIDEO_POS =11 ;
  private static final int UPDATE_yyyy =335 ;
  private static final int UPDATE_VIDEO_DUR =33 ;
  private static final int UPDATE_VIDEO_PREVIOUS =99 ;
  private static final int UPDATE_VIDEO_NEXT =444 ;
  private static final int UPDATE_VIDEO_SEEK=333 ;

  private static final int UPDATE_VIDEO_PLAY =434 ;
  private static final int UPDATE_VIDEO_PAUSE =55 ;
  private static final int UPDATE_VIDEO_ITEM =346 ;

  private static final int UPDATE_VIDEO_MEDIA =4422 ;

  long video_position =0;
  long video_duration =0;
  long video_seek =0;
  //------------------------------------------------------------------------------------------------
  int  ix_folder_list =0;
  int  ix_play_list =0;
  int  ix_device_list =0;

  //------------------------------------------------------------------------------------------------
  static {
    System.loadLibrary("vrapi");
    System.loadLibrary("vrqplayer");
  }
  public class ContextInfo  {
      ContextInfo( List<MediaItem> Play,  List<String> Name, String Target ){
          this.mPlay =Play;
          this.mTarget =Target;
          this.mName = Name;
      }
      List<MediaItem> mPlay;
      List<String> mName;
      String mTarget;
  }

  public class DeviceInfo  {
    DeviceInfo( String Name ){
      this.mName =Name;
    }
    String mName;
  }


//==================================================================================================
  class MyHandler extends Handler
  {
  @Override
  public void handleMessage(Message msg) {
    super.handleMessage(msg);
    switch (msg.what) {
      case UPDATE_VIDEO_POS:
        if (exoPlayer != null)
          video_position = exoPlayer.getCurrentPosition();
        break;
      case UPDATE_VIDEO_DUR:
        if (exoPlayer != null)
          video_duration =exoPlayer.getDuration();
        break;
      case UPDATE_VIDEO_PREVIOUS:
        if (exoPlayer != null)
          exoPlayer.seekToPrevious();;
        break;
      case UPDATE_VIDEO_NEXT:
        if (exoPlayer != null)
          exoPlayer.seekToNext();;
        break;
      case UPDATE_VIDEO_SEEK:
        if (exoPlayer != null)
          exoPlayer.seekTo(video_seek);
        break;
      case UPDATE_VIDEO_PLAY:
        Log.v(TAG, "xoPlayer play" );
        if (exoPlayer != null)
        exoPlayer.play();
        break;
      case UPDATE_VIDEO_PAUSE:
        if (exoPlayer != null)
          Log.v(TAG, "xoPlayer pause" );
        exoPlayer.pause();
        break;
      case UPDATE_VIDEO_ITEM:

        ContainerInfo x = mContainerAdapter.getItem( ix_folder_list );
        if ( null != x ) {
          Log.v(TAG, "name : "+x.getName()+ Integer.toString(x.getType()));
          Log.v(TAG, "name : "+x.getResource());


          if( x.getType() ==TYPE_Contain||x.getType() ==TYPE_ee  ||x.getType() ==TYPE_dd) {
              getPlayList2(x);

          }

          if( x.getType() == ContainerInfo.TYPE_Item ){

            Log.v(TAG, "xxxxxxxxxxxxxx : "+x.getURL());

            Log.v(TAG, "count  : "+ mContainerAdapter.getCount());


             List<MediaItem> _folder = new ArrayList<MediaItem> ();
              List<String> _name = new ArrayList<String> ();
            ix_play_list =0;
            int count=-1;
            for (int i = 0; i < mContainerAdapter.getCount(); i++) {
                ContainerInfo zx = mContainerAdapter.getItem( i );
                if( zx.getType() == ContainerInfo.TYPE_Item  ){
                  _folder.add(MediaItem.fromUri(zx.getURL()));
                  _name.add(zx.getName());
                  count++;

                }
              if( x==zx ) ix_play_list = count;

            }

            Log.v(TAG, "ix_play_list  : "+ ix_play_list );

            mPlayItems.clear();
            mPlayItems.add(new ContextInfo(

                    _folder,_name
                    , "list 1"
            ));

             exoPlayer.setMediaItems(mPlayItems.get(0).mPlay, true);

            exoPlayer.seekTo(ix_play_list,0);
          }
        }
    //    exoPlayer.setMediaItems(mPlayItems.get(ix_folder_list).mPlay, true);
        break;

      case UPDATE_VIDEO_MEDIA:
        exoPlayer.seekTo(ix_play_list,0);
        break;

      case UPDATE_yyyy:
      //  exoPlayer.setMediaItems(mPlayItems.get(ix_folder_list).mPlay, /* resetPosition= */ true);

        exoPlayer.addAnalyticsListener(new AnalyticsListener() {
          @Override
          public void onVideoSizeChanged(EventTime eventTime, int width, int height, int unappliedRotationDegrees, float pixelWidthHeightRatio) {
            Log.v(TAG, String.format("onVideoSizeChanged: %dx%d", width, height));
            if (width == 0 || height == 0) {
              Log.e( TAG, "The video size is 0. Could be because there was no video, no display surface was set," +
                      " or the value was not determined yet.");
            } else {
              nativeSetVideoSize(appPtr, width, height);
            }
          }



          @Override
          public void onPlaybackStateChanged(
                  EventTime eventTime, @Player.State int state) {
          }

          @Override
          public void onDroppedVideoFrames(
                  EventTime eventTime, int droppedFrames, long elapsedMs) {
          }

          @Override
          public void onSurfaceSizeChanged(EventTime eventTime, int width, int height) {}

        });
        exoPlayer.setVideoSurface(movieSurface);


        exoPlayer.setMediaItems(mPlayItems.get(0).mPlay, /* resetPosition= */ true);
        exoPlayer.prepare();
        /// Always restart movie from the beginning
        exoPlayer.setSeekParameters(SeekParameters.CLOSEST_SYNC);
        exoPlayer.seekTo(0);
        exoPlayer.setPlayWhenReady(true);
        exoPlayer.setRepeatMode(Player.REPEAT_MODE_ALL);


        exoPlayer.play();


        break;

      default:
        break;
    }
  }
}



  private final Handler hd = new MyHandler();

  public  Context getContext() {
    return getApplication().getApplicationContext();
  }

  public static native void nativeSetVideoSize(long appPtr, int width, int height);
  public static native SurfaceTexture nativePrepareNewVideo(long appPtr);
  public static native void nativeVideoCompletion(long appPtr);
  public static native long nativeSetAppInterface(android.app.NativeActivity act);
  public static native void nativeReloadPhoto(long appPtr);

  SurfaceTexture movieTexture = null;
  Surface movieSurface = null;
  SimpleExoPlayer exoPlayer = null;
  AudioManager audioManager = null;


  List<ContextInfo> mPlayItems= new ArrayList<ContextInfo> ();

  List<DeviceInfo> mDeviceItems= new ArrayList<DeviceInfo> ();


  //--------------------------------------



  //================================================================================================

  //================================================================================================
  @Override
  public void onConnect(com.yen.dlna.bean.DeviceInfo deviceInfo, int errorCode) {
    Log.v(TAG, "DLNS onConnect");
    if (errorCode == CONNECT_INFO_CONNECT_SUCCESS) {
        mDeviceInfo = deviceInfo;
        getPlayList1();
    }
  }

  @Override
  public void onDisconnect(com.yen.dlna.bean.DeviceInfo deviceInfo, int type, int errorCode) {
    Log.v(TAG, "onDisconnect");
  }
//================================================================================================
  private DLNAPlayer mDLNAPlayer;
  private DLNARegistryListener mDLNARegistryListener =null;

  private void initDlna() {
    Log.v(TAG, "initDlna");

    mDLNAPlayer = new DLNAPlayer(this);
    mDLNAPlayer.setConnectListener(this);
    //
    if( mDLNARegistryListener == null) {
      mDLNARegistryListener = new DLNARegistryListener()
      {
        @Override
        public void onDeviceChanged( List<com.yen.dlna.bean.DeviceInfo> deviceInfoList) {

          mDevicesAdapter.clear();
          mDevicesAdapter.addAll(deviceInfoList);
         // mDevicesAdapter.notifyDataSetChanged();
/*

          if( mDevicesAdapter.getCount() >0 )
          {
                for ( int i =0; i< mDevicesAdapter.getCount();i++) {
                  com.yen.dlna.bean.DeviceInfo  temp = mDevicesAdapter.getItem(i);
                  Log.v(TAG, temp.getName());

                  if( temp.getName().equals("QUBIT-NAS")){
                    Log.v(TAG, "XXXXXXXXXXXXXXXXXXXXxx");
                    mDLNAPlayer.connect(temp);
                  }


                }

          }

* */




        }
      };
      DLNAManager.getInstance().registerListener(mDLNARegistryListener);
    }

  }

  //================================================================================================
  //
  //================================================================================================
  Long appPtr = 0L;
  @Override protected void onCreate(Bundle savedInstanceState)
  {
    Log.v(TAG, "===== onCreate ====");
    super.onCreate(savedInstanceState);
    Intent intent = getIntent();
    appPtr = nativeSetAppInterface(this);
    audioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
    //----------------------------------------------------------------------------------------------
    // DLNA
    DLNAManager.getInstance().init(
            MainActivity.this,
      new DLNAStateCallback() {
        @Override public void onConnected() {
         initDlna();
        }
        @Override public void onDisconnected() { }
      }
    );
    //----------------------------------------------------------------------------------------------
    mFolder = new ArrayList<ContainerInfo>();
    mHandler = new Handler(Looper.getMainLooper());
    mDevicesAdapter = new DevicesAdapter(MainActivity.this);
    mContainerAdapter = new ContainerAdapter(MainActivity.this,
            new ContainerCallback() { @Override public void onView(int ix) {}});
  }
  //================================================================================================
  //
  //================================================================================================
  @Override
  protected void onDestroy() {
    Log.v(TAG, "==== onDestroy ====");
    if (exoPlayer != null) {
      exoPlayer.release();
      exoPlayer = null;
    }
    super.onDestroy();
  }
  @Override 
  protected void onPause() {
    Log.v(TAG, "==== onPause ====");
    if (exoPlayer != null) exoPlayer.pause();
    super.onPause();
  }

  @Override
  protected void onResume() {
    Log.v(TAG, "==== onResume ====");
    if (exoPlayer != null) exoPlayer.play();
    super.onResume();
  }

  @Override
  public void onRequestPermissionsResult(
          int requestCode, String[] permissions, int[] grantResults)
  {
    Log.v(TAG, "==== onRequestPermissionsResult ====");
  }

public void startMovie()
{
    startMovieAfterPermissionGranted();

}

public void startMovieAfterPermissionGranted()
{
      //============================================================================================
      if (exoPlayer != null) exoPlayer.release();
      synchronized (this) {
        exoPlayer = new SimpleExoPlayer.Builder(getContext()).build();
      }

    //  mDeviceItems.add(new DeviceInfo( "Device 1") );
      //mDeviceItems.add(new DeviceInfo( "Device 2") );
      //============================================================================================

      if(true) {

        mPlayItems.add(new ContextInfo(ImmutableList.of(

                MediaItem.fromUri(Uri.parse("asset:///demo.mp4"))

         ),
                ImmutableList.of(
                        ("show")
                )
                , "list 1"
        ));
      }
      new Thread(new Runnable() {
        @Override
        public void run() {
          Message msg = new Message();
          msg.what = UPDATE_yyyy;
          hd.sendMessage(msg);
        }
      }).start();


}

  public void onVideoSizeChanged(MediaPlayer mp, int width, int height)
  {
      Log.v(TAG, String.format("onVideoSizeChanged: %dx%d", width, height));
      if (width == 0 || height == 0) {
          Log.e( TAG, "The video size is 0. Could be because there was no video, no display surface was set," +
                        " or the value was not determined yet.");
      } else {
          nativeSetVideoSize(appPtr, width, height);
      }
  }

  /// MediaPlayer.OnCompletionListener
  public void onCompletion(MediaPlayer mp)
  {
    Log.v(TAG, String.format("onCompletion"));
    nativeVideoCompletion(appPtr);
  }


  //------------------------------------------------------------------------------------------------
  // called from native code for starting movie
  public void startMovieFromNative(final String pathName) {
      Log.v(TAG, "startMovieFromNative");

      movieTexture = nativePrepareNewVideo(appPtr);
      if (movieTexture == null) {
        Log.w(TAG, "could not create movieTexture ");
        return;
      }
      if( movieSurface != null ) movieSurface.release();
      movieSurface = new Surface(movieTexture);
      //
      startMovie();
  }
  //------------------------------------------------------------------------------------------------
  public long numPlayList() {
    int count = 0;
    if (mPlayItems != null){
      if(mPlayItems.size() >0 )

        count = mPlayItems.get(0).mPlay.size();;
    }
    return count;
  }
  public String namePlayList( int pos ) {
    String name = "___";
    if (mPlayItems != null) {
      if(mPlayItems.size() >0 )

        if (pos >= 0 && pos < mPlayItems.get(0).mName.size()) {
          name = mPlayItems.get(0).mName.get(pos);
        }
    }
    return name;

  }
  public void pickPlayList( int pos ) {


    ix_play_list = pos;
    new Thread(() -> { Message msg = new Message();
      msg.what = UPDATE_VIDEO_MEDIA;
      hd.sendMessage(msg);  }).start();

  }

  //------------------------------------------------------------------------------------------------
  public long numFolderList() {


     int count = 0;
    if (mContainerAdapter != null)  count = mContainerAdapter.getCount();
    return count;
  }
  public String nameFolderList( int pos ) {
    String name = "___";
    if (mContainerAdapter != null)
      if( pos >=0 && pos < mContainerAdapter.getCount() )
        name = mContainerAdapter.getItem(pos).getName();
    return name;
  }
  public void pickFolderList( int pos ) {
    ix_folder_list = pos;
    new Thread(() -> { Message msg = new Message();
      msg.what = UPDATE_VIDEO_ITEM;
      hd.sendMessage(msg);  }).start();
  }
  //------------------------------------------------------------------------------------------------
  public long numDLNAList() {
    int count = 0;
    if (mDevicesAdapter != null)  count = mDevicesAdapter.getCount();
    return count;
  }
  public String nameDLNAList( int pos ) {
    String name = "___";
    if (mDevicesAdapter != null)
      if( pos >=0 && pos < mDevicesAdapter.getCount() )
        name = mDevicesAdapter.getItem(pos).getName();
    return name;
  }
  public void pickDLNAList( int pos ) {
    ix_device_list = pos;

    com.yen.dlna.bean.DeviceInfo deviceInfo = mDevicesAdapter.getItem(ix_device_list);
    if (null == deviceInfo) {
      return;
    }
  ///  mDeviceListView.setVisibility(View.GONE);
   // mContainerListView.setVisibility(View.VISIBLE);
    mContainerAdapter.clear();
    mFolder.clear();
    mDLNAPlayer.connect(deviceInfo);

  }


  //------------------------------------------------------------------------------------------------
  public void pauseMovie() {
    new Thread(() -> { Message msg = new Message();
      msg.what = UPDATE_VIDEO_PAUSE;
      hd.sendMessage(msg);  }).start();
  }
  public void resumeMovie() {
    Log.v(TAG, "resumeMovie");
    new Thread(() -> { Message msg = new Message();
      msg.what = UPDATE_VIDEO_PLAY;
      hd.sendMessage(msg);  }).start();
  }
  public void seekMovie( int pos ) {
      video_seek = pos;
      new Thread(() -> { Message msg = new Message();
      msg.what = UPDATE_VIDEO_SEEK;
      hd.sendMessage(msg); }).start();
  }
  public void nextMovie() {
      new Thread(() -> { Message msg = new Message();
        msg.what = UPDATE_VIDEO_NEXT;
        hd.sendMessage(msg); }).start();
  }
  public void previousMovie() {
      new Thread(() -> { Message msg = new Message();
        msg.what = UPDATE_VIDEO_PREVIOUS;
        hd.sendMessage(msg); }).start();
  }
  public long CurrentPosition() {
      new Thread(() -> { Message msg = new Message();
        msg.what = UPDATE_VIDEO_POS;
        hd.sendMessage(msg); }).start();
      return video_position;
  }
  public long Duration() {
      new Thread(() -> { Message msg = new Message();
        msg.what = UPDATE_VIDEO_DUR;
        hd.sendMessage(msg); }).start();
      return video_duration;
  }
  public void moveMovie( int offset ) { }
//=================================================================================================
private void getPlayList2(ContainerInfo x) {
  if( x.getType() ==TYPE_Contain ||x.getType() ==TYPE_ee){
    mFolder.add(x);
  }else{
    mFolder.remove(mFolder.size()-1);
  }
  mDLNAPlayer.playList2( x, new DLNAControlCallback() {
    @Override
    public void onCantainer(@Nullable  List<ContainerInfo>deviceInfoList) {
      Log.v(TAG, "getPlayList2");
      if( mFolder.size() >0)
        deviceInfoList.add( 0, new ContainerInfo(
                mFolder.get(mFolder.size()-1).getContainer(), "..." ));
      mHandler.post(() -> {
        synchronized (MainActivity.class) {
          mContainerAdapter.clear();
          mContainerAdapter.addAll(deviceInfoList);
          mContainerAdapter.notifyDataSetChanged();
        }
      });
    }
    @Override
    public void onSuccess(@Nullable ActionInvocation invocation) {}
    @Override
    public void onReceived(@Nullable ActionInvocation invocation, @Nullable Object... extra) { }
    @Override
    public void onFailure(@Nullable ActionInvocation invocation, int errorCode, @Nullable String errorMsg) { }
  });
}
  private void getPlayList3( int y) {
    ContainerInfo t = mContainerAdapter.getItem(y);


    if(t.getType() ==TYPE_Contain) {
      Container x = t.getContainer();
      mDLNAPlayer.playList0(x, new DLNAControlCallback() {
        @Override
        public void onCantainer(@Nullable List<ContainerInfo> deviceInfoList) {
          Log.v(TAG, "getPlayList3");
          mHandler.post(() -> {
            synchronized (MainActivity.class) {
              if (mContainerAdapter.getCount() > y) {
                ContainerInfo t2 = mContainerAdapter.getItem(y);
                if (t2 == t) {
                  //  Log.v(TAG, "xxxxxxxxxxxxx");
                  // Log.v(TAG, (String.valueOf(deviceInfoList.size())));
                  if (deviceInfoList.size() > 0) {
                    // Log.v(TAG, deviceInfoList.get(0).getName());

                    List<Res> c = deviceInfoList.get(0).getItem().getResources();
                    if (c.size() >= 1) {
                      //  Log.v(TAG, "oooo");
                      mContainerAdapter.remove(t);
                      t.setURL(c.get(c.size() - 1).getValue());
                      mContainerAdapter.insert(t, y);
                      mContainerAdapter.notifyDataSetChanged();
                      t.showView();

                    }
                  }
                }
              }
            }
          });
        }
        @Override
        public void onSuccess(@Nullable ActionInvocation invocation) { }
        @Override
        public void onReceived(@Nullable ActionInvocation invocation, @Nullable Object... extra) { }
        @Override
        public void onFailure(@Nullable ActionInvocation invocation, int errorCode, @Nullable String errorMsg) { }
      });
    }
  }
  private void getPlayList1()
  {

    mDLNAPlayer.playList( new DLNAControlCallback()
    {
      @Override
      public void onCantainer( @Nullable List<ContainerInfo>deviceInfoList ) {
        Log.v(TAG, "getPlayList");
        mHandler.post(() -> {
          synchronized (MainActivity.class) {


            mContainerAdapter.clear();
            mContainerAdapter.addAll(deviceInfoList);
            mContainerAdapter.notifyDataSetChanged();
          }
        });
      }
      @Override
      public void onSuccess(@Nullable ActionInvocation invocation) { }
      @Override
      public void onReceived(@Nullable ActionInvocation invocation, @Nullable Object... extra) { }
      @Override
      public void onFailure(@Nullable ActionInvocation invocation, int errorCode, @Nullable String errorMsg) { }

    });
  }


}
