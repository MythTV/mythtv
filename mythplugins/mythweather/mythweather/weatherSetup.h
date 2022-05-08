#ifndef WEATHER_SETUP_H
#define WEATHER_SETUP_H

// QT headers
#include <QList>

// MythTV headers
#include <libmyth/mythcontext.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythscreentype.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuicheckbox.h>
#include <libmythui/mythuispinbox.h>
#include <libmythui/mythuitext.h>
#include <libmythui/mythuitextedit.h>

// MythWeather headers
#include "weatherUtils.h"

class SourceManager;

struct SourceListInfo
{
    QString name;
    QString author;
    QString email;
    QString version;
    std::chrono::minutes update_timeout   {DEFAULT_UPDATE_TIMEOUT};
    std::chrono::seconds retrieve_timeout {0s};
    uint id               {};
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
    ~GlobalSetup() override = default;

    bool Create(void) override; // MythScreenType

  protected slots:
    void saveData(void);

  private:
    void loadData(void);

  private:
    MythUICheckBox *m_backgroundCheckbox {nullptr};
    MythUISpinBox  *m_timeoutSpinbox     {nullptr};
    int             m_timeout            {0};
    MythUIButton   *m_finishButton       {nullptr};
};

class ScreenSetup : public MythScreenType
{
    Q_OBJECT

  public:
    ScreenSetup(MythScreenStack *parent, const QString &name, SourceManager *srcman);
    ~ScreenSetup() override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType
    void customEvent(QEvent *event) override; // MythUIType

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
    SourceManager    *m_sourceManager { nullptr };
    bool              m_createdSrcMan { false   };
    MythUIText       *m_helpText      { nullptr };
    MythUIButtonList *m_activeList    { nullptr };
    MythUIButtonList *m_inactiveList  { nullptr };
    MythUIButton     *m_finishButton  { nullptr };
};

class SourceSetup : public MythScreenType
{
    Q_OBJECT

  public:
    SourceSetup(MythScreenStack *parent, const QString &name)
      : MythScreenType(parent, name) {};
    ~SourceSetup() override;

    bool Create(void) override; // MythScreenType

    bool loadData(void);

  protected slots:
    void sourceListItemSelected(MythUIButtonListItem *item);
#if 0
    void sourceListItemSelected() { sourceListItemSelected(nullptr) };
#endif
    void updateSpinboxUpdate(void);
    void retrieveSpinboxUpdate(void);
    void saveData(void);

  private:
    MythUISpinBox    *m_updateSpinbox   { nullptr };
    MythUISpinBox    *m_retrieveSpinbox { nullptr };
    MythUIButtonList *m_sourceList      { nullptr };
    MythUIButton     *m_finishButton    { nullptr };
    MythUIText       *m_sourceText      { nullptr };
};

struct ResultListInfo
{
    QString idstr;
    ScriptInfo *src { nullptr };
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
    ~LocationDialog() override;

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
    ScreenListInfo   *m_screenListInfo { nullptr };
    SourceManager    *m_sourceManager  { nullptr };

    MythScreenType   *m_retScreen      { nullptr };

    MythUIButtonList *m_locationList   { nullptr };
    MythUITextEdit   *m_locationEdit   { nullptr };
    MythUIButton     *m_searchButton   { nullptr };
    MythUIText       *m_resultsText    { nullptr };
    MythUIText       *m_sourceText     { nullptr };
};

#endif /* WEATHER_SETUP_H */
