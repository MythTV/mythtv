#ifndef STANDARDSETTINGS_H_
#define STANDARDSETTINGS_H_

#include "mythexp.h"
#include "mythuibuttonlist.h"
#include "mythdialogbox.h"
#include "mythstorage.h"

#include <QMap>
#include <QObject>
#include <initializer_list>

class StandardSetting;

class MythUIButtonListItemSetting : public QObject, public MythUIButtonListItem
{
    Q_OBJECT

  public:
    MythUIButtonListItemSetting (MythUIButtonList *lbtype, const QString &text)
        : MythUIButtonListItem(lbtype, text) {}

  public slots:
    void ShouldUpdate(StandardSetting *setting);
};

class MPUBLIC StandardSetting : public QObject, public StorageUser
{
    Q_OBJECT

  public:
    virtual void setLabel(QString str) { m_label = str; }
    QString getLabel(void) const { return m_label; }

    virtual void setHelpText(const QString &str) { m_helptext = str; }
    QString getHelpText(void) const { return m_helptext; }

    virtual void setName(const QString &name);
    QString getName(void) const { return m_name; }
    StandardSetting * byName(const QString &name);

    bool isVisible(void) const { return m_visible; }
    bool isEnabled() const { return m_enabled; }
    bool haveChanged();
    void setChanged(bool changed);
    StandardSetting *getParent() const {return m_parent;}

    // Gets
    virtual QString getValue(void) const {return m_settingValue;}

    virtual void updateButton(MythUIButtonListItem *item);

    // StorageUser
    void SetDBValue(const QString &val) override { setValue(val); } // StorageUser
    QString GetDBValue(void) const override { return getValue(); } // StorageUser

    MythUIButtonListItem *createButton(MythUIButtonList *list);

    //subsettings
    virtual void addChild(StandardSetting *child);
    virtual void removeChild(StandardSetting *child);
    virtual QList<StandardSetting *> *getSubSettings();
    virtual bool haveSubSettings();
    virtual void clearSettings();
    void clearTargetedSettings(const QString &value);

    virtual void edit(MythScreenType *screen) = 0;
    virtual void resultEdit(DialogCompletionEvent *dce) = 0;

    virtual void Load(void);
    virtual void Save(void);

    virtual void Open(void) {}
    virtual void Close(void) {}

    Storage *GetStorage(void) const { return m_storage; }

    void addTargetedChild(const QString &value, StandardSetting *setting);
    void addTargetedChildren(const QString &value,
                             std::initializer_list<StandardSetting *> settings);
    void removeTargetedChild(const QString &value, StandardSetting *child);

    virtual bool keyPressEvent(QKeyEvent *);

    void MoveToThread(QThread *thread);

  public slots:
    virtual void setEnabled(bool enabled);
    void setVisible(bool visible);
    virtual void setValue(const QString &newValue);
    virtual void setValue(int newValue);
    virtual void childChanged(StandardSetting *) {}

  signals:
    void valueChanged(const QString &);
    void valueChanged(StandardSetting *);
    void ShouldRedraw(StandardSetting *);
    void settingsChanged(StandardSetting *selectedSetting = nullptr);
    void ChangeSaved();

  protected:
    explicit StandardSetting(Storage *_storage = nullptr)
        : m_storage(_storage) {}
    virtual ~StandardSetting();
    void setParent(StandardSetting *parent);
    QString m_settingValue;
    bool    m_enabled         {true};
    QString m_label;
    QString m_helptext;
    QString m_name;
    bool    m_visible         {true};

  private:
    bool    m_haveChanged     {false};
    Storage *m_storage        {nullptr};
    StandardSetting *m_parent {nullptr};
    QList<StandardSetting *> m_children;
    QMap<QString, QList<StandardSetting *> > m_targets;
};

class MPUBLIC AutoIncrementSetting : public StandardSetting
{
  public:
    AutoIncrementSetting(QString _table, QString _column);

    void Save(void) override; // StandardSetting
    void edit(MythScreenType * /*screen*/) override { } // StandardSetting
    void resultEdit(DialogCompletionEvent * /*dce*/) override { } // StandardSetting

