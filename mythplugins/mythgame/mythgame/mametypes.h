#ifndef MAMETYPES_H_
#define MAMETYPES_H_

#include <qstring.h>

#define MAX_ROMNAME 10
#define MAX_GAMENAME 100
#define MAX_MANU 50
#define MAX_YEAR 5
#define MAX_CPU 20
#define MAX_CONTROL 20
#define MAX_CATEGORY 40
#define MAX_MAMEVER 20

class Prefs
{
  public:
    QString xmame_exe;
    QString rom_dir;
    QString screenshot_dir;
    QString highscore_dir;
    QString flyer_dir;
    QString cabinet_dir;
    QString game_history_file;
    QString cheat_file;

    QString xmame_display_target;
    int xmame_major;
    QString xmame_minor;
    int xmame_release;
    int romdir_time;

    int show_disclaimer;
    int show_gameinfo;

    QString rom_url;
    QString screenshot_url;
    QString flyer_url;
    QString cabinet_url;

    int automatically_download_images;
    QString image_downloader;
};

class GameSettings
{
  public:
    bool default_options;

    int fullscreen;
    bool scanlines;
    bool extra_artwork;
    bool autoframeskip;
    bool auto_colordepth;
    bool rot_left;
    bool rot_right;
    bool flipx;
    bool flipy;
    int scale;

    bool antialias;
    bool translucency;
    float beam;
    float flicker;
    int vectorres;
    bool analog_joy;
    bool mouse;
    bool winkeys;
    bool grab_mouse;
    int joytype;
    bool sound;
    bool samples;
    bool fake_sound;
    int volume;

    bool cheat;
    QString extra_options;
};

#endif
