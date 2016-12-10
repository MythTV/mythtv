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

public class SelectionFailedEvent extends ServiceContextEvent {
    public SelectionFailedEvent(ServiceContext context, int reason)
    {
        super(context);

        this.reason = reason;
    }

    public int getReason()
    {
        return reason;
    }

    public static final int INTERRUPTED = 1;
    public static final int CA_REFUSAL = 2;
    public static final int CONTENT_NOT_FOUND = 3;
    public static final int MISSING_HANDLER = 4;
    public static final int TUNING_FAILURE = 5;
    public static final int INSUFFICIENT_RESOURCES = 6;
    public static final int OTHER = 255;

    private int reason;
    private static final long serialVersionUID = 3991728273494160910L;
}