  protected:
    QString m_table;
    QString m_column;
};

/*******************************************************************************
*                           TextEdit Setting                                   *
*******************************************************************************/


class MPUBLIC MythUITextEditSetting : public StandardSetting
{
  public:
    void resultEdit(DialogCompletionEvent *dce) override; // StandardSetting
    void edit(MythScreenType *screen) override; // StandardSetting
    void updateButton(MythUIButtonListItem *item) override; // StandardSetting
    void SetPasswordEcho(bool b);

  protected:
    explicit MythUITextEditSetting(Storage *_storage = nullptr)
        : StandardSetting(_storage) {}
    bool m_passwordEcho {false};
};


class MPUBLIC TransTextEditSetting: public MythUITextEditSetting
{
  public:
    TransTextEditSetting() :
        MythUITextEditSetting() { }
};


class MPUBLIC HostTextEditSetting: public MythUITextEditSetting
{
  public:
    explicit HostTextEditSetting(const QString &name) :
        MythUITextEditSetting(new HostDBStorage(this, name)) { }
};

class MPUBLIC GlobalTextEditSetting: public MythUITextEditSetting
{
  public:
    explicit GlobalTextEditSetting(const QString &name) :
        MythUITextEditSetting(new GlobalDBStorage(this, name)) { }
};

/*******************************************************************************
*                           Directory Setting                                  *
*******************************************************************************/


class MPUBLIC MythUIFileBrowserSetting : public StandardSetting
{
  public:
    void resultEdit(DialogCompletionEvent *dce) override; // StandardSetting
    void edit(MythScreenType *screen) override; // StandardSetting
    void updateButton(MythUIButtonListItem *item) override; // StandardSetting
    void SetTypeFilter(QDir::Filters filter) { m_typeFilter = filter; };
    void SetNameFilter(QStringList filter) { m_nameFilter = filter; };

  protected:
    explicit MythUIFileBrowserSetting(Storage *_storage)
        : StandardSetting(_storage) {}
    QDir::Filters      m_typeFilter {QDir::AllDirs  | QDir::Drives |
                                     QDir::Files    | QDir::Readable |
                                     QDir::Writable | QDir::Executable};
    QStringList        m_nameFilter {"*"};
};


class MPUBLIC HostFileBrowserSetting: public MythUIFileBrowserSetting
{
  public:
    explicit HostFileBrowserSetting(const QString &name) :
        MythUIFileBrowserSetting(new HostDBStorage(this, name)) { }
};


/*******************************************************************************
*                           ComboBox Setting                                   *
*******************************************************************************/
//TODO implement rw=true
class MPUBLIC MythUIComboBoxSetting : public StandardSetting
{
  Q_OBJECT
  public:
    void setValue(int value) override; // StandardSetting
    int getValueIndex(const QString &value) const;
    QString getValueLabel(void) const;
    void resultEdit(DialogCompletionEvent *dce) override; // StandardSetting
    void edit(MythScreenType *screen) override; // StandardSetting
    void addSelection(const QString &label, QString value = QString(),
                      bool select = false);
    void clearSelections();
    void fillSelectionsFromDir(const QDir &dir, bool absPath = true);
    void updateButton(MythUIButtonListItem *item) override; // StandardSetting
    virtual int size(void) const;

  public slots:
    void setValue(const QString&) override; // StandardSetting

  protected:
    /**
     * Create a Setting Widget to select the value from a list
     * \param _storage An object that knows how to get/set the value for
     *                 this item from/to a database.  This should be
     *                 created with a call to XXXStorage.
     * \param rw if set to true, the user can input it's own value
     */
    MythUIComboBoxSetting(Storage *_storage = nullptr, bool rw = false)
        : StandardSetting(_storage), m_rewrite(rw) {}
    ~MythUIComboBoxSetting();
    QVector<QString> m_labels;
    QVector<QString> m_values;

  private:
    bool m_rewrite;
    bool m_isSet {false};

};

class MPUBLIC HostComboBoxSetting: public MythUIComboBoxSetting
{
  public:
    HostComboBoxSetting(const QString &name, bool rw = false) :
        MythUIComboBoxSetting(new HostDBStorage(this, name), rw) { }
};


