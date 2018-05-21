/*
 * This file is part of libbluray
 * Copyright (C) 2010      William Hahne
 * Copyright (C) 2012-2014 Petri Hintukainen <phintuka@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

package org.videolan;

import java.awt.BDFontMetrics;
import java.awt.BDToolkit;
import java.awt.event.KeyEvent;
import java.io.File;
import java.util.HashMap;
import java.util.Map;
import java.util.Vector;

import javax.media.PackageManager;
import javax.tv.service.SIManager;
import javax.tv.service.SIManagerImpl;
import javax.tv.service.selection.ServiceContextFactory;
import javax.tv.service.selection.ServiceContextFactoryImpl;
import org.bluray.bdplus.Status;
import org.bluray.net.BDLocator;
import org.bluray.ti.DiscManager;
import org.bluray.ti.TitleImpl;
import org.bluray.ti.selection.TitleContext;
import org.bluray.ui.event.HRcEvent;
import org.dvb.event.EventManager;
import org.dvb.io.ixc.IxcRegistry;
import org.dvb.ui.FontFactory;
import org.videolan.bdjo.Bdjo;
import org.videolan.media.content.PlayerManager;

/**
 * This class allows BDJ to call various libbluray functions.
 */
public class Libbluray {

    /* hook system properties: make "user.dir" point to current Xlet home directory */

    private static void hookProperties() {
        java.util.Properties p = new java.util.Properties(System.getProperties()) {
                public String getProperty(String key) {
                    if (key.equals("user.dir")) {
                        BDJXletContext ctx = BDJXletContext.getCurrentContext();
                        if (ctx != null) {
                            return ctx.getXletHome();
                        }
                        System.err.println("getProperty(user.dir): no context !  " + Logger.dumpStack());
                    }
                    return super.getProperty(key);
                }
            };
        System.setProperties(p);
    }

    private static boolean initOnce = false;
    private static void initOnce() {
        if (initOnce) {
            return;
        }
        initOnce = true;

        /* hook system properties (provide Xlet-specific user.dir) */
        try {
            hookProperties();
        } catch (Throwable t) {
            System.err.println("hookProperties() failed: " + t);
        }

        /* hook sockets (limit network connections) */
        try {
            BDJSocketFactory.init();
        } catch (Throwable t) {
            System.err.println("Hooking socket factory failed: " + t + "\n" + Logger.dumpStack(t));
        }
    }

    private static String canonicalize(String path, boolean create) {
        try {
            File dir = new File(path);
            if (create) {
                dir.mkdirs();
            }
            return dir.getCanonicalPath();
        } catch (Exception ioe) {
            System.err.println("error canonicalizing " + path + ": " + ioe);
        }
        return path;
    }

