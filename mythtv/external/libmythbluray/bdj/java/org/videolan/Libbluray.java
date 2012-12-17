/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
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

import java.awt.event.KeyEvent;
import java.util.Vector;

import javax.media.PackageManager;
import javax.tv.service.selection.ServiceContextFactory;

import org.bluray.ti.DiscManager;
import org.bluray.ti.TitleImpl;
import org.bluray.ti.selection.TitleContext;
import org.bluray.ui.event.HRcEvent;
import org.dvb.event.EventManager;
import org.dvb.ui.FontFactory;
import org.videolan.bdjo.Bdjo;
import org.videolan.media.content.BDHandler;

/**
 * This class allows BDJ to call various libbluray functions.
 */
public class Libbluray {
        protected static void init(long nativePointer, String discID) {
            System.loadLibrary("bluray");

            Libbluray.nativePointer = nativePointer;
            DiscManager.getDiscManager().setCurrentDisc(discID);

            Vector prefix = new Vector();
            prefix.add("org.videolan");
            PackageManager.setContentPrefixList(prefix);
            PackageManager.setProtocolPrefixList(prefix);
            PackageManager.commitContentPrefixList();
            PackageManager.commitProtocolPrefixList();

            FontFactory.loadDiscFonts();

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

            System.setProperty("bluray.profile.1", "YES");
            System.setProperty("bluray.p1.version.major", "1");
            System.setProperty("bluray.p1.version.minor", "1");
            System.setProperty("bluray.p1.version.micro", "0");

            System.setProperty("bluray.profile.2", "YES");
            System.setProperty("bluray.p2.version.major", "1");
            System.setProperty("bluray.p2.version.minor", "0");
            System.setProperty("bluray.p2.version.micro", "0");

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

            System.setProperty("bluray.rccapability.release", "No");
            System.setProperty("bluray.rccapability.holdandrelease", "NO");
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
    }

    public static void shutdown() {
        try {
            BDJLoader.shutdown();
            BDJActionManager.getInstance().finalize();
            MountManager.unmountAll();
        } catch (Throwable e) {
            e.printStackTrace();
        }
        nativePointer = 0;
    }

    public static int getTitles() {
        return getTitlesN(nativePointer);
    }

    public static TitleInfo getTitleInfo(int titleNum) {
        if (titleNum < 0)
            throw new IllegalArgumentException();

        return getTitleInfoN(nativePointer, titleNum);
    }

    public static PlaylistInfo getPlaylistInfo(int playlist) {
        return getPlaylistInfoN(nativePointer, playlist);
    }

    public static long seek(long pos) {
        return seekN(nativePointer, pos);
    }

    public static long seekTime(long tick) {
        return seekTimeN(nativePointer, tick);
    }

    public static long seekChapter(int chapter) {
        if (chapter < 0)
            throw new IllegalArgumentException("Chapter cannot be negative");

        return seekChapterN(nativePointer, chapter);
    }

    public static long chapterPos(int chapter) {
        if (chapter < 0)
            throw new IllegalArgumentException("Chapter cannot be negative");

        return chapterPosN(nativePointer, chapter);
    }

    public static int getCurrentChapter(){
        return getCurrentChapterN(nativePointer);
    }

    public static long seekMark(int mark) {
        if (mark < 0)
            throw new IllegalArgumentException("Mark cannot be negative");

        long result = seekMarkN(nativePointer, mark);
        if (result == -1)
            throw new IllegalArgumentException("Seek error");
        return result;
    }

    public static long seekPlayItem(int clip) {
        if (clip < 0)
            throw new IllegalArgumentException("Mark cannot be negative");

        long result = seekPlayItemN(nativePointer, clip);
        if (result == -1)
            throw new IllegalArgumentException("Seek error");
        return result;
    }

    public static boolean selectPlaylist(int playlist) {
        if (playlist < 0)
            throw new IllegalArgumentException("Playlist cannot be negative");

        return selectPlaylistN(nativePointer, playlist) == 1 ? true : false;
    }

    public static boolean selectTitle(TitleImpl title) {
        TitleInfo ti = title.getTitleInfo();
        if (ti.isBdj()) {
                try {
                        ((TitleContext)ServiceContextFactory.getInstance().getServiceContext(null)).select(title);
                        return true;
                } catch (Exception e) {
                        e.printStackTrace();
                        return false;
                }
        }

        return selectTitleN(nativePointer, title.getTitleNum()) == 1 ? true : false;
    }

    public static boolean selectAngle(int angle) {
        if (angle < 0)
            throw new IllegalArgumentException("Angle cannot be negative");

        return selectAngleN(nativePointer, angle) == 1 ? true : false;
    }

