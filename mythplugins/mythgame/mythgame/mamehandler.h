#ifndef MAMEHANDLER_H_
#define MAMEHANDLER_H_

#include "gamehandler.h"
#include "mamerominfo.h"
#include <qapplication.h>

#define MAX_ROMNAME 10
#define MAX_GAMENAME 100
#define MAX_MANU 50
#define MAX_YEAR 5
#define MAX_CPU 20
#define MAX_CONTROL 20
#define MAX_CATEGORY 40
#define MAX_MAMEVER 20

struct Prefs {
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

        /*int sort_by;
        int current_row;
        int current_tab;
        int pane_position;
        int notebook_page;
        int x_size;
        int y_size;
        QString column_size_string;

        bool save_win_on_quit;

        bool playback_no_highscore;
        bool iconify_while_xmame;
        bool loadsa_image_formats;
        bool look_for_clone_images;
        bool indicate_notebook_changes;

        GameFilter ListFilter;
        GList *active_columns;*/

        QString rom_url;
        QString screenshot_url;
        QString flyer_url;
        QString cabinet_url;
};

typedef struct _GameSettings {
        bool default_options;

        bool fullscreen;
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
        char *extra_options;
}GameSettings;


class MameHandler : public GameHandler
{
  public:
    MameHandler() {
                    systemname = "Mame";
                    SetGeneralPrefs();
                  }
    virtual ~MameHandler();

    void error(const QString &e);
    void start_game(RomInfo * romdata);
    QString Systemname() { return systemname; }
    void processGames();

    static MameHandler* getHandler();

  protected:
    bool check_xmame_exe();
    void makecmd_line(const char * game, QString* exec, MameRomInfo * romentry);
    void SetGeneralPrefs();
    void SetGameSettings(GameSettings &game_settings, MameRomInfo *rominfo);
    struct Prefs general_prefs;

    bool xmame_version_ok;
    int supported_games;

  private:
    static MameHandler* pInstance;
};

#endif