    /* called only from native code */
    private static void init(long nativePointer, String discID, String discRoot,
                               String persistentRoot, String budaRoot) {

        initOnce();

        /* set up directories */

        try {
            if (persistentRoot == null) {
                /* no persistent storage */
                persistentRoot = CacheDir.create("dvb.persistent.root").getPath() + File.separator;
            }
            if (budaRoot == null) {
                /* no persistent storage for BUDA */
                budaRoot = CacheDir.create("bluray.bindingunit.root").getPath() + File.separator;
            }
        } catch (java.io.IOException e) {
            System.err.println("Cache creation failed: " + e);
            /* not fatal with most discs */
        }
        if (persistentRoot != null) {
            persistentRoot = canonicalize(persistentRoot, true);
        }
        if (budaRoot != null) {
            budaRoot       = canonicalize(budaRoot, true);
        }

        System.setProperty("dvb.persistent.root", persistentRoot);
        System.setProperty("bluray.bindingunit.root", budaRoot);

        if (discRoot != null) {
            discRoot = canonicalize(discRoot, false);
            System.setProperty("bluray.vfs.root", discRoot);
        } else {
            System.getProperties().remove("bluray.vfs.root");
        }

        /* */

        Libbluray.nativePointer = nativePointer;
        DiscManager.getDiscManager().setCurrentDisc(discID);

        BDJActionManager.createInstance();

        Vector prefix = new Vector();
        prefix.add("org.videolan");
        PackageManager.setContentPrefixList(prefix);
        PackageManager.setProtocolPrefixList(prefix);
        PackageManager.commitContentPrefixList();
        PackageManager.commitProtocolPrefixList();

        try {
            BDFontMetrics.init();
        } catch (Throwable t) {}

        System.setProperty("mhp.profile.enhanced_broadcast", "YES");
        System.setProperty("mhp.profile.interactive_broadcast", "YES");
        System.setProperty("mhp.profile.internet_access", "YES");

        System.setProperty("mhp.eb.version.major", "1");
        System.setProperty("mhp.eb.version.minor", "0");
        System.setProperty("mhp.eb.version.micro", "3");
        System.setProperty("mhp.ia.version.major", "1");
        System.setProperty("mhp.ia.version.minor", "0");
        System.setProperty("mhp.ia.version.micro", "3");
        System.setProperty("mhp.ib.version.major", "1");
        System.setProperty("mhp.ib.version.minor", "0");
        System.setProperty("mhp.ib.version.micro", "3");

        System.setProperty("mhp.option.ip.multicast", "UNSUPPORTED");
        System.setProperty("mhp.option.dsmcc.uu", "UNSUPPORTED");
        System.setProperty("mhp.option.dvb.html", "UNSUPPORTED");

        System.setProperty("dvb.returnchannel.timeout", "30");

        /* get profile from PSR */
        int psr31 = readPSR(PSR_PROFILE_VERSION);
        int profile = psr31 >> 16;
        boolean p11 = (profile & 0x01) != 0;
        boolean p2  = (profile & 0x02) != 0;
        boolean p5  = (profile & 0x10) != 0;

        System.setProperty("bluray.profile.1", "YES");
        System.setProperty("bluray.p1.version.major", "1");
        System.setProperty("bluray.p1.version.minor", p11 ? "1" : "0");
        System.setProperty("bluray.p1.version.micro", "0");

        System.setProperty("bluray.profile.2", p2 ? "YES" : "NO");
        System.setProperty("bluray.p2.version.major", "1");
        System.setProperty("bluray.p2.version.minor", "0");
        System.setProperty("bluray.p2.version.micro", "0");

        System.setProperty("bluray.profile.5", p5 ? "YES" : "NO");
        System.setProperty("bluray.p5.version.major", "1");
        System.setProperty("bluray.p5.version.minor", "0");
        System.setProperty("bluray.p5.version.micro", "0");

        System.setProperty("bluray.disc.avplayback.readcapability", "NO");

        System.setProperty("bluray.video.fullscreenSD", "YES");
        System.setProperty("bluray.video.fullscreenSDPG", "YES");

        System.setProperty("aacs.bluray.online.capability", "YES");
        System.setProperty("aacs.bluray.mc.capability", "NO");

        System.setProperty("bluray.prefetchedplaylistloading", "NO");
        System.setProperty("bluray.video.autoresume", "NO");

        System.setProperty("bluray.mediaselect", "NO");

        System.setProperty("bluray.event.extension", "YES");

        System.setProperty("bluray.jmf.subtitlestyle", "YES");

        System.setProperty("bluray.rccapability.release", "YES");
        System.setProperty("bluray.rccapability.holdandrelease", "YES");
        System.setProperty("bluray.rccapability.repeatonhold", "NO");

        System.setProperty("bluray.localstorage.level", "5");
        System.setProperty("bluray.localstorage.maxlevel", "5");

        System.setProperty("bluray.localstorage.removable", "NO");
        System.setProperty("bluray.localstorage.upgradable", "NO");
        System.setProperty("bluray.localstorage.name", "HDD");

        System.setProperty("bluray.memory.images", "65536");
        System.setProperty("bluray.memory.audio", "8192");
        System.setProperty("bluray.memory.audio_plus_img", "73728");
        System.setProperty("bluray.memory.java_heap", "32768");
        System.setProperty("bluray.memory.font_cache", "4096");

        System.setProperty("bluray.network.connected", "YES");

        try {
            System.setSecurityManager(new BDJSecurityManager(discRoot, persistentRoot, budaRoot));
        } catch (Exception ex) {
            System.err.println("System.setSecurityManager() failed: " + ex);
            throw new SecurityException("Failed initializing SecurityManager");
        }
    }

