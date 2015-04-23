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

package org.dvb.dsmcc;

public class StreamEvent extends java.util.EventObject {
    public StreamEvent(DSMCCStreamEvent source, long npt, String name,
            int eventId, byte[] eventData)
    {
        super(source);
        
        this.source = source;
        this.npt = npt;
        this.name = name;
        this.eventId = eventId;
        this.eventData = eventData;
    }

    public Object getSource()
    {
        return source;
    }

    public String getEventName()
    {
        return name;
    }

    public int getEventId()
    {
        return eventId;
    }

    public long getEventNPT()
    {
        return npt;
    }

    public byte[] getEventData()
    {
        return eventData;
    }

    private DSMCCStreamEvent source;
    private long npt;
    private String name;
    private int eventId; 
    private byte[] eventData;
    private static final long serialVersionUID = -8054274744134638856L;
}
