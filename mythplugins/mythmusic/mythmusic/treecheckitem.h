#ifndef TREECHECKITEM_H_
#define TREECHECKITEM_H_

#include <qlistview.h>

class QPixmap;
class PlaylistsContainer;
class Playlist;
class Track;

class TreeCheckItem : public QCheckListItem
{
  public:
    TreeCheckItem(QListView *parent, QString &ltext, const QString &llevel, int l_id); 
    TreeCheckItem(TreeCheckItem *parent, QString &ltext, const QString &llevel, int l_id);
    TreeCheckItem(TreeCheckItem *parent, TreeCheckItem *after, QString &ltext, const QString &llevel, int l_id);
    QString getLevel(void) { return level; }
    int getID(){return id;}
    void    setCheckable(bool flag); //  Need different use than setEnabled();
    bool    isCheckable(){return checkable;}
    void    paintCell(QPainter * p, const QColorGroup & cg, int column, int width, int align);
    void    setOn(bool flag);
    
  private:
    void pickPixmap(void);

    int id;
    QString level;

    bool    checkable;
};

class CDCheckItem : public TreeCheckItem
{
  public:
    CDCheckItem(QListView *parent, QString &ltext, const QString &llevel, int l_track);
    CDCheckItem(CDCheckItem *parent, QString &ltext, const QString &llevel, int l_track);

};

class PlaylistItem : public QListViewItem
{
  public:
    PlaylistItem(QListView *parent, const QString &title);
    PlaylistItem(QListViewItem *parent, const QString &title);
    PlaylistItem(QListViewItem *parent, QListViewItem *after, const QString &title);
    virtual void userSelectedMe(){} // pure virtual, should always be reimplemented
};

class PlaylistTitle : public PlaylistItem
{
  public:

    PlaylistTitle(QListView *parent, const QString &title);
    PlaylistTitle(QListViewItem *parent, const QString &title);
    void setOwner(Playlist *owner);
    void userSelectedMe();
    void removeTrack(int x);
    bool isActive(){return active;}
    bool isDefault();

  private:
  
    Playlist       *ptr_to_owner;
    bool           active;
};

class PlaylistTrack : public PlaylistItem
{
  public:
    PlaylistTrack(QListViewItem *parent, const QString &title);
    PlaylistTrack(QListViewItem *parent, QListViewItem *after, 
                  const QString &title);
    void setOwner(Track *owner);
    void userSelectedMe();
    void beMoving(bool flag);
    void moveUpDown(bool flag);
    int  getID();
    QPixmap         *pixmap;

  private:
    Track           *ptr_to_owner;
    bool            held;    
};

class PlaylistPlaylist : public PlaylistTrack
{
  public:
    PlaylistPlaylist(QListViewItem *parent, const QString &title);
    PlaylistPlaylist(QListViewItem *parent, QListViewItem *after, 
                  const QString &title);

};

class PlaylistCD : public PlaylistTrack
{
  public:
    PlaylistCD(QListViewItem *parent, const QString &title);
    PlaylistCD(QListViewItem *parent, QListViewItem *after, 
                  const QString &title);

};

#endif
