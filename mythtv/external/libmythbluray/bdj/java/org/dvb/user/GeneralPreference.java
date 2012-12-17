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

package org.dvb.user;

public final class GeneralPreference extends Preference {
    public GeneralPreference(String name) throws IllegalArgumentException {
        super(name, (String)null);
        if (!isGeneralPreference(name))
            throw new IllegalArgumentException();
    }

    protected static boolean isGeneralPreference(String name) {
        for (int i = 0; i < GENERAL_PREFERENCES.length; i++)
            if (name.equals(GENERAL_PREFERENCES[i]))
                return true;
        return false;
    }

    private static final String[] GENERAL_PREFERENCES = { "User Language", "Parental Rating", "User Name", "User Address", "User @", "Country Code", "Default Font Size" };
}
