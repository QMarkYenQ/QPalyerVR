package com.yen.vrqplayer;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.TextView;

import com.yen.dlna.bean.DeviceInfo;

import org.fourthline.cling.model.meta.Device;


public class DevicesAdapter extends ArrayAdapter<DeviceInfo>
{
    private LayoutInflater mInflater;

    public DevicesAdapter(Context context) {
        super(context, 0);
        mInflater = LayoutInflater.from(context);
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent)
    {

        return convertView;
    }
}