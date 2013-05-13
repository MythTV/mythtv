// -*- Mode: c++ -*-

#ifndef SETTINGS_H
#define SETTINGS_H

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <QObject>

// MythTV headers
#include "mythexp.h"
#include "mythwidgets.h"
#include "mythdialogs.h"
#include "mythdbcon.h"
#include "mythstorage.h"

class QWidget;
class ConfigurationGroup;
class QDir;
class Setting;

class MPUBLIC Configurable : public QObject
{
    Q_OBJECT

  public:
    /// Create and return a QWidget for configuring this entity
    /// Note: Any class calling this should call widgetInvalid()
    ///       before configWidget() is called on the class again,
    ///       and before the class is deleted; just before removing
    ///       the instance from a layout or scheduling the delete
    ///       of a parent container is a good time. Some UI classes
    ///       depend on this for properly updating the UI.
    virtual QWidget *configWidget(ConfigurationGroup *cg, QWidget *parent,
                                  const char *widgetName = NULL);
    /// Tell any Configurable keeping a pointer to a widget,
    /// that the pointer returned by an earlier configWidget
    /// call is invalid.
    /// Note: It is possible that this may be called after
    ///       configWidget() has been called another time
    ///       so you must check the pointer param.
    virtual void widgetInvalid(QObject*) { }

    // A name for looking up the setting
    void setName(const QString &str)
    {
        configName = str;
        if (label.isEmpty())
            setLabel(str);
    }
    QString getName(void) const { return configName; }
    virtual Setting *byName(const QString &name) = 0;

    // A label displayed to the user
    virtual void setLabel(QString str) { label = str; }
    QString getLabel(void) const { return label; }
    void setLabelAboveWidget(bool l = true) { labelAboveWidget = l; }

    virtual void setHelpText(const QString &str)
        { helptext = str; }
    QString getHelpText(void) const { return helptext; }

    void setVisible(bool b) { visible = b; };
    bool isVisible(void) const { return visible; };

    virtual void setEnabled(bool b) { enabled = b; }
    bool isEnabled() { return enabled; }

    Storage *GetStorage(void) { return storage; }

  public slots:
    virtual void enableOnSet(const QString &val);
    virtual void enableOnUnset(const QString &val);
    virtual void widgetDeleted(QObject *obj);

  protected:
    Configurable(Storage *_storage) :
        labelAboveWidget(false), enabled(true), storage(_storage),
        configName(""), label(""), helptext(""), visible(true) { }
    virtual ~Configurable() { }

  protected:
    bool labelAboveWidget;
    bool enabled;
    Storage *storage;
    QString configName;
    QString label;
    QString helptext;
    bool visible;
};

class MPUBLIC Setting : public Configurable, public StorageUser
{
    Q_OBJECT

  public:
    // Gets
    virtual QString getValue(void) const;

    // non-const Gets
    virtual Setting *byName(const QString &name)
        { return (name == configName) ? this : NULL; }

    // StorageUser
    void SetDBValue(const QString &val) { setValue(val); }
    QString GetDBValue(void) const { return getValue(); }
  public slots:
    virtual void setValue(const QString &newValue);

  signals:
    void valueChanged(const QString&);

  protected:
    Setting(Storage *_storage) : Configurable(_storage) {};
    virtual ~Setting() {};

  protected:
    QString settingValue;
};

///////////////////////////////////////////////////////////////////////////////

// Read-only display of a setting
class MPUBLIC LabelSetting : public Setting
{
  protected:
    LabelSetting(Storage *_storage) : Setting(_storage) { }
  public:
    virtual QWidget *configWidget(ConfigurationGroup *cg, QWidget *parent,
                                  const char *widgetName = NULL);
};

class MPUBLIC LineEditSetting : public Setting
{
  protected:
    LineEditSetting(Storage *_storage, bool readwrite = true) :
        Setting(_storage), bxwidget(NULL), edit(NULL),
        rw(readwrite), password_echo(false) { }

  public:
    virtual QWidget *configWidget(ConfigurationGroup *cg, QWidget *parent,
                                  const char *widgetName = NULL);
    virtual void widgetInvalid(QObject *obj);

    void setRW(bool readwrite = true)
    {
        rw = readwrite;
        if (edit)
            edit->setRW(rw);
    }

    void setRO(void) { setRW(false); }

    virtual void setEnabled(bool b);
    virtual void setVisible(bool b);
    virtual void SetPasswordEcho(bool b);

    virtual void setHelpText(const QString &str);

  private:
    QWidget      *bxwidget;
    MythLineEdit *edit;
    bool rw;
    bool password_echo;
};

