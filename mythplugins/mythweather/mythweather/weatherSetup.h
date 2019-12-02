#ifndef _WEATHER_SETUP_H_
#define _WEATHER_SETUP_H_

// QT headers
#include <QList>

// MythTV headers
#include <mythcontext.h>
#include <mythscreentype.h>
#include <mythuibuttonlist.h>
#include <mythuibutton.h>
#include <mythuitext.h>
#include <mythuitextedit.h>
#include <mythuispinbox.h>
#include <mythuicheckbox.h>
#include <mythmainwindow.h>
#include <mythdialogbox.h>

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

Q_DECLARE_METATYPE(SourceListInfo *);

/** \class GlobalSetup
 *  \brief Screen for mythweather global settings
 */
class GlobalSetup : public MythScreenType
{
    Q_OBJECT

  public:
    GlobalSetup(MythScreenStack *parent, const QString &name)
        : MythScreenType(parent, name) {}
    ~GlobalSetup() = default;

    bool Create(void) override; // MythScreenType

  protected slots:
    void saveData(void);

  private:
    void loadData(void);

  private:
    MythUICheckBox *m_backgroundCheckbox {nullptr};
    MythUISpinBox  *m_timeoutSpinbox     {nullptr};
    int             m_timeout            {0};
    int             m_hold_timeout       {0};
    MythUIButton   *m_finishButton       {nullptr};
};

class ScreenSetup : public MythScreenType
{
    Q_OBJECT

  public:
    ScreenSetup(MythScreenStack *parent, const QString &name, SourceManager *srcman);
    ~ScreenSetup();

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *) override; // MythScreenType
    void customEvent(QEvent*) override; // MythUIType

  protected slots:
    void updateHelpText(void);
    void saveData(void);
    void doListSelect(MythUIButtonListItem *selected);

  private:
    void loadData(void);

    void showUnitsPopup(const QString &name, ScreenListInfo *si);
    void doLocationDialog(ScreenListInfo *si);

    void deleteScreen(void);

  private:
    SourceManager *m_sourceManager;
    bool m_createdSrcMan;
    MythUIText *m_helpText;
    MythUIButtonList *m_activeList;
    MythUIButtonList *m_inactiveList;
    MythUIButton *m_finishButton;
};

class SourceSetup : public MythScreenType
{
    Q_OBJECT

  public:
    SourceSetup(MythScreenStack *parent, const QString &name);
    ~SourceSetup();

    bool Create(void) override; // MythScreenType

    bool loadData(void);

  protected slots:
    void sourceListItemSelected(MythUIButtonListItem *item = nullptr);
    void updateSpinboxUpdate(void);
    void retrieveSpinboxUpdate(void);
    void saveData(void);

  private:
    MythUISpinBox *m_updateSpinbox;
    MythUISpinBox *m_retrieveSpinbox;
    MythUIButtonList *m_sourceList;
    MythUIButton *m_finishButton;
    MythUIText *m_sourceText;
};

struct ResultListInfo
{
    QString idstr;
    ScriptInfo *src;
};

Q_DECLARE_METATYPE(ResultListInfo *)

using CacheMap = QMultiHash<QString, QList<ScriptInfo*> >;

class LocationDialog : public MythScreenType
{
    Q_OBJECT

  public:
    LocationDialog(MythScreenStack *parent, const QString &name,
                   MythScreenType *retScreen,
                   ScreenListInfo *si, SourceManager *srcman);
    ~LocationDialog();

    bool Create(void) override; // MythScreenType

  protected slots:
    void doSearch(void);
    void itemSelected(MythUIButtonListItem *item);
    void itemClicked(MythUIButtonListItem *item);

  private:
    void clearResults();

  private:
    CacheMap m_cache;
    QStringList m_types;
    ScreenListInfo *m_screenListInfo;
    SourceManager *m_sourceManager;

    MythScreenType *m_retScreen;

    MythUIButtonList *m_locationList;
    MythUITextEdit *m_locationEdit;
    MythUIButton *m_searchButton;
    MythUIText *m_resultsText;
    MythUIText *m_sourceText;
};

#endif
