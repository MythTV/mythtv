package org.bluray.ti;

import java.util.Date;

import javax.tv.locator.Locator;
import javax.tv.service.ServiceInformationType;

import org.bluray.net.BDLocator;
import org.davic.net.InvalidLocatorException;
import org.videolan.Libbluray;
import org.videolan.TIClip;
import org.videolan.PlaylistInfo;

public class PlayListImpl implements PlayList {
    protected PlayListImpl(String filename, Title service)
    {
        this.filename = filename;
        this.id = Integer.parseInt(filename);
        this.playlist = Libbluray.getPlaylistInfo(id);
        this.service = service;
    }

    public String getFileName()
    {
        return filename;
    }

    public int getId()
    {
        return id;
    }

    public PlayItem[] getPlayItems()
    {
        TIClip[] clips = playlist.getClips();
        PlayItem[] items = new PlayItem[clips.length];
        
        for (int i = 0; i < clips.length; i++) {
            items[i] = new PlayItemImpl(id, i + 1, clips[i], service);
        }
        
        return items;
    }

    public Locator getLocator()
    {
        int title = Libbluray.getCurrentTitle();
        
        try {
            return new BDLocator(null, title, id);
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

    String filename;
    PlaylistInfo playlist;
    int id;
    Title service;
}
