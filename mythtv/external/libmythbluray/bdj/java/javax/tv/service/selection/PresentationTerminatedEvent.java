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

package javax.tv.service.selection;

public class PresentationTerminatedEvent extends ServiceContextEvent {
    public PresentationTerminatedEvent(ServiceContext context, int reason)
    {
        super(context);

        this.reason = reason;
    }

    public int getReason()
    {
        return reason;
    }

    public static final int SERVICE_VANISHED = 1;
    public static final int TUNED_AWAY = 2;
    public static final int RESOURCES_REMOVED = 3;
    public static final int ACCESS_WITHDRAWN = 4;
    public static final int USER_STOP = 5;
    public static final int OTHER = 255;

    private int reason;
    private static final long serialVersionUID = 4787886890628229164L;
}
