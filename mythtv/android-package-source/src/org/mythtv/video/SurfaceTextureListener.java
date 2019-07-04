package org.mythtv.video;

import android.graphics.SurfaceTexture;

public class SurfaceTextureListener implements SurfaceTexture.OnFrameAvailableListener {

  private long m_callbackData = 0;

  public SurfaceTextureListener(long callbackData) {
    m_callbackData = callbackData;
  }

  @Override
  public void onFrameAvailable (SurfaceTexture surfaceTexture) {
    frameAvailable(m_callbackData, surfaceTexture);
  }

  public native void frameAvailable(long nativeCallbackData, SurfaceTexture surfaceTexture);
}
