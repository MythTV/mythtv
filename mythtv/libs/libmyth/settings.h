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

class QSqlDatabase;
class QWidget;
class ConfigurationGroup;
class QDir;

class Configurable: virtual public QObject {
    Q_OBJECT
public:
    Configurable():
        visible(true) {};
    virtual ~Configurable() {};

    // Create and return a widget for configuring this entity
    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent,
                                  const char* widgetName = 0);

    virtual void load(QSqlDatabase* db) = 0;
    virtual void save(QSqlDatabase* db) = 0;

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

    void setHelpText(QString str) { helptext = str; };
    QString getHelpText(void) const { return helptext; }

    void setVisible(bool b) { visible = b; };
    bool isVisible(void) const { return visible; };

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

public slots:
    virtual void setValue(const QString& newValue) {
        settingValue = newValue;
        changed = true;
        emit valueChanged(settingValue);
    };
signals:
    void valueChanged(const QString&);

protected:
    void setUnchanged(void) { changed = false; };

    QString settingValue;
    bool changed;
};

class ConfigurationGroup: virtual public Configurable 
{
    Q_OBJECT
  public:
    ConfigurationGroup(bool luselabel = true) { uselabel = luselabel; }
    virtual ~ConfigurationGroup();


    void addChild(Configurable* child) {
        children.push_back(child);
    };

    virtual Setting* byName(QString name);

    virtual void load(QSqlDatabase* db);

    virtual void save(QSqlDatabase* db);

    void setUseLabel(bool useit) { uselabel = useit; }

  signals:
    void changeHelpText(QString);
    
  protected:
    typedef vector<Configurable*> childList;
    childList children;
    bool uselabel;
};

class VerticalConfigurationGroup: virtual public ConfigurationGroup {
 public:
    VerticalConfigurationGroup(bool uselabel = true) 
                : ConfigurationGroup(uselabel) { }

    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent,
                                  const char* widgetName = 0);
};

class HorizontalConfigurationGroup: virtual public ConfigurationGroup {
 public:
    HorizontalConfigurationGroup(bool uselabel = true) 
                : ConfigurationGroup(uselabel) { }

    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent,
                                  const char* widgetName = 0);
};

class StackedConfigurationGroup: virtual public ConfigurationGroup {
    Q_OBJECT
public:
    StackedConfigurationGroup(bool uselabel = true)
                : ConfigurationGroup(uselabel) { top = 0; saveAll = true; };

    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent,
                                  const char* widgetName = 0);

    void raise(Configurable* child);
    virtual void save(QSqlDatabase* db);

    // save all children, or only the top?
    void setSaveAll(bool b) { saveAll = b; };

signals:
    void raiseWidget(int);

protected:
    unsigned top;
    bool saveAll;
};

class ConfigurationDialogWidget: public MythDialog {
public:
    ConfigurationDialogWidget(MythMainWindow *parent, 
                              const char* widgetName = 0):
        MythDialog(parent, widgetName) {};

    virtual void keyPressEvent(QKeyEvent* e);
};

class ConfigurationDialog: virtual public Configurable {
public:
    // Make a modal dialog containing configWidget
    virtual MythDialog* dialogWidget(MythMainWindow *parent,
                                     const char* widgetName = 0);

    // Show a dialogWidget, and save if accepted
    int exec(QSqlDatabase* db);
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
    LineEditSetting() {};
public:
    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent, 
                                  const char* widgetName = 0);
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
    SpinBoxSetting(int min, int max, int step):
        BoundedIntegerSetting(min, max, step) {};
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

    virtual void fillSelectionsFromDir(const QDir& dir);

signals:
    void selectionAdded(const QString& label, QString value);
    void selectionsCleared(void);

public slots:

    virtual void setValue(const QString& newValue);

    virtual void setValue(int which) {
        if ((unsigned)which > values.size()-1) {
            cout << "SelectSetting::setValue(): invalid index " << which << endl;
            return;
        }
        setValue(values[which]);
    };

    virtual QString getSelectionLabel(void) const {
        if (!isSet)
            return QString::null;
        return labels[current];
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
protected:
    ComboBoxSetting(bool _rw = false) {
        rw = _rw;
    };
public:
    virtual void setValue(QString newValue) {
        if (!rw)
            cout << "BUG: attempted to set value of read-only ComboBox as string\n";
        else
            Setting::setValue(newValue);
    };

    virtual void setValue(int which) {
        SelectSetting::setValue(which);
    };

    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent, 
                                  const char* widgetName = 0);
private:
    bool rw;
};

class ListBoxSetting: public SelectSetting {
    Q_OBJECT
public:
    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent, 
                                  const char* widgetName = 0);

protected slots:
    void setValueByLabel(const QString& label);
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
    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent,
                                  const char* widgetName = 0);
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

    static void fillSelections(QSqlDatabase* db, SelectSetting* setting);
    virtual void fillSelections(QSqlDatabase* db) {
        fillSelections(db, this);
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

    virtual void load(QSqlDatabase* db) = 0;
    virtual void save(QSqlDatabase* db) = 0;

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

    virtual void load(QSqlDatabase* db);
    virtual void save(QSqlDatabase* db);

protected:

    virtual QString whereClause(void) = 0;
    virtual QString setClause(void) {
        return QString("%1 = '%2'").arg(column).arg(getValue());
    };
};

class TransientStorage: virtual public Setting {
public:
    virtual void load(QSqlDatabase* db) { (void)db; }
    virtual void save(QSqlDatabase* db) { (void)db; }
};

class AutoIncrementStorage: virtual public IntegerSetting, public DBStorage {
public:
    AutoIncrementStorage(QString table, QString column):
        DBStorage(table, column) {
        setValue(0);
    };

    virtual void load(QSqlDatabase* db) { (void)db; };
    virtual void save(QSqlDatabase* db);
};

#endif
