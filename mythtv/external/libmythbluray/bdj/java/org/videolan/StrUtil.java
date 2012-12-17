package org.videolan;

import java.util.ArrayList;

public class StrUtil {
    public static String[] split(String str, char delimiter) {
        ArrayList elements = new ArrayList();
        int i, j = 0, len = str.length();
        for (i = 0; i < len; i++)
            if (str.charAt(i) == delimiter) {
                elements.add(str.substring(j, i));
                j = i + 1;
            }
        if (j < i)
            elements.add(str.substring(j, i));
        return (String[])elements.toArray(new String[elements.size()]);
    }

    public static String[] split(String str, char delimiter, int num) {
        ArrayList elements = new ArrayList();
        int i, j = 0, n = 0, len = str.length();
        for (i = 0; (i < len) && (n < num); i++)
            if (str.charAt(i) == delimiter) {
                elements.add(str.substring(j, i));
                j = i + 1;
                n++;
            }
        if ((n < num) && (j < i)) {
            elements.add(str.substring(j, i));
            n++;
        }
        return (String[])elements.toArray(new String[elements.size()]);
    }
}
