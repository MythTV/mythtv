#ifndef DATABASEBOX_H_
#define DATABASEBOX_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qstringlist.h>
#include <qthread.h>
#include <qtimer.h>
#include <qptrlist.h>

#include "metadata.h"
#include "playlist.h"
#include <mythtv/mythwidgets.h>
#include <mythtv/lcddevice.h>

class QSqlDatabase;
class QListViewItem;
class TreeCheckItem;
class QLabel;
class DatabaseBox;

class ReadCDThread : public QThread
{
  public:

    ReadCDThread(PlaylistsContainer *all_the_playlist, AllMusic *all_the_music);
    virtual void run();
    bool    statusChanged(){return cd_status_changed;}

  private:

    AllMusic*           all_music;
    PlaylistsContainer* the_playlists;
    bool                cd_status_changed;
};

class DatabaseBox : public MythDialog
{
    Q_OBJECT
  public:
    DatabaseBox(PlaylistsContainer *all_playlists,
                AllMusic *music_ptr,
                MythMainWindow *parent, const char *name = 0);
   ~DatabaseBox();

    void dealWithTracks(PlaylistItem *item_ptr);
    void setCDTitle(const QString& title);
    void fillCD(void);
    
  protected slots:
    void selected(QListViewItem *);
    void doMenus(QListViewItem *);
    void alternateDoMenus(QListViewItem *, int);
    void keyPressEvent(QKeyEvent *e);
    void moveHeldUpDown(bool flag);
    void deleteTrack(QListViewItem *item);
    void copyNewPlaylist();
    void copyToActive();
    void deletePlaylist();
    void renamePlaylist();
    void popBackPlaylist();
    void clearActive();
    void closeActivePopup();
    void closePlaylistPopup();
    void occasionallyCheckCD();
    void keepFilling();
    void showWaiting();
    void updateLCDMenu(QKeyEvent *e);

    void ErrorPopup(const QString &msg);
    void closeErrorPopup();

    void CreateCDAudio();
    void CreateCDMP3();
    void BlankCDRW();

  protected:
    bool eventFilter(QObject *o, QEvent *e);
 
  private:
    void doSelected(QListViewItem *, bool cd_flag);
    void doPlaylistPopup(TreeCheckItem *item_ptr);
    void doActivePopup(PlaylistTitle *item_ptr);
    void checkParent(QListViewItem *);
    LCDMenuItem *buildLCDMenuItem(TreeCheckItem *item_ptr, bool curMenuItem);
    LCDMenuItem *buildLCDMenuItem(QListViewItem *item_ptr, bool curMenuItem);
    void buildMenuTree(QPtrList<LCDMenuItem> *menuItems,
                       TreeCheckItem *item_ptr, int level);
    void buildMenuTree(QPtrList<LCDMenuItem> *menuItems,
                       QListViewItem *item_ptr, int level);

    QString indentMenuItem(QString itemLevel);
 
    void checkTree();
    QPixmap getPixmap(QString &level);

    CDCheckItem         *cditem;
    MythListView        *listview;
    PlaylistsContainer  *the_playlists;
    
    AllMusic            *all_music;
    bool                holding_track;
    PlaylistTrack       *track_held;
    TreeCheckItem       *allmusic;
    TreeCheckItem       *alllists;
    PlaylistTitle       *allcurrent;
    Playlist            *active_playlist;
    
    MythPopupBox        *active_popup;
    MythRemoteLineEdit  *active_pl_edit;

    MythPopupBox        *playlist_popup;
    MythRemoteLineEdit  *playlist_rename;

    MythPopupBox        *error_popup;

    ReadCDThread        *cd_reader_thread;
    QTimer              *cd_watcher;
    bool                cd_checking_flag;

    QTimer              *fill_list_timer;
    int                 wait_counter;
    int                 numb_wait_dots;

    QStringList         treelevels;
};

#endif
