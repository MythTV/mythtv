package org.videolan.bdjo;

public class AppProfile {
    public AppProfile(short profile, byte major, byte minor, byte micro)
    {
        this.profile = profile;
        this.major = major;
        this.minor = minor;
        this.micro = micro;
    }
    
    public short getProfile()
    {
        return profile;
    }
    
    public byte getMajor()
    {
        return major;
    }
    
    public byte getMinor()
    {
        return minor;
    }
    
    public byte getMicro()
    {
        return micro;
    }

    public String toString()
    {
        return "AppProfile [major=" + major + ", micro=" + micro + ", minor="
                + minor + ", profile=" + profile + "]";
    }

    private short profile;
    private byte major;
    private byte minor;
    private byte micro;
}
