// -*- Mode: c++ -*-

#ifndef _MYTHSETTINGS_H_
#define _MYTHSETTINGS_H_

#include <QStringList>
#include <QMap>

class MythSettingBase
{
  public:
    MythSettingBase() {}
    virtual ~MythSettingBase() {}
    virtual QString ToHTML(uint) const { return QString(); }
};
typedef QList<MythSettingBase*> MythSettingList;

class MythSettingGroup : public MythSettingBase
{
  public:
    MythSettingGroup(QString hlabel, QString ulabel,
                     QString script = "") :
        human_label(hlabel), unique_label(ulabel), ecma_script(script) {}

    QString ToHTML(uint) const;

  public:
    QString human_label;
    QString unique_label; ///< div name for stylesheets & javascript
    MythSettingList settings;
    QString ecma_script;
};

class MythSetting : public MythSettingBase
{
  public:
    typedef enum {
        kFile,
        kHost,
        kGlobal,
        kInvalidSettingType,
    } SettingType;

    typedef enum {
        kInteger,
        kUnsignedInteger,
        kIntegerRange,
        kCheckBox,
        kSelect,   ///< list where only data_list are valid
        kComboBox, ///< list where user input is allowed
        kTVFormat,
        kFrequencyTable,
        kFloat,
        kIPAddress,
        kLocalIPAddress,
        kString,
        kTimeOfDay,
        kOther,
        kInvalidDataType,
    } DataType;

    MythSetting(QString _value, QString _default_data, SettingType _stype,
                QString _label, QString _help_text, DataType _dtype) :
        value(_value), data(_default_data), default_data(_default_data),
        stype(_stype), label(_label), help_text(_help_text), dtype(_dtype)
    {
    }

    MythSetting(QString _value, QString _default_data, SettingType _stype,
            QString _label, QString _help_text, DataType _dtype,
            QStringList _data_list, QStringList _display_list) :
        value(_value), data(_default_data), default_data(_default_data),
        stype(_stype), label(_label), help_text(_help_text), dtype(_dtype),
        data_list(_data_list), display_list(_display_list)
    {
    }

    MythSetting(QString _value, QString _default_data, SettingType _stype,
                QString _label, QString _help_text, DataType _dtype,
                long long _range_min, long long _range_max) :
        value(_value), data(_default_data), default_data(_default_data),
        stype(_stype), label(_label), help_text(_help_text), dtype(_dtype),
        range_min(_range_min), range_max(_range_max)
    {
    }

    MythSetting(QString _value, QString _default_data, SettingType _stype,
                QString _label, QString _help_text, DataType _dtype,
                QStringList _data_list, QStringList _display_list,
                long long _range_min, long long _range_max) :
        value(_value), data(_default_data), default_data(_default_data),
        stype(_stype), label(_label), help_text(_help_text), dtype(_dtype),
        data_list(_data_list), display_list(_display_list),
        range_min(_range_min), range_max(_range_max)
    {
    }

    QString ToHTML(uint) const;

  public:
    QString value;
    QString data;
    QString default_data;
    SettingType stype;
    QString label;
    QString help_text;
    DataType dtype;
    QStringList data_list;
    QStringList display_list;
    long long range_min;
    long long range_max;
};

bool parse_settings(MythSettingList &settings, const QString &filename,
                    const QString &group = "");
bool load_settings(MythSettingList &settings, const QString &hostname);
bool check_settings(MythSettingList &database_settings,
                    const QMap<QString,QString> &params, QString &result);

QStringList           GetSettingValueList(const QString &type);
QString               StringMapToJSON(const QMap<QString,QString> &map);
QString               StringListToJSON(const QString &key, const QStringList &sList);
QMap<QString,QString> GetConfigFileSettingValues();
QMap<QString,QString> GetSettingsMap(MythSettingList &settings, const QString &hostname);

#endif
