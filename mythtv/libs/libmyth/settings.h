#ifndef SETTINGS_H
#define SETTINGS_H

#include <qobject.h>
#include <qwidget.h>
#include <qdialog.h>
#include <qstring.h>
#include <iostream>
#include <vector>

using namespace std;

class QSqlDatabase;
class QWidget;
class MythContext;

class Configurable: virtual public QObject {
    Q_OBJECT
public:
    Configurable():
        visible(true) {};
    virtual ~Configurable() {};

    // Create and return a widget for configuring this entity
    virtual QWidget* configWidget(QWidget* parent,
                                  const char* widgetName = 0) = 0;

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

    void setVisible(bool b) { visible = b; };
    bool isVisible(void) const { return visible; };

private:
    QString configName;
    QString label;
    bool visible;
};

class Setting: virtual public Configurable {
    Q_OBJECT
public:
    virtual ~Setting() {};

    virtual QString getValue(void) const {
        return settingValue;
    };

    virtual Setting* byName(QString name) {
        if (name == getName())
            return this;
        return NULL;
    };

public slots:
    virtual void setValue(const QString& newValue) {
        settingValue = newValue;
        emit valueChanged(settingValue);
    };
signals:
    void valueChanged(const QString&);
protected:
    QString settingValue;
};

class ConfigurationGroup: virtual public Configurable {
public:
    virtual ~ConfigurationGroup() {
        for(childList::iterator i = children.begin() ;
            i != children.end() ;
            ++i )
            delete *i;
    };


    void addChild(Configurable* child) {
        children.push_back(child);
    };

    virtual Setting* byName(QString name) {
        for(childList::iterator i = children.begin() ;
            i != children.end() ;
            ++i ) {

            Setting* c = (*i)->byName(name);
            if (c != NULL)
                return c;
        }

        return NULL;
    }

    virtual void load(QSqlDatabase* db) {
        for(childList::iterator i = children.begin() ;
            i != children.end() ;
            ++i )
            (*i)->load(db);
    };

    virtual void save(QSqlDatabase* db) {
        for(childList::iterator i = children.begin() ;
            i != children.end() ;
            ++i )
            (*i)->save(db);
    };

protected:
    typedef vector<Configurable*> childList;
    childList children;
};

class VerticalConfigurationGroup: virtual public ConfigurationGroup {
 public:
    virtual QWidget* configWidget(QWidget* parent,
                                  const char* widgetName = 0);
};

class HorizontalConfigurationGroup: virtual public ConfigurationGroup {
 public:
    virtual QWidget* configWidget(QWidget* parent,
                                  const char* widgetName = 0);
};

class StackedConfigurationGroup: virtual public ConfigurationGroup {
    Q_OBJECT
public:
    StackedConfigurationGroup() { top = 0; };

    virtual QWidget* configWidget(QWidget* parent,
                                  const char* widgetName = 0);

    void raise(Configurable* child);

signals:
    void raiseWidget(int);

protected:
    unsigned top;
};

class ConfigurationDialogWidget: public QDialog {
public:
    ConfigurationDialogWidget(QWidget* parent = NULL, const char* widgetName = 0):
        QDialog(parent, widgetName, TRUE) {};

    virtual void keyPressEvent(QKeyEvent* e);
};

class ConfigurationDialog: virtual public Configurable {
public:
    ConfigurationDialog(MythContext *context) { m_context = context; }

    // Make a modal dialog containing configWidget
    virtual QDialog* dialogWidget(QWidget* parent,
                                  const char* widgetName = 0);

    // Show a dialogWidget, and save if accepted
    int exec(QSqlDatabase* db);

protected:
    MythContext *m_context;
};

// A wizard is a group with one child per page
class ConfigurationWizard: public ConfigurationDialog,
                           public ConfigurationGroup {
public:
    ConfigurationWizard(MythContext *context) : ConfigurationDialog(context)
                                             { }

    virtual QDialog* dialogWidget(QWidget* parent,
                                  const char* widgetName=0);
    virtual QWidget* configWidget(QWidget* parent,
                                  const char* widgetName=0);
};

