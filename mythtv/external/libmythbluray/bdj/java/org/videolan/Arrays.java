package org.videolan;

public class Arrays {
    public static int hashCode(Object[] array) {
        if (array == null)
            return 0;
        int hash = 1;
        for (int i = 0; i < array.length; i++)
            hash = 31 * hash + (array[i] == null ? 0 : array[i].hashCode());
        return hash;
    }

    public static String toString(int[] array) {
        if (array == null)
            return "null";
        StringBuffer buffer = new StringBuffer();
        buffer.append('[');
        int length = array.length;
        for (int j = 0; j < length; j++) {
                buffer.append(array[j]);
            if (j < (length - 1))
                buffer.append(", ");
        }
        buffer.append(']');
        return buffer.toString();
    }

    public static String toString(Object[] array) {
        if (array == null)
            return "null";
        StringBuffer buffer = new StringBuffer();
        buffer.append('[');
        int length = array.length;
        for (int j = 0; j < length; j++) {
                buffer.append(String.valueOf(array[j]));
            if (j < (length - 1))
                buffer.append(", ");
        }
        buffer.append(']');
        return buffer.toString();
    }
}