// TODO: set things up so that setting the value as a string emits
// the int signal also
class MPUBLIC IntegerSetting : public Setting
{
    Q_OBJECT

  protected:
    IntegerSetting(Storage *_storage) : Setting(_storage) { }

  public:
    int intValue(void) const { return settingValue.toInt(); }

  public slots:
    virtual void setValue(int newValue)
    {
        Setting::setValue(QString::number(newValue));
        emit valueChanged(newValue);
    }
    virtual void setValue(const QString &nv) { setValue(nv.toInt()); }

  signals:
    void valueChanged(int newValue);
};

class MPUBLIC BoundedIntegerSetting : public IntegerSetting
{
  protected:
    BoundedIntegerSetting(Storage *_storage, int _min, int _max, int _step) :
        IntegerSetting(_storage), min(_min), max(_max), step(_step) { }

  public:
    virtual void setValue(int newValue);
    virtual void setValue(const QString &nv) { setValue(nv.toInt()); }

  protected:
    int min;
    int max;
    int step;
};

class MPUBLIC SliderSetting: public BoundedIntegerSetting
{
    Q_OBJECT

  protected:
    SliderSetting(Storage *_storage, int min, int max, int step) :
        BoundedIntegerSetting(_storage, min, max, step) { }
  public:
    virtual QWidget *configWidget(ConfigurationGroup *cg, QWidget *parent,
                                  const char *widgetName = NULL);
};

class MPUBLIC SpinBoxSetting: public BoundedIntegerSetting
{
    Q_OBJECT

  public:
    SpinBoxSetting(Storage *_storage, int min, int max, int step,
                   bool allow_single_step = false,
                   QString special_value_text = "");

    virtual QWidget *configWidget(ConfigurationGroup *cg, QWidget *parent,
                                  const char *widgetName = NULL);
    virtual void widgetInvalid(QObject *obj);

    void setFocus(void);
    void clearFocus(void);
    bool hasFocus(void) const;

    void SetRelayEnabled(bool enabled) { relayEnabled = enabled; }
    bool IsRelayEnabled(void) const { return relayEnabled; }

    virtual void setHelpText(const QString &str);

  public slots:
    virtual void setValue(int newValue);
    virtual void setValue(const QString &nv) { setValue(nv.toInt()); }

  signals:
    void valueChanged(const QString &name, int newValue);

  private slots:
    void relayValueChanged(int newValue);

  private:
    QWidget     *bxwidget;
    MythSpinBox *spinbox;
    bool         relayEnabled;
    bool         sstep;
    QString      svtext;
};

class MPUBLIC SelectSetting : public Setting
{
    Q_OBJECT

  protected:
    SelectSetting(Storage *_storage) :
        Setting(_storage), current(0), isSet(false) { }

  public:
    virtual int  findSelection(  const QString &label,
                                 QString        value  = QString::null) const;
    virtual void addSelection(   const QString &label,
                                 QString        value  = QString::null,
                                 bool           select = false);
    virtual bool removeSelection(const QString &label,
                                 QString        value  = QString::null);

    virtual void clearSelections(void);

    virtual void fillSelectionsFromDir(const QDir &dir, bool absPath=true);

    virtual uint size(void) const { return labels.size(); }

    virtual QString GetLabel(uint i) const
        { return (i < labels.size()) ? labels[i] : QString::null; }
    virtual QString GetValue(uint i) const
        { return (i < values.size()) ? values[i] : QString::null; }

  signals:
    void selectionAdded(const QString &label, QString value);
    void selectionRemoved(const QString &label, const QString &value);
    void selectionsCleared(void);

  public slots:
    virtual void setValue(const QString &newValue);
    virtual void setValue(int which);

    virtual QString getSelectionLabel(void) const;
    virtual int getValueIndex(QString value);

  protected:
    virtual bool ReplaceLabel(
        const QString &new_label, const QString &value);

    typedef vector<QString> selectionList;
    selectionList labels;
    selectionList values;
    unsigned current;
    bool isSet;
};

class MPUBLIC SelectLabelSetting : public SelectSetting
{
  protected:
    SelectLabelSetting(Storage *_storage) : SelectSetting(_storage) { }

  public:
    virtual QWidget *configWidget(ConfigurationGroup *cg, QWidget *parent,
                                  const char *widgetName = NULL);
};

class MPUBLIC ComboBoxSetting: public SelectSetting
{
    Q_OBJECT