class MPUBLIC GlobalComboBoxSetting: public MythUIComboBoxSetting
{
  public:
    GlobalComboBoxSetting(const QString &name, bool rw = false) :
        MythUIComboBoxSetting(new GlobalDBStorage(this, name), rw) { }
};

class MPUBLIC TransMythUIComboBoxSetting: public MythUIComboBoxSetting
{
  public:
    explicit TransMythUIComboBoxSetting(bool rw = false) :
        MythUIComboBoxSetting(nullptr, rw) { }
};

class MPUBLIC HostTimeBoxSetting : public HostComboBoxSetting
{
  public:
    HostTimeBoxSetting(const QString &name,
                       const QString &defaultTime = "00:00",
                       const int interval = 1) :
        HostComboBoxSetting(name, false)
    {
        int hour;
        int minute;
        QString timeStr;

        for (hour = 0; hour < 24; hour++)
        {
            for (minute = 0; minute < 60; minute += interval)
            {
                timeStr = timeStr.sprintf("%02d:%02d", hour, minute);
                addSelection(timeStr, timeStr,
                             timeStr == defaultTime);
            }
        }
    }
};

class MPUBLIC GlobalTimeBoxSetting : public GlobalComboBoxSetting
{
  public:
    GlobalTimeBoxSetting(const QString &name,
                       const QString &defaultTime = "00:00",
                       const int interval = 1) :
        GlobalComboBoxSetting(name, false)
    {
        int hour;
        int minute;
        QString timeStr;

        for (hour = 0; hour < 24; hour++)
        {
            for (minute = 0; minute < 60; minute += interval)
            {
                timeStr = timeStr.sprintf("%02d:%02d", hour, minute);
                addSelection(timeStr, timeStr,
                             timeStr == defaultTime);
            }
        }
    }
};

/*******************************************************************************
*                           SpinBox Setting                                    *
*******************************************************************************/


class MPUBLIC MythUISpinBoxSetting : public StandardSetting
{
  public:
    //void setValue(int value);
    int intValue();
    void resultEdit(DialogCompletionEvent *dce) override; // StandardSetting
    void edit(MythScreenType *screen) override; // StandardSetting
    void updateButton(MythUIButtonListItem *item) override; // StandardSetting

  protected:
    // MythUISpinBoxSetting(Storage *_storage, int min, int max, int step,
    //                      bool allow_single_step = false,
    //                      const QString &special_value_text = QString());

    MythUISpinBoxSetting(Storage *_storage, int min, int max, int step,
                         int pageMultiple = 8,
                         QString special_value_text = QString());
  private:
    int m_min;
    int m_max;
    int m_step;
    int m_pageMultiple;
    QString m_special_value_text;
};

class MPUBLIC TransMythUISpinBoxSetting: public MythUISpinBoxSetting
{
  public:
    TransMythUISpinBoxSetting(int min, int max, int step,
                              int pageMultiple = 5,
                              const QString &special_value_text = QString()) :
        MythUISpinBoxSetting(nullptr, min, max, step, pageMultiple,
                             special_value_text)
    { }
};

class MPUBLIC HostSpinBoxSetting: public MythUISpinBoxSetting
{
  public:
    HostSpinBoxSetting(const QString &name, int min, int max, int step,
                       int pageMultiple = 8,
                       const QString &special_value_text = QString()) :
        MythUISpinBoxSetting(new HostDBStorage(this, name), min, max, step,
                             pageMultiple, special_value_text)
    { }
};

class MPUBLIC GlobalSpinBoxSetting: public MythUISpinBoxSetting
{
  public:
    GlobalSpinBoxSetting(const QString &name, int min, int max, int step,
                         int pageMultiple = 8,
                         const QString &special_value_text = QString()) :
        MythUISpinBoxSetting(new GlobalDBStorage(this, name), min, max, step,
                             pageMultiple, special_value_text)
    { }
};

/*******************************************************************************
*                           CheckBox Setting                                   *
*******************************************************************************/

class MPUBLIC MythUICheckBoxSetting : public StandardSetting
{
    Q_OBJECT

