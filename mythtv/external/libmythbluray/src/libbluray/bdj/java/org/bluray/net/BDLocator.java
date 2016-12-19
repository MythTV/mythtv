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

package org.bluray.net;

import org.davic.net.Locator;
import org.davic.net.InvalidLocatorException;

import org.videolan.BDJUtil;
import org.videolan.Logger;

public class BDLocator extends Locator {

    /*
    /* range checks
     */

    private void checkDiscId(String disc) throws InvalidLocatorException {
        if (disc == null) {
            return;
        }
        if (disc.length() == 32) {
            try {
                new java.math.BigInteger(disc, 16);
                return;
            } catch (NumberFormatException e) {
            }
        }
        logger.error("Invalid Disc ID: " + disc);
        throw new InvalidLocatorException();
    }

    private void checkTitle(int title) throws InvalidLocatorException {
        if ((title >= -1) && (title <= 999)) {
            return;
        }
        if ((title == 65534) || (title == 65535)) {
            // "resume" and First Play
            return;
        }
        logger.error("Invalid title number: " + title);
        throw new InvalidLocatorException();
    }

    private void checkPlaylist(int pl) throws InvalidLocatorException {
        if ((pl >= -1) && (pl <= 1999)) {
            return;
        }
        logger.error("Invalid playlist id: " + pl);
        throw new InvalidLocatorException();
    }

    private void checkPlayitem(int pi) throws InvalidLocatorException {
        if ((pi >= -1) && (pi <= 998)) {
            return;
        }
        logger.error("Invalid playitem id: " + pi);
        throw new InvalidLocatorException();
    }

    private void checkMark(int mark) throws InvalidLocatorException {
        if ((mark >= -1) && (mark <= 998)) {
            return;
        }
        logger.error("Invalid playmark id: " + mark);
        throw new InvalidLocatorException();
    }

    private void checkJar(int jar) throws InvalidLocatorException {
        if ((jar >= -1) && (jar <= 99999)) {
            return;
        }
        logger.error("Invalid JAR id: " + jar);
        throw new InvalidLocatorException();
    }

    private void checkSound(int sound) throws InvalidLocatorException {
        if ((sound >= -1) && (sound <= 127)) {
            return;
        }
        logger.error("Invalid sound id: " + sound);
        throw new InvalidLocatorException();
    }

    /*
     *
     */

    public BDLocator(String url) throws InvalidLocatorException {
        super(url);
        try {

            if (!url.startsWith("bd://"))
                throw new InvalidLocatorException();
            String str = url.substring(5);
            if (!parseJar(str) && !parseSound(str) && !parsePlaylist(str))
                throw new InvalidLocatorException();

        } catch (InvalidLocatorException e) {
            logger.error("Invalid locator: " + url);
            throw e;
        }
    }

    public BDLocator(String disc, int titleNum, int playList) throws InvalidLocatorException {
        super(null);

        checkDiscId(disc);
        checkTitle(titleNum);
        checkPlaylist(playList);

        this.disc = disc;
        this.titleNum = titleNum;
        this.playList = playList;
        url = getUrl();
    }

    public BDLocator(String disc, int titleNum, int jar, int sound) throws InvalidLocatorException {
        super(null);

        checkDiscId(disc);
        checkTitle(titleNum);
        checkJar(jar);
        checkSound(sound);

        if ((jar >= 0) && (sound >= 0)) {
            logger.error("Invalid locator: jar ID and sound ID set");
            throw new InvalidLocatorException();
        }

        this.disc = disc;
        this.titleNum = titleNum;
        this.jar = jar;
        this.sound = sound;
        url = getUrl();
    }

    public BDLocator(String disc, int titleNum, int playList, int playItem, int mark, String[] componentTags)
            throws InvalidLocatorException {
        super(null);

        checkDiscId(disc);
        checkTitle(titleNum);
        checkPlaylist(playList);
        checkPlayitem(playItem);
        checkMark(mark);

        this.disc = disc;
        this.titleNum = titleNum;
        this.playList = playList;
        this.playItem = playItem;
        this.mark = mark;

        if (componentTags != null) {
            try {
                for (int i = 0; i < componentTags.length; i++) {
                    String comp = componentTags[i];
                    if (comp.startsWith("A1:"))
                        primaryAudioNum = Integer.parseInt(comp.substring(3));
                    else if (comp.startsWith("A2:"))
                        secondaryAudioNum = Integer.parseInt(comp.substring(3));
                    else if (comp.startsWith("V1:"))
                        primaryVideoNum = Integer.parseInt(comp.substring(3));
                    else if (comp.startsWith("V2:"))
                        secondaryVideoNum = Integer.parseInt(comp.substring(3));
                    else if (comp.startsWith("P:"))
                        textStreamNum = Integer.parseInt(comp.substring(2));
                    else {
                        logger.error("Invalid locator: unknown component tag in " + comp);
                        throw new InvalidLocatorException();
                    }
                }
            } catch (NumberFormatException e) {
                logger.error("Invalid locator: invalid component tag found");
                throw new InvalidLocatorException();
            }
        }
        url = getUrl();
    }