  protected:
    ComboBoxSetting(Storage *_storage, bool _rw = false, int _step = 1) :
        SelectSetting(_storage), rw(_rw),
        bxwidget(NULL), cbwidget(NULL), step(_step) { }

  public:
    virtual QWidget *configWidget(ConfigurationGroup *cg, QWidget *parent,
                                  const char *widgetName = NULL);
    virtual void widgetInvalid(QObject *obj);

    void setFocus(void) { if (cbwidget) cbwidget->setFocus(); }
    void resetMaxCount(int count)
        { if (cbwidget) cbwidget->setMaxCount(count + rw); }

    virtual void setEnabled(bool b);
    virtual void setVisible(bool b);

    virtual void setHelpText(const QString &str);

  public slots:
    virtual void SetDBValue(const QString &newValue)
        { SelectSetting::setValue(newValue); }
    virtual void setValue(const QString &newValue);
    virtual void setValue(int which);

    void addSelection(const QString &label,
                      QString value = QString::null,
                      bool select = false);
    bool removeSelection(const QString &label,
                         QString value = QString::null);
    void editTextChanged(const QString &newText);

  private:
    bool rw;
    QWidget      *bxwidget;
    MythComboBox *cbwidget;

  protected:
    int step;
};

class MPUBLIC ListBoxSetting: public SelectSetting
{
    Q_OBJECT

  public:
    ListBoxSetting(Storage *_storage) :
        SelectSetting(_storage),
        bxwidget(NULL), lbwidget(NULL), eventFilter(NULL),
        selectionMode(MythListBox::SingleSelection) { }

    virtual QWidget *configWidget(ConfigurationGroup *cg, QWidget *parent,
                                  const char *widgetName = NULL);
    virtual void widgetInvalid(QObject *obj);

    void setFocus(void)
        { if (lbwidget) lbwidget->setFocus(); }
    void setSelectionMode(MythListBox::SelectionMode mode);
    void setCurrentItem(int i)
        { if (lbwidget) lbwidget->setCurrentRow(i); }
    void setCurrentItem(const QString &str)
        { if (lbwidget) lbwidget->setCurrentItem(str, true, false); }
    int currentItem(void)
        { return (lbwidget) ? lbwidget->currentRow() : -1; }

    virtual void setEnabled(bool b);

    virtual void clearSelections(void);

    virtual void setHelpText(const QString &str);

    virtual void SetEventFilter(QObject *filter) { eventFilter = filter; }
    virtual bool ReplaceLabel(
        const QString &new_label, const QString &value);

  signals:
    void accepted(int);
    void menuButtonPressed(int);
    void editButtonPressed(int);
    void deleteButtonPressed(int);

  public slots:
    void addSelection(const QString &label,
                      QString        value  = QString::null,
                      bool           select = false);

    void setValueByIndex(int index);

  protected:
    QWidget     *bxwidget;
    MythListBox *lbwidget;
    QObject     *eventFilter;
    MythListBox::SelectionMode selectionMode;
};

class MPUBLIC BooleanSetting : public Setting
{
    Q_OBJECT

  public:
    BooleanSetting(Storage *_storage) : Setting(_storage) {}

    bool boolValue(void) const { return getValue().toInt(); }

  public slots:
    virtual void setValue(bool check)
    {
        if (check)
            Setting::setValue("1");
        else
            Setting::setValue("0");
        emit valueChanged(check);
    };

    virtual void setValue(const QString &newValue)
    {
        setValue((newValue=="1" ||
                  newValue.toLower().startsWith("y") ||
                  newValue.toLower().startsWith("t")));
    }

  signals:
    void valueChanged(bool);
};

class MPUBLIC CheckBoxSetting: public BooleanSetting
{
    Q_OBJECT

  public:
    CheckBoxSetting(Storage *_storage) :
        BooleanSetting(_storage), widget(NULL) { }

    virtual QWidget *configWidget(ConfigurationGroup *cg, QWidget *parent,
                                  const char *widgetName = NULL);

    virtual void widgetInvalid(QObject*);

    virtual void setVisible(bool b);
    virtual void setLabel(QString str);
    virtual void setEnabled(bool b);

    virtual void setHelpText(const QString &str);

  protected:
    MythCheckBox *widget;
};

class MPUBLIC PathSetting : public ComboBoxSetting
{
    Q_OBJECT

  public:
    PathSetting(Storage *_storage, bool _mustexist):
        ComboBoxSetting(_storage, true), mustexist(_mustexist) { }

    // TODO: this should support globbing of some sort
    virtual void addSelection(const QString &label,
                              QString value=QString::null,
                              bool select=false);

