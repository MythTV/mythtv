package org.mythtv.audio;

import android.media.AudioTrack;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTimestamp;
import java.nio.ByteBuffer;

public class AudioOutputAudioTrack
{
    AudioTrack player;
    AudioTimestamp timestamp = new AudioTimestamp();
    long timelasttaken;
    int samplerate;
    long firstwritetime;
    long lastwritetime;
    int bufferedBytes;
    Object syncBuffer;
    int bufferSize;
    int channels;
    int bitsPer10Frames;
    long bytesWritten;
    int latency;
    int latencyTot;
    int latencyCount;
    boolean isSettled;
    boolean isBufferInFlux;
    boolean isOutputThreadStarted;

    public AudioOutputAudioTrack(int encoding, int sampleRate, int bufferSize, int channels)
    {
        syncBuffer = new Object();
        this.bufferSize = bufferSize;
        this.channels = channels;
        AudioAttributes.Builder aab = new AudioAttributes.Builder();
        aab.setUsage(AudioAttributes.USAGE_MEDIA);
        aab.setContentType(AudioAttributes.CONTENT_TYPE_MOVIE);
        AudioAttributes aa = aab.build();

        AudioFormat.Builder afb = new AudioFormat.Builder();
        afb.setEncoding (encoding);
        afb.setSampleRate (sampleRate);
        int channelMask = 0;
        switch (channels)
        {
            case 1:
                channelMask = AudioFormat.CHANNEL_OUT_MONO;
                break;
            case 2:
                channelMask = AudioFormat.CHANNEL_OUT_STEREO;
                break;
            case 3:
                channelMask = AudioFormat.CHANNEL_OUT_STEREO
                    | AudioFormat.CHANNEL_OUT_FRONT_CENTER;
                break;
            case 4:
                channelMask = AudioFormat.CHANNEL_OUT_QUAD;
                break;
            case 5:
                channelMask = AudioFormat.CHANNEL_OUT_QUAD
                    | AudioFormat.CHANNEL_OUT_FRONT_CENTER;
                break;
            case 6:
                channelMask = AudioFormat.CHANNEL_OUT_5POINT1;
                break;
            case 7:
                channelMask = AudioFormat.CHANNEL_OUT_5POINT1
                    | AudioFormat.CHANNEL_OUT_BACK_CENTER;
                break;
            case 8:
                // CHANNEL_OUT_7POINT1_SURROUND is only defined
                // in api level 23 and up.
                // channelMask = AudioFormat.CHANNEL_OUT_7POINT1_SURROUND;
                channelMask = AudioFormat.CHANNEL_OUT_5POINT1
                    | AudioFormat.CHANNEL_OUT_SIDE_LEFT
                    | AudioFormat.CHANNEL_OUT_SIDE_RIGHT;
                break;
            default:
                // default treated as 2 channel (stereo)
                channelMask |=  AudioFormat.CHANNEL_OUT_STEREO;
                break;
        }
        afb.setChannelMask(channelMask);
        AudioFormat af = afb.build();
        samplerate = sampleRate;
        int state = 0;

        for (int i = 0; i < 10; i++)
        {
            player = new AudioTrack(aa, af, bufferSize,
                AudioTrack.MODE_STREAM, AudioManager.AUDIO_SESSION_ID_GENERATE);
            state = player.getState();
            if (state == AudioTrack.STATE_INITIALIZED)
                break;
            try
            {
                Thread.sleep(50);
            }
            catch (InterruptedException ex) { }
        }

        player.play();
    }

    public int write(byte[] audioData, int sizeInBytes)
    {
        isBufferInFlux = false;
        if (player.getPlayState() != AudioTrack.PLAYSTATE_PLAYING)
            player.play();
        if (firstwritetime == 0)
            firstwritetime = System.nanoTime();
        ByteBuffer buf = ByteBuffer.wrap(audioData);
        int written = 0;
        int ret = 0;
        int i;
        while (buf.hasRemaining())
        {
            synchronized(syncBuffer)
            {
                // get out if we should not be here
                if (player == null || !isOutputThreadStarted)
                    break;
                ret = player.write(buf, buf.remaining(), AudioTrack.WRITE_NON_BLOCKING);
                if (ret < 0)
                {
                    break;
                }
                written += ret;
                bytesWritten += ret;
                lastwritetime = System.nanoTime();
                // Note that only after this method returns is this data
                // removed from the caller's buffer.
                // bufferedBytes may be negative because actually some
                // data still in the "Audio circular buffer" may have
                // already played.
                bufferedBytes = buf.remaining() - sizeInBytes;
            }
            try
            {
                Thread.sleep(10);
            }
            catch (InterruptedException ex) {}
        }
        synchronized(syncBuffer)
        {
            // After we return to caller, the data will be removed from
            // the "Audio circular buffer", so the buffered bytes are now zero
            isBufferInFlux = true;
            bufferedBytes = 0;
        }
        return written;
    }

    public void resetFlux ()
    {
        isBufferInFlux = false;
    }

    public int getBufferedBytes ()
    {
        int ret;
        synchronized(syncBuffer)
        {
            int i = 0;
            if (isBufferInFlux)
            {
                try
                {
                    // sleep 1 millisec to let code get from write to
                    // data removal from "Audio circular buffer".
                    // This is a crude way of doing it. A better way
                    // would be a call to resetFlux
                    // after "m_raud = next_raud" and a wait here
                    // to ensure this is only done after that resetFlux.
                    Thread.sleep(1);
                }
                catch (InterruptedException ex) {}
            }
            ret = bufferedBytes;
        }
        return ret;
    }

    // Get latency in milliseconds
    // averaged over the first 10 seconds of playback plus
    // at least 100 readings
    public int getLatencyViaHeadPosition ()
    {
        if (!isSettled && player != null && bytesWritten > 0
            && bitsPer10Frames > 0)
        {
            int headPos = player.getPlaybackHeadPosition();
            synchronized(syncBuffer)
            {
                long frameWritten = bytesWritten * 80 / bitsPer10Frames;
                long framesInProg = frameWritten - headPos;
                int new_latency = (int)(framesInProg * 1000 / samplerate);
                if (new_latency >= 0 && new_latency < 1500)
                {
                    latencyTot += new_latency;
                    latency = latencyTot / (++latencyCount);
                }
            }
            if ((headPos > samplerate * 10 && latencyCount > 100)
                || latencyCount > 1000)
                isSettled = true;
        }
        return latency;
    }


    public void setBitsPer10Frames (int bitsPer10Frames)
    {
        this.bitsPer10Frames = bitsPer10Frames;
    }

    public void pause (boolean doPause)
    {
        if (player == null)
            return;

        if (doPause)
        {
            if (player.getPlayState() == AudioTrack.PLAYSTATE_PLAYING)
                player.pause();
        }
        else
        {
            if (player.getPlayState() != AudioTrack.PLAYSTATE_PLAYING)
                player.play();
        }
    }

    public void release ()
    {
        if (player == null)
            return;

        synchronized(syncBuffer)
        {
            pause(true);
            player.flush();
            player.release();
            player = null;
        }
    }

    public void setOutputThread (boolean isStarted)
    {
        isOutputThreadStarted = isStarted;
    }

}
