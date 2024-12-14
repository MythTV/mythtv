// Qt
#include <QDomDocument>
#include <QFile>
#include <QNetworkInterface>

// MythTV
#include "libmyth/mythcontext.h"
#include "libmythbase/mythdb.h"
#include "libmythtv/channelsettings.h" // for ChannelTVFormat::GetFormats()
#include "libmythtv/frequencies.h"

// MythBackend
#include "mythsettings.h"

MythSetting::SettingType parse_setting_type(const QString &str);
MythSetting::DataType parse_data_type(const QString &str);
bool parse_dom(MythSettingList &settings, const QDomElement &element,
               const QString &filename, const QString &group,
               bool includeAllChildren, bool &foundGroup);

static QString indent(uint level)
{
    QString ret;
    for (uint i = 0; i < level; i++)
        ret += "    ";
    return ret;
}

static QString extract_query_list(
    const MythSettingList &settings, MythSetting::SettingType stype)
{
    QString list;

    for (const auto *val : std::as_const(settings))
    {
        const auto *group = dynamic_cast<const MythSettingGroup*>(val);
        if (group)
        {
            list += extract_query_list(group->m_settings, stype);
            continue;
        }
        const auto *setting = dynamic_cast<const MythSetting*>(val);
        if (setting && (setting->m_stype == stype))
            list += QString(",'%1'").arg(setting->m_value);
    }
    if (!list.isEmpty() && (list[0] == QChar(',')))
        list = list.mid(1);

    return list;
}

static void fill_setting(
    MythSettingBase *sb, const QMap<QString,QString> &map,
    MythSetting::SettingType stype)
{
    const auto *group = dynamic_cast<const MythSettingGroup*>(sb);
    if (group)
    {
        for (auto *setting : std::as_const(group->m_settings))
            fill_setting(setting, map, stype);
        return;
    }

    auto *setting = dynamic_cast<MythSetting*>(sb);
    if (setting && (setting->m_stype == stype))
    {
        QMap<QString,QString>::const_iterator it = map.find(setting->m_value);
        if (it != map.end())
            setting->m_data = *it;

        bool do_option_check = false;
        if (MythSetting::kLocalIPAddress == setting->m_dtype)
        {
            setting->m_data_list = GetSettingValueList("LocalIPAddress");
            setting->m_display_list = setting->m_data_list;
            do_option_check = true;
        }
        else if (MythSetting::kSelect == setting->m_dtype)
        {
            do_option_check = true;
        }
        else if (MythSetting::kTVFormat == setting->m_dtype)
        {
            setting->m_data_list = setting->m_display_list =
                ChannelTVFormat::GetFormats();
            do_option_check = true;
        }
        else if (MythSetting::kFrequencyTable == setting->m_dtype)
        {
            setting->m_data_list.clear();
            for (const auto &list : gChanLists)
                setting->m_data_list.push_back(list.name);
            setting->m_display_list = setting->m_data_list;
            do_option_check = true;
        }

        if (do_option_check)
        {
            if (!setting->m_data_list.empty() &&
                !setting->m_data_list.contains(setting->m_data.toLower(),
                                               Qt::CaseInsensitive))
            {
                bool ok = false;
                long long idata = setting->m_data.toLongLong(&ok);
                if (ok)
                {
                    uint sel = 0;
                    for (int i = setting->m_data_list.size() - 1; i >= 0; i--)
                    {
                        if (idata < setting->m_data_list[i].toLongLong())
                            break;
                        sel = i;
                    }
                    setting->m_data = setting->m_data_list[sel];
                }
                else
                {
                    setting->m_data =
                        (setting->m_data_list.contains(
                            setting->m_default_data, Qt::CaseInsensitive)) ?
                        setting->m_default_data : setting->m_data_list[0];
                }
            }
        }
    }
}

static void fill_settings(
    MythSettingList &settings, MSqlQuery &query, MythSetting::SettingType stype)
{
    QMap<QString,QString> map;
    while (query.next())
        map[query.value(0).toString()] = query.value(1).toString();

    for (auto *setting : std::as_const(settings))
        fill_setting(setting, map, stype);
}

QString MythSettingGroup::ToHTML(uint depth) const
{
    QString ret;

    ret = indent(depth) +
        QString("<div class=\"group\" id=\"%1\">\r\n").arg(m_unique_label);
    if (!m_human_label.isEmpty())
    {
        ret += indent(depth+1) + QString("<h%1 class=\"config\">%2</h%3>\r\n")
            .arg(depth+1).arg(m_human_label).arg(depth+1);
    }

    for (const auto *setting : std::as_const(m_settings))
        ret += setting->ToHTML(depth+1);

    ret += indent(depth) +"</div>";

    return ret;
}

