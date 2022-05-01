package com.yen.vrqplayer;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.Drawable;
import android.os.AsyncTask;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;

import com.yen.dlna.bean.ContainerInfo;
import com.yen.dlna.bean.DeviceInfo;
import com.yen.dlna.listener.ContainerCallback;
import com.yen.dlna.listener.DLNAControlCallback;

import org.fourthline.cling.model.meta.Device;
import org.fourthline.cling.support.model.container.Container;

import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;



public class ContainerAdapter extends ArrayAdapter<ContainerInfo>
{
    private static final String TAG = "ContainerAdapter";
    private LayoutInflater mInflater;

    private  ContainerCallback mcallback;

    public ContainerAdapter(Context context, final @NonNull ContainerCallback callback) {
        super(context, 0);
        mInflater = LayoutInflater.from(context);

        mcallback =  callback;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent)
    {




        return convertView;
    }
}