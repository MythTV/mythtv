#ifndef SETTINGS_H
#define SETTINGS_H

#include <qobject.h>
#include <qstring.h>
#include <iostream>
#include <vector>
#include <map>

#include "mythwidgets.h"
#include "mythdialogs.h"

using namespace std;

//class QSqlDatabase;
class QWidget;
class ConfigurationGroup;
class QDir;

class Configurable: virtual public QObject {
    Q_OBJECT
public:
    Configurable():
        labelAboveWidget(false), enabled(true), visible(true) {};
    virtual ~Configurable() {};

    // Create and return a widget for configuring this entity
    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent,
                                  const char* widgetName = 0);

    virtual void load() = 0;
    virtual void save() = 0;
    virtual void save(QString /*destination*/) { }

    // A name for looking up the setting
    void setName(QString str) {
        configName = str;
        if (label == QString::null)
            setLabel(str);
    };
    QString getName(void) const { return configName; };
    virtual class Setting* byName(QString name) = 0;

    // A label displayed to the user
    void setLabel(QString str) { label = str; };
    QString getLabel(void) const { return label; };
    void setLabelAboveWidget(bool l = true) { labelAboveWidget = l; }

    void setHelpText(QString str) { helptext = str; };
    QString getHelpText(void) const { return helptext; }

    void setVisible(bool b) { visible = b; };
    bool isVisible(void) const { return visible; };

    virtual void setEnabled(bool b) { enabled = b; }
    bool isEnabled() { return enabled; }

public slots:
    virtual void enableOnSet(const QString &val);
    virtual void enableOnUnset(const QString &val);

protected:
    bool labelAboveWidget; 
    bool enabled;

private:
    QString configName;
    QString label;
    QString helptext;
    bool visible;
};

class Setting: virtual public Configurable {
    Q_OBJECT
public:
    Setting(): changed(false) {};
    virtual ~Setting() {};

    virtual QString getValue(void) const {
        return settingValue;
    };

    virtual Setting* byName(QString name) {
        if (name == getName())
            return this;
        return NULL;
    };

    bool isChanged(void) { return changed; };
    void setUnchanged(void) { changed = false; };
    void setChanged(void) { changed = true; };

public slots:
    virtual void setValue(const QString& newValue) {
        settingValue = newValue;
        changed = true;
        emit valueChanged(settingValue);
    };
signals:
    void valueChanged(const QString&);

protected:
    QString settingValue;
    bool changed;
};

class ConfigurationGroup: virtual public Configurable 
{
    Q_OBJECT
  public:
    ConfigurationGroup(bool luselabel = true, bool luseframe = true,
                       bool lzeroMargin = false, bool lzeroSpace = false) 
              { uselabel = luselabel; useframe = luseframe; 
               zeroMargin = lzeroMargin; zeroSpace = lzeroSpace; }
    virtual ~ConfigurationGroup();


    void addChild(Configurable* child) {
        children.push_back(child);
    };

    virtual Setting* byName(QString name);

    virtual void load();

    virtual void save();
    virtual void save(QString destination);

    void setUseLabel(bool useit) { uselabel = useit; }
    void setUseFrame(bool useit) { useframe = useit; }

  signals:
    void changeHelpText(QString);
    
  protected:
    typedef vector<Configurable*> childList;
    childList children;
    bool uselabel;
    bool useframe;
    bool zeroMargin;
    bool zeroSpace;
};

class VerticalConfigurationGroup: virtual public ConfigurationGroup {
 public:
    VerticalConfigurationGroup(bool uselabel = true, bool useframe = true,
                                 bool zeroMargin = false, bool zeroSpace = false) 
                : ConfigurationGroup(uselabel, useframe, zeroMargin, zeroSpace) { }

    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent,
                                  const char* widgetName = 0);
};

class HorizontalConfigurationGroup: virtual public ConfigurationGroup {
 public:
    HorizontalConfigurationGroup(bool uselabel = true, bool useframe = true,
                                 bool zeroMargin = false, bool zeroSpace = false) 
                : ConfigurationGroup(uselabel, useframe, zeroMargin, zeroSpace) { }

    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent,
                                  const char* widgetName = 0);
};

class GridConfigurationGroup: virtual public ConfigurationGroup {
 public:
    GridConfigurationGroup(uint col, bool uselabel = true, bool useframe = true,
                           bool zeroMargin = false, bool zeroSpace = false) 
                : ConfigurationGroup(uselabel, useframe, zeroMargin, zeroSpace), 
                  columns(col) { }

    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent,
                                  const char* widgetName = 0);
 private:
    uint columns;
};

class StackedConfigurationGroup: virtual public ConfigurationGroup {
    Q_OBJECT
public:
    StackedConfigurationGroup(bool uselabel = true)
                : ConfigurationGroup(uselabel) { top = 0; saveAll = true; };

    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent,
                                  const char* widgetName = 0);

    void raise(Configurable* child);
    virtual void save();
    virtual void save(QString destination);

    // save all children, or only the top?
    void setSaveAll(bool b) { saveAll = b; };

signals:
    void raiseWidget(int);

protected:
    unsigned top;
    bool saveAll;
};

class ConfigurationDialogWidget: public MythDialog {
    Q_OBJECT
public:
    ConfigurationDialogWidget(MythMainWindow *parent, 
                              const char* widgetName = 0):
        MythDialog(parent, widgetName) {};

    virtual void keyPressEvent(QKeyEvent* e);

signals:
    void editButtonPressed();
    void deleteButtonPressed();
};

class ConfigurationDialog: virtual public Configurable {
public:
    // Make a modal dialog containing configWidget
    virtual MythDialog* dialogWidget(MythMainWindow *parent,
                                     const char* widgetName = 0);

    // Show a dialogWidget, and save if accepted
    int exec(bool saveOnExec = true, bool doLoad = true);

protected:
    MythDialog *dialog;
};

// A wizard is a group with one child per page
class ConfigurationWizard: public ConfigurationDialog,
                           public ConfigurationGroup {
public:
    virtual MythDialog* dialogWidget(MythMainWindow *parent,
                                     const char* widgetName=0);
};

// Read-only display of a setting
class LabelSetting: virtual public Setting {
protected:
    LabelSetting() {};
public:
    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent, 
                                  const char* widgetName = 0);
};

class LineEditSetting: virtual public Setting {
protected:
    LineEditSetting(bool readwrite = true) : edit(NULL) { rw = readwrite; };
public:
    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent, 
                                  const char* widgetName = 0);

    void setRW(bool readwrite = true) { rw = readwrite; edit->setRW(rw); };
    void setRO() { rw = false; edit->setRO(); };

    virtual void setEnabled(bool b);
    virtual void setVisible(bool b);

private:
    MythLineEdit* edit;
    bool rw;
};

// TODO: set things up so that setting the value as a string emits
// the int signal also
class IntegerSetting: virtual public Setting {
    Q_OBJECT
protected:
    IntegerSetting() {};
public:
    int intValue(void) const {
        return settingValue.toInt();
    };
public slots:
    virtual void setValue(int newValue) {
        Setting::setValue(QString::number(newValue));
        emit valueChanged(newValue);
    };
signals:
    void valueChanged(int newValue);
};

class BoundedIntegerSetting: public IntegerSetting {
protected:
    BoundedIntegerSetting(int _min, int _max, int _step) {
        min=_min, max=_max, step=_step;
    };
protected:
    int min;
    int max;
    int step;
};

class SliderSetting: public BoundedIntegerSetting {
protected:
    SliderSetting(int min, int max, int step):
        BoundedIntegerSetting(min, max, step) {};
public:
    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent, 
                                  const char* widgetName = 0);
};

class SpinBoxSetting: public BoundedIntegerSetting {
protected:
    SpinBoxSetting(int min, int max, int step, bool allow_single_step = false):
        BoundedIntegerSetting(min, max, step),
	sstep(allow_single_step) {};

    bool sstep;
    
public:
    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent, 
                                  const char* widgetName = 0);
};

class SelectSetting: virtual public Setting {
    Q_OBJECT
protected:
    SelectSetting() { isSet = false; };
public:
    virtual void addSelection(const QString& label,
                              QString value=QString::null,
                              bool select=false);

    virtual void clearSelections(void);

    virtual void fillSelectionsFromDir(const QDir& dir, bool absPath=true);

signals:
    void selectionAdded(const QString& label, QString value);
    void selectionsCleared(void);

public slots:

    virtual void setValue(const QString& newValue);
    virtual void setValue(int which);

    virtual QString getSelectionLabel(void) const {
        if (!isSet)
            return QString::null;
        return labels[current];
    };