    /* called only from native code */
    private static void shutdown() {
        if (nativePointer == 0) {
            return;
        }
        try {
            stopTitle(true);
            BDJLoader.shutdown();
            BDJActionManager.shutdown();

            /* all Xlet contexts (and threads) should be terminated now */
            try {
                System.setSecurityManager(null);
            } catch (Exception ex) {
                System.err.println("System.setSecurityManager(null) failed: " + ex);
            }

            MountManager.unmountAll();
            GUIManager.shutdown();
            BDToolkit.shutdownDisc();
            BDFontMetrics.shutdown();
            SIManagerImpl.shutdown();
            IxcRegistry.shutdown();
            EventManager.shutdown();
            Status.shutdown();
            ServiceContextFactoryImpl.shutdown();
            FontFactory.unloadDiscFonts();
            CacheDir.remove();
        } catch (Throwable e) {
            System.err.println("shutdown() failed: " + e + "\n" + Logger.dumpStack(e));
        }
        nativePointer = 0;
        titleInfos = null;
        synchronized (bdjoFilesLock) {
            bdjoFiles = null;
        }
    }

    /*
     * Package private
     */

    /* called by BDJLoader to select HDMV title */
    protected static boolean selectHdmvTitle(int title) {
        return selectTitleN(nativePointer, title) == 1 ? true : false;
    }

    protected static boolean cacheBdRomFile(String path, String cachePath) {
        return cacheBdRomFileN(nativePointer, path, cachePath) == 0;
    }

    protected static void setUOMask(boolean menuCallMask, boolean titleSearchMask) {
        setUOMaskN(nativePointer, menuCallMask, titleSearchMask);
    }

    protected static void setKeyInterest(int mask) {
        setKeyInterestN(nativePointer, mask);
    }

    protected static int setVirtualPackage(String vpPath, boolean initBackupRegs) {
        return setVirtualPackageN(nativePointer, vpPath, initBackupRegs);
    }

    /*
     * Disc titles
     */

    /* used by javax/tv/service/SIManagerImpl */
    public static int numTitles() {
        if (titleInfos == null) {
            titleInfos = getTitleInfosN(nativePointer);
            if (titleInfos == null) {
                return -1;
            }
        }
        return titleInfos.length - 2;
    }

    /* used by org/bluray/ti/TitleImpl */
    public static TitleInfo getTitleInfo(int titleNum) {
        int numTitles = numTitles();
        if (numTitles < 0)
            return null;

        if (titleNum == 0xffff) {
            return titleInfos[titleInfos.length - 1];
        }

        if (titleNum < 0 || titleNum > numTitles)
            throw new IllegalArgumentException();

        return titleInfos[titleNum];
    }

    /* used by org/bluray/ti/PlayListImpl */
    public static int getCurrentTitle() {
        return readPSR(PSR_TITLE_NUMBER);
    }


    /*
     * Disc data
     */

    /* cache parsed .bdjo files */
    private static Map bdjoFiles = null;
    private static Object bdjoFilesLock = new Object();

    public static byte[] getAacsData(int type) {
        return getAacsDataN(nativePointer, type);
    }

    public static PlaylistInfo getPlaylistInfo(int playlist) {
        return getPlaylistInfoN(nativePointer, playlist);
    }

    public static Bdjo getBdjo(String name) {
        Bdjo bdjo;
        synchronized (bdjoFilesLock) {
            if (bdjoFiles == null) {
                bdjoFiles = new HashMap();
            } else {
                bdjo = (Bdjo)bdjoFiles.get(name);
                if (bdjo != null) {
                    return bdjo;
                }
            }

            bdjo = getBdjoN(nativePointer, name + ".bdjo");
            if (bdjo != null) {
                bdjoFiles.put(name, bdjo);
            }
            return bdjo;
         }
    }

    public static String[] listBdFiles(String path, boolean onlyBdRom) {
        return listBdFilesN(nativePointer, path, onlyBdRom);
    }

    /*
     * Playback control
     */

    public static boolean selectPlaylist(int playlist, int playitem, int playmark, long time) {
        if (playlist < 0)
            throw new IllegalArgumentException("Playlist cannot be negative");

        return selectPlaylistN(nativePointer, playlist, playitem, playmark, time) == 1 ? true : false;
    }

    public static boolean selectPlaylist(int playlist) {
        return selectPlaylist(playlist, -1, -1, -1);
    }

    public static void stopPlaylist() {
        selectPlaylistN(nativePointer, -1, -1, -1, -1);
    }

