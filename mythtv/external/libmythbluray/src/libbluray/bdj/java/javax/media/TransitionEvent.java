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

public class TransitionEvent extends ControllerEvent {
    public TransitionEvent(Controller from, int previous, int current,
            int target) {
        super(from);
        this.previous = previous;
        this.current = current;
        this.target = target;
    }

    public int getPreviousState() {
        return previous;
    }

    public int getCurrentState() {
        return current;
    }

    public int getTargetState() {
        return target;
    }

    public String toString() {
        return getClass().toString() + "[source=" + source + ",previous=" + previous + ",current=" + current + ",target=" + target + "]";
    }

    private final int previous;
    private final int current;
    private final int target;

    private static final long serialVersionUID = 6818216020447063912L;
}
