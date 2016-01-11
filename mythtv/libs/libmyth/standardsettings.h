#ifndef STANDARDSETTINGS_H_
#define STANDARDSETTINGS_H_

#include "mythuibuttonlist.h"
#include <mythdialogbox.h>
#include <mythuibuttontree.h>
#include <mythgenerictree.h>
#include <QMap>
#include "settings.h"

#include "mythlogging.h"

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

    virtual void setName(const QString &str);
    QString getName(void) const { return m_name; }
    virtual StandardSetting * byName(const QString &name);

    bool isVisible(void) const { return m_visible; };
    bool isEnabled() const { return m_enabled; }
    bool haveChanged();
    StandardSetting *getParent() const {return m_parent;}

    // Gets
    virtual QString getValue(void) const {return m_settingValue;}

    virtual void updateButton(MythUIButtonListItem *item);

    // StorageUser
    void SetDBValue(const QString &val) { setValue(val); }
    QString GetDBValue(void) const { return getValue(); }

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

    //not sure I want to do that yet
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
    void settingsChanged(StandardSetting *selectedSetting = NULL);

  protected:
    StandardSetting(Storage *_storage = NULL);
    virtual ~StandardSetting();
    void setParent(StandardSetting *parent);
    void setChanged(bool changed);
    QString m_settingValue;
    bool m_enabled;
    QString m_label;
    QString m_helptext;
    QString m_name;
    bool m_visible;

  private:
    bool m_haveChanged;
    Storage *m_storage;
    StandardSetting *m_parent;
    QList<StandardSetting *> m_children;
    QMap<QString, QList<StandardSetting *> > m_targets;
};


/*******************************************************************************
*                           TextEdit Setting                                   *
*******************************************************************************/


class MPUBLIC MythUITextEditSetting : public StandardSetting
{
  public:
    virtual void resultEdit(DialogCompletionEvent *dce);
    virtual void edit(MythScreenType *screen);
    virtual void updateButton(MythUIButtonListItem *item);
    void SetPasswordEcho(bool b);

  protected:
    MythUITextEditSetting(Storage *_storage = NULL);
    bool m_passwordEcho;
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
    HostTextEditSetting(const QString &name) :
        MythUITextEditSetting(new HostDBStorage(this, name)) { }
};

class MPUBLIC GlobalTextEditSetting: public MythUITextEditSetting
{
  public:
    GlobalTextEditSetting(const QString &name) :
        MythUITextEditSetting(new GlobalDBStorage(this, name)) { }
};

/*******************************************************************************
*                           Directory Setting                                  *
*******************************************************************************/


class MPUBLIC MythUIFileBrowserSetting : public StandardSetting
{
  public:
    virtual void resultEdit(DialogCompletionEvent *dce);
    virtual void edit(MythScreenType *screen);
    virtual void updateButton(MythUIButtonListItem *item);
    void SetTypeFilter(QDir::Filters filter) { m_typeFilter = filter; };
    void SetNameFilter(QStringList filter) { m_nameFilter = filter; };

  protected:
    MythUIFileBrowserSetting(Storage *_storage);
    QDir::Filters      m_typeFilter;
    QStringList        m_nameFilter;
};


class MPUBLIC HostFileBrowserSetting: public MythUIFileBrowserSetting
{
  public:
    HostFileBrowserSetting(const QString &name) :
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
    void setValue(int value);
    int getValueIndex(const QString &value) const;
    QString getValueLabel(void) const;
    virtual void resultEdit(DialogCompletionEvent *dce);
    virtual void edit(MythScreenType *screen);
    void addSelection(const QString &label, QString value = QString::null,
                      bool select = false);
    void clearSelections();
    void fillSelectionsFromDir(const QDir &dir, bool absPath = true);
    virtual void updateButton(MythUIButtonListItem *item);

  public slots:
    void setValue(const QString&);

  protected:
    MythUIComboBoxSetting(Storage *_storage = NULL, bool rw = false);
    ~MythUIComboBoxSetting();
    QVector<QString> m_labels;
    QVector<QString> m_values;

  private:
    bool m_rewrite;
    bool m_isSet;

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
    TransMythUIComboBoxSetting() :
        MythUIComboBoxSetting() { }
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

/*******************************************************************************
*                           SpinBox Setting                                    *
*******************************************************************************/


class MPUBLIC MythUISpinBoxSetting : public MythUIComboBoxSetting
{
  public:
    //void setValue(int value);
    int intValue();
    virtual void resultEdit(DialogCompletionEvent *dce);
    virtual void edit(MythScreenType *screen);
    virtual void updateButton(MythUIButtonListItem *item);

