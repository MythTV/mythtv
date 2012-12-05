package org.videolan.bdjo;


public class AppCache {
    public AppCache(int type, String refToName, String language)
    {
        this.type = type;
        this.refToName = refToName;
        this.language = language;
    }

    public int getType()
    {
        return type;
    }
    
    public String getRefToName()
    {
        return refToName;
    }
    
    public String getLanguage()
    {
        return language;
    }
    
    public String toString()
    {
        return "AppCache [language=" + language + ", refToName=" + refToName + ", type=" + type + "]";
    }
    
    public static final int JAR_FILE = 1;
    public static final int DIRECTORY = 2;

    private int type;
    private String refToName;
    private String language;
}
