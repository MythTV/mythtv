// -*- Mode: c++ -*-

#ifndef MYTHSETTINGS_H
#define MYTHSETTINGS_H

#include <utility>

// Qt headers
#include <QMap>
#include <QStringList>

class MythSettingBase
{
  public:
    MythSettingBase() = default;
    virtual ~MythSettingBase() = default;
    virtual QString ToHTML(uint /*depth*/) const { return {}; }
};
using MythSettingList = QList<MythSettingBase*>;

class MythSettingGroup : public MythSettingBase
{
  public:
    MythSettingGroup(QString  hlabel, QString  ulabel,
                     QString  script = "") :
        m_human_label(std::move(hlabel)),
        m_unique_label(std::move(ulabel)),
        m_ecma_script(std::move(script)) {}

    QString ToHTML(uint depth) const override; // MythSettingBase

  public:
    QString         m_human_label;
    QString         m_unique_label; ///< div name for stylesheets & javascript
    MythSettingList m_settings;
    QString         m_ecma_script;
};

class MythSetting : public MythSettingBase
{
  public:
    enum SettingType : std::uint8_t {
        kFile,
        kHost,
        kGlobal,
        kInvalidSettingType,
    };

    enum DataType : std::uint8_t {
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

    MythSetting(QString  _value, const QString& _default_data,
                SettingType _stype, QString  _label,
                QString  _help_text, DataType _dtype) :
        m_value(std::move(_value)), m_data(_default_data),
        m_default_data(_default_data), m_stype(_stype),
        m_label(std::move(_label)), m_help_text(std::move(_help_text)),
        m_dtype(_dtype)
    {
    }

    MythSetting(QString  _value, const QString& _default_data,
                SettingType _stype, QString  _label,
                QString  _help_text, DataType _dtype,
                QStringList  _data_list, QStringList  _display_list) :
        m_value(std::move(_value)), m_data(_default_data),
        m_default_data(_default_data), m_stype(_stype),
        m_label(std::move(_label)), m_help_text(std::move(_help_text)),
        m_dtype(_dtype), m_data_list(std::move(_data_list)),
        m_display_list(std::move(_display_list))
    {
    }

    MythSetting(QString  _value, const QString& _default_data, SettingType _stype,
                QString  _label, QString  _help_text, DataType _dtype,
                long long _range_min, long long _range_max) :
        m_value(std::move(_value)), m_data(_default_data),
        m_default_data(_default_data), m_stype(_stype),
        m_label(std::move(_label)), m_help_text(std::move(_help_text)),
        m_dtype(_dtype), m_range_min(_range_min), m_range_max(_range_max)
    {
    }

    MythSetting(QString  _value, const QString& _default_data, SettingType _stype,
                QString  _label, QString  _help_text, DataType _dtype,
                QStringList  _data_list, QStringList  _display_list,
                long long _range_min, long long _range_max,
                QString  _placeholder) :
        m_value(std::move(_value)), m_data(_default_data),
        m_default_data(_default_data), m_stype(_stype),
        m_label(std::move(_label)), m_help_text(std::move(_help_text)),
        m_dtype(_dtype), m_data_list(std::move(_data_list)),
        m_display_list(std::move(_display_list)),
        m_range_min(_range_min), m_range_max(_range_max),
        m_placeholder_text(std::move(_placeholder))
    {
    }

    QString ToHTML(uint level) const override; // MythSettingBase

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
QMap<QString,QString> GetSettingsMap(const MythSettingList &settings, const QString &hostname);

#endif // MYTHSETTINGS_H
