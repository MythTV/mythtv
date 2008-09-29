#ifndef _WEATHER_SETUP_H_
#define _WEATHER_SETUP_H_

// QT headers
//Added by qt3to4:
#include <Q3ValueList>
#include <Q3PtrList>

// MythTV headers
#include <mythtv/mythcontext.h>
#include <mythtv/libmythui/mythscreentype.h>
#include <mythtv/libmythui/mythuibuttonlist.h>
#include <mythtv/libmythui/mythuibutton.h>
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythuitextedit.h>
#include <mythtv/libmythui/mythuispinbox.h>
#include <mythtv/libmythui/mythuicheckbox.h>
#include <mythtv/libmythui/mythmainwindow.h>
#include <mythtv/libmythui/mythdialogbox.h>

// MythWeather headers
#include "weatherUtils.h"

class SourceManager;

struct SourceListInfo
{
    QString name;
    QString author;
    QString email;
    QString version;
    uint update_timeout;
    uint retrieve_timeout;
    uint id;
};

/** \class GlobalSetup
 *  \brief Screen for mythweather global settings
 */
class GlobalSetup : public MythScreenType
{
    Q_OBJECT

  public:
    GlobalSetup(MythScreenStack *parent, const char *name);
    ~GlobalSetup();

    bool Create(void);

  protected slots:
    void saveData(void);

  private:
    void loadData(void);

  private:
    MythUICheckBox *m_backgroundCheckbox;
    MythUISpinBox *m_timeoutSpinbox;
    int m_timeout;
    int m_hold_timeout;
    MythUIButton *m_finishButton;
};

class ScreenSetup : public MythScreenType
{
    Q_OBJECT

  public:
    ScreenSetup(MythScreenStack *parent, const char *name, SourceManager *srcman);
    ~ScreenSetup();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent*);

  protected slots:
    void updateHelpText(void);
    void saveData(void);
    void doListSelect(MythListButtonItem *selected);

  private:
    void loadData(void);

    void showUnitsPopup(const QString &name, ScreenListInfo *si);
    void doLocationDialog(ScreenListInfo *si);

    void deleteScreen(void);

  private:
    SourceManager *m_sourceManager;
    bool m_createdSrcMan;
    MythUIText *m_helpText;
    MythListButton *m_activeList;
    MythListButton *m_inactiveList;
    MythUIButton *m_finishButton;
};

class SourceSetup : public MythScreenType
{
    Q_OBJECT

  public:
    SourceSetup(MythScreenStack *parent, const char *name);
    ~SourceSetup();

    bool Create(void);

    bool loadData(void);

  protected slots:
    void sourceListItemSelected(MythListButtonItem *itm = 0);
    void updateSpinboxUpdate(void);
    void retrieveSpinboxUpdate(void);
    void saveData(void);

  private:
    MythUISpinBox *m_updateSpinbox;
    MythUISpinBox *m_retrieveSpinbox;
    MythListButton *m_sourceList;
    MythUIButton *m_finishButton;
    MythUIText *m_sourceText;
};

struct ResultListInfo
{
    QString idstr;
    ScriptInfo *src;
};

class LocationDialog : public MythScreenType
{
    Q_OBJECT

  public:
    LocationDialog(MythScreenStack *parent, const char *name,
                   MythScreenType *retScreen,
                   ScreenListInfo *si, SourceManager *srcman);
    ~LocationDialog();

    bool Create(void);

  protected slots:
    void doSearch(void);
    void itemSelected(MythListButtonItem *item);
    void itemClicked(MythListButtonItem *item);

  private:
    Q3Dict<Q3ValueList<ScriptInfo *> > m_cache;
    QStringList m_types;
    ScreenListInfo *m_screenListInfo;
    SourceManager *m_sourceManager;

    MythScreenType *m_retScreen;

    MythListButton *m_locationList;
    MythUITextEdit *m_locationEdit;
    MythUIButton *m_searchButton;
    MythUIText *m_resultsText;
    MythUIText *m_sourceText;
};

#endif
