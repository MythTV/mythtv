#ifndef MYTHFLIX_H
#define MYTHFLIX_H

// MythTV headers
#include <mythtv/libmythui/mythscreentype.h>
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythuibuttonlist.h>
#include <mythtv/libmythui/mythuiimage.h>
#include <mythtv/libmythui/mythdialogbox.h>

#include "newsengine.h"

/** \class MythFlix
 *  \brief The netflix browser class.
 */
class MythFlix : public MythScreenType
{
    Q_OBJECT

  public:

    MythFlix(MythScreenStack *parent, const char *name);
    ~MythFlix();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent*);

  private:
    void loadData();

    void displayOptions();

    void cancelRetrieve();
    void processAndShowNews(NewsSite *site);
    void InsertMovieIntoQueue(QString queueName = "", bool atTop = false);

    QString executeExternal(const QStringList& args, const QString& purpose);

    QString LoadPosterImage(const QString &location) const;

    MythUIButtonList *m_sitesList;
    MythUIButtonList *m_articlesList;

    MythUIText *m_statusText;
    MythUIText *m_titleText;
    MythUIText *m_descText;

    MythUIImage *m_boxshotImage;

    MythDialogBox  *m_menuPopup;
    QString        m_zoom;
    QString        m_browser;
    NewsSite::List m_NewsSites;

private slots:
    void updateInfoView(MythUIButtonListItem*);
    void slotRetrieveNews();
    void slotNewsRetrieved(NewsSite* site);

    void slotSiteSelected(MythUIButtonListItem *item);
    void slotShowNetFlixPage();
};

#endif /* MYTHFLIX_H */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
