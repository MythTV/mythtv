#ifndef MYTHFLIXQUEUE_H
#define MYTHFLIXQUEUE_H

// QT headers
#include <q3http.h>
#include <QKeyEvent>

// MythTV headers
#include <mythtv/libmythui/mythscreentype.h>
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythuibuttonlist.h>
#include <mythtv/libmythui/mythuiimage.h>
#include <mythtv/libmythui/mythdialogbox.h>

#include "newsengine.h"

/** \class MythFlix
 *  \brief The netflix queue browser class.
 */
class MythFlixQueue : public MythScreenType
{
    Q_OBJECT

public:

    MythFlixQueue(MythScreenStack *, const char *);
    ~MythFlixQueue();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent*);

  private:
    void loadData();
    MythImage* LoadPosterImage(QString location);

    void UpdateNameText();

    void displayOptions();

    void cancelRetrieve();
    void processAndShowNews(NewsSite *site);

    QString executeExternal(const QStringList& args, const QString& purpose);

    MythUIButtonList *m_articlesList;

    MythUIText *m_nameText;
    MythUIText *m_titleText;
    MythUIText *m_descText;

    MythUIImage *m_boxshotImage;

    MythDialogBox  *m_menuPopup;
    QString        zoom;
    QString        browser;
    NewsSite::List m_NewsSites;

    QString        m_queueName;

    Q3Http         *http;

private slots:
    void updateInfoView(MythUIButtonListItem*);
    void slotRetrieveNews();
    void slotNewsRetrieved(NewsSite* site);

    void slotMoveToTop();
    void slotRemoveFromQueue();
    void slotMoveToQueue();
    void slotShowNetFlixPage();
};

#endif /* MYTHFLIXQUEUE_H */

