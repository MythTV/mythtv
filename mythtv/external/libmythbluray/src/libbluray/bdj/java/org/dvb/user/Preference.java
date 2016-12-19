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

import java.util.Iterator;
import java.util.Vector;

public class Preference {
    protected Preference() {
    }

    public Preference(String name, String value) {
        this.name = name;
        if (value != null)
            values.add(value);
    }

    public Preference(String name, String value[]) {
        this.name = name;
        for (int i = 0; i < value.length; i++)
            values.add(value[i]);
    }

    public void add(String value) {
        synchronized (values) {
            values.remove(value);
            values.add(value);
        }
    }

    public void add(String value[]) {
        for (int i = 0; i < value.length; i++)
            add(value[i]);
    }

    public void add(int position, String value) {
        synchronized (values) {
            values.remove(value);
            if (position < 0)
                position = 0;
            else if (position > values.size())
                position = values.size();
            values.add(position, value);
        }
    }

    public String[] getFavourites() {
        synchronized (values) {
            if (values.isEmpty())
                return new String[0];
            String[] result = new String[values.size()];
            values.copyInto(result);
            return result;
        }
    }

    public String getMostFavourite() {
        synchronized (values) {
            if (values.isEmpty())
                return null;
            return (String)values.get(0);
        }
    }

    public String getName() {
        return name;
    }

    public int getPosition(String value) {
        synchronized (values) {
            return values.indexOf(value);
        }
    }

    public boolean hasValue() {
        synchronized (values) {
            return !values.isEmpty();
        }
    }

    public void remove(String value) {
        synchronized (values) {
            values.remove(value);
        }
    }

    public void removeAll() {
        synchronized (values) {
            values.clear();
        }
    }

    public void setMostFavourite(String value) {
        add(0, value);
    }

    public String toString() {
        String result = "Preference:" + name + "[";
        String comma = "";
        Iterator it = values.iterator();
        while (it.hasNext()) {
            result += comma + (String)it.next();
            comma = ",";
        }
        result += ']';
        return result;
    }

    private String name;
    private Vector values = new Vector();
}