QString MythSetting::ToHTML(uint level) const
{
    QString ret = indent(level) +
        QString("<div class=\"setting\" id=\"%1_div\">\r\n").arg(m_value);

    int size = 20;
    switch (m_dtype)
    {
        case kFloat:
        case kInteger:
        case kIntegerRange:
        case kUnsignedInteger:
        case kTimeOfDay:
        case kIPAddress:
            size = 20;
            break;
        case kString:
            size = 60;
            break;
        case kCheckBox:
        case kSelect:
        case kComboBox:
        case kTVFormat:
        case kFrequencyTable:
        case kLocalIPAddress:
        case kOther:
        case kInvalidDataType:
            break;
    }

    switch (m_dtype)
    {
        case kInteger:
        case kUnsignedInteger:
            ret += indent(level) +
                QString("<p class=\"setting_paragraph\"><label class=\"setting_label\" "
                "for=\"%1\">%2</label>")
                .arg(m_value, m_label);
            ret += indent(level) +
                QString("<input class=\"setting_input\" name=\"%1\" id=\"%1\" type=\"number\""
                        " value='%2' step='1' size='%3'/>\r\n")
                .arg(m_value, m_data, QString::number(size));
            ret += indent(level) +
                QString("<a class=\"setting_helplink ui-icon ui-icon-help\" href=\"javascript:showSettingHelp('%1')"
                        "\"></a></label></p>\r\n").arg(m_value);
            ret += indent(level) +
                QString("<div class=\"form_error\""
                        "id=\"%1_error\"></div><div style=\"display:none;"
                        "position:absolute;left:-4000px\" "
                        "id=\"%1_default\">%2</div>\r\n")
                .arg(m_value, m_default_data);
            break;
         case kIntegerRange:
            ret += indent(level) +
                QString("<p class=\"setting_paragraph\"><label class=\"setting_label\" "
                "for=\"%1\">%2</label>")
                .arg(m_value, m_label);
            ret += indent(level) +
                QString("<input class=\"setting_input\" name=\"%1\" id=\"%1\" type=\"number\""
                        " value='%2' min='%3' max='%4' step='1' size='%5'/>\r\n")
                .arg(m_value, m_data).arg(m_range_min).arg(m_range_max).arg(size);
            ret += indent(level) +
                QString("<a class=\"setting_helplink ui-icon ui-icon-help\" href=\"javascript:showSettingHelp('%1')"
                        "\"></a></label></p>\r\n").arg(m_value);
            ret += indent(level) +
                QString("<div class=\"form_error\""
                        "id=\"%1_error\"></div><div style=\"display:none;"
                        "position:absolute;left:-4000px\" "
                        "id=\"%1_default\">%2</div>\r\n")
                .arg(m_value, m_default_data);
            break;
        case kFloat:
        case kComboBox:
        case kIPAddress:
        case kString:
        case kTimeOfDay:
        case kOther:
            ret += indent(level) +
                QString("<p class=\"setting_paragraph\"><label class=\"setting_label\" "
                "for=\"%1\">%2</label>")
                .arg(m_value, m_label);
            ret += indent(level) +
                QString("<input class=\"setting_input\" name=\"%1\" id=\"%1\" type=\"text\""
                        " value=\"%2\" size='%3' placeholder=\"%4\"/>\r\n")
                .arg(m_value, m_data, QString::number(size), m_placeholder_text);
            ret += indent(level) +
                QString("<a class=\"setting_helplink ui-icon ui-icon-help\" href=\"javascript:showSettingHelp('%1')"
                        "\"></a></label></p>\r\n").arg(m_value);
            ret += indent(level) +
                QString("<div class=\"form_error\""
                        "id=\"%1_error\"></div><div style=\"display:none;"
                        "position:absolute;left:-4000px\" "
                        "id=\"%1_default\">%2</div>\r\n")
                .arg(m_value, m_default_data);
            break;
        case kCheckBox:
            ret += indent(level) +
                QString("<p class=\"setting_paragraph\">"
                        "<input class=\"setting_input\" name=\"%1_input\" id=\"%1\" type=\"checkbox\""
                        " value=\"1\" %2/><label class=\"setting_label_checkbox\" for=\"%1\">%3</label>")
                .arg(m_value, (m_data.toUInt()) ? "checked" : "", m_label);
            ret += indent(level) +
                QString("<a class=\"setting_helplink ui-icon ui-icon-help\" href=\"javascript:showSettingHelp('%1'"
                        ")\"></a></p><div class=\"form_error\""
                        " id=\"%1_error\"></div>").arg(m_value);
            ret += indent(level) +
                QString("<div style=\"display:none;"
                        "position:absolute;left:-4000px\" "
                        "id=\"%1_default\">%2</div>")
                .arg(m_value, m_default_data);
            break;
        case kLocalIPAddress:
        case kTVFormat:
        case kFrequencyTable:
        case kSelect:
            ret += indent(level) +
                QString("<p class=\"setting_paragraph\"><label class=\"setting_label\" "
                "for=\"%1\">%2</label>")
                .arg(m_value, m_label);
            ret +=  indent(level) +
                QString("<select class=\"setting_select\" name=\"%1_input\" id=\"%1\">\r\n")
                .arg(m_value);
            for (uint i = 0; (i < (uint)m_data_list.size()) &&
                     (i < (uint)m_display_list.size()); i++)
            {
                ret += indent(level+1) +
                    QString("<option value=\"%1\" %2>%3</option>\r\n")
                    .arg(m_data_list[i],
                         (m_data_list[i].toLower() == m_data.toLower()) ?
                         "selected" : "",
                         m_display_list[i]);
            }
            ret += indent(level) + "</select>" +
                QString("<a class=\"setting_helplink ui-icon ui-icon-help\" href=\"javascript:showSettingHelp('%1')"
                        "\"></a></p>\r\n").arg(m_value);
            ret += indent(level) +
                QString("<div class=\"form_error\""
                        "id=\"%1_error\"></div><div style=\"display:none;"
                        "position:absolute;left:-4000px\" "
                        "id=\"%1_default\">%2</div>\r\n")
                .arg(m_value, m_default_data);
            break;
        case kInvalidDataType:
            break;
    }

    ret += indent(level) + "</div>\r\n";

    return ret;
}