    public static long seekTime(long tick) {
        return seekN(nativePointer, -1, -1, tick);
    }

    public static long seekMark(int mark) {
        if (mark < 0)
            throw new IllegalArgumentException("Mark cannot be negative");

        long result = seekN(nativePointer, -1, mark, -1);
        if (result == -1)
            throw new IllegalArgumentException("Seek error");
        return result;
    }

    public static long seekPlayItem(int clip) {
        if (clip < 0)
            throw new IllegalArgumentException("Mark cannot be negative");

        long result = seekN(nativePointer, clip, -1, -1);
        if (result == -1)
            throw new IllegalArgumentException("Seek error");
        return result;
    }

    public static boolean selectAngle(int angle) {
        if (angle < 1)
            throw new IllegalArgumentException("Angle cannot be negative");

        return selectAngleN(nativePointer, angle) == 1 ? true : false;
    }

    public static int soundEffect(int id) {
        return soundEffectN(nativePointer, id);
    }

    public static int getCurrentAngle() {
        return readPSR(PSR_ANGLE_NUMBER);
    }

    public static long getUOMask() {
        return getUOMaskN(nativePointer);
    }

    public static long tellTime() {
        return tellTimeN(nativePointer);
    }

    public static boolean selectRate(float rate) {
        return selectRateN(nativePointer, rate, 0) == 1 ? true : false;
    }
    public static boolean selectRate(float rate, boolean start) {
        return selectRateN(nativePointer, rate, start ? 1 : 2) == 1 ? true : false;
    }

    /*
     * Register access
     */

    public static void writeGPR(int num, int value) {
        int ret = writeGPRN(nativePointer, num, value);

        if (ret == -1)
            throw new IllegalArgumentException("Invalid GPR");
    }

    public static void writePSR(int num, int value) {
        writePSR(num, value, 0xffffffff);
    }

    public static void writePSR(int num, int value, int psr_value_mask) {
        int ret = writePSRN(nativePointer, num, value, psr_value_mask);

        if (ret == -1)
            throw new IllegalArgumentException("Invalid PSR");
    }

    public static int readGPR(int num) {
        if (num < 0 || (num >= 4096))
            throw new IllegalArgumentException("Invalid GPR");

        return readGPRN(nativePointer, num);
    }

    public static int readPSR(int num) {
        if (num < 0 || (num >= 128))
            throw new IllegalArgumentException("Invalid PSR");

        return readPSRN(nativePointer, num);
    }

    /*
     * Graphics
     */

    public static void updateGraphic(int width, int height, int[] rgbArray) {
        updateGraphicN(nativePointer, width, height, rgbArray,
                       0, 0, width - 1, height - 1);
    }

    public static void updateGraphic(int width, int height, int[] rgbArray,
                                     int x0, int y0, int x1, int y1) {
        updateGraphicN(nativePointer, width, height, rgbArray,
                       x0, y0, x1, y1);
    }

    /*
     * Events from native side
     */

    private static boolean startTitle(int titleNumber) {

        TitleContext titleContext = null;
        try {
            BDLocator locator = new BDLocator(null, titleNumber, -1);
            TitleImpl title   = (TitleImpl)SIManager.createInstance().getService(locator);

            titleContext = (TitleContext)ServiceContextFactory.getInstance().getServiceContext(null);
            titleContext.start(title, true);
            return true;

        } catch (Throwable e) {
            System.err.println("startTitle() failed: " + e + "\n" + Logger.dumpStack(e));
            return false;
        }
    }

    private static boolean stopTitle(boolean shutdown) {
        TitleContext titleContext = null;
        try {
            titleContext = (TitleContext)ServiceContextFactory.getInstance().getServiceContext(null);
            if (shutdown) {
                titleContext.destroy();
            } else {
                titleContext.stop();
            }
            return true;
        } catch (Throwable e) {
            System.err.println("stopTitle() failed: " + e + "\n" + Logger.dumpStack(e));
            return false;
        }
    }

