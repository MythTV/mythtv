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

package org.dvb.ui;

public final class DVBAlphaComposite {

    private DVBAlphaComposite(int rule)
    {
        this.rule = rule;
        this.alpha = 1.0f;
    }

    private DVBAlphaComposite(int rule, float alpha)
    {
        this.rule = rule;
        this.alpha = alpha;
    }

    public int getRule()
    {
        return rule;
    }

    public float getAlpha()
    {
        return alpha;
    }

    public static DVBAlphaComposite getInstance(int rule)
    {
        switch (rule) {
        case CLEAR:
            return Clear;
        case SRC:
            return Src;
        case SRC_OVER:
            return SrcOver;
        case DST_OVER:
            return DstOver;
        case SRC_IN:
            return SrcIn;
        case DST_IN:
            return DstIn;
        case SRC_OUT:
            return SrcOut;
        case DST_OUT:
            return DstOut;
        default:
            throw new IllegalArgumentException("Unknown rule");
        }
    }

    public static DVBAlphaComposite getInstance(int rule, float alpha)
    {
        return new DVBAlphaComposite(rule, alpha);
    }

    public int hashCode()
    {
        final int prime = 31;
        int result = 1;
        result = prime * result + Float.floatToIntBits(alpha);
        result = prime * result + rule;
        return result;
    }

    public boolean equals(Object obj)
    {
        if (this == obj)
            return true;
        if (obj == null)
            return false;
        if (getClass() != obj.getClass())
            return false;
        DVBAlphaComposite other = (DVBAlphaComposite) obj;
        if (Float.floatToIntBits(alpha) != Float.floatToIntBits(other.alpha))
            return false;
        if (rule != other.rule)
            return false;
        return true;
    }

    public static final int CLEAR = 1;
    public static final int SRC = 2;
    public static final int SRC_OVER = 3;
    public static final int DST_OVER = 4;
    public static final int SRC_IN = 5;
    public static final int DST_IN = 6;
    public static final int SRC_OUT = 7;
    public static final int DST_OUT = 8;

    public static final DVBAlphaComposite Clear = new DVBAlphaComposite(CLEAR);
    public static final DVBAlphaComposite Src = new DVBAlphaComposite(SRC);
    public static final DVBAlphaComposite SrcOver = new DVBAlphaComposite(
            SRC_OVER);
    public static final DVBAlphaComposite DstOver = new DVBAlphaComposite(
            DST_OVER);
    public static final DVBAlphaComposite SrcIn = new DVBAlphaComposite(SRC_IN);
    public static final DVBAlphaComposite DstIn = new DVBAlphaComposite(DST_IN);
    public static final DVBAlphaComposite SrcOut = new DVBAlphaComposite(
            SRC_OUT);
    public static final DVBAlphaComposite DstOut = new DVBAlphaComposite(
            DST_OUT);

    float alpha;
    int rule;
}