  public:
    void resultEdit(DialogCompletionEvent *dce) override; // StandardSetting
    void edit(MythScreenType *screen) override; // StandardSetting
    void updateButton(MythUIButtonListItem *item) override; // StandardSetting
    void setValue(const QString&) override; // StandardSetting
    virtual void setValue(bool value);
    using StandardSetting::setValue;
    bool boolValue() { return m_settingValue == "1"; }

  signals:
    void valueChanged(bool);

  protected:
    explicit MythUICheckBoxSetting(Storage *_storage = nullptr);

};

class MPUBLIC TransMythUICheckBoxSetting: public MythUICheckBoxSetting
{
  public:
    TransMythUICheckBoxSetting() :
        MythUICheckBoxSetting() { }
};

class MPUBLIC HostCheckBoxSetting: public MythUICheckBoxSetting
{
  public:
    explicit HostCheckBoxSetting(const QString &name) :
        MythUICheckBoxSetting(new HostDBStorage(this, name)) { }
};

class MPUBLIC GlobalCheckBoxSetting: public MythUICheckBoxSetting
{
  public:
    explicit GlobalCheckBoxSetting(const QString &name) :
        MythUICheckBoxSetting(new GlobalDBStorage(this, name)) { }
};

/*******************************************************************************
*                              Group Setting                                   *
*******************************************************************************/

class MPUBLIC GroupSetting : public StandardSetting
{
    Q_OBJECT

  public:
    GroupSetting() = default;

    void edit(MythScreenType *screen) override; // StandardSetting
    void resultEdit(DialogCompletionEvent *) override {} // StandardSetting
    virtual void applyChange() {}
    void updateButton(MythUIButtonListItem *item) override; // StandardSetting
    virtual bool canDelete(void) {return false;}
    virtual void deleteEntry(void) {}
};

class MPUBLIC ButtonStandardSetting : public StandardSetting
{
    Q_OBJECT

  public:
    explicit ButtonStandardSetting(const QString &label);

    void edit(MythScreenType *screen) override; // StandardSetting
    void resultEdit(DialogCompletionEvent *) override {}; // StandardSetting

  signals:
    void clicked();
};

/*******************************************************************************
*                            Standard Dialog Setting                           *
*******************************************************************************/

class MPUBLIC StandardSettingDialog : public MythScreenType
{
    Q_OBJECT

  public:

    StandardSettingDialog(MythScreenStack *parent, const char *name,
                          GroupSetting *groupSettings = nullptr)
        : MythScreenType(parent, name), m_settingsTree(groupSettings) {}
    virtual ~StandardSettingDialog();
    bool Create(void) override; // MythScreenType
    void customEvent(QEvent *event) override; // MythUIType
    bool keyPressEvent(QKeyEvent *) override; // MythScreenType
    void ShowMenu(void) override; // MythScreenType
    void deleteEntry(void);

  public slots:
    void Close(void) override; // MythScreenType
    void updateSettings(StandardSetting *selectedSetting = nullptr);
    void editEntry(void);
    void deleteSelected(void);
    void deleteEntryConfirmed(bool ok);

  protected:
    void Load(void) override; // MythScreenType
    void Init(void) override; // MythScreenType
    GroupSetting *GetGroupSettings(void) const;
    MythUIButtonList *m_buttonList         {nullptr};

  private slots:
    void settingSelected(MythUIButtonListItem *item);
    void settingClicked(MythUIButtonListItem *item);

  private:
    void LevelUp();
    void LevelDown();
    void setCurrentGroupSetting(StandardSetting *groupSettings,
                                StandardSetting *selectedSetting = nullptr);
    void Save();

    MythUIText      *m_title               {nullptr};
    MythUIText      *m_groupHelp           {nullptr};
    MythUIText      *m_selectedSettingHelp {nullptr};
    MythDialogBox   *m_menuPopup           {nullptr};
    GroupSetting    *m_settingsTree        {nullptr};
    StandardSetting *m_currentGroupSetting {nullptr};
    bool             m_loaded              {false};
};

Q_DECLARE_METATYPE(StandardSetting *);


#endif
