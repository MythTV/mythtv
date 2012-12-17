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

public class ServiceXFRErrorEvent extends AsynchronousLoadingEvent {
    public ServiceXFRErrorEvent(DSMCCObject source, ServiceXFRReference ref)
    {
        super(source);

        this.source = source;
        this.ref = ref;
    }

    public java.lang.Object getSource()
    {
        return source;
    }

    public ServiceXFRReference getServiceXFR()
    {
        return ref;
    }

    private DSMCCObject source;
    private ServiceXFRReference ref;
    private static final long serialVersionUID = 1404372890571397741L;
}
