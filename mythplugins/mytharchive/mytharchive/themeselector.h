#ifndef THEMESELECTOR_H_
#define THEMESELECTOR_H_

// qt
#include <QStringList>

// mythtv
#include <libmythui/mythscreentype.h>

// mytharchive
#include "archiveutil.h"

class MythUIText;
class MythUIButton;
class MythUIButtonList;
class MythUIButtonListItem;
class MythUIImage;

class DVDThemeSelector : public MythScreenType
{

  Q_OBJECT

  public:
    DVDThemeSelector(MythScreenStack *parent, MythScreenType *destinationScreen,
                     const ArchiveDestination& archiveDestination, const QString& name);
    ~DVDThemeSelector(void) override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

  protected slots:
    void handleNextPage(void);
    void handlePrevPage(void);
    void handleCancel(void);

    void themeChanged(MythUIButtonListItem *item);

  private:
    void getThemeList(void);
    static QString loadFile(const QString &filename);
    void loadConfiguration(void);
    void saveConfiguration(void);

    MythScreenType    *m_destinationScreen {nullptr};
    ArchiveDestination m_archiveDestination;

    QString           m_themeDir;

    MythUIButtonList *m_themeSelector      {nullptr};
    MythUIImage      *m_themeImage         {nullptr};
    int               m_themeNo            {0};
    QStringList       m_themeList;

    MythUIImage      *m_introImage         {nullptr};
    MythUIImage      *m_mainmenuImage      {nullptr};
    MythUIImage      *m_chapterImage       {nullptr};
    MythUIImage      *m_detailsImage       {nullptr};
    MythUIText       *m_themedescText      {nullptr};

    MythUIButton     *m_nextButton         {nullptr};
    MythUIButton     *m_prevButton         {nullptr};
    MythUIButton     *m_cancelButton       {nullptr};
};

#endif


