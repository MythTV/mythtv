#ifndef THEMESELECTOR_H_
#define THEMESELECTOR_H_

// qt
#include <QStringList>

// mythtv
#include <mythscreentype.h>

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
                     ArchiveDestination archiveDestination, const QString& name);
    ~DVDThemeSelector(void);

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *) override; // MythScreenType

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

    QString themeDir;

    MythUIButtonList *theme_selector       {nullptr};
    MythUIImage      *theme_image          {nullptr};
    int               theme_no             {0};
    QStringList       theme_list;

    MythUIImage      *intro_image          {nullptr};
    MythUIImage      *mainmenu_image       {nullptr};
    MythUIImage      *chapter_image        {nullptr};
    MythUIImage      *details_image        {nullptr};
    MythUIText       *themedesc_text       {nullptr};

    MythUIButton     *m_nextButton         {nullptr};
    MythUIButton     *m_prevButton         {nullptr};
    MythUIButton     *m_cancelButton       {nullptr};
};

#endif


