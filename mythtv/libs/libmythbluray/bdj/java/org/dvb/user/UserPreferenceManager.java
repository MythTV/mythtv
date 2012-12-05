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

import java.io.IOException;
import java.util.Iterator;
import java.util.LinkedList;

public class UserPreferenceManager {
    private UserPreferenceManager() {
    }

    public static UserPreferenceManager getInstance() {
        synchronized (UserPreferenceManager.class) {
            if (instance == null)
                instance = new UserPreferenceManager();
        }
        return instance;
    }

    public void read(Preference p) {
        SecurityManager sm = System.getSecurityManager();
        sm.checkPermission(new UserPreferencePermission("read"));
        p.removeAll();
        String name = p.getName();
        Iterator it = preferences.iterator();
        while (it.hasNext()) {
            Preference preference = (Preference)it.next();
            if (name.equals(preference.getName())) {
                p.add(preference.getFavourites());
                break;
            }
        }
    }

    public void read(Preference p, Facility facility) {
        SecurityManager sm = System.getSecurityManager();
        sm.checkPermission(new UserPreferencePermission("read"));
        p.removeAll();
        String name = p.getName();
        if (name.equals(facility.getPreference())) {
            Iterator it = preferences.iterator();
            while (it.hasNext()) {
                Preference preference = (Preference)it.next();
                if (name.equals(preference.getName())) {
                    String[] values = preference.getFavourites();
                    String[] valuesFacility = facility.getValues();
                    for (int i = 0; i < values.length; i++)
                        for (int j = 0; j < valuesFacility.length; j++)
                            if (values[i].equals(valuesFacility[j])) {
                                p.add(values[i]);
                                break;
                            }
                    break;
                }
            }
        }
    }

    public void write(Preference p) throws UnsupportedPreferenceException, IOException {
        String name = p.getName();
        if (!GeneralPreference.isGeneralPreference(name))
            throw new UnsupportedPreferenceException();
        SecurityManager sm = System.getSecurityManager();
        sm.checkPermission(new UserPreferencePermission("write"));
        Iterator it = preferences.iterator();
        while (it.hasNext()) {
            Preference preference = (Preference)it.next();
            if (name.equals(preference.getName())) {
                it.remove();
                break;
            }
        }
        preferences.add(p);
        synchronized (listeners) {
            int size = listeners.size();
            if (size > 0) {
                UserPreferenceChangeEvent event = new UserPreferenceChangeEvent(name);
                for (int i  = 0; i < size; i++)
                    ((UserPreferenceChangeListener)listeners.get(i)).receiveUserPreferenceChangeEvent(event);
            }
        }
    }

    public void addUserPreferenceChangeListener(UserPreferenceChangeListener l) {
        synchronized (listeners) {
            listeners.add(l);
        }
    }

    public void removeUserPreferenceChangeListener(UserPreferenceChangeListener l) {
        synchronized (listeners) {
            listeners.remove(l);
        }
    }

    private static UserPreferenceManager instance;
    private LinkedList preferences = new LinkedList();
    private LinkedList listeners = new LinkedList();
}