    /* called only from native code */
    private static boolean processEventImpl(int event, int param) {
        boolean result = true;
        int key = 0;

        switch (event) {

        case BDJ_EVENT_START:
            return startTitle(param);
        case BDJ_EVENT_STOP:
            return stopTitle(false);

        case BDJ_EVENT_CHAPTER:
        case BDJ_EVENT_MARK:
        case BDJ_EVENT_PLAYITEM:
        case BDJ_EVENT_PLAYLIST:
        case BDJ_EVENT_ANGLE:
        case BDJ_EVENT_SUBTITLE:
        case BDJ_EVENT_AUDIO_STREAM:
        case BDJ_EVENT_SECONDARY_STREAM:
        case BDJ_EVENT_END_OF_PLAYLIST:
        case BDJ_EVENT_PTS:
        case BDJ_EVENT_UO_MASKED:
        case BDJ_EVENT_SEEK:
        case BDJ_EVENT_RATE:
            result = PlayerManager.getInstance().onEvent(event, param);
            break;

        case BDJ_EVENT_PSR102:
            org.bluray.bdplus.Status.getInstance().receive(param);
            break;

        case BDJ_EVENT_VK_KEY:
            switch (param) {
            case  0: key = KeyEvent.VK_0; break;
            case  1: key = KeyEvent.VK_1; break;
            case  2: key = KeyEvent.VK_2; break;
            case  3: key = KeyEvent.VK_3; break;
            case  4: key = KeyEvent.VK_4; break;
            case  5: key = KeyEvent.VK_5; break;
            case  6: key = KeyEvent.VK_6; break;
            case  7: key = KeyEvent.VK_7; break;
            case  8: key = KeyEvent.VK_8; break;
            case  9: key = KeyEvent.VK_9; break;
            case 11: key = HRcEvent.VK_POPUP_MENU; break;
            case 12: key = KeyEvent.VK_UP; break;
            case 13: key = KeyEvent.VK_DOWN; break;
            case 14: key = KeyEvent.VK_LEFT; break;
            case 15: key = KeyEvent.VK_RIGHT; break;
            case 16: key = KeyEvent.VK_ENTER; break;
            case 403: key = HRcEvent.VK_COLORED_KEY_0; break;
            case 404: key = HRcEvent.VK_COLORED_KEY_1; break;
            case 405: key = HRcEvent.VK_COLORED_KEY_2; break;
            case 406: key = HRcEvent.VK_COLORED_KEY_3; break;
            case 17:
                result = java.awt.BDJHelper.postMouseEvent(0);
                key = -1;
                break;
            default:
                key = -1;
                result = false;
                break;
            }
            if (key > 0) {
                boolean r1 = EventManager.getInstance().receiveKeyEventN(KeyEvent.KEY_PRESSED, 0, key);
                boolean r2 = EventManager.getInstance().receiveKeyEventN(KeyEvent.KEY_TYPED, 0, key);
                boolean r3 = EventManager.getInstance().receiveKeyEventN(KeyEvent.KEY_RELEASED, 0, key);
                result = r1 || r2 || r3;
            }
            break;
        case BDJ_EVENT_MOUSE:
            result = java.awt.BDJHelper.postMouseEvent(param >> 16, param & 0xffff);
            break;
        default:
            System.err.println("Unknown event " + event + "." + param);
            result = false;
        }

        return result;
    }

    private static boolean processEvent(int event, int param) {
        try {
            return processEventImpl(event, param);
        } catch (Throwable e) {
            System.err.println("processEvent() failed: " + e + "\n" + Logger.dumpStack(e));
            return false;
        }
    }

    private static final int BDJ_EVENT_START                    = 1;
    private static final int BDJ_EVENT_STOP                     = 2;
    private static final int BDJ_EVENT_PSR102                   = 3;

    public  static final int BDJ_EVENT_PLAYLIST                 = 4;
    public  static final int BDJ_EVENT_PLAYITEM                 = 5;
    public  static final int BDJ_EVENT_CHAPTER                  = 6;
    public  static final int BDJ_EVENT_MARK                     = 7;
    public  static final int BDJ_EVENT_PTS                      = 8;
    public  static final int BDJ_EVENT_END_OF_PLAYLIST          = 9;

    public  static final int BDJ_EVENT_SEEK                     = 10;
    public  static final int BDJ_EVENT_RATE                     = 11;

    public  static final int BDJ_EVENT_ANGLE                    = 12;
    public  static final int BDJ_EVENT_AUDIO_STREAM             = 13;
    public  static final int BDJ_EVENT_SUBTITLE                 = 14;
    public  static final int BDJ_EVENT_SECONDARY_STREAM         = 15;

