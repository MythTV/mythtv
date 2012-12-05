package org.videolan.bdjo;

import org.videolan.Arrays;

public class PlayListTable {
    public PlayListTable(boolean accessToAll, boolean autostartFirst, String[] playLists)
    {
        this.accessToAll = accessToAll;
        this.autostartFirst = autostartFirst;
        this.playLists = playLists;
    }
    
    public boolean isAccessToAll()
    {
        return accessToAll;
    }
    
    public boolean isAutostartFirst()
    {
        return autostartFirst;
    }
    
    public String[] getPlayLists()
    {
        return playLists;
    }

    public String toString()
    {
        return "PlayListTable [accessToAll=" + accessToAll + ", playLists="
                + Arrays.toString(playLists) + "]";
    }

    private boolean accessToAll;
    private boolean autostartFirst;
    private String[] playLists;
}
