package org.videolan.bdjo;

public class TerminalInfo {
    public TerminalInfo(String defaultFont, int resolution, boolean menuCallMask, boolean titleSearchMask)
    {
        this.defaultFont = defaultFont;
        this.resolution = GraphicsResolution.fromId(resolution);
        this.menuCallMask = menuCallMask;
        this.titleSearchMask = titleSearchMask;
    }

    public String getDefaultFont()
    {
        return defaultFont;
    }

    public GraphicsResolution getResolution()
    {
        return resolution;
    }

    public boolean getMenuCallMask()
    {
        return menuCallMask;
    }

    public boolean getTitleSearchMask()
    {
        return titleSearchMask;
    }

    public String toString()
    {
        return "TerminalInfo [defaultFont=" + defaultFont + ", menuCallMask="
                + menuCallMask + ", resolution=" + resolution
                + ", titleSearchMask=" + titleSearchMask + "]";
    }

    private String defaultFont;
    private GraphicsResolution resolution;
    private boolean menuCallMask;
    private boolean titleSearchMask;
}
