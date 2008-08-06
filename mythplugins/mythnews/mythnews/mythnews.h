#ifndef MYTHNEWS_H
#define MYTHNEWS_H

// MythTV headers
#include <mythtv/httpcomms.h>

// MythUI headers
#include <mythtv/libmythui/mythscreentype.h>
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythuibuttonlist.h>
#include <mythtv/libmythui/mythuiimage.h>
#include <mythtv/libmythui/mythdialogbox.h>
#include <mythtv/libmythui/mythprogressdialog.h>

// MythNews headers
#include "newsengine.h"

class QTimer;

/** \class MythNews
 *  \brief Plugin for browsing RSS news feeds.
 */
class MythNews : public MythScreenType
{
    Q_OBJECT

  public:

    MythNews(MythScreenStack *parent, const char *name);
    ~MythNews();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent*);

private:
    void updateInfoView();

    void cancelRetrieve();
    void processAndShowNews(NewsSite *site);

    bool getHttpFile(QString sFilename, QString cmdURL);
    void createProgress(QString title);
    QString formatSize(long long bytes, int prec);
    void playVideo(const QString &filename);

    // menu stuff
    void ShowMenu();
    void deleteNewsSite();
    void ShowEditDialog(bool edit);


    NewsSite::List m_NewsSites;

    QTimer        *m_RetrieveTimer;
    int            m_TimerTimeout;
    unsigned int   m_UpdateFreq;

    QString        timeFormat;
    QString        dateFormat;
    QString        zoom;
    QString        browser;
    MythDialogBox *m_menuPopup;

    bool           abortHttp;

    MythUIProgressDialog *m_progressPopup;
    HttpComms      *httpGrabber;

    MythUIButtonList *m_sitesList;
    MythUIButtonList *m_articlesList;

    MythUIText *m_updatedText;
    MythUIText *m_titleText;
    MythUIText *m_descText;

    MythUIImage *m_thumbnailImage;
    MythUIImage *m_downloadImage;
    MythUIImage *m_enclosureImage;
    MythUIImage *m_podcastImage;

private slots:
    void loadSites();
    void updateInfoView(MythUIButtonListItem*);
    void slotViewArticle(MythUIButtonListItem*);
    void slotRetrieveNews();
    void slotNewsRetrieved(NewsSite*);
    void slotSiteSelected(MythUIButtonListItem*);

    void slotProgressCancelled();
};

#endif /* MYTHNEWS_H */
