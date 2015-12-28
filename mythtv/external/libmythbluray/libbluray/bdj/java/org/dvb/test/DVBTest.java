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

package org.dvb.test;

import java.io.IOException;

public class DVBTest {
    private DVBTest() {
    }

    public static void log(String id, String message) throws IOException {
        System.out.println("log: " + id + ": " + message);
    }

    public static void log(String id, int no) throws IOException {
        System.out.println("log: " + id + ": " + no);
    }

    public static void terminate(String id, int terminationCondition) throws IOException {
        System.out.print("terminate: " + id + ": ");
        switch (terminationCondition) {
        case PASS:
            System.out.println("PASS");
            break;
        case FAIL:
            System.out.println("FAIL");
            break;
        default:
            System.out.println("UNKNOWN " + terminationCondition);
            break;
        }
    }

    public static void prompt(String id, int controlCode, String message) throws IOException {
        System.out.println("prompt: " + id + ": " + controlCode + ": " + message);
    }

    public final static int PASS = 0x00;
    public final static int FAIL = -0x01;

    public final static int OPTION_UNSUPPORTED = -0x02;
    public final static int HUMAN_INTERVENTION = -0x03;
    public final static int UNRESOLVED = -0x04;
    public final static int UNTESTED = -0x05;
}