    virtual int getValueIndex(QString value) {
        selectionList::iterator iter = values.begin();
        int ret = 0;
        while (iter != values.end()) {
            if (*iter == value) {
                return ret;
            } else {
                ret++;
                iter++;
            }
        }
        return 0;
    };

protected:
    typedef vector<QString> selectionList;
    selectionList labels;
    selectionList values;
    unsigned current;
    bool isSet;
};

class SelectLabelSetting: public LabelSetting, public SelectSetting {
protected:
    SelectLabelSetting() {};

public:
    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent, 
                                  const char* widgetName = 0);
};

class ComboBoxSetting: public SelectSetting {
    Q_OBJECT;

protected:
    ComboBoxSetting(bool _rw = false, int _step = 1) {
        rw = _rw;
        step = _step;
        widget = NULL;
    };

public:
    virtual void setValue(QString newValue);
    virtual void setValue(int which);

    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent, 
                                  const char* widgetName = 0);

    void setFocus() { if (widget) widget->setFocus(); }

    virtual void setEnabled(bool b);
    virtual void setVisible(bool b);

public slots:
    void addSelection(const QString& label,
                      QString value=QString::null,
                      bool select=false) {
        if (widget != NULL)
            widget->insertItem(label);
        SelectSetting::addSelection(label, value, select);
    };
protected slots:
    void widgetDestroyed() { widget=NULL; };

private:
    bool rw;
    MythComboBox *widget;

protected:
    int step;
};

class ListBoxSetting: public SelectSetting {
    Q_OBJECT
public:
    ListBoxSetting(): widget(NULL), selectionMode(MythListBox::Single) { }
    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent, 
                                  const char* widgetName = 0);

    void setFocus() { if (widget) widget->setFocus(); }
    void setSelectionMode(MythListBox::SelectionMode mode);
    void setCurrentItem(int i) { if (widget) widget->setCurrentItem(i); }
    void setCurrentItem(const QString& str)  { if (widget) widget->setCurrentItem(str); }

    virtual void setEnabled(bool b);

signals:
    void accepted(int);
    void menuButtonPressed(int);

protected slots:
    void setValueByLabel(const QString& label);
    void addSelection(const QString& label,
                      QString value=QString::null,
                      bool select=false) {
        SelectSetting::addSelection(label, value, select);
        if (widget != NULL)
            widget->insertItem(label);
    };
    void widgetDestroyed() { widget=NULL; };
protected:
    MythListBox* widget;
    MythListBox::SelectionMode selectionMode;
};

class RadioSetting: public SelectSetting {
public:
    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent, 
                                  const char* widgetName = 0);
};

class ImageSelectSetting: public SelectSetting {
    Q_OBJECT
public:
    virtual ~ImageSelectSetting();
    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent, 
                                  const char* widgetName = 0);

    virtual void addImageSelection(const QString& label,
                                   QImage* image,
                                   QString value=QString::null,
                                   bool select=false);
protected slots:
    void imageSet(int);

protected:
    vector<QImage*> images;
    QLabel *imagelabel;
    float m_hmult, m_wmult;
};

class BooleanSetting: virtual public Setting {
    Q_OBJECT
public:
    bool boolValue(void) const {
        return getValue().toInt() != 0;
    };
public slots:
    virtual void setValue(bool check) {
        if (check)
            Setting::setValue("1");
        else
            Setting::setValue("0");
        emit valueChanged(check);
    };
signals:
    void valueChanged(bool);
};

class CheckBoxSetting: public BooleanSetting {
public:
    CheckBoxSetting() : widget(NULL) {}
    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent,
                                  const char* widgetName = 0);
    virtual void setEnabled(bool b);
protected:
    MythCheckBox *widget;
};


class TriggeredConfigurationGroup: virtual public ConfigurationGroup {
    Q_OBJECT
public:
    TriggeredConfigurationGroup(bool uselabel = true) 
           : ConfigurationGroup(uselabel)
    {
        trigger = configStack = NULL;
    };

    void setTrigger(Configurable* _trigger);

    void addTarget(QString triggerValue, Configurable* target);

    void setSaveAll(bool b) { configStack->setSaveAll(b); };

protected slots:
    virtual void triggerChanged(const QString& value) {
        configStack->raise(triggerMap[value]);
    };
protected:
    StackedConfigurationGroup* configStack;
    Configurable* trigger;
    map<QString,Configurable*> triggerMap;
};
    
class TabbedConfigurationGroup: virtual public ConfigurationGroup {
    Q_OBJECT
public:
    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent,
                                  const char* widgetName = 0);
};

