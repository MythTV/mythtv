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

package javax.media;

public interface Controller extends Clock, Duration {
    public int getState();

    public int getTargetState();

    public void realize();

    public void prefetch();

    public void deallocate();

    public void close();

    public Time getStartLatency();

    public Control[] getControls();

    public Control getControl(String forName);

    public void addControllerListener(ControllerListener listener);

    public void removeControllerListener(ControllerListener listener);

    public static final Time LATENCY_UNKNOWN = null;

    public static final int Unrealized = 100;
    public static final int Realizing = 200;
    public static final int Realized = 300;
    public static final int Prefetching = 400;
    public static final int Prefetched = 500;
    public static final int Started = 600;
}
