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
    DVDThemeSelector(MythScreenStack *parent, MythScreenType *previousScreen,
                     ArchiveDestination archiveDestination, QString name);
    ~DVDThemeSelector(void);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

  protected slots:
    void handleNextPage(void);
    void handlePrevPage(void);
    void handleCancel(void);

    void themeChanged(MythUIButtonListItem *item);

  private:
    void getThemeList(void);
    QString loadFile(const QString &filename);
    void loadConfiguration(void);
    void saveConfiguration(void);

    MythScreenType    *m_destinationScreen;
    ArchiveDestination m_archiveDestination;

    QString themeDir;

    MythUIButtonList *theme_selector;
    MythUIImage      *theme_image;
    int               theme_no;
    QStringList       theme_list;

    MythUIImage      *intro_image;
    MythUIImage      *mainmenu_image;
    MythUIImage      *chapter_image;
    MythUIImage      *details_image;
    MythUIText       *themedesc_text;

    MythUIButton     *m_nextButton;
    MythUIButton     *m_prevButton;
    MythUIButton     *m_cancelButton;
};

#endif


