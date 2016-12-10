/*
 * This file is part of libbluray
 * Copyright (C) 2010  VideoLAN
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

package org.videolan.bdjo;

import java.util.BitSet;

import org.davic.net.Locator;
import org.bluray.application.AppAttributes;
import org.bluray.net.BDLocator;
import org.dvb.application.AppID;
import org.dvb.application.AppIcon;
import org.dvb.application.IllegalProfileParameterException;
import org.dvb.application.LanguageNotAvailableException;
import org.dvb.user.GeneralPreference;
import org.dvb.user.UserPreferenceManager;
import org.videolan.StrUtil;

public class AppEntry implements AppAttributes {
    public AppIcon getAppIcon() {
        return icon;
    }

    public AppID getIdentifier() {
        return appid;
    }

    public boolean getIsServiceBound() {
        return ((binding & TITLE_BOUND) != 0);
    }

    public String getName() {
        UserPreferenceManager pm = UserPreferenceManager.getInstance();
        GeneralPreference p = new GeneralPreference("User Language");
        pm.read(p);
        String lang = p.getMostFavourite();
        if (lang != null)
            try {
                return getName(lang);
            } catch (LanguageNotAvailableException e) {

            }
        if (names == null || names.length < 1) {
            return null;
        }

        return names[0][1];
    }

    public String getName(String language) throws LanguageNotAvailableException {
        for (int i = 0; i < names.length; i++)
            if (language.equals(names[i][0]))
                return names[i][1];
        throw new LanguageNotAvailableException();
    }

    public String[][] getNames() {
        return names;
    }

    public int getPriority() {
        return priority;
    }

    public String[] getProfiles() {
        String[] ret = new String[0];
        for (int i = 0; i < profiles.length; i++) {
            if (profiles[i].getProfile() == 1)
                ret = new String[] { "mhp.profile.enhanced_broadcast" };
            if (profiles[i].getProfile() == 2)
                ret = new String[] { "mhp.profile.enhanced_broadcast", "mhp.profile.interactive_broadcast" };
        }
        return ret;
    }

    public Object getProperty(String name) {
        if (name.equals("dvb.j.location.base"))
            return basePath;
        if (name.equals("dvb.j.location.cpath.extension"))
            return StrUtil.split(classpathExt, ';');
        if (name.equals("dvb.transport.oc.component.tag"))
            return null;
        return null;
    }

    public Locator getServiceLocator() {
        org.videolan.Logger.unimplemented("AppEntry","getServiceLocator");
        /*
        try {
            String discID = BDJTitleInfoHelper.getCurrent32LengthDiscIDFromC();
            int titleNumber = BDJTitleInfoHelper.getCurrentTitleNumberFromC();
            return new BDLocator(discID, titleNumber, 0);
        }
        catch (InvalidLocatorException e) {
            BDJTrace.printStack(e.getStackTrace());
            BDJAssert.doAssert(false, "can not get the service locator");
        }
        */
        return null;
    }

    public int getType() {
        return type;
    }

    public int[] getVersions(String profile) throws IllegalProfileParameterException {
        if (profile.equals("mhp.profile.enhanced_broadcast")) {
            for (int i = 0; i < profiles.length; i++)
                if (profiles[i].getProfile() == 2)
                    return new int[] { profiles[i].getMajor(), profiles[i].getMinor(), profiles[i].getMicro() };
            return null;
        }
        if (profile.equals("mhp.profile.interactive_broadcast")) {
            for (int i = 0; i < profiles.length; i++)
                if (profiles[i].getProfile() == 1)
                    return new int[] { profiles[i].getMajor(), profiles[i].getMinor(), profiles[i].getMicro() };
            return null;
        }
        throw new IllegalProfileParameterException();
    }

    public boolean isStartable() {
        return ((controlCode == AUTOSTART) || (controlCode == PRESENT));
    }

    public boolean isVisible() {
        // Annex S
        return false;
    }

    public boolean isDiscBound() {
        return ((binding & DISC_BOUND) != 0);
    }

    public AppEntry(int controlCode, int type, int orgId,
                    short appId, AppProfile[] profiles, short priority,
                    int binding, int visibility, String[][] names,
                    String iconLocator, short iconFlags, String basePath,
                    String classpathExt, String initialClass, String[] params) {
        this.controlCode = controlCode;
        this.type = type;
        this.appid = new AppID(orgId, appId);

        this.profiles = profiles;
        this.priority = priority;
        this.binding = binding;
        this.visibility = visibility;
        this.names = names;
        if ((iconLocator != null) && (iconLocator.length() > 0)) {
            try {
                BDLocator locator = new BDLocator("bd://JAR:" + basePath + "/" + iconLocator);
                BitSet flags = new BitSet(16);
                for (int i = 0; i < 9; i++)
                    if ((iconFlags & (1 << i)) != 0)
                        flags.set(i, true);
                this.icon = new AppIcon(locator, flags);
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        this.basePath = basePath;
        this.classpathExt = classpathExt;
        this.initialClass = initialClass;
        this.params = params;

        if ((binding & (DISC_BOUND | TITLE_BOUND)) == 0) {
            System.err.println("Disc unbound application: " + initialClass);
        }
    }

    public int getControlCode() {
        return controlCode;
    }

    public String getBasePath() {
        return basePath;
    }

    public String getClassPathExt() {
        return classpathExt;
    }

    public String getInitialClass() {
        return initialClass;
    }

    public String[] getParams() {
        return params;
    }

    public static final int AUTOSTART = 1;
    public static final int PRESENT = 2;
    protected static final int DISC_BOUND = 1;
    protected static final int TITLE_BOUND = 2;
    protected static final int VISIBLE_TO_APPS = 1;
    protected static final int VISIBLE_TO_USERS = 2;

    private int controlCode;
    private int type;
    private AppID appid;
    private AppProfile[] profiles;
    private short priority;
    private int binding;
    private int visibility;
    private String[][] names;
    private AppIcon icon;
    private String basePath;
    private String classpathExt;
    private String initialClass;
    private String[] params;
}
