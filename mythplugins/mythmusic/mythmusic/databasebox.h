#ifndef DATABASEBOX_H_
#define DATABASEBOX_H_

#include <vector>
using namespace std;

#include <QStringList>
#include <QPixmap>

#include "metadata.h"
#include "playlist.h"

#include <mythwidgets.h>
#include <lcddevice.h>
#include <uilistbtntype.h>
#include <mthread.h>

class TreeCheckItem;
class QTimer;
class QKeyEvent;

class ReadCDThread : public MThread
{
  public:

    ReadCDThread(const QString &dev);
    virtual void run();
    bool    statusChanged(){return cd_status_changed;}
    QMutex  *getLock(){return &music_lock;}

  private:

    QString             m_CDdevice;
    bool                cd_status_changed;
    QMutex              music_lock;
};

class DatabaseBox : public MythThemedDialog
{
    Q_OBJECT
  public:
    DatabaseBox(MythMainWindow *parent,
                const QString dev, const QString &window_name,
                const QString &theme_filename, const char *name = 0);
   ~DatabaseBox();

    void dealWithTracks(PlaylistItem *item_ptr);
    void setCDTitle(const QString& title);
    void fillCD(void);

  protected slots:
    void selected(UIListGenericTree *);
    void entered(UIListTreeType *, UIListGenericTree *);
    void doMenus(UIListGenericTree *);
    void alternateDoMenus(UIListGenericTree *, int);
    void keyPressEvent(QKeyEvent *e);
    void moveHeldUpDown(bool flag);
    void deleteTrack(UIListGenericTree *item);
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

    void ErrorPopup(const QString &msg);
    void closeErrorPopup();

    void CreateCDAudio();
    void CreateCDMP3();
    void BlankCDRW();

  private:
    void doSelected(UIListGenericTree *, bool cd_flag);
    void doPlaylistPopup(TreeCheckItem *item_ptr);
    void doActivePopup(PlaylistTitle *item_ptr);
    void checkParent(UIListGenericTree *);

    void checkTree(UIListGenericTree *startingpoint = NULL);
    QPixmap getPixmap(QString &level);

    UIListGenericTree   *rootNode;
    UIListTreeType      *tree;

    CDCheckItem         *cditem;

    QString             m_CDdevice;
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

    vector<UITextType*> m_lines;
};

#endif