MythSetting::SettingType parse_setting_type(const QString &str)
{
    QString s = str.toLower();
    if (s=="file")
        return MythSetting::kFile;
    if (s=="host")
        return MythSetting::kHost;
    if (s=="global")
        return MythSetting::kGlobal;
    return MythSetting::kInvalidSettingType;
}

MythSetting::DataType parse_data_type(const QString &str)
{
    QString s = str.toLower();
    if (s == "integer")
        return MythSetting::kInteger;
    if (s == "unsigned")
        return MythSetting::kUnsignedInteger;
    if (s == "integer_range")
        return MythSetting::kIntegerRange;
    if (s == "checkbox")
        return MythSetting::kCheckBox;
    if (s == "select")
        return MythSetting::kSelect;
    if (s == "combobox")
        return MythSetting::kComboBox;
    if (s == "tvformat")
        return MythSetting::kTVFormat;
    if (s == "frequency_table")
        return MythSetting::kFrequencyTable;
    if (s == "float")
        return MythSetting::kFloat;
    if (s == "ipaddress")
        return MythSetting::kIPAddress;
    if (s == "localipaddress")
        return MythSetting::kLocalIPAddress;
    if (s == "string")
        return MythSetting::kString;
    if (s == "timeofday")
        return MythSetting::kTimeOfDay;
    if (s == "other")
        return MythSetting::kOther;
    LOG(VB_GENERAL, LOG_ERR, QString("Unknown type: %1").arg(str));
    return MythSetting::kInvalidDataType;
}

QMap<QString,QString> GetSettingsMap(const MythSettingList &settings,
                                     const QString &hostname)
{
    QMap<QString,QString> result;
    MSqlQuery query(MSqlQuery::InitCon());

    QString list = extract_query_list(settings, MythSetting::kHost);
    QString qstr =
        "SELECT value, data "
        "FROM settings "
        "WHERE hostname = '" + hostname + "' AND "
        "      value in (" + list + ")";

    if (!list.isEmpty())
    {
        if (!query.exec(qstr))
        {
            MythDB::DBError("GetSettingsMap() 1", query);
            return result;
        }

        while (query.next())
            result[query.value(0).toString()] = query.value(1).toString();
    }

    list = extract_query_list(settings, MythSetting::kGlobal);
    qstr =
        "SELECT value, data "
        "FROM settings "
        "WHERE hostname IS NULL AND "
        "      value in (" + list + ")";

    if (!list.isEmpty())
    {
        if (!query.exec(qstr))
        {
            MythDB::DBError("GetSettingsMap() 2", query);
            return result;
        }

        while (query.next())
            result[query.value(0).toString()] = query.value(1).toString();
    }

    return result;
}

