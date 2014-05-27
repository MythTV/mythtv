#ifndef __THEMECHOOSER_H__
#define __THEMECHOOSER_H__

// Qt headers
#include <QString>
#include <QList>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QObject>

// MythTV headers
#include "mythdialogbox.h"
#include "mythdirs.h"
#include "mythscreenstack.h"
#include "mythscreentype.h"
#include "themeinfo.h"

class MythDialogBox;
class MythUIButtonList;
class MythUIText;
class MythUIStateType;

/** \class ThemeChooser
 *  \brief View and select installed themes.
 */
class ThemeChooser : public MythScreenType
{
    Q_OBJECT

  public:
    ThemeChooser(MythScreenStack *parent,
               const QString &name = "ThemeChooser");
   ~ThemeChooser();

    bool Create(void);
    void Load(void);
    void Init(void);
    bool keyPressEvent(QKeyEvent*);
    void customEvent(QEvent *e);

  private slots:
    void itemChanged(MythUIButtonListItem *item);
    void saveAndReload(MythUIButtonListItem *item);

  protected slots:
    void popupClosed(QString which, int result);
    void saveAndReload(void);
    void toggleFullscreenPreview(void);
    void toggleThemeUpdateNotifications(void);
    void refreshDownloadableThemes(void);
    void removeTheme(void);

  signals:
    void themeChanged(void);

  private:
    void LoadVersion(const QString &version, QStringList &themesSeen);

    enum DownloadState
    {
        dsIdle = 0,
        dsDownloadingOnBackend,
        dsDownloadingOnFrontend,
        dsExtractingTheme
    };

    ThemeInfo *loadThemeInfo(QFileInfo &theme);
    void showPopupMenu(void);
    void updateProgressBar(int bytesReceived, int bytesTotal);
    void removeThemeDir(const QString &dirname);

    MythUIButtonList *m_themes;
    MythUIImage      *m_preview;

    bool              m_fullPreviewShowing;
    MythUIStateType  *m_fullPreviewStateType;
    MythUIText       *m_fullScreenName;
    MythUIImage      *m_fullScreenPreview;

    QFileInfoList     m_infoList;
    bool              m_refreshDownloadableThemes;
    QString           m_userThemeDir;

    QMap<QString, ThemeInfo*>  m_themeNameInfos;
    QMap<QString, ThemeInfo*>  m_themeFileNameInfos;
    QMap<QString, QString>     m_themeStatuses;
    ThemeInfo                 *m_downloadTheme;
    QString                    m_downloadFile;
    DownloadState              m_downloadState;

    MythDialogBox      *m_popupMenu;
};

////////////////////////////////////////////////////////////////////////////

class ThemeUpdateChecker : public QObject
{
    Q_OBJECT

  public:
    ThemeUpdateChecker();
   ~ThemeUpdateChecker();

  protected slots:
    void checkForUpdate(void);

  private:
    QTimer    *m_updateTimer;
    QString    m_mythVersion;
    QString    m_infoPackage;
    QString    m_lastKnownThemeVersion;
    QString    m_currentVersion;
    QString    m_newVersion;
};

#endif /* THEMECHOOSER */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
