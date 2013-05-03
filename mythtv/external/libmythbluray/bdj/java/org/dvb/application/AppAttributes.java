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

package org.dvb.application;

import org.davic.net.Locator;

public abstract interface AppAttributes {


    public abstract AppIcon getAppIcon();

    public abstract AppID getIdentifier();

    public abstract boolean getIsServiceBound();

    public abstract String getName();

    public abstract String getName(String language)
      throws LanguageNotAvailableException;

    public abstract String[][] getNames();

    public abstract int getPriority();

    public abstract String[] getProfiles();

    public abstract Object getProperty(String name);

    public abstract Locator getServiceLocator();

    public abstract int getType();

    public abstract int[] getVersions(String profile)
      throws IllegalProfileParameterException;

    public abstract boolean isStartable();

    public abstract boolean isVisible();
    
    public static final int DVB_HTML_application = 2;
    public static final int DVB_J_application = 1;
}
