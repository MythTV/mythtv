package org.videolan.bdjo;

public class GraphicsResolution {
    public static final GraphicsResolution HD_1920_1080 = new GraphicsResolution(1, 1920, 1080);
    public static final GraphicsResolution HD_1280_720 = new GraphicsResolution(2, 1280, 720);
    public static final GraphicsResolution SD = new GraphicsResolution(3, 720, 480); // FIXME: probably should take into consideration PAL
    public static final GraphicsResolution SD_50HZ_720_576 = new GraphicsResolution(4, 720, 576);
    public static final GraphicsResolution SD_60HZ_720_480 = new GraphicsResolution(5, 720, 480);
    public static final GraphicsResolution QHD_960_540 = new GraphicsResolution(6, 960, 540);

    GraphicsResolution(int id, int width, int height)
{
        this.id = id;
        this.width = width;
        this.height = height;
    }

    public int getId() {
        return id;
    }

    public int getWidth() {
        return width;
    }

    public int getHeight() {
        return height;
    }

    public static GraphicsResolution fromId(int id) {
        switch (id) {
        case 1:
            return HD_1920_1080;
        case 2:
            return HD_1280_720;
        case 3:
            return SD;
        case 4:
            return SD_50HZ_720_576;
        case 5:
            return SD_60HZ_720_480;
        case 6:
            return QHD_960_540;
        }
        return null;
    }

    private int id;
    private int width;
    private int height;
}
