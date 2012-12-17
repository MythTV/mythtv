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

package org.dvb.media;

public interface VideoFormatControl extends javax.media.Control {
    public int getAspectRatio();

    public int getActiveFormatDefinition();

    public int getDecoderFormatConversion();

    public VideoTransformation getVideoTransformation(int dfc);

    public int getDisplayAspectRatio();

    public boolean isPlatform();

    public void addVideoFormatListener(VideoFormatListener listener);

    public void removeVideoFormatListener(VideoFormatListener listener);

    public static final int ASPECT_RATIO_UNKNOWN = -1;
    public static final int ASPECT_RATIO_4_3 = 2;
    public static final int ASPECT_RATIO_16_9 = 3;
    public static final int ASPECT_RATIO_2_21_1 = 4;

    public static final int AFD_NOT_PRESENT = -1;
    public static final int AFD_16_9_TOP = 2;
    public static final int AFD_14_9_TOP = 3;
    public static final int AFD_GT_16_9 = 4;
    public static final int AFD_SAME = 8;
    public static final int AFD_4_3 = 9;
    public static final int AFD_16_9 = 10;
    public static final int AFD_14_9 = 11;
    public static final int AFD_4_3_SP_14_9 = 13;
    public static final int AFD_16_9_SP_14_9 = 14;
    public static final int AFD_16_9_SP_4_3 = 15;

    public static final int DFC_PROCESSING_UNKNOWN = -1;
    public static final int DFC_PROCESSING_NONE = 0;
    public static final int DFC_PROCESSING_FULL = 1;
    public static final int DFC_PROCESSING_LB_16_9 = 2;
    public static final int DFC_PROCESSING_LB_14_9 = 3;
    public static final int DFC_PROCESSING_CCO = 4;
    public static final int DFC_PROCESSING_PAN_SCAN = 5;
    public static final int DFC_PROCESSING_LB_2_21_1_ON_4_3 = 6;
    public static final int DFC_PROCESSING_LB_2_21_1_ON_16_9 = 7;
    public static final int DFC_PLATFORM = 8;
    public static final int DFC_PROCESSING_16_9_ZOOM = 9;
    public static final int DAR_4_3 = 1;
    public static final int DAR_16_9 = 2;
}
