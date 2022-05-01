package com.yen.dlna;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;

import android.content.ServiceConnection;

import android.os.IBinder;

import android.util.Log;

import com.yen.dlna.bean.ContainerInfo;
import com.yen.dlna.bean.DeviceInfo;

import com.yen.dlna.listener.DLNAControlCallback;
import com.yen.dlna.listener.DLNADeviceConnectListener;

import com.yen.dlna.xml.*;
import org.fourthline.cling.android.AndroidUpnpService;
import org.fourthline.cling.controlpoint.ActionCallback;

import org.fourthline.cling.model.action.ActionException;
import org.fourthline.cling.model.action.ActionInvocation;
import org.fourthline.cling.model.message.UpnpResponse;
import org.fourthline.cling.model.meta.Device;
import org.fourthline.cling.model.meta.Service;
import org.fourthline.cling.model.types.ErrorCode;
import org.fourthline.cling.model.types.ServiceType;
import org.fourthline.cling.model.types.UDAServiceType;
import org.fourthline.cling.model.types.UnsignedIntegerFourBytes;

import org.fourthline.cling.support.contentdirectory.callback.Browse;
import org.fourthline.cling.support.model.BrowseFlag;
import org.fourthline.cling.support.model.BrowseResult;
import org.fourthline.cling.support.model.DIDLContent;
import org.fourthline.cling.support.model.container.Container;
import org.fourthline.cling.support.model.item.Item;
import java.util.ArrayList;
import java.util.List;

import static com.yen.dlna.DLNAManager.logD;


public class DLNAPlayer {
    private static final String TAG = "DLNAPlayer";

    public static final int UNKNOWN = -1;
    public static final int CONNECTED = 0;
    public static final int DISCONNECTED = 1;
    private int currentState = UNKNOWN;
    //
    private DeviceInfo mDeviceInfo;
    private Device mDevice;

    private Context mContext;//鉴权预留
    private ServiceConnection mServiceConnection;
    private AndroidUpnpService mUpnpService;

    private DLNADeviceConnectListener connectListener;

    private ServiceType ContentDirectory_SERVICE;
    public DLNAPlayer( Context context) {
        Log.v(TAG, "DLNAPlayer");
        mContext = context;
        ContentDirectory_SERVICE = new UDAServiceType("ContentDirectory");
        initConnection();
    }

    public void setConnectListener(DLNADeviceConnectListener connectListener) {
        this.connectListener = connectListener;
    }

