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
import java.util.LinkedList;

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
        values.remove(value);
        values.add(value);
    }

    public void add(String value[]) {
        for (int i = 0; i < value.length; i++)
            add(value[i]);
    }

    public void add(int position, String value) {
        values.remove(value);
        if (position < 0)
            position = 0;
        else if (position > values.size())
            position = values.size();
        values.add(position, value);
    }

    public String[] getFavourites() {
        if (values.isEmpty())
            return new String[0];
        return (String[])values.toArray();
    }

    public String getMostFavourite() {
        if (values.isEmpty())
            return null;
        return (String)values.get(0);
    }

    public String getName() {
        return name;
    }

    public int getPosition(String value) {
        return values.indexOf(value);
    }

    public boolean hasValue() {
        return !values.isEmpty();
    }

    public void remove(String value) {
        values.remove(value);
    }

    public void removeAll() {
        values.clear();
    }

    public void setMostFavourite(String value) {
        values.remove(value);
        values.addFirst(value);
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
    private LinkedList values = new LinkedList();
}
