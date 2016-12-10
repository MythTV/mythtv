/*
 * This file is part of libbluray
 * Copyright (C) 2014  Libbluray
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

package org.blurayx.s3d.ui;

public class S3DProperty {

    public static final S3DProperty ONE_PLANE = new S3DProperty("ONE_PLANE");
    public static final S3DProperty TWO_PLANES = new S3DProperty("TWO_PLANES");
    public static final S3DProperty TWOD_OUTPUT = new S3DProperty("TWOD_OUTPUT");

    private String name;

    protected S3DProperty(String name) {
        this.name = name;
    }
}
