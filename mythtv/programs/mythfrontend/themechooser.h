#ifndef __THEMECHOOSER_H__
#define __THEMECHOOSER_H__

// Qt headers
#include <QString>
#include <QList>
#include <QDir>
#include <QFileInfo>

// MythTV headers
#include "themeinfo.h"
#include "mythdirs.h"
#include "mythscreentype.h"

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
               const QString name = "ThemeChooser");
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
    void toggleDownloadableThemes(void);
    void removeTheme(void);

  signals:
    void themeChanged(void);

  private:
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
    bool              m_includeDownloadableThemes;

    QMap<QString, ThemeInfo*>  m_themeNameInfos;
    QMap<QString, ThemeInfo*>  m_themeFileNameInfos;
    QMap<QString, QString>     m_themeStatuses;
    ThemeInfo                 *m_downloadTheme;
    QString                    m_downloadFile;
    DownloadState              m_downloadState;

    MythDialogBox      *m_popupMenu;
};

#endif /* THEMECHOOSER */
