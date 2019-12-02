// -*- Mode: c++ -*-

#ifndef _MYTHSETTINGS_H_
#define _MYTHSETTINGS_H_

#include <QStringList>
#include <QMap>

class MythSettingBase
{
  public:
    MythSettingBase() = default;
    virtual ~MythSettingBase() = default;
    virtual QString ToHTML(uint) const { return QString(); }
};
using MythSettingList = QList<MythSettingBase*>;

class MythSettingGroup : public MythSettingBase
{
  public:
    MythSettingGroup(const QString& hlabel, const QString& ulabel,
                     const QString& script = "") :
        m_human_label(hlabel), m_unique_label(ulabel), m_ecma_script(script) {}

    QString ToHTML(uint) const override; // MythSettingBase

  public:
    QString         m_human_label;
    QString         m_unique_label; ///< div name for stylesheets & javascript
    MythSettingList m_settings;
    QString         m_ecma_script;
};

class MythSetting : public MythSettingBase
{
  public:
    enum SettingType {
        kFile,
        kHost,
        kGlobal,
        kInvalidSettingType,
    };

    enum DataType {
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
    };

    MythSetting(const QString& _value, const QString& _default_data,
                SettingType _stype, const QString& _label,
                const QString& _help_text, DataType _dtype) :
        m_value(_value), m_data(_default_data), m_default_data(_default_data),
        m_stype(_stype), m_label(_label), m_help_text(_help_text), m_dtype(_dtype)
    {
    }

    MythSetting(const QString& _value, const QString& _default_data,
                SettingType _stype, const QString& _label,
                const QString& _help_text, DataType _dtype,
                const QStringList& _data_list, const QStringList& _display_list) :
        m_value(_value), m_data(_default_data), m_default_data(_default_data),
        m_stype(_stype), m_label(_label), m_help_text(_help_text), m_dtype(_dtype),
        m_data_list(_data_list), m_display_list(_display_list)
    {
    }

    MythSetting(const QString& _value, const QString& _default_data, SettingType _stype,
                const QString& _label, const QString& _help_text, DataType _dtype,
                long long _range_min, long long _range_max) :
        m_value(_value), m_data(_default_data), m_default_data(_default_data),
        m_stype(_stype), m_label(_label), m_help_text(_help_text), m_dtype(_dtype),
        m_range_min(_range_min), m_range_max(_range_max)
    {
    }

    MythSetting(const QString& _value, const QString& _default_data, SettingType _stype,
                const QString& _label, const QString& _help_text, DataType _dtype,
                const QStringList& _data_list, const QStringList& _display_list,
                long long _range_min, long long _range_max,
                const QString& _placeholder) :
        m_value(_value), m_data(_default_data), m_default_data(_default_data),
        m_stype(_stype), m_label(_label), m_help_text(_help_text), m_dtype(_dtype),
        m_data_list(_data_list), m_display_list(_display_list),
        m_range_min(_range_min), m_range_max(_range_max),
        m_placeholder_text(_placeholder)
    {
    }

    QString ToHTML(uint) const override; // MythSettingBase

  public:
    QString m_value;
    QString m_data;
    QString m_default_data;
    SettingType m_stype;
    QString m_label;
    QString m_help_text;
    DataType m_dtype;
    QStringList m_data_list;
    QStringList m_display_list;
    long long m_range_min {-1};
    long long m_range_max {-1};
    QString m_placeholder_text;
};

bool parse_settings(MythSettingList &settings, const QString &filename,
                    const QString &group = "");
bool load_settings(MythSettingList &settings, const QString &hostname);
bool check_settings(MythSettingList &database_settings,
                    const QMap<QString,QString> &params, QString &result);

QStringList           GetSettingValueList(const QString &type);
QString               StringMapToJSON(const QMap<QString,QString> &map);
QString               StringListToJSON(const QString &key, const QStringList &sList);
QMap<QString,QString> GetSettingsMap(MythSettingList &settings, const QString &hostname);

#endif