  protected:
    MythUISpinBoxSetting(Storage *_storage, int min, int max, int step,
                         bool allow_single_step = false,
                         const QString &special_value_text = QString());
  private:
    int m_min;
    int m_max;
    int m_step;
    bool m_allow_single_step;
    QString m_special_value_text;
};

class MPUBLIC TransMythUISpinBoxSetting: public MythUISpinBoxSetting
{
  public:
    TransMythUISpinBoxSetting(int min, int max, int step,
                              bool allow_single_step = false) :
        MythUISpinBoxSetting(NULL, min, max, step, allow_single_step)
    { }
};

class MPUBLIC HostSpinBoxSetting: public MythUISpinBoxSetting
{
  public:
    HostSpinBoxSetting(const QString &name, int min, int max, int step,
                       bool allow_single_step = false) :
        MythUISpinBoxSetting(new HostDBStorage(this, name), min, max, step,
                             allow_single_step)
    { }
};

class MPUBLIC GlobalSpinBoxSetting: public MythUISpinBoxSetting
{
  public:
    GlobalSpinBoxSetting(const QString &name, int min, int max, int step,
                         bool allow_single_step = false) :
        MythUISpinBoxSetting(new GlobalDBStorage(this, name), min, max, step,
                             allow_single_step)
    { }
};

/*******************************************************************************
*                           CheckBox Setting                                   *
*******************************************************************************/

class MPUBLIC MythUICheckBoxSetting : public StandardSetting
{
    Q_OBJECT

  public:
    virtual void resultEdit(DialogCompletionEvent *dce);
    virtual void edit(MythScreenType *screen);
    virtual void updateButton(MythUIButtonListItem *item);
    virtual void setValue(const QString&);
    virtual void setValue(bool value);
    using StandardSetting::setValue;
    bool boolValue();

  signals:
    void valueChanged(bool);

  protected:
    MythUICheckBoxSetting(Storage *_storage = NULL);

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
    HostCheckBoxSetting(const QString &name) :
        MythUICheckBoxSetting(new HostDBStorage(this, name)) { }
};

class MPUBLIC GlobalCheckBoxSetting: public MythUICheckBoxSetting
{
  public:
    GlobalCheckBoxSetting(const QString &name) :
        MythUICheckBoxSetting(new GlobalDBStorage(this, name)) { }
};

/*******************************************************************************
*                              Group Setting                                   *
*******************************************************************************/

class MPUBLIC GroupSetting : public StandardSetting
{
  public:
    GroupSetting();

    virtual void edit(MythScreenType *screen);
    virtual void resultEdit(DialogCompletionEvent *) {}
    virtual void applyChange() {}
    virtual void updateButton(MythUIButtonListItem *item);
    virtual StandardSetting* byName(const QString &name);
};

class MPUBLIC ButtonStandardSetting : public StandardSetting
{
    Q_OBJECT

  public:
    ButtonStandardSetting(const QString &label);

    virtual void edit(MythScreenType *screen);
    virtual void resultEdit(DialogCompletionEvent *){};

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
                          GroupSetting *groupSettings = NULL);
    virtual ~StandardSettingDialog();
    bool Create(void);
    virtual void customEvent(QEvent *event);
    virtual bool keyPressEvent(QKeyEvent *);

  public slots:
    void Close(void);
    void updateSettings(StandardSetting *selectedSetting = NULL);

  protected:
    virtual void Load(void);
    virtual void Init(void);
    GroupSetting *GetGroupSettings(void) const;
    MythUIButtonList *m_buttonList;

  private slots:
    void settingSelected(MythUIButtonListItem *item);
    void settingClicked(MythUIButtonListItem *item);

  private:
    void LevelUp();
    void LevelDown();
    void setCurrentGroupSetting(StandardSetting *groupSettings,
                                StandardSetting *selectedSetting = 0);
    void Save();

    MythUIText      *m_title;
    MythUIText      *m_groupHelp;
    MythUIText      *m_selectedSettingHelp;
    MythDialogBox   *m_menuPopup;
    GroupSetting    *m_settingsTree;
    StandardSetting *m_currentGroupSetting;
    bool m_loaded;
};

Q_DECLARE_METATYPE(StandardSetting *);


#endif