class PathSetting: public ComboBoxSetting {
public:
    PathSetting(bool _mustexist):
        ComboBoxSetting(true), mustexist(_mustexist) {
    };

    // TODO: this should support globbing of some sort
    virtual void addSelection(const QString& label,
                              QString value=QString::null,
                              bool select=false);

    // Use a combobox for now, maybe a modified file dialog later
    //virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent, const char* widgetName = 0);

protected:
    bool mustexist;
};

class HostnameSetting: virtual public Setting {
public:
    HostnameSetting();
};

class ChannelSetting: virtual public SelectSetting {
public:
    ChannelSetting() {
        setLabel("Channel");
    };

    static void fillSelections(SelectSetting* setting);
    virtual void fillSelections() {
        fillSelections(this);
    };
};

class QDate;
class DateSetting: virtual public Setting {
    Q_OBJECT
public:
    QDate dateValue(void) const;

    virtual QWidget* configWidget(ConfigurationGroup* cg, QWidget* parent,
                                  const char* widgetName = 0);

 public slots:
    void setValue(const QDate& newValue);
};

class QTime;
class TimeSetting: virtual public Setting {
    Q_OBJECT
public:
    QTime timeValue(void) const;

    virtual QWidget* configWidget(ConfigurationGroup* cg, QWidget* parent,
                                  const char* widgetName = 0);

 public slots:
    void setValue(const QTime& newValue);
};

class DBStorage: virtual public Setting {
public:
    DBStorage(QString _table, QString _column):
        table(_table), column(_column) {};

    virtual void load() = 0;
    virtual void save() = 0;
    virtual void save(QString /*destination*/) { }

protected:
    QString getColumn(void) const { return column; };
    QString getTable(void) const { return table; };

    QString table;
    QString column;
};

class SimpleDBStorage: public DBStorage {
public:
    SimpleDBStorage(QString table, QString column):
        DBStorage(table, column) {};

    virtual ~SimpleDBStorage() {};

    virtual void load();
    virtual void save();
    virtual void save(QString destination);

protected:

    virtual QString whereClause(void) = 0;
    virtual QString setClause(void) {
        return QString("%1 = '%2'").arg(column).arg(getValue());
    };
};

class TransientStorage: virtual public Setting {
public:
    virtual void load() {  }
    virtual void save() {  }
    virtual void save(QString) {  }
};

class AutoIncrementStorage: virtual public IntegerSetting, public DBStorage {
public:
    AutoIncrementStorage(QString table, QString column):
        DBStorage(table, column) {
        setValue(0);
    };

    virtual void load() { };
    virtual void save();
    virtual void save(QString destination);
};

class ButtonSetting: virtual public Setting {
    Q_OBJECT
public:
    ButtonSetting() : button(NULL) {}
    virtual QWidget* configWidget(ConfigurationGroup* cg, QWidget* parent,
                                  const char* widgetName=0);

    virtual void setEnabled(bool b);

signals:
    void pressed();
protected:
    MythPushButton *button;
};

class TransButtonSetting: public ButtonSetting, public TransientStorage {
public:
    TransButtonSetting() {};
};

class ConfigPopupDialogWidget: public MythPopupBox {
    Q_OBJECT
public:
    ConfigPopupDialogWidget(MythMainWindow* parent, const char* widgetName=0):
        MythPopupBox(parent, widgetName) {};

    virtual void keyPressEvent(QKeyEvent* e);
    void accept() { MythPopupBox::accept(); };
    void reject() { MythPopupBox::reject(); };
};

class ConfigurationPopupDialog: virtual public Configurable {
    Q_OBJECT
public:
    ConfigurationPopupDialog() { dialog=NULL; };
    ~ConfigurationPopupDialog() { delete dialog; };

    virtual MythDialog* dialogWidget(MythMainWindow* parent,
                                     const char* widgetName=0);
    int exec(bool saveOnAccept=true);

public slots:
    void accept() { if (dialog) dialog->accept(); };
    void reject() { if (dialog) dialog->reject(); };

signals:
    void popupDone();

protected:
    ConfigPopupDialogWidget* dialog;
};

class ProgressSetting: virtual public IntegerSetting {
public:
    ProgressSetting(int _totalSteps): totalSteps(_totalSteps) {};

    QWidget* configWidget(ConfigurationGroup* cg, QWidget* parent,
                          const char* widgetName = 0);

private:
    int totalSteps;
};

class TransLabelSetting: public LabelSetting, public TransientStorage {
public:
    TransLabelSetting() {};
};

