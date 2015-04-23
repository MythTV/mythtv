package org.bluray.ti;

import java.util.Date;

import javax.tv.locator.Locator;
import javax.tv.service.ServiceInformationType;
import javax.tv.service.navigation.ServiceComponent;

import org.bluray.net.BDLocator;
import org.davic.net.InvalidLocatorException;
import org.videolan.Libbluray;
import org.videolan.StreamInfo;
import org.videolan.TIClip;

public class PlayItemImpl implements PlayItem {
    protected PlayItemImpl(int playlistId, int playitemId, TIClip clip, Title service)
    {
        this.playlistId = playlistId;
        this.playitemId = playitemId;
        this.clip = clip;
        this.service = service;
    }

    public Locator getLocator()
    {
        int title = Libbluray.getCurrentTitle();
        try {
            return new BDLocator("bd://" + title + ".PLAYLIST:" + playlistId + ".ITEM:" + playitemId);
        } catch (InvalidLocatorException e) {
            return null;
        }
    }

    public ServiceInformationType getServiceInformationType()
    {
        return TitleInformationType.BD_ROM;
    }

    public Date getUpdateTime()
    {
        return null;
    }

    public ServiceComponent[] getComponents()
    {
        StreamInfo[] video = clip.getVideoStreams();
        StreamInfo[] audio = clip.getVideoStreams();
        StreamInfo[] pg = clip.getVideoStreams();
        StreamInfo[] ig = clip.getVideoStreams();
        StreamInfo[] secVideo = clip.getVideoStreams();
        StreamInfo[] secAudio = clip.getVideoStreams();
        
        int count = video.length + audio.length + pg.length +
            ig.length + secVideo.length + secAudio.length;
        
        ServiceComponent[] components = new ServiceComponent[count];
        
        int i = 0;
        for (int j = 0; j < video.length; i++, j++)
            components[i] = new TitleComponentImpl(j + 1, video[j], StreamType.VIDEO, true, playlistId, playitemId, service);
        for (int j = 0; j < audio.length; i++, j++)
            components[i] = new TitleComponentImpl(j + 1, audio[j], StreamType.AUDIO, true, playlistId, playitemId, service);
        for (int j = 0; j < pg.length; i++, j++)
            components[i] = new TitleComponentImpl(j + 1, pg[j], StreamType.SUBTITLES, true, playlistId, playitemId, service);
        for (int j = 0; j < ig.length; i++, j++)
            components[i] = new TitleComponentImpl(j + 1, ig[j], StreamType.DATA, true, playlistId, playitemId, service);
        for (int j = 0; j < secVideo.length; i++, j++)
            components[i] = new TitleComponentImpl(j + 1, secVideo[j], StreamType.VIDEO, false, playlistId, playitemId, service);
        for (int j = 0; j < secAudio.length; i++, j++)
            components[i] = new TitleComponentImpl(j + 1, secAudio[j], StreamType.AUDIO, false, playlistId, playitemId, service);
        
        return components;
    }

    int playlistId;
    int playitemId;
    TIClip clip;
    Title service;
}
