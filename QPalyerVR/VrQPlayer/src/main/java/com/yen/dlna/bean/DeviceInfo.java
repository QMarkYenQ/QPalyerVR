package com.yen.dlna.bean;



import org.fourthline.cling.model.meta.Device;

import java.io.Serializable;


public class DeviceInfo implements Serializable {
    private Device device;
    private String name;
    private String mediaID;
    private String oldMediaID;
    private int state ;
    private boolean connected;

    public DeviceInfo(Device device, String name) {
        this.device = device;
        this.name = name;
    }

    public DeviceInfo() {
    }

    public Device getDevice() {
        return this.device;
    }
    public void setDevice(Device device) {
        this.device = device;
    }

    public String getName() {
        return this.name;
    }
    public void setName(String name) {
        this.name = name;
    }

    public void setMediaID(String mediaId) {
        this.mediaID = mediaId;
    }
    public String getMediaID() {
        return this.mediaID;
    }

    public void setOldMediaID(String oldMediaID) {
        this.oldMediaID = oldMediaID;
    }
    public String getOldMediaID() {
        return this.oldMediaID;
    }

    public int getState() {
        return state;
    }
    public void setState(int state) {
        this.state = state;
    }

    public boolean isConnected() {
        return connected;
    }
    public void setConnected(boolean connected) {
        this.connected = connected;
    }
}