    /*
     *
     */

    public boolean equals(Object obj) {
        if (obj == null)
            return false;
        if (this == obj)
            return true;
        if (getClass() != obj.getClass())
            return false;
        return url.equals(((BDLocator)obj).url);
    }

    public boolean isJarFileItem() {
        return jar >= 0;
    }

    public boolean isPlayListItem() {
        return playList >= 0;
    }

    public boolean isSoundItem() {
        return sound >= 0;
    }

    public int getComponentTagsCount() {
        if (!isPlayListItem())
            return 0;
        int nTags = 0;;
        if (primaryVideoNum > 0)
            nTags++;
        if (primaryAudioNum > 0)
            nTags++;
        if (secondaryVideoNum > 0)
            nTags++;
        if (secondaryAudioNum > 0)
            nTags++;
        if (textStreamNum > 0)
            nTags++;
        return nTags;
    }

    public String[] getComponentTags() {
        int nTags = getComponentTagsCount();
        if (nTags <= 0)
            return new String[0];
        String[] tags = new String[nTags];
        if (textStreamNum > 0)
            tags[--nTags] = "P:" + textStreamNum;
        if (secondaryAudioNum > 0)
            tags[--nTags] = "A2:" + secondaryAudioNum;
        if (secondaryVideoNum > 0)
            tags[--nTags] = "V2:" + secondaryVideoNum;
        if (primaryAudioNum > 0)
            tags[--nTags] = "A1:" + primaryAudioNum;
        if (primaryVideoNum > 0)
            tags[--nTags] = "V1:" + primaryVideoNum;
        return tags;
    }

    public String getDiscId() {
        return disc;
    }

    public int getTitleNumber() {
        return titleNum;
    }

    public int getJarFileId() {
        return jar;
    }

    public String getPathSegments() {
        return pathSegments;
    }

    public int getSoundId() {
        return sound;
    }

    public int getPlayListId() {
        return playList;
    }

    public int getMarkId() {
        return mark;
    }

    public int getPlayItemId() {
        return playItem;
    }

    public int getPrimaryAudioStreamNumber() {
        return primaryAudioNum;
    }

    public int getSecondaryAudioStreamNumber() {
        return secondaryAudioNum;
    }

    public int getPrimaryVideoStreamNumber() {
        return primaryVideoNum;
    }

    public int getSecondaryVideoStreamNumber() {
        return secondaryVideoNum;
    }

    public int getPGTextStreamNumber() {
        return textStreamNum;
    }

    /*
     *
     */

    public void setPlayListId(int id) {
        if ((id >= 0) && (id != playList)) {
            playList = id;
            url = getUrl();
        }
    }

    public void setMarkId(int id) {
        if ((id >= 0) && (id != mark)) {
            mark = id;
            url = getUrl();
        }
    }

    public void setPlayItemId(int id) {
        if ((id >= 0) && (id != playItem)) {
            playItem = id;
            url = getUrl();
        }
    }

    public void setPrimaryAudioStreamNumber(int num) {
        if ((num >= 0) && (num != primaryAudioNum)) {
            primaryAudioNum = num;
            url = getUrl();
        }
    }

    public void setSecondaryAudioStreamNumber(int num) {
        if ((num >= 0) && (num != secondaryAudioNum)) {
            secondaryAudioNum = num;
            url = getUrl();
        }
    }

    public void setPrimaryVideoStreamNumber(int num) {
        if ((num >= 0) && (num != primaryVideoNum)) {
            primaryVideoNum = num;
            url = getUrl();
        }
    }

    public void setSecondaryVideoStreamNumber(int num) {
        if ((num >= 0) && (num != secondaryVideoNum)) {
            secondaryVideoNum = num;
            url = getUrl();
        }
    }

    public void setPGTextStreamNumber(int num) {
        if ((num >= 0) && (num != textStreamNum)) {
            textStreamNum = num;
            url = getUrl();
        }
    }

    protected String getUrl() {
        String str = "bd://";

        if (disc != null && disc != "") {
            str += disc;
            if (titleNum >= 0) {
                str += ".";
            }
        }

        if (titleNum >= 0) {
            str += Integer.toString(titleNum, 16);
            if (jar >= 0 || playList >= 0 || sound >= 0) {
                str += ".";
            }
        }

        if (jar >= 0) {
            str += "JAR:" + BDJUtil.makeFiveDigitStr(jar);

            if (pathSegments != null)
                str += pathSegments;
        } else if (playList >= 0) {
            str += "PLAYLIST:" + BDJUtil.makeFiveDigitStr(playList);

            if (playItem >= 0)
                str += ".ITEM:" + BDJUtil.makeFiveDigitStr(playItem);
            if (mark >= 0)
                str += ".MARK:" + BDJUtil.makeFiveDigitStr(mark);

            String prefix = ".";
            if (primaryAudioNum > 0) {
                str += prefix + "A1:" + primaryAudioNum;
                if (prefix.equals("."))
                    prefix = "&";
            }
            if (secondaryAudioNum > 0) {
                str += prefix + "A2:" + secondaryAudioNum;
                if (prefix.equals("."))
                    prefix = "&";
            }
            if (primaryVideoNum > 0) {
                str += prefix + "V1:" + primaryVideoNum;
                if (prefix.equals("."))
                    prefix = "&";
            }
            if (secondaryVideoNum > 0) {
                str += prefix + "V2:" + secondaryVideoNum;
                if (prefix.equals("."))
                    prefix = "&";
            }
            if (textStreamNum > 0) {
                str += prefix + "P:" + textStreamNum;
                if (prefix.equals("."))
                    prefix = "&";
            }
        } else if (sound >= 0) {
            str += "SOUND:" + Integer.toString(sound, 16);
        }

        return str;
    }

