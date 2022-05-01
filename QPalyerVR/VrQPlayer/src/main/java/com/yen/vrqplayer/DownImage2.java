package com.yen.vrqplayer;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.AsyncTask;
import android.util.Log;
import android.widget.ImageView;

import java.io.InputStream;
import java.lang.ref.WeakReference;
import java.net.HttpURLConnection;
import java.net.URL;
import com.yen.dlna.bean.ContainerInfo;
/**
 * Downloads an image asynchronously from the internet
 */



public class DownImage2 extends AsyncTask<ContainerInfo, Void, Bitmap> {

    private static final String TAG = MainActivity.class.getSimpleName();
    private ContainerInfo imageView = null;

    /**
     * Download the image
     * @param imageViews image view with tag set to image URL
     * @return the downloaded bitmap
     */
    @Override
    protected Bitmap doInBackground(ContainerInfo[] imageViews) {
        this.imageView = imageViews[0];
        return downloadImage((String)imageView.getResource());
    }


    /**
     * Populate the imageView with the downloaded image
     * @param bitmap the downloaded image
     */
    @Override
    protected void onPostExecute(Bitmap bitmap) {
        Log.d(TAG, "onPostExecute");
        Log.d(TAG,  imageView.getResource());
        Log.d(TAG, String.valueOf(imageView.getType()));
        Log.d(TAG,  imageView.getName());

        synchronized (MainActivity.class) {
            imageView.setBMP(bitmap);
            imageView.showView();
        }

    }

    /**
     * Download the image from the given URL
     * @param urlString the URL to download
     * @return a bitmap of the downloaded image
     */
    private Bitmap downloadImage(String urlString) {
        Bitmap bmp = null;
        try {
            Log.d(TAG, "downloadImage");
            Log.d(TAG, urlString);
            URL url = new URL(urlString);
            HttpURLConnection conn = (HttpURLConnection) url.openConnection();
            InputStream is = conn.getInputStream();
            bmp = BitmapFactory.decodeStream(is);
            // Thread.sleep(500);
            return bmp;
        }
        catch (Exception e) {
            e.printStackTrace();
        }
        return bmp;
    }
}