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

package org.dvb.event;

import java.util.Iterator;
import java.util.LinkedList;
import java.awt.event.KeyEvent;

import org.havi.ui.event.HRcEvent;

public class UserEventRepository extends RepositoryDescriptor {

    public UserEventRepository(String name)
    {
        super(null, name);
    }

    public void addUserEvent(UserEvent event)
    {
        events.add(event);
    }

    public UserEvent[] getUserEvent()
    {
        int size = events.size();
        UserEvent[] userEvents = new UserEvent[size];
        for (int i = 0; i < size; i++)
            userEvents[i] = events.get(i);
        return userEvents;
    }

    public void removeUserEvent(UserEvent event)
    {
        events.remove(event);
    }

    public void addKey(int keycode)
    {
        events.add(new UserEvent(this, UserEvent.UEF_KEY_EVENT,
                                 KeyEvent.KEY_PRESSED, keycode, 0, 0));
        events.add(new UserEvent(this, UserEvent.UEF_KEY_EVENT,
                                 KeyEvent.KEY_RELEASED, keycode, 0, 0));
    }

    public void removeKey(int keycode)
    {
        for(Iterator<UserEvent> it = events.iterator(); it.hasNext() == true; ) {
            UserEvent event = it.next();

            if (event.getCode() == keycode)
                it.remove();
        }
    }

    public void addAllNumericKeys()
    {
        addKey(HRcEvent.VK_0);
        addKey(HRcEvent.VK_1);
        addKey(HRcEvent.VK_2);
        addKey(HRcEvent.VK_3);
        addKey(HRcEvent.VK_4);
        addKey(HRcEvent.VK_5);
        addKey(HRcEvent.VK_6);
        addKey(HRcEvent.VK_7);
        addKey(HRcEvent.VK_8);
        addKey(HRcEvent.VK_9);
    }

    public void addAllColourKeys()
    {
        addKey(HRcEvent.VK_COLORED_KEY_0);
        addKey(HRcEvent.VK_COLORED_KEY_1);
        addKey(HRcEvent.VK_COLORED_KEY_2);
        addKey(HRcEvent.VK_COLORED_KEY_3);
    }

    public void addAllArrowKeys()
    {
        addKey(HRcEvent.VK_LEFT);
        addKey(HRcEvent.VK_RIGHT);
        addKey(HRcEvent.VK_UP);
        addKey(HRcEvent.VK_DOWN);
    }

    public void removeAllNumericKeys()
    {
        removeKey(HRcEvent.VK_0);
        removeKey(HRcEvent.VK_1);
        removeKey(HRcEvent.VK_2);
        removeKey(HRcEvent.VK_3);
        removeKey(HRcEvent.VK_4);
        removeKey(HRcEvent.VK_5);
        removeKey(HRcEvent.VK_6);
        removeKey(HRcEvent.VK_7);
        removeKey(HRcEvent.VK_8);
        removeKey(HRcEvent.VK_9);
    }

    public void removeAllColourKeys()
    {
        removeKey(HRcEvent.VK_COLORED_KEY_0);
        removeKey(HRcEvent.VK_COLORED_KEY_1);
        removeKey(HRcEvent.VK_COLORED_KEY_2);
        removeKey(HRcEvent.VK_COLORED_KEY_3);
    }

    public void removeAllArrowKeys()
    {
        removeKey(HRcEvent.VK_LEFT);
        removeKey(HRcEvent.VK_RIGHT);
        removeKey(HRcEvent.VK_UP);
        removeKey(HRcEvent.VK_DOWN);
    }

    private LinkedList<UserEvent> events = new LinkedList<UserEvent>();
}
