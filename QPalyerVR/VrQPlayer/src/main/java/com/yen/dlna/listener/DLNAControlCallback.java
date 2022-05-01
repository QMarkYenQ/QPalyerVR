package com.yen.dlna.listener;


//import androidx.annotation.IntRange;
//import androidx.annotation.Nullable;

import com.yen.dlna.bean.ContainerInfo;
import com.yen.dlna.bean.DeviceInfo;

import org.fourthline.cling.model.action.ActionInvocation;
import org.fourthline.cling.support.model.container.Container;

import java.util.List;


public interface DLNAControlCallback {
    int ERROR_CODE_NO_ERROR = 0;

    int ERROR_CODE_RE_PLAY = 1;

    int ERROR_CODE_RE_PAUSE = 2;

    int ERROR_CODE_RE_STOP = 3;

    int ERROR_CODE_DLNA_ERROR = 4;

    int ERROR_CODE_SERVICE_ERROR = 5;

    int ERROR_CODE_NOT_READY = 6;

    int ERROR_CODE_BIND_SCREEN_RECORDER_SERVICE_ERROR = 7;


    void onSuccess(ActionInvocation invocation);

    void onReceived(ActionInvocation invocation, Object... extra);

    void onFailure(ActionInvocation invocation,
                    int errorCode,
                   String errorMsg);

    void onCantainer(List<ContainerInfo> deviceInfoList );

}