    // Use a combobox for now, maybe a modified file dialog later
    //virtual QWidget *configWidget(ConfigurationGroup *cg, QWidget *parent,
    //                              const char *widgetName = NULL);

  protected:
    bool mustexist;
};

class MPUBLIC HostnameSetting : public Setting
{
    Q_OBJECT
  public:
    HostnameSetting(Storage *_storage);
};

class MPUBLIC ChannelSetting : public SelectSetting
{
    Q_OBJECT
  public:
    ChannelSetting(Storage *_storage) : SelectSetting(_storage)
    {
        setLabel("Channel");
    };

    static void fillSelections(SelectSetting *setting);
    virtual void fillSelections(void) { fillSelections(this); }
};

class QDate;
class MPUBLIC DateSetting : public Setting
{
    Q_OBJECT

  public:
    DateSetting(Storage *_storage) : Setting(_storage) { }

    QString getValue(void) const;

    QDate dateValue(void) const;

  public slots:
    void setValue(const QDate &newValue);
    void setValue(const QString &newValue);
};

class QTime;
class MPUBLIC TimeSetting : public Setting
{
    Q_OBJECT

  public:
    TimeSetting(Storage *_storage) : Setting(_storage) { }
    QTime timeValue(void) const;

  public slots:
    void setValue(const QTime &newValue);
    void setValue(const QString &newValue);
};

class MPUBLIC AutoIncrementDBSetting :
    public IntegerSetting, public DBStorage
{
    Q_OBJECT

  public:
    AutoIncrementDBSetting(QString _table, QString _column) :
        IntegerSetting(this), DBStorage(this, _table, _column)
    {
        setValue(0);
    }

    virtual void Load(void) { }
    virtual void Save(void);
    virtual void Save(QString destination);
};

class MPUBLIC ButtonSetting: public Setting
{
    Q_OBJECT

  public:
    ButtonSetting(Storage *_storage, QString _name = "button") :
        Setting(_storage), name(_name), button(NULL) { }

    virtual QWidget *configWidget(ConfigurationGroup *cg, QWidget *parent,
                                  const char *widgetName=0);
    virtual void widgetInvalid(QObject *obj);

    virtual void setEnabled(bool b);

    virtual void setLabel(QString);

    virtual void setHelpText(const QString &);

  signals:
    void pressed(void);
    void pressed(QString name);

  protected slots:
    void SendPressedString();

  protected:
    QString name;
    MythPushButton *button;
};

class MPUBLIC ProgressSetting : public IntegerSetting
{
    Q_OBJECT
  public:
    ProgressSetting(Storage *_storage, int _totalSteps) :
        IntegerSetting(_storage), totalSteps(_totalSteps) { }

    QWidget *configWidget(ConfigurationGroup *cg, QWidget *parent,
                          const char *widgetName = NULL);

  private:
    int totalSteps;
};

///////////////////////////////////////////////////////////////////////////////

class MPUBLIC TransButtonSetting :
    public ButtonSetting, public TransientStorage
{
    Q_OBJECT
  public:
    TransButtonSetting(QString name = "button") :
        ButtonSetting(this, name), TransientStorage() { }
};

class MPUBLIC TransLabelSetting :
    public LabelSetting, public TransientStorage
{
    Q_OBJECT
  public:
    TransLabelSetting() : LabelSetting(this), TransientStorage() { }
};

class MPUBLIC TransLineEditSetting :
    public LineEditSetting, public TransientStorage
{
    Q_OBJECT
  public:
    TransLineEditSetting(bool rw = true) :
        LineEditSetting(this, rw), TransientStorage() { }
};

class MPUBLIC TransCheckBoxSetting :
    public CheckBoxSetting, public TransientStorage
{
    Q_OBJECT
  public:
    TransCheckBoxSetting() : CheckBoxSetting(this), TransientStorage() { }
};

class MPUBLIC TransComboBoxSetting :
    public ComboBoxSetting, public TransientStorage
{
    Q_OBJECT
  public:
    TransComboBoxSetting(bool rw = false, int _step = 1) :
        ComboBoxSetting(this, rw, _step), TransientStorage() { }
};

class MPUBLIC TransSpinBoxSetting :
    public SpinBoxSetting, public TransientStorage
{
    Q_OBJECT
  public:
    TransSpinBoxSetting(int minv, int maxv, int step,
                        bool allow_single_step = false,
                        QString special_value_text = "") :
        SpinBoxSetting(this, minv, maxv, step,
                       allow_single_step, special_value_text) { }
};

