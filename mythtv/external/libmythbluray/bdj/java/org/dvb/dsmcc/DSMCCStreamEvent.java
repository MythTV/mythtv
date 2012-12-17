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

import java.io.IOException;

public class DSMCCStreamEvent extends DSMCCStream {

    public DSMCCStreamEvent(DSMCCObject aDSMCCObject)
            throws NotLoadedException, IllegalObjectTypeException
    {
        super(aDSMCCObject);
        throw new Error("Not implemented");
    }

    public DSMCCStreamEvent(String path) throws IOException,
            IllegalObjectTypeException
    {
        super(path);
        throw new Error("Not implemented");
    }

    public DSMCCStreamEvent(String path, String name) throws IOException,
            IllegalObjectTypeException
    {
        super(path, name);
        throw new Error("Not implemented");
    }

    public synchronized int subscribe(String eventName, StreamEventListener l)
            throws UnknownEventException, InsufficientResourcesException
    {
        throw new Error("Not implemented");
    }

    public synchronized void unsubscribe(int eventId, StreamEventListener l)
            throws UnknownEventException
    {
        throw new Error("Not implemented");
    }

    public synchronized void unsubscribe(String eventName, StreamEventListener l)
            throws UnknownEventException
    {
        throw new Error("Not implemented");
    }

    public String[] getEventList()
    {
        throw new Error("Not implemented");
    }

}
