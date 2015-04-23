package org.bluray.ti;

import javax.tv.locator.Locator;
import javax.tv.service.SIRequest;
import javax.tv.service.SIRequestor;
import javax.tv.service.ServiceType;

import org.bluray.net.BDLocator;
import org.davic.net.InvalidLocatorException;
import org.videolan.Libbluray;
import org.videolan.TitleInfo;
import org.videolan.bdjo.Bdjo;

public class TitleImpl implements Title {
    public TitleImpl(int titleNum) {
        this.titleNum = titleNum;
        this.ti = Libbluray.getTitleInfo(titleNum);
        if (ti == null)
            throw new Error("Invalid title " + titleNum);
        if (ti.isBdj()) {
            bdjo = Libbluray.getBdjo(ti.getBdjoName());
            if (bdjo == null)
                throw new Error("Invalid title " + titleNum);
        }
    }

    public PlayList[] getPlayLists() {
        if (bdjo == null)
            return new PlayList[0];
        String[] playlistNames = bdjo.getAccessiblePlaylists().getPlayLists();
        PlayList[] playlists = new PlayList[playlistNames.length];
        for (int i = 0; i < playlistNames.length; i++)
            playlists[i] = new PlayListImpl(playlistNames[i], this);

        return playlists;
    }

    public boolean hasAutoPlayList() {
        if (bdjo == null)
            return false;
        return bdjo.getAccessiblePlaylists().isAutostartFirst();
    }

    public Locator getLocator() {
        String url = "bd://" + Integer.toString(titleNum, 16);
        try {
            return new BDLocator(url);
        } catch (InvalidLocatorException ex) {
            return null;
        }
    }

    public String getName() {
        return "Title " + titleNum;
    }

    public ServiceType getServiceType() {
        switch (ti.getPlaybackType()) {
        case TitleInfo.HDMV_PLAYBACK_TYPE_MOVIE:
            return TitleType.HDMV_MOVIE;
        case TitleInfo.HDMV_PLAYBACK_TYPE_INTERACTIVE:
            return TitleType.HDMV_INTERACTIVE;
        case TitleInfo.BDJ_PLAYBACK_TYPE_MOVIE:
            return TitleType.BDJ_MOVIE;
        case TitleInfo.BDJ_PLAYBACK_TYPE_INTERACTIVE:
            return TitleType.BDJ_INTERACTIVE;
        }
        return null;
    }

    public boolean hasMultipleInstances() {
        return false;
    }

    public SIRequest retrieveDetails(SIRequestor requestor) {
        //TODO
        throw new Error("Not implemented");
    }

    public int getTitleNum() {
        return titleNum;
    }

    public TitleInfo getTitleInfo() {
        return ti;
    }

    private int titleNum;
    private TitleInfo ti;
    private Bdjo bdjo = null;
}
