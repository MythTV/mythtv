#ifndef SETTINGS_H
#define SETTINGS_H

#include <qobject.h>
#include <qwidget.h>
#include <qstring.h>
#include <iostream>
#include <vector>

using namespace std;

class QSqlDatabase;
class QWidget;
class QDialog;

class Configurable: virtual public QObject {
    Q_OBJECT
public:
    Configurable() {};
    virtual ~Configurable() {};

    // Create and return a widget for configuring this entity
    virtual QWidget* configWidget(QWidget* parent,
                                  const char* widgetName = 0) = 0;

    virtual void load(QSqlDatabase* db) = 0;
    virtual void save(QSqlDatabase* db) = 0;
    virtual void purge(QSqlDatabase* db) = 0;

    // A name for looking up the setting
    void setName(QString str) {
        configName = str;
        if (label == QString::null)
            setLabel(str);
    };
    QString getName(void) const { return configName; };
    virtual QString byName(QString name) = 0;

    // A label displayed to the user
    void setLabel(QString str) { label = str; };
    QString getLabel(void) const { return label; };

private:
    QString configName;
    QString label;
};

class Setting: virtual public Configurable {
    Q_OBJECT
public:
    virtual ~Setting() {};

    virtual QString getValue(void) {
        cerr << "getValue(" << getName() << "): " << settingValue << endl;
        return settingValue;
    };

    virtual QString byName(QString name) {
        if (name == getName())
            return getValue();
        return QString::null;
    };

public slots:
    virtual void setValue(const QString& newValue) {
        cerr << "setValue(" << getName() << "): " << newValue << endl;
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

    virtual QString byName(QString name) {
        for(childList::iterator i = children.begin() ;
            i != children.end() ;
            ++i ) {

            QString c = (*i)->byName(name);
            if (c != QString::null)
                return c;
        }

        return QString::null;
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

    virtual void purge(QSqlDatabase* db) {
        for(childList::iterator i = children.begin() ;
            i != children.end() ;
            ++i )
            (*i)->purge(db);
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

class TabbedConfigurationGroup: virtual public ConfigurationGroup {
    Q_OBJECT
public:
    virtual QWidget* configWidget(QWidget* parent,
                                  const char* widgetName = 0);
};

class ConfigurationDialog: virtual public Configurable {
public:
    // Make a modal dialog containing configWidget
    virtual QDialog* dialogWidget(QWidget* parent,
                                  const char* widgetName = 0);

    // Show a dialogWidget, and save if accepted
    virtual void exec(QSqlDatabase* db);
};

// A wizard is a group with one child per page
class ConfigurationWizard: virtual public ConfigurationDialog,
                           virtual public ConfigurationGroup {
public:
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
    int intValue(void) {
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
    void addSelection(const QString& label,
                      QString value=QString::null,
                      bool select=false) {

        if (value == QString::null)
            value = label;

        labels.push_back(label);
        values.push_back(value);

        if (select || !isSet)
            setValue(value);
    };

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
            cerr << "BUG: attempted to set value of read-only ComboBox as string\n";
        else
            Setting::setValue(newValue);
    };

    virtual QWidget* configWidget(QWidget* parent, const char* widgetName = 0);
private:
    bool rw;
};

class RadioSetting: public StringSelectSetting {
public:
    virtual QWidget* configWidget(QWidget* parent, const char* widgetName = 0);
};

class BooleanSetting: virtual public Setting {
    Q_OBJECT
public:
    bool boolValue(void) {
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

class SimpleDBStorage: virtual public Setting {
public:
    SimpleDBStorage(QString _table, QString _column) {
        table = _table;
        column = _column;
    };

    virtual ~SimpleDBStorage() {};

    virtual void load(QSqlDatabase* db);
    virtual void save(QSqlDatabase* db);
    virtual void purge(QSqlDatabase* db);

protected:
    QString table;
    QString column;

    QString getColumn(void) const { return column; };

    virtual QString whereClause(void) = 0;
    virtual QString setClause(void) {
        return QString("%1 = '%2'").arg(column).arg(getValue());
    };
};

#endif