    private void initConnection() {
        Log.v(TAG, "initConnection");
        mServiceConnection = new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
                mUpnpService = (AndroidUpnpService) service;
                currentState = CONNECTED;
                if (null != mDeviceInfo) {

                    mDeviceInfo.setState(CONNECTED);
                    mDeviceInfo.setConnected(true);
                }
                if (null != connectListener) {
                    connectListener.onConnect(mDeviceInfo, DLNADeviceConnectListener.CONNECT_INFO_CONNECT_SUCCESS);
                }
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {
                currentState = DISCONNECTED;
                if (null != mDeviceInfo) {
                    mDeviceInfo.setState(DISCONNECTED);
                    mDeviceInfo.setConnected(false);
                }
                if (null != connectListener) {
                    connectListener.onDisconnect(mDeviceInfo, DLNADeviceConnectListener.TYPE_DLNA,
                        DLNADeviceConnectListener.CONNECT_INFO_DISCONNECT_SUCCESS);
                }


                mUpnpService = null;
                connectListener = null;
                mDeviceInfo = null;
                mDevice = null;

                mServiceConnection = null;

            }
        };
    }

    public void connect( DeviceInfo deviceInfo) {
        Log.v(TAG, "connect");
        checkConfig();
        mDeviceInfo = deviceInfo;
        mDevice = mDeviceInfo.getDevice();
        if (null != mUpnpService ) {
            if (null != connectListener) {
                connectListener.onConnect(mDeviceInfo, DLNADeviceConnectListener.CONNECT_INFO_CONNECT_SUCCESS);
            }
            return;
        }
        mContext.bindService(
                new Intent(mContext, DLNABrowserService.class), mServiceConnection, Context.BIND_AUTO_CREATE);
    }

    public void disconnect() {
        Log.v(TAG, "disconnect");
        checkConfig();
        try {
            if (null != mUpnpService && null != mServiceConnection) {
                mContext.unbindService(mServiceConnection);
            }
        } catch (Exception e) {
            DLNAManager.logE("DLNAPlayer disconnect UPnpService error.", e);
        }

    }

    //----------------------------------------------------------------------------------------------
    private void checkPrepared() {
        if (null == mUpnpService) {
            throw new IllegalStateException("Invalid AndroidUPnpService");
        }
    }

    private void checkConfig() {
        if (null == mContext) {
            throw new IllegalStateException("Invalid context");
        }
    }

    private void execute( ActionCallback actionCallback) {
        checkPrepared();
        mUpnpService.getControlPoint().execute(actionCallback);

    }


    //----------------------------------------------------------------------------------------------


    public void playList(final  DLNAControlCallback callback)
    {
    logD("playList" );

    final Service avtService = mDevice.findService(ContentDirectory_SERVICE);
    if (null == avtService) {
        callback.onFailure(null, DLNAControlCallback.ERROR_CODE_SERVICE_ERROR, null);
        return;
    }

    execute(new Browse(  avtService, "0", BrowseFlag.DIRECT_CHILDREN ){
                public void success(ActionInvocation invocation) {
                  logD("success" );


                    BrowseResult result = new BrowseResult(
                            invocation.getOutput("Result").getValue().toString(),
                            (UnsignedIntegerFourBytes) invocation.getOutput("NumberReturned").getValue(),
                            (UnsignedIntegerFourBytes) invocation.getOutput("TotalMatches").getValue(),
                            (UnsignedIntegerFourBytes) invocation.getOutput("UpdateID").getValue()
                    );

                    boolean proceed = receivedRaw(invocation, result);

                    if (proceed && result.getCountLong() > 0 && result.getResult().length() > 0) {

                        try {
                            //logD( result.getResult());

                            DIDLxParser didlParser = new DIDLxParser();


                            logD("didlParser.parse" );

                            DIDLContent didl = didlParser.parse(result.getResult());
                            logD("didlParser.parse3" );


                            final List<Container> containers =   didl.getContainers();
                            logD(String.valueOf(containers.size()));
                            for(Container i:containers) {
                                logD(i.getTitle());
                                logD(i.getId());
                            //    logD(i.getFirstResource());
                            }



                            received(invocation, didl);


                          updateStatus(Status.OK);

                        } catch (Exception ex) {
                            invocation.setFailure(
                                    new ActionException(ErrorCode.ACTION_FAILED, "  xxCan't parse DIDL XML response: " + ex, ex)
                            );
                            failure(invocation, null);
                        }

                    } else {
                        received(invocation, new DIDLContent());
                        updateStatus(Status.NO_CONTENT);
                    }
                }

                @Override
                public void failure(ActionInvocation actionInvocation, UpnpResponse upnpResponse, String s)
                {


                    logD("failure" );
                    logD(actionInvocation.toString());
                    logD("failure" +s );
                }
                @Override
                public void received(ActionInvocation actionInvocation, DIDLContent didl)
                {
                    final List<Container> c =   didl.getContainers();
                    List<ContainerInfo> containers = new ArrayList<ContainerInfo>();
                    containers.clear();
                    for(Container i:c) {

                        containers.add(new ContainerInfo(i));
                    }

                    callback.onCantainer(containers);

                    logD("xxx received" );
                    // final List<Item> items = didl.getItems();
                    // String st=didl.getItems().get(0).getFirstResource().getValue();
                    // Log.d("URL IS",st);
                    //Item item = didl.getItems().get(0);
                    //Item item = didl.getItems().get(0);
                    //String url = item.getFirstResource().getValue();
                }

                @Override
                public void updateStatus(Status status) {
                    logD("updateStatus" );
                }
            }
    );

    }
    public void playList0(Container x, final  DLNAControlCallback callback) {
            logD("playList0" );
            final Service avtService = mDevice.findService(ContentDirectory_SERVICE);
            if (null == avtService) {
                return;

            }

            String name="";

            name =x.getId();

            execute(
                    new Browse(  avtService,   name, BrowseFlag.DIRECT_CHILDREN )
                    {
                        public void success(ActionInvocation invocation) {
                            logD("success" );


                            BrowseResult result = new BrowseResult(
                                    invocation.getOutput("Result").getValue().toString(),
                                    (UnsignedIntegerFourBytes) invocation.getOutput("NumberReturned").getValue(),
                                    (UnsignedIntegerFourBytes) invocation.getOutput("TotalMatches").getValue(),
                                    (UnsignedIntegerFourBytes) invocation.getOutput("UpdateID").getValue()
                            );

                            boolean proceed = receivedRaw(invocation, result);

                            if (proceed && result.getCountLong() > 0 && result.getResult().length() > 0) {

                                try {
                                    logD( result.getResult());
                                    DIDLxParser didlParser = new DIDLxParser();

                                    //  logD("didlParser.parse" );

                                    DIDLContent didl = didlParser.parse(result.getResult());



                                    received(invocation, didl);


                                    updateStatus(Status.OK);

                                } catch (Exception ex) {
                                    invocation.setFailure(
                                            new ActionException(ErrorCode.ACTION_FAILED, "  xxCan't parse DIDL XML response: " + ex, ex)
                                    );
                                    failure(invocation, null);
                                }

                            } else {
                                received(invocation, new DIDLContent());
                                updateStatus(Status.NO_CONTENT);
                            }
                        }

                        @Override
                        public void failure(ActionInvocation actionInvocation, UpnpResponse upnpResponse, String s)
                        {

                        }
                        @Override
                        public void received(ActionInvocation actionInvocation, DIDLContent didl)
                        {
                            final List<Item> t =   didl.getItems();
                            List<ContainerInfo> containers = new ArrayList<ContainerInfo>();
                            for(Item i:t) {
                                containers.add(new ContainerInfo(i));

                            }

                            callback.onCantainer(containers);

                        }

                        @Override
                        public void updateStatus(Status status) {

                        }
                    }
            );

        }
    public void playList2(ContainerInfo x, final  DLNAControlCallback callback) {
    logD("playList" );

    final Service avtService = mDevice.findService(ContentDirectory_SERVICE);
    if (null == avtService) {
        callback.onFailure(null, DLNAControlCallback.ERROR_CODE_SERVICE_ERROR, null);
        return;

    }

    String name="";

    if(x.getType()==ContainerInfo.TYPE_Contain || x.getType()==ContainerInfo.TYPE_ee)
        name =x.getId();
    else
        name =x.getParentId();

    execute(
            new Browse(  avtService,   name, BrowseFlag.DIRECT_CHILDREN )
            {
                public void success(ActionInvocation invocation) {
                    logD("success" );


                    BrowseResult result = new BrowseResult(
                            invocation.getOutput("Result").getValue().toString(),
                            (UnsignedIntegerFourBytes) invocation.getOutput("NumberReturned").getValue(),
                            (UnsignedIntegerFourBytes) invocation.getOutput("TotalMatches").getValue(),
                            (UnsignedIntegerFourBytes) invocation.getOutput("UpdateID").getValue()
                    );

                    boolean proceed = receivedRaw(invocation, result);

                    if (proceed && result.getCountLong() > 0 && result.getResult().length() > 0) {

                        try {
                            logD( result.getResult());
                            DIDLxParser didlParser = new DIDLxParser();

                          //  logD("didlParser.parse" );

                            DIDLContent didl = didlParser.parse(result.getResult());



                            received(invocation, didl);


                            updateStatus(Status.OK);

                        } catch (Exception ex) {
                            invocation.setFailure(
                                    new ActionException(ErrorCode.ACTION_FAILED, "  xxCan't parse DIDL XML response: " + ex, ex)
                            );
                            failure(invocation, null);
                        }

                    } else {
                        received(invocation, new DIDLContent());
                        updateStatus(Status.NO_CONTENT);
                    }
                }

                @Override
                public void failure(ActionInvocation actionInvocation, UpnpResponse upnpResponse, String s)
                {


                    logD("failure" );
                    logD(actionInvocation.toString());
                    logD("failure" +s );
                }
                @Override
                public void received(ActionInvocation actionInvocation, DIDLContent didl)
                {

                    final List<Container> c =   didl.getContainers();
                    final List<Item> t =   didl.getItems();
                    List<ContainerInfo> containers = new ArrayList<ContainerInfo>();
                    containers.clear();
                    logD("received" );


                    Log.d(TAG,  "size Container " + Integer.toString(c.size()));

                    Log.d(TAG,  "size Item " + Integer.toString(t.size()));

                    for(Item i:t) {
                        containers.add(new ContainerInfo(i));

                    }

                   // logD(        containers.get(0).getName() );
                    for(Container i:c) {

                        containers.add(new ContainerInfo(i));

                    }

                    callback.onCantainer(containers);

                  //  logD("xxx received" );
                    // final List<Item> items = didl.getItems();
                    // String st=didl.getItems().get(0).getFirstResource().getValue();
                    // Log.d("URL IS",st);
                    //Item item = didl.getItems().get(0);
                    //Item item = didl.getItems().get(0);
                    //String url = item.getFirstResource().getValue();
                }

                @Override
                public void updateStatus(Status status) {
                    logD("updateStatus" );
                }
            }
    );

}

//==================================================================================================

    public void destroy() {
        checkConfig();

        disconnect();
    }






}
