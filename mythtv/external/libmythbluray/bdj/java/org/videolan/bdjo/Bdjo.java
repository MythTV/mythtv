package org.videolan.bdjo;

import org.videolan.Arrays;

public class Bdjo {
    public Bdjo(TerminalInfo terminalInfo, AppCache[] appCaches,
            PlayListTable accessiblePlaylists, AppEntry[] appTable,
            int keyInterestTable, String fileAccessInfo)
    {
        this.terminalInfo = terminalInfo;
        this.appCaches = appCaches;
        this.accessiblePlaylists = accessiblePlaylists;
        this.appTable = appTable;
        this.keyInterestTable = keyInterestTable;
        this.fileAccessInfo = fileAccessInfo;
    }

    public TerminalInfo getTerminalInfo()
    {
        return terminalInfo;
    }

    public AppCache[] getAppCaches()
    {
        return appCaches;
    }

    public PlayListTable getAccessiblePlaylists()
    {
        return accessiblePlaylists;
    }

    public AppEntry[] getAppTable()
    {
        return appTable;
    }

    public int getKeyInterestTable()
    {
        return keyInterestTable;
    }

    public String getFileAccessInfo()
    {
        return fileAccessInfo;
    }

    public String toString()
    {
        return "Bdjo [accessiblePlaylists=" + accessiblePlaylists
                + ", appCaches=" + Arrays.toString(appCaches) + ", appTable="
                + Arrays.toString(appTable) + ", fileAccessInfo="
                + fileAccessInfo + ", keyInterestTable=" + keyInterestTable
                + ", terminalInfo=" + terminalInfo + "]";
    }

    private TerminalInfo terminalInfo;
    private AppCache[] appCaches;
    private PlayListTable accessiblePlaylists;
    private AppEntry[] appTable;
    private int keyInterestTable;
    private String fileAccessInfo;
}