    /*
     * parsing (used in constructor)
     */

    private boolean parseJar(String str) throws InvalidLocatorException {
        if (!str.startsWith("JAR:"))
            return false;
        if (str.length() < 9)
            throw new InvalidLocatorException();
        try {
            jar = Integer.parseInt(str.substring(4, 9));
        } catch(NumberFormatException e) {
            throw new InvalidLocatorException();
        }
        if (str.length() > 9)
            pathSegments = str.substring(9);
        return true;
    }

    private boolean parseSound(String str) throws InvalidLocatorException {
        if (!str.startsWith("SOUND:"))
            return false;
        try {
            sound = Integer.parseInt(str.substring(6), 16);
        } catch (NumberFormatException e) {
            throw new InvalidLocatorException();
        }
        return true;
    }

    private boolean parsePlaylist(String str) throws InvalidLocatorException {
        boolean isTag = false;
        int length, begin, end;
        length = str.length();
        begin = 0;
        end = str.indexOf('.');
        if (end < 0)
            end = length;
        while (end <= length) {
            String element = str.substring(begin, end);
            try {
                if (playList < 0) {
                    if ((end - begin) == 32) {
                        checkDiscId(element);
                        disc = element;
                    } else if ((end - begin) <= 4) {
                        titleNum = Integer.parseInt(element, 16);
                        checkTitle(titleNum);
                    } else if (element.startsWith("PLAYLIST:")) {
                        playList = Integer.parseInt(element.substring(9));
                        checkPlaylist(playList);
                    } else {
                        throw new InvalidLocatorException();
                    }
                } else if (element.startsWith("MARK:")) {
                    mark = Integer.parseInt(element.substring(5));
                    checkMark(mark);
                } else if (element.startsWith("ITEM:")) {
                    playItem = Integer.parseInt(element.substring(5));
                    checkPlayitem(playItem);
                } else if (element.startsWith("A1:")) {
                    primaryAudioNum = Integer.parseInt(element.substring(3));
                    if (primaryAudioNum < 0)
                        throw new InvalidLocatorException();
                    isTag = true;
                } else if (element.startsWith("A2:")) {
                    secondaryAudioNum = Integer.parseInt(element.substring(3));
                    if (secondaryAudioNum < 0)
                        throw new InvalidLocatorException();
                    isTag = true;
                } else if (element.startsWith("V1:")) {
                    primaryVideoNum = Integer.parseInt(element.substring(3));
                    if (primaryVideoNum < 0)
                        throw new InvalidLocatorException();
                    isTag = true;
                } else if (element.startsWith("V2:")) {
                    secondaryVideoNum = Integer.parseInt(element.substring(3));
                    if (secondaryVideoNum < 0)
                        throw new InvalidLocatorException();
                    isTag = true;
                } else if (element.startsWith("P:")) {
                    textStreamNum = Integer.parseInt(element.substring(2));
                    if (textStreamNum < 0)
                        throw new InvalidLocatorException();
                    isTag = true;
                } else {
                    logger.error("Unknown tag: " + element);
                    throw new InvalidLocatorException();
                }
            } catch (NumberFormatException e) {
                logger.error("Parse error: " + e);
                throw new InvalidLocatorException();
            }
            if (end >= length)
                break;
            begin = end + 1;
            if (isTag) {
                end = str.indexOf('&', begin);
                if (end < 0) {
                    isTag = false;
                    end = str.indexOf('.', begin);
                }
            } else {
                end = str.indexOf('.', begin);
                if (end < 0)
                    end = str.indexOf('&', begin);
            }
            if (end < 0)
                end = length;
        }
        return true;
    }

    public static final int NOTLOCATED = -1;

    protected String pathSegments = null;
    protected String disc = null;

    protected int primaryAudioNum = -1;
    protected int secondaryAudioNum = -1;
    protected int primaryVideoNum = -1;
    protected int secondaryVideoNum = -1;
    protected int textStreamNum = -1;
    protected int jar = -1;
    protected int mark = -1;
    protected int playItem = -1;
    protected int playList = -1;
    protected int sound = -1;
    protected int titleNum = -1;

    private static final Logger logger = Logger.getLogger(BDLocator.class.getName());
}
