#ifndef TREECHECKITEM_H_
#define TREECHECKITEM_H_

#include <QString>

#include <uilistbtntype.h>

class QPixmap;
class PlaylistContainer;
class Playlist;
class Track;

class TreeCheckItem : public UIListGenericTree
{
  public:
    TreeCheckItem(UIListGenericTree *parent, const QString &text, 
                  const QString &level, int id);
    virtual ~TreeCheckItem() { }

    QString getLevel(void) { return m_level; }
    int getID(void) { return m_id; }

    void setCheckable(bool flag); //  Need different use than setEnabled();
    bool isCheckable(void) { return m_checkable; }
    //void paintCell(QPainter * p, const QColorGroup & cg, int column, int width, int align);
    void setCheck(int flag);
 
  private:
    void pickPixmap(void);

    int m_id;
    QString m_level;
    bool m_checkable;
};

class CDCheckItem : public TreeCheckItem
{
  public:
    CDCheckItem(UIListGenericTree *parent, const QString &text, 
                const QString &level, int track);
};

class PlaylistItem : public UIListGenericTree
{
  public:
    PlaylistItem(UIListGenericTree *parent, const QString &title);

    virtual void userSelectedMe() = 0;

    QString getText(void) { return text; }

  private:
    QString text;
};

class PlaylistTitle : public PlaylistItem
{
  public:
    PlaylistTitle(UIListGenericTree *parent, const QString &title);

    void setOwner(Playlist *owner);
    void userSelectedMe(void);
    void removeTrack(int x);
    bool isActive(void) { return active; }

  private:  
    Playlist *ptr_to_owner;
    bool active;
};

class PlaylistTrack : public PlaylistItem
{
  public:
    PlaylistTrack(UIListGenericTree *parent, const QString &title);

    void setOwner(Track *owner);
    void userSelectedMe();
    void beMoving(bool flag);
    void moveUpDown(bool flag);
    int  getID();

  protected:
    QPixmap *pixmap;
    Track *ptr_to_owner;
    bool held;    
};

class PlaylistPlaylist : public PlaylistTrack
{
  public:
    PlaylistPlaylist(UIListGenericTree *parent, const QString &title);
};

class PlaylistCD : public PlaylistTrack
{
  public:
    PlaylistCD(UIListGenericTree *parent, const QString &title);
};

#endif
