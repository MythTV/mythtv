package org.mythtv;


public class Utility {

    // added for suspending sleep
    public static void setSuspendSleep(android.app.Activity activity)
    {
        activity.runOnUiThread( new Runnable() {
            public void run() {
                activity.getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON );
            }
        } );
    }

    public static void setAllowSleep(android.app.Activity activity)
    {
        activity.runOnUiThread( new Runnable() {
            public void run() {
                activity.getWindow().clearFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON );
            }
        } );
    }

}
