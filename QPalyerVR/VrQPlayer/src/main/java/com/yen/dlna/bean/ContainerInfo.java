package com.yen.dlna.bean;


import com.yen.dlna.*;

import android.graphics.Bitmap;
import android.util.Log;
import android.view.View;
import android.widget.ImageView;

import org.fourthline.cling.model.meta.Device;
import org.fourthline.cling.support.model.Res;
import org.fourthline.cling.support.model.container.Container;
import org.fourthline.cling.support.model.item.Item;
import org.fourthline.cling.transport.spi.ServletContainerAdapter;

import java.io.Serializable;
import java.lang.ref.WeakReference;
import java.util.List;
import java.util.Optional;

import com.yen.dlna.*;

public class ContainerInfo implements Serializable {


    public static final int TYPE_Contain= 1;
    public static final int TYPE_Item = 2;
    public static final int TYPE_dd = 3;
    public static final int TYPE_ee= 4;
    private static final String TAG = ContainerInfo.class.getSimpleName();
    private ImageView imageView = null;

    private Device mDevice;
    private Container mContainer;
    private Item mItem;
    private int mType;

    private String mID;
    private String name;
    private String mediaID;
    private String oldMediaID;
    private String url;

    private Bitmap bitmap= null;

    private int state ;
    private boolean connected;

    public int lock = 0;


    public void setBMP( Bitmap bitmap_i ) {
        lock =0;
        bitmap = bitmap_i;
    }


    public Bitmap getBMP( ) {

        return bitmap ;
    }

    public void setView( View view,ImageView imageViews )

    {
     //   this.view = view;
        this.imageView = imageViews;
    }

    public void showView( ) {
        Log.d(TAG, "showView");

      if(bitmap!= null&&this.imageView != null) {
          Log.d(TAG,  getResource());
          imageView.setVisibility(View.GONE);
          imageView.setVisibility(View.VISIBLE);
          imageView.setImageBitmap(bitmap);

          Log.d(TAG, String.valueOf(bitmap.getHeight()) +"   "  +  String.valueOf(imageView.getImageAlpha()) );

      }

    }

    public ContainerInfo( Container contain ) {
        this.mContainer = contain;
        this.mType = TYPE_Contain;
        this.name =  this.mContainer.getTitle();
        this.url= "";
    }

    public ContainerInfo( Container contain, String nickname) {
        this.mContainer = contain;
        this.mType = TYPE_dd;
        this.name =  nickname;
        this.url= "";
    }

    public ContainerInfo(Item item) {
        this.mItem = item;
        this.mType = TYPE_Item;
        this.name =  this.mItem.getTitle();
        List<Res> c = this.mItem.getResources();
        if( c.size() >1)
            this.url=  c.get(c.size()-1).getValue();
        else
            this.url= "";
    }
    public void setURL(String url_) {

        if(this.mType ==TYPE_Contain) {
            this.url = url_;
            this.mType = TYPE_ee;
        }
    }

    public String getURL( ) {

        String  cx = "";
        if(this.mType ==TYPE_Item) {
            List<Res> c = this.mItem.getResources();
            if( c.size() >1)
                cx=  c.get(0).getValue();

        }
        return cx;
    }

    public Container getContainer() {
        return this.mContainer;
    }
    public Item getItem() {
        return this.mItem;
    }
    public void setContainer(Container contain) {
        this.mContainer = contain;
    }

    public String getName() {
        return this.name;
    }
    public String getId() {

        String name ="";
        if(TYPE_Item == this.mType)
            name = this.mItem.getId();
        else  if(TYPE_Contain == this.mType ||TYPE_ee == this.mType )
            name = this.mContainer.getId();
        else
            name = mID;


        return name;
    }

    public String getParentId() {

        String name ="";
        if(TYPE_Item == this.mType)
            name = this.mItem.getParentID();
        else  if(TYPE_Contain == this.mType ||TYPE_ee == this.mType )
            name = this.mContainer.getParentID();
        else
            name = this.mContainer.getParentID();

        return name;
    }

    public String getResource() {
        return this.url;
    }

    public int getType() {
        return this.mType;
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