    public static void seamlessAngleChange(int angle) {
        if (angle < 0)
            throw new IllegalArgumentException("Angle cannot be negative");

        seamlessAngleChangeN(nativePointer, angle);
    }

    public static long getTitleSize() {
        return getTitleSizeN(nativePointer);
    }

    public static int getCurrentTitle() {
        return getCurrentTitleN(nativePointer);
    }

    public static int getCurrentAngle() {
        return getCurrentAngleN(nativePointer);
    }

    public static long tell() {
        return tellN(nativePointer);
    }

    public static long tellTime() {
        return tellTimeN(nativePointer);
    }

    public static boolean selectRate(float rate) {
        return selectRateN(nativePointer, rate) == 1 ? true : false;
    }

    public static void writeGPR(int num, int value) {
        int ret = writeGPRN(nativePointer, num, value);

        if (ret == -1)
            throw new IllegalArgumentException("Invalid GPR");
    }

    public static void writePSR(int num, int value) {
        int ret = writePSRN(nativePointer, num, value);

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

    public static Bdjo getBdjo(String name) {
        return getBdjoN(nativePointer, name);
    }

    public static void updateGraphic(int width, int height, int[] rgbArray) {
        updateGraphicN(nativePointer, width, height, rgbArray);
    }

    public static void processEvent(int event, int param) {
        int key = 0;
        switch (event) {
        case BDJ_EVENT_CHAPTER:
            BDHandler.onChapterReach(param);
            break;
        case BDJ_EVENT_PLAYITEM:
            BDHandler.onPlayItemReach(param);
            break;
        case BDJ_EVENT_ANGLE:
            BDHandler.onAngleChange(param);
            break;
        case BDJ_EVENT_SUBTITLE:
            BDHandler.onSubtitleChange(param);
            break;
        case BDJ_EVENT_PIP:
            BDHandler.onPiPChange(param);
            break;
        case BDJ_EVENT_END_OF_PLAYLIST:
            BDHandler.activePlayerEndOfMedia();
            break;
        case BDJ_EVENT_PTS:
            BDHandler.activePlayerUpdateTime(param);
            break;
        case BDJ_EVENT_VK_KEY:
            //case KeyEvent.KEY_TYPED:
            //case KeyEvent.KEY_PRESSED:
            //case KeyEvent.KEY_RELEASED:
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
            default: key = -1; break;
            }
            if (key > 0) {
                EventManager.getInstance().receiveKeyEvent(KeyEvent.KEY_TYPED, 0, key);
            }
            break;
        }
    }

    private static final int BDJ_EVENT_CHAPTER                  = 1;
    private static final int BDJ_EVENT_PLAYITEM                 = 2;
    private static final int BDJ_EVENT_ANGLE                    = 3;
    private static final int BDJ_EVENT_SUBTITLE                 = 4;
    private static final int BDJ_EVENT_PIP                      = 5;
    private static final int BDJ_EVENT_END_OF_PLAYLIST          = 6;
    private static final int BDJ_EVENT_PTS                      = 7;
    private static final int BDJ_EVENT_VK_KEY                   = 8;

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

    private static native TitleInfo getTitleInfoN(long np, int title);
    private static native PlaylistInfo getPlaylistInfoN(long np, int playlist);
    private static native int getTitlesN(long np);
    private static native long seekN(long np, long pos);
    private static native long seekTimeN(long np, long tick);
    private static native long seekChapterN(long np, int chapter);
    private static native long chapterPosN(long np, int chapter);
    private static native int getCurrentChapterN(long np);
    private static native long seekMarkN(long np, int mark);
    private static native long seekPlayItemN(long np, int clip);
    private static native int selectPlaylistN(long np, int playlist);
    private static native int selectTitleN(long np, int title);
    private static native int selectAngleN(long np, int angle);
    private static native void seamlessAngleChangeN(long np, int angle);
    private static native long getTitleSizeN(long np);
    private static native int getCurrentTitleN(long np);
    private static native int getCurrentAngleN(long np);
    private static native long tellN(long np);
    private static native long tellTimeN(long np);
    private static native int selectRateN(long np, float rate);
    private static native int writeGPRN(long np, int num, int value);
    private static native int writePSRN(long np, int num, int value);
    private static native int readGPRN(long np, int num);
    private static native int readPSRN(long np, int num);
    private static native Bdjo getBdjoN(long np, String name);
    private static native void updateGraphicN(long np, int width, int height, int[] rgbArray);

    protected static long nativePointer = 0;
}
