#ifndef DATABASEBOX_H_
#define DATABASEBOX_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qstringlist.h>
#include <qthread.h>
#include <qtimer.h>

#include "metadata.h"
#include "playlist.h"
#include <mythtv/mythwidgets.h>

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
                QWidget *parent = 0, const char *name = 0);

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
    void occasionallyCheckCD();
    void keepFilling();
    
  private:

    void doSelected(QListViewItem *, bool cd_flag);
    void doPlaylistPopup(TreeCheckItem *item_ptr);
    void doActivePopup(PlaylistTitle *item_ptr);
    void checkParent(QListViewItem *);
    
    void checkTree();    //Given the current playlist and ListView, check the tree appropriately
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
    MythPushButton      *pop_back_button;

    MythPopupBox        *playlist_popup;
    MythPushButton      *playlist_mac_b;
    MythPushButton      *playlist_del_b;
    MythRemoteLineEdit  *playlist_rename;
    MythPushButton      *playlist_rename_button;

    ReadCDThread        *cd_reader_thread;
    QTimer              *cd_watcher;
    bool                cd_checking_flag;

    QTimer              *fill_list_timer;
    int                 wait_counter;
};

#endif