class TransCheckBoxSetting: public CheckBoxSetting, public TransientStorage {
public:
    TransCheckBoxSetting() {};
};

class HostSetting: public SimpleDBStorage, virtual public Configurable {
public:
    HostSetting(QString name):
        SimpleDBStorage("settings", "data") {
        setName(name);
    };
    virtual ~HostSetting() { ; }

protected:
    virtual QString whereClause(void);
    virtual QString setClause(void);
};

class GlobalSetting: public SimpleDBStorage, virtual public Configurable {
public:
    GlobalSetting(QString name):
        SimpleDBStorage("settings", "data") {
        setName(name);
    };

protected:
    virtual QString whereClause(void);
    virtual QString setClause(void);
};

class HostSlider: public SliderSetting, public HostSetting {
  public:
    HostSlider(const QString &name, int min, int max, int step) :
        SliderSetting(min, max, step),
        HostSetting(name) { }
};

class HostSpinBox: public SpinBoxSetting, public HostSetting {
  public:
    HostSpinBox(const QString &name, int min, int max, int step, 
                  bool allow_single_step = false) :
        SpinBoxSetting(min, max, step, allow_single_step),
        HostSetting(name) { }
};

class HostCheckBox: public CheckBoxSetting, public HostSetting {
  public:
    HostCheckBox(const QString &name) :
        HostSetting(name) { }
    virtual ~HostCheckBox() { ; }
};

class HostComboBox: public ComboBoxSetting, public HostSetting {
  public:
    HostComboBox(const QString &name, bool rw = false) :
        ComboBoxSetting(rw),
        HostSetting(name) { }
    virtual ~HostComboBox() { ; }
};

class HostRefreshRateComboBox: virtual public HostComboBox
{
    Q_OBJECT
  public:
    HostRefreshRateComboBox(const QString &name, bool rw = false) :
        HostComboBox(name, rw) { ; }
    virtual ~HostRefreshRateComboBox() { ; }
  public slots:
    virtual void ChangeResolution(const QString& resolution);
  private:
    static const vector<short> GetRefreshRates(const QString &resolution);
};

class HostTimeBox: public ComboBoxSetting, public HostSetting {
  public:
    HostTimeBox(const QString &name, const QString &defaultTime = "00:00",
                const int interval = 1) :
        ComboBoxSetting(false, 30 / interval),
        HostSetting(name)
    {
        int hour;
        int minute;
        QString timeStr;

        for (hour = 0; hour < 24; hour++)
        {
            for (minute = 0; minute < 60; minute += interval)
            {
                timeStr = timeStr.sprintf("%02d:%02d", hour, minute);
                addSelection(timeStr, timeStr, timeStr == defaultTime);
            }
        }
    }
};

class HostLineEdit: public LineEditSetting, public HostSetting {
  public:
    HostLineEdit(const QString &name, bool rw = true) :
        LineEditSetting(rw),
        HostSetting(name) { }
};

class HostImageSelect: public ImageSelectSetting, public HostSetting {
  public:
    HostImageSelect(const QString &name) :
        HostSetting(name) { }
};

class GlobalSlider: public SliderSetting, public GlobalSetting {
  public:
    GlobalSlider(const QString &name, int min, int max, int step) :
        SliderSetting(min, max, step),
        GlobalSetting(name) { }
};

class GlobalSpinBox: public SpinBoxSetting, public GlobalSetting {
  public:
    GlobalSpinBox(const QString &name, int min, int max, int step,
                   bool allow_single_step = false) :
        SpinBoxSetting(min, max, step, allow_single_step),
        GlobalSetting(name) { }
};

class GlobalCheckBox: public CheckBoxSetting, public GlobalSetting {
  public:
    GlobalCheckBox(const QString &name) :
        GlobalSetting(name) { }
};

class GlobalComboBox: public ComboBoxSetting, public GlobalSetting {
  public:
    GlobalComboBox(const QString &name, bool rw = false) :
        ComboBoxSetting(rw),
        GlobalSetting(name) { }
};

class GlobalLineEdit: public LineEditSetting, public GlobalSetting {
  public:
    GlobalLineEdit(const QString &name, bool rw = true) :
        LineEditSetting(rw),
        GlobalSetting(name) { }
};

class GlobalImageSelect: public ImageSelectSetting, public GlobalSetting {
  public:
    GlobalImageSelect(const QString &name) :
        GlobalSetting(name) { }
};


#endif