QStringList GetSettingValueList(const QString &type)
{
    QStringList sList;

    if (type == "LocalIPAddress")
    {
        QList<QHostAddress> list = QNetworkInterface::allAddresses();
        for (const auto & addr : std::as_const(list))
        {
            if (addr.toString().contains(":"))
                continue; // ignore IP6 addresses for now
            sList << addr.toString();
        }

        if (sList.isEmpty())
            sList << "127.0.0.1";
    }

    return sList;
}

QString StringMapToJSON(const QMap<QString,QString> &map)
{
    QString result;

    QMap<QString,QString>::const_iterator it = map.begin();
    for (; it != map.end(); ++it)
    {
        if (result.isEmpty())
            result += "{ ";
        else
            result += ", ";

        // FIXME, howto encode double quotes in JSON?
        result += "\"" + it.key() + "\": \"" + it.value() + "\"";
    }

    if (!result.isEmpty())
        result += " }";
    else
        result = "{ }";

    return result;
}

QString StringListToJSON(const QString &key,
                                      const QStringList &sList)
{
    QString result;

    for (const auto & item : std::as_const(sList))
    {
        if (result.isEmpty())
            result += QString("{ \"%1\" : [ ").arg(key);
        else
            result += ", ";

        // FIXME, howto encode double quotes in JSON?
        result += "\"" + item + "\"";
    }

    if (!result.isEmpty())
        result += " ] }";
    else
        result = "{ }";

    return result;
}

bool parse_dom(MythSettingList &settings, const QDomElement &element,
               const QString &filename, const QString &group,
               bool includeAllChildren, bool &foundGroup)
{
#define LOC QString("parse_dom(%1@~%2), error: ") \
            .arg(filename).arg(e.lineNumber())

    bool mFoundGroup = false;

    QDomNode n = element.firstChild();
    while (!n.isNull())
    {
        const QDomElement e = n.toElement();
        if (e.isNull())
        {
            n = n.nextSibling();
            continue;
        }

        if (e.tagName() == "group")
        {
            QString m_human_label  = e.attribute("human_label");
            QString m_unique_label = e.attribute("unique_label");
            QString m_ecma_script  = e.attribute("ecma_script");

            bool tmpFoundGroup = false;
            bool tmpIncludeAllChildren = includeAllChildren;
            if (group.isEmpty() || m_unique_label == group)
            {
                mFoundGroup = true;
                tmpIncludeAllChildren = true;
            }

            auto *g = new MythSettingGroup(
                m_human_label, m_unique_label, m_ecma_script);

            if ((e.hasChildNodes()) &&
                (!parse_dom(g->m_settings, e, filename, group, tmpIncludeAllChildren,
                            tmpFoundGroup)))
            {
                delete g;
                return false;
            }

            if (tmpFoundGroup || tmpIncludeAllChildren)
            {
                settings.push_back(g);
                mFoundGroup = true;
            }
            else
            {
                delete g;
            }

        }
        else if (e.tagName() == "setting" && includeAllChildren)
        {
            QMap<QString,QString> m;
            m["value"]            = e.attribute("value");
            m["setting_type"]     = e.attribute("setting_type");
            m["label"]            = e.attribute("label");
            m["help_text"]        = e.attribute("help_text");
            m["data_type"]        = e.attribute("data_type");

            MythSetting::DataType dtype = parse_data_type(m["data_type"]);
            if (MythSetting::kInvalidDataType == dtype)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Setting has invalid or missing data_type attribute.");
                return false;
            }

            QStringList data_list;
            QStringList display_list;
            if ((MythSetting::kComboBox == dtype) ||
                (MythSetting::kSelect   == dtype))
            {
                if (!e.hasChildNodes())
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC +
                        "Setting missing selection items.");
                    return false;
                }

                QDomNode n2 = e.firstChild();
                while (!n2.isNull())
                {
                    const QDomElement e2 = n2.toElement();
                    if (e2.tagName() != "option")
                    {
                        LOG(VB_GENERAL, LOG_ERR, LOC +
                            "Setting selection contains invalid tags.");
                        return false;
                    }
                    QString display = e2.attribute("display");
                    QString data    = e2.attribute("data");
                    if (data.isEmpty())
                    {
                        LOG(VB_GENERAL, LOG_ERR, LOC +
                            "Setting selection item missing data.");
                        return false;
                    }
                    display = (display.isEmpty()) ? data : display;
                    data_list.push_back(data);
                    display_list.push_back(display);

                    n2 = n2.nextSibling();
                }
            }

            if (MythSetting::kIntegerRange == dtype)
            {
                m["range_min"] = e.attribute("range_min");
                m["range_max"] = e.attribute("range_max");
            }

            QMap<QString,QString>::const_iterator it = m.cbegin();
            for (; it != m.cend(); ++it)
            {
                if ((*it).isEmpty())
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC +
                        QString("Setting has invalid or missing %1 attribute")
                            .arg(it.key()));
                    return false;
                }
            }

            m["default_data"] = e.attribute("default_data");
            m["placeholder_text"] = e.attribute("placeholder_text");

            MythSetting::SettingType stype =
                parse_setting_type(m["setting_type"]);
            if (MythSetting::kInvalidSettingType == stype)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Setting has invalid setting_type attribute.");
                return false;
            }

            long long range_min = m["range_min"].toLongLong();
            long long range_max = m["range_max"].toLongLong();
            if (range_max < range_min)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Setting has invalid range attributes");
                return false;
            }

            auto *s = new MythSetting(
                m["value"], m["default_data"], stype,
                m["label"], m["help_text"], dtype,
                data_list, display_list, range_min, range_max,
                m["placeholder_text"]);

            settings.push_back(s);
        }
        else if (group.isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Unknown element: %1").arg(e.tagName()));
            return false;
        }
        n = n.nextSibling();
    }

    if (mFoundGroup)
        foundGroup = true;

    return true;
