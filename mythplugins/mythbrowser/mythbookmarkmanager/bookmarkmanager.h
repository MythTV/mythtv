#ifndef BOOKMARKMANAGER_H
#define BOOKMARKMANAGER_H

#include <qsqldatabase.h>

#include <mythtv/uitypes.h>
#include <mythtv/xmlparse.h>
#include <mythtv/oldsettings.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/mythdialogs.h>

//class BookmarkConfigPriv;

class PopupBox : public QDialog
{
    Q_OBJECT

public:
    PopupBox(QWidget *parent);
    ~PopupBox();

signals:
    void finished(const char* group, const char* desc,const char* url);

private slots:
    void slotOkClicked();

private:
    QLineEdit* group;
    QLineEdit* desc;
    QLineEdit* url;
};

class BookmarksConfig : public MythDialog
{
    Q_OBJECT

public:

    BookmarksConfig(QSqlDatabase *db,
                   MythMainWindow *parent,
                   const char *name = 0);
    ~BookmarksConfig();

private:

    void populateListView();
    void setupView();

    MythSpinBox        *zoom;
    MythLineEdit       *browser;
    QSqlDatabase       *myDb;
    MythListView       *myBookmarksView;

private slots:

    void slotBookmarksViewExecuted(QListViewItem *item);
    void slotAddBookmark();
    void slotFinish();
    void slotWebSiteAdded(const char* group, const char* desc, const char* url);
};


class Bookmarks : public MythDialog
{
    Q_OBJECT

public:

    Bookmarks(QSqlDatabase *db,
                   MythMainWindow *parent,
                   const char *name = 0);
    ~Bookmarks();

private:

    void populateListView();
    void setupView();

    QSqlDatabase       *myDb;
    MythListView       *myBookmarksView;
//    BookmarkConfigPriv *myTree;

private slots:

    void slotBookmarksViewExecuted(QListViewItem *item);
};

#endif /* BOOKMARKMANAGER_H */