class MPUBLIC TransListBoxSetting :
    public ListBoxSetting, public TransientStorage
{
    Q_OBJECT
  public:
    TransListBoxSetting() : ListBoxSetting(this), TransientStorage() { }
};


///////////////////////////////////////////////////////////////////////////////

class MPUBLIC HostSlider : public SliderSetting, public HostDBStorage
{
    Q_OBJECT
  public:
    HostSlider(const QString &name, int min, int max, int step) :
        SliderSetting(this, min, max, step),
        HostDBStorage(this, name) { }
};

class MPUBLIC HostSpinBox: public SpinBoxSetting, public HostDBStorage
{
    Q_OBJECT
  public:
    HostSpinBox(const QString &name, int min, int max, int step,
                bool allow_single_step = false) :
        SpinBoxSetting(this, min, max, step, allow_single_step),
        HostDBStorage(this, name) { }
};

class MPUBLIC HostCheckBox : public CheckBoxSetting, public HostDBStorage
{
    Q_OBJECT
  public:
    HostCheckBox(const QString &name) :
        CheckBoxSetting(this), HostDBStorage(this, name) { }
    virtual ~HostCheckBox() { ; }
};

class MPUBLIC HostComboBox : public ComboBoxSetting, public HostDBStorage
{
    Q_OBJECT
  public:
    HostComboBox(const QString &name, bool rw = false) :
        ComboBoxSetting(this, rw), HostDBStorage(this, name) { }
    virtual ~HostComboBox() { ; }
};

class MPUBLIC HostRefreshRateComboBox : public HostComboBox
{
    Q_OBJECT
  public:
    HostRefreshRateComboBox(const QString &name, bool rw = false) :
        HostComboBox(name, rw) { }
    virtual ~HostRefreshRateComboBox() { ; }

  public slots:
    virtual void ChangeResolution(const QString &resolution);

  private:
    static const vector<double> GetRefreshRates(const QString &resolution);
};

class MPUBLIC HostTimeBox : public ComboBoxSetting, public HostDBStorage
{
    Q_OBJECT
  public:
    HostTimeBox(const QString &name, const QString &defaultTime = "00:00",
                const int interval = 1) :
        ComboBoxSetting(this, false, 30 / interval),
        HostDBStorage(this, name)
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

class MPUBLIC HostLineEdit: public LineEditSetting, public HostDBStorage
{
    Q_OBJECT
  public:
    HostLineEdit(const QString &name, bool rw = true) :
        LineEditSetting(this, rw), HostDBStorage(this, name) { }
};

///////////////////////////////////////////////////////////////////////////////

class MPUBLIC GlobalSlider : public SliderSetting, public GlobalDBStorage
{
    Q_OBJECT
  public:
    GlobalSlider(const QString &name, int min, int max, int step) :
        SliderSetting(this, min, max, step), GlobalDBStorage(this, name) { }
};

class MPUBLIC GlobalSpinBox : public SpinBoxSetting, public GlobalDBStorage
{
    Q_OBJECT
  public:
    GlobalSpinBox(const QString &name, int min, int max, int step,
                  bool allow_single_step = false) :
        SpinBoxSetting(this, min, max, step, allow_single_step),
        GlobalDBStorage(this, name) { }
};

class MPUBLIC GlobalCheckBox : public CheckBoxSetting, public GlobalDBStorage
{
    Q_OBJECT
  public:
    GlobalCheckBox(const QString &name) :
        CheckBoxSetting(this), GlobalDBStorage(this, name) { }
};

class MPUBLIC GlobalComboBox : public ComboBoxSetting, public GlobalDBStorage
{
    Q_OBJECT
  public:
    GlobalComboBox(const QString &name, bool rw = false) :
        ComboBoxSetting(this, rw), GlobalDBStorage(this, name) { }
};

class MPUBLIC GlobalLineEdit : public LineEditSetting, public GlobalDBStorage
{
    Q_OBJECT
  public:
    GlobalLineEdit(const QString &name, bool rw = true) :
        LineEditSetting(this, rw), GlobalDBStorage(this, name) { }
};

class MPUBLIC GlobalTimeBox : public ComboBoxSetting, public GlobalDBStorage
{
    Q_OBJECT
  public:
    GlobalTimeBox(const QString &name, const QString &defaultTime = "00:00",
                  const int interval = 1) :
        ComboBoxSetting(this, false, 30 / interval),
        GlobalDBStorage(this, name)
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

#ifndef MYTHCONFIG
#include "mythconfigdialogs.h"
#include "mythconfiggroups.h"
#endif // MYTHCONFIG

#endif // SETTINGS_H