#undef LOC
}

bool parse_settings(MythSettingList &settings, const QString &filename,
                    const QString &group)
{
    QDomDocument doc;
    QFile f(filename);

    if (!f.open(QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("parse_settings: Can't open: '%1'")
                .arg(filename));
        return false;
    }

#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    if (!doc.setContent(&f, false, &errorMsg, &errorLine, &errorColumn))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("parse_settings: ") +
            QString("Parsing: %1 at line: %2 column: %3")
                .arg(filename).arg(errorLine).arg(errorColumn) +
            QString("\n\t\t\t%1").arg(errorMsg));
        f.close();
        return false;
    }
#else
    auto parseResult = doc.setContent(&f);
    if (!parseResult)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("parse_settings: ") +
            QString("Parsing: %1 at line: %2 column: %3")
                .arg(filename).arg(parseResult.errorLine)
                .arg(parseResult.errorColumn) +
            QString("\n\t\t\t%1").arg(parseResult.errorMessage));
        f.close();
        return false;
    }
#endif
    f.close();

    settings.clear();
    bool foundGroup = false;
    bool includeAllChildren = group.isEmpty();
    return parse_dom(settings, doc.documentElement(), filename, group,
                     includeAllChildren, foundGroup);
}

bool load_settings(MythSettingList &settings, const QString &hostname)
{
    MSqlQuery query(MSqlQuery::InitCon());

    QString list = extract_query_list(settings, MythSetting::kHost);
    QString qstr =
        "SELECT value, data "
        "FROM settings "
        "WHERE hostname = '" + hostname + "' AND "
        "      value in (" + list + ")";

    if (!list.isEmpty())
    {
        if (!query.exec(qstr))
        {
            MythDB::DBError("HttpConfig::LoadMythSettings() 1", query);
            return false;
        }
        fill_settings(settings, query, MythSetting::kHost);
    }

    list = extract_query_list(settings, MythSetting::kGlobal);
    qstr =
        "SELECT value, data "
        "FROM settings "
        "WHERE hostname IS NULL AND "
        "      value in (" + list + ")";

    if (!list.isEmpty())
    {
        if (!query.exec(qstr))
        {
            MythDB::DBError("HttpConfig::LoadMythSettings() 2", query);
            return false;
        }
        fill_settings(settings, query, MythSetting::kGlobal);
    }

    return true;
}

bool check_settings(MythSettingList &/*database_settings*/,
                    const QMap<QString,QString> &params,
                    QString &result)
{
    QMap<QString,QString>::const_iterator it = params.begin();
    for (; it != params.end(); ++it)
    {
        if (it.key().startsWith("__"))
            continue;

        if (result.isEmpty())
            result += "{ ";
        else
            result += ", ";

        result += QString(R"("%1": "DEBUG: New value for '%2' would be '%3'")")
                          .arg(it.key(), it.key(), *it);
    }

    if (!result.isEmpty())
        result += " }";

    // FIXME, do some actual validation here
    return result.isEmpty();
}