class LineEditSetting: virtual public Setting {
protected:
    LineEditSetting() {};
public:
    virtual QWidget* configWidget(QWidget* parent, const char* widgetName = 0);
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
        Setting::setValue(QString("%1").arg(newValue));
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
    virtual QWidget* configWidget(QWidget* parent, const char* widgetName = 0);
};

class SpinBoxSetting: public BoundedIntegerSetting {
protected:
    SpinBoxSetting(int min, int max, int step):
        BoundedIntegerSetting(min, max, step) {};
public:
    virtual QWidget* configWidget(QWidget* parent, const char* widgetName = 0);
};

class StringSelectSetting: virtual public Setting {
    Q_OBJECT
protected:
    StringSelectSetting() { isSet = false; };
public:
    virtual void addSelection(const QString& label,
                              QString value=QString::null,
                              bool select=false) {

        if (value == QString::null)
            value = label;

        labels.push_back(label);
        values.push_back(value);

        emit selectionAdded(label, value);

        if (select || !isSet)
            setValue(value);
    };

    virtual void clearSelections(void) {
        labels.clear();
        values.clear();
        isSet = false;
    };

signals:
    void selectionAdded(const QString& label, QString value);

public slots:

    virtual void setValue(const QString& newValue) {
        bool found = false;
        for(unsigned i = 0 ; i < values.size() ; ++i)
            if (values[i] == newValue) {
                current = i;
                found = true;
                isSet = true;
                break;
            }
        if (found)
            Setting::setValue(newValue);
        else
            addSelection(newValue, newValue, true);
    }

    virtual void setValue(int which) {
        setValue(values[which]);
    };
protected:
    typedef vector<QString> selectionList;
    selectionList labels;
    selectionList values;
    unsigned current;
    bool isSet;
};

class ComboBoxSetting: public StringSelectSetting {
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

    virtual QWidget* configWidget(QWidget* parent, const char* widgetName = 0);
private:
    bool rw;
};

class ListBoxSetting: public StringSelectSetting {
    Q_OBJECT
public:
    virtual QWidget* configWidget(QWidget* parent, const char* widgetName = 0);

signals:
    void itemAdded(QString);

protected slots:
    void addItem(const QString& label, QString value) {
        (void)value;
        emit itemAdded(label);
    };

    void setValueByLabel(const QString& label);
};

class RadioSetting: public StringSelectSetting {
public:
    virtual QWidget* configWidget(QWidget* parent, const char* widgetName = 0);
};

class BooleanSetting: virtual public Setting {
    Q_OBJECT
public:
    bool boolValue(void) const {
        return getValue() == "true";
    };
public slots:
    virtual void setValue(bool check) {
        if (check)
            Setting::setValue("true");
        else
            Setting::setValue(QString::null);
        emit valueChanged(check);
    };
signals:
    void valueChanged(bool);
};

class CheckBoxSetting: public BooleanSetting {
public:
    virtual QWidget* configWidget(QWidget* parent, const char* widgetName = 0);
};


class TriggeredConfigurationGroup: virtual public ConfigurationGroup {
    Q_OBJECT
public:
    TriggeredConfigurationGroup() {
        trigger = configStack = NULL;
    };

    void setTrigger(Configurable* _trigger) {
        trigger = _trigger;
        // Make sure the stack is after the trigger
        addChild(configStack = new StackedConfigurationGroup());

        connect(trigger, SIGNAL(valueChanged(const QString&)),
                this, SLOT(triggerChanged(const QString&)));
    };

    void addTrigger(QString triggerValue, Configurable* target) {
        configStack->addChild(target);
        triggerMap[triggerValue] = target;
    };

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
    virtual QWidget* configWidget(QWidget* parent,
                                  const char* widgetName = 0);
};

class PathSetting: public ComboBoxSetting {
public:
    PathSetting(bool _mustexist):
        ComboBoxSetting(true), mustexist(_mustexist) {
    };
    virtual void addSelection(const QString& label,
                              QString value=QString::null,
                              bool select=false);
    // Use combobox for now, maybe a modified file dialog later
    //virtual QWidget* configWidget(QWidget* parent, const char* widgetName = 0);
protected:
    bool mustexist;
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
