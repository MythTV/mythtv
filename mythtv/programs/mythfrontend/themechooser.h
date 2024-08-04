#ifndef THEMECHOOSER_H
#define THEMECHOOSER_H

// Qt headers
#include <QString>
#include <QList>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QObject>

// MythTV headers
#include "libmythbase/mythdirs.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythscreenstack.h"
#include "libmythui/mythscreentype.h"
#include "libmythui/themeinfo.h"

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
    explicit ThemeChooser(MythScreenStack *parent,
               const QString &name = "ThemeChooser");
   ~ThemeChooser() override;

    bool Create(void) override; // MythScreenType
    void Load(void) override; // MythScreenType
    void Init(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType
    void customEvent(QEvent *e) override; // MythUIType

  private slots:
    void itemChanged(MythUIButtonListItem *item);
    void saveAndReload(MythUIButtonListItem *item);

  protected slots:
    void popupClosed(const QString& which, int result);
    void saveAndReload(void);
    void toggleFullscreenPreview(void);
    static void toggleThemeUpdateNotifications(void);
    void refreshDownloadableThemes(void);
    void removeTheme(void);

  signals:
    void themeChanged(void);

  private:
    bool LoadVersion(const QString &version, QStringList &themesSeen,
                     bool alert_user);

    enum DownloadState : std::uint8_t
    {
        dsIdle = 0,
        dsDownloadingOnBackend,
        dsDownloadingOnFrontend,
        dsExtractingTheme
    };

    ThemeInfo *loadThemeInfo(const QFileInfo &theme);
    void showPopupMenu(void);
    void updateProgressBar(int bytesReceived, int bytesTotal);
    bool removeThemeDir(const QString &dirname);

    MythUIButtonList *m_themes                    {nullptr};
    MythUIImage      *m_preview                   {nullptr};

    bool              m_fullPreviewShowing        {false};
    MythUIStateType  *m_fullPreviewStateType      {nullptr};
    MythUIText       *m_fullScreenName            {nullptr};
    MythUIImage      *m_fullScreenPreview         {nullptr};

    QFileInfoList     m_infoList;
    bool              m_refreshDownloadableThemes {false};
    QString           m_userThemeDir;

    QMap<QString, ThemeInfo*>  m_themeNameInfos;
    QMap<QString, ThemeInfo*>  m_themeFileNameInfos;
    QMap<QString, QString>     m_themeStatuses;
    ThemeInfo                 *m_downloadTheme    {nullptr};
    QString                    m_downloadFile;
    DownloadState              m_downloadState    {dsIdle};

    MythDialogBox      *m_popupMenu               {nullptr};
};

////////////////////////////////////////////////////////////////////////////

class ThemeUpdateChecker : public QObject
{
    Q_OBJECT

  public:
    ThemeUpdateChecker(void);
   ~ThemeUpdateChecker(void) override;

  protected slots:
    void checkForUpdate(void);

  private:
    QTimer    *m_updateTimer {nullptr};
    QStringList m_mythVersions;
    QString    m_infoPackage;
    QString    m_lastKnownThemeVersion;
    QString    m_currentVersion;
    QString    m_newVersion;
};

#endif /* THEMECHOOSER_H */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