    private static final int BDJ_EVENT_VK_KEY                   = 16;
    public  static final int BDJ_EVENT_UO_MASKED                = 17;
    private static final int BDJ_EVENT_MOUSE                    = 18;

    /* TODO: use org/bluray/system/RegisterAccess instead */
    public static final int PSR_IG_STREAM_ID     = 0;
    public static final int PSR_PRIMARY_AUDIO_ID = 1;
    public static final int PSR_PG_STREAM        = 2;
    public static final int PSR_ANGLE_NUMBER     = 3;
    public static final int PSR_TITLE_NUMBER     = 4;
    public static final int PSR_CHAPTER          = 5;
    public static final int PSR_PLAYLIST         = 6;
    public static final int PSR_PLAYITEM         = 7;
    public static final int PSR_TIME             = 8;
    public static final int PSR_NAV_TIMER        = 9;
    public static final int PSR_SELECTED_BUTTON_ID = 10;
    public static final int PSR_MENU_PAGE_ID     = 11;
    public static final int PSR_STYLE            = 12;
    public static final int PSR_PARENTAL         = 13;
    public static final int PSR_SECONDARY_AUDIO_VIDEO = 14;
    public static final int PSR_AUDIO_CAP        = 15;
    public static final int PSR_AUDIO_LANG       = 16;
    public static final int PSR_PG_AND_SUB_LANG  = 17;
    public static final int PSR_MENU_LANG        = 18;
    public static final int PSR_COUNTRY          = 19;
    public static final int PSR_REGION           = 20;
    public static final int PSR_VIDEO_CAP        = 29;
    public static final int PSR_TEXT_CAP         = 30;
    public static final int PSR_PROFILE_VERSION  = 31;
    public static final int PSR_BACKUP_PSR4      = 36;
    public static final int PSR_BACKUP_PSR5      = 37;
    public static final int PSR_BACKUP_PSR6      = 38;
    public static final int PSR_BACKUP_PSR7      = 39;
    public static final int PSR_BACKUP_PSR8      = 40;
    public static final int PSR_BACKUP_PSR10     = 42;
    public static final int PSR_BACKUP_PSR11     = 43;
    public static final int PSR_BACKUP_PSR12     = 44;

    public static final int AACS_DISC_ID           = 1;
    public static final int AACS_MEDIA_VID         = 2;
    public static final int AACS_MEDIA_PMSN        = 3;
    public static final int AACS_DEVICE_BINDING_ID = 4;
    public static final int AACS_DEVICE_NONCE      = 5;
    public static final int AACS_MEDIA_KEY         = 6;
    public static final int AACS_CONTENT_CERT_ID   = 7;
    public static final int AACS_BDJ_ROOT_CERT_HASH= 8;

    private static native byte[] getAacsDataN(long np, int type);
    private static native TitleInfo[] getTitleInfosN(long np);
    private static native PlaylistInfo getPlaylistInfoN(long np, int playlist);
    private static native long seekN(long np, int playitem, int playmark, long time);
    private static native int selectPlaylistN(long np, int playlist, int playitem, int playmark, long time);
    private static native int selectTitleN(long np, int title);
    private static native int selectAngleN(long np, int angle);
    private static native int soundEffectN(long np, int id);
    private static native long getUOMaskN(long np);
    private static native void setUOMaskN(long np, boolean menuCallMask, boolean titleSearchMask);
    private static native void setKeyInterestN(long np, int mask);
    private static native long tellTimeN(long np);
    private static native int selectRateN(long np, float rate, int reason);
    private static native int writeGPRN(long np, int num, int value);
    private static native int writePSRN(long np, int num, int value, int psr_value_mask);
    private static native int readGPRN(long np, int num);
    private static native int setVirtualPackageN(long np, String vpPath, boolean psrBackup);
    private static native int readPSRN(long np, int num);
    private static native int cacheBdRomFileN(long np, String path, String cachePath);
    private static native String[] listBdFilesN(long np, String path, boolean onlyBdRom);
    private static native Bdjo getBdjoN(long np, String name);
    private static native void updateGraphicN(long np, int width, int height, int[] rgbArray,
                                              int x0, int y0, int x1, int y1);

    private static long nativePointer = 0;
    private static TitleInfo[] titleInfos = null;
}
