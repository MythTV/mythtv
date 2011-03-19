#include <QNetworkInterface>
#include <QDomDocument>
#include <QFile>

#include "channelsettings.h" // for ChannelTVFormat::GetFormats()
#include "mythsettings.h"
#include "frequencies.h"
#include "mythcontext.h"
#include "mythdb.h"

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

    MythSettingList::const_iterator it = settings.begin();
    for (; it != settings.end(); ++it)
    {
        const MythSettingGroup *group =
            dynamic_cast<const MythSettingGroup*>(*it);
        if (group)
        {
            list += extract_query_list(group->settings, stype);
            continue;
        }
        const MythSetting *setting = dynamic_cast<const MythSetting*>(*it);
        if (setting && (setting->stype == stype))
            list += QString(",'%1'").arg(setting->value);
    }
    if (!list.isEmpty() && (list[0] == QChar(',')))
        list = list.mid(1);

    return list;
}

static void fill_setting(
    MythSettingBase *sb, const QMap<QString,QString> &map,
    MythSetting::SettingType stype)
{
    const MythSettingGroup *group =
        dynamic_cast<const MythSettingGroup*>(sb);
    if (group)
    {
        MythSettingList::const_iterator it = group->settings.begin();
        for (; it != group->settings.end(); ++it)
            fill_setting(*it, map, stype);
        return;
    }

    MythSetting *setting = dynamic_cast<MythSetting*>(sb);
    if (setting && (setting->stype == stype))
    {
        QMap<QString,QString>::const_iterator it = map.find(setting->value);
        if (it != map.end())
            setting->data = *it;

        bool do_option_check = false;
        if (MythSetting::kLocalIPAddress == setting->dtype)
        {
            setting->data_list.clear();
            setting->display_list.clear();
            QList<QHostAddress> list = QNetworkInterface::allAddresses();
            for (uint i = 0; i < (uint)list.size(); i++)
            {
                if (list[i].toString().contains(":"))
                    continue; // ignore IP6 addresses for now
                setting->data_list.push_back(list[i].toString());
                setting->display_list.push_back(setting->data_list.back());
            }
            if (setting->data_list.isEmpty())
                setting->data_list.push_back("127.0.0.1");
            do_option_check = true;
        }
        else if (MythSetting::kSelect == setting->dtype)
        {
            do_option_check = true;
        }
        else if (MythSetting::kTVFormat == setting->dtype)
        {
            setting->data_list = setting->display_list =
                ChannelTVFormat::GetFormats();
            do_option_check = true;
        }
        else if (MythSetting::kFrequencyTable == setting->dtype)
        {
            setting->data_list.clear();
            for (uint i = 0; chanlists[i].name; i++)
                setting->data_list.push_back(chanlists[i].name);
            setting->display_list = setting->data_list;
            do_option_check = true;
        }

        if (do_option_check)
        {
            if (!setting->data_list.empty() &&
                !setting->data_list.contains(setting->data.toLower(),
                                             Qt::CaseInsensitive))
            {
                bool ok;
                long long idata = setting->data.toLongLong(&ok);
                if (ok)
                {
                    uint sel = 0;
                    for (uint i = setting->data_list.size(); i >= 0; i--)
                    {
                        if (idata < setting->data_list[i].toLongLong())
                            break;
                        sel = i;
                    }
                    setting->data = setting->data_list[sel];
                }
                else
                {
                    setting->data =
                        (setting->data_list.contains(
                            setting->default_data, Qt::CaseInsensitive)) ?
                        setting->default_data : setting->data_list[0];
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

    MythSettingList::const_iterator it = settings.begin();
    for (; it != settings.end(); ++it)
        fill_setting(*it, map, stype);
}

QString MythSettingGroup::ToHTML(uint depth) const
{
    QString ret;

    ret = indent(depth) +
        QString("<div class=\"group\" id=\"%1\">\r\n").arg(unique_label);
    if (!human_label.isEmpty())
    {
        ret += indent(depth+1) + QString("<h%1 class=\"config\">%2</h%3>\r\n")
            .arg(depth+1).arg(human_label).arg(depth+1);
    }

    MythSettingList::const_iterator it = settings.begin();
    for (; it != settings.end(); ++it)
        ret += (*it)->ToHTML(depth+1);

    ret += indent(depth) +"</div>";

    return ret;
}

QString MythSetting::ToHTML(uint level) const
{
    QString ret = indent(level) +
        QString("<div class=\"setting\" id=\"%1_div\">\r\n").arg(value);

    switch (dtype)
    {
        case kInteger:
        case kUnsignedInteger:
        case kIntegerRange:
        case kFloat:
        case kComboBox:
        case kIPAddress:
        case kString:
        case kTimeOfDay:
        case kOther:
            ret += indent(level) +
                "<div class=\"setting_label\">" + label + "</div>\r\n";
            ret += indent(level) +
                QString("<input name=\"%1\" id=\"%2_input\" type=\"text\""
                        " value=\"%3\"/>\r\n")
                .arg(value).arg(value).arg(data);
            ret += indent(level) +
                QString("<div style=\"display:none;"
                        "position:absolute;left:-4000px\" "
                        "id=\"%1_default\">%2</div>\r\n")
                .arg(value).arg(default_data);
            break;
        case kCheckBox:
            ret += indent(level) +
                "<div class=\"setting_label\">" + label + "</div>\r\n";
            ret += indent(level) +
                QString("<input name=\"%1\" id=\"%2_input\" type=\"checkbox\""
                        " value=\"1\" %3/>\r\n")
                .arg(value).arg(value).arg((data.toUInt()) ? "checked" : "");
            ret += indent(level) +
                QString("<div style=\"display:none;"
                        "position:absolute;left:-4000px\" "
                        "id=\"%1_default\">%2</div>\r\n")
                .arg(value).arg(default_data);
            break;
        case kLocalIPAddress:
        case kTVFormat:
        case kFrequencyTable:
        case kSelect:
            ret +=  indent(level) +
                "<div class=\"setting_label\">" + label + "</div>\r\n";
            ret +=  indent(level) +
                QString("<select name=\"%1\" id=\"%2_input\">\r\n")
                .arg(value).arg(value);
            for (uint i = 0; (i < (uint)data_list.size()) &&
                     (i < (uint)display_list.size()); i++)
            {
                ret += indent(level+1) +
                    QString("<option value=\"%1\" %2>%3</option>\r\n")
                    .arg(data_list[i])
                    .arg((data_list[i].toLower() == data.toLower()) ?
                         "selected" : "")
                    .arg(display_list[i]);
            }
            ret += indent(level) + "</select>\r\n";
            ret += indent(level) +
                QString("<div style=\"display:none;"
                        "position:absolute;left:-4000px\" "
                        "id=\"%1_default\">%2</div>\r\n")
                .arg(value).arg(default_data);
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
    VERBOSE(VB_IMPORTANT, QString("Unknown type: %1").arg(str));
    return MythSetting::kInvalidDataType;
}

bool parse_dom(MythSettingList &settings, const QDomElement &element,
               const QString &filename)
{
#define LOC QString("parse_dom(%1@~%2), error: ") \
            .arg(filename).arg(e.lineNumber())

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
            QString human_label  = e.attribute("human_label");
            QString unique_label = e.attribute("unique_label");
            QString ecma_script  = e.attribute("ecma_script");

            MythSettingGroup *g = new MythSettingGroup(
                human_label, unique_label, ecma_script);

            if (e.hasChildNodes() && !parse_dom(g->settings, e, filename))
                return false;

            settings.push_back(g);
        }
        else if (e.tagName() == "setting")
        {
            QMap<QString,QString> m;
            m["value"]        = e.attribute("value");
            m["setting_type"] = e.attribute("setting_type");
            m["label"]        = e.attribute("label");
            m["help_text"]    = e.attribute("help_text");
            m["data_type"]    = e.attribute("data_type");

            MythSetting::DataType dtype = parse_data_type(m["data_type"]);
            if (MythSetting::kInvalidDataType == dtype)
            {
                VERBOSE(VB_IMPORTANT, LOC +
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
                    VERBOSE(VB_IMPORTANT, LOC +
                            "Setting missing selection items.");
                    return false;
                }

                QDomNode n2 = e.firstChild();
                while (!n2.isNull())
                {
                    const QDomElement e2 = n2.toElement();
                    if (e2.tagName() != "option")
                    {
                        VERBOSE(VB_IMPORTANT, LOC +
                                "Setting selection contains invalid tags.");
                        return false;
                    }
                    QString display = e2.attribute("display");
                    QString data    = e2.attribute("data");
                    if (data.isEmpty())
                    {
                        VERBOSE(VB_IMPORTANT, LOC +
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

            QMap<QString,QString>::const_iterator it = m.begin();
            for (; it != m.end(); ++it)
            {
                if ((*it).isEmpty())
                {
                    VERBOSE(VB_IMPORTANT, LOC +
                            QString("Setting has invalid or missing "
                                    "%1 attribute")
                            .arg(it.key()));
                    return false;
                }
            }

            m["default_data"] = e.attribute("default_data");

            MythSetting::SettingType stype =
                parse_setting_type(m["setting_type"]);
            if (MythSetting::kInvalidSettingType == stype)
            {
                VERBOSE(VB_IMPORTANT, LOC +
                        "Setting has invalid setting_type attribute.");
                return false;
            }

            long long range_min = m["range_min"].toLongLong();
            long long range_max = m["range_max"].toLongLong();
            if (range_max < range_min)
            {
                VERBOSE(VB_IMPORTANT, LOC +
                        "Setting has invalid range attributes");
                return false;
            }

            MythSetting *s = new MythSetting(
                m["value"], m["default_data"], stype,
                m["label"], m["help_text"], dtype,
                data_list, display_list, range_min, range_max);

            settings.push_back(s);
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC +
                    QString("Unknown element: %1").arg(e.tagName()));
            return false;
        }
        n = n.nextSibling();
    }
    return true;
#undef LOC
}

bool parse_settings(MythSettingList &settings, const QString &filename)
{
    QDomDocument doc;
    QFile f(filename);

    if (!f.open(QIODevice::ReadOnly))
    {
        VERBOSE(VB_IMPORTANT, QString("parse_settings: Can't open: '%1'")
                .arg(filename));
        return false;
    }

    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    if (!doc.setContent(&f, false, &errorMsg, &errorLine, &errorColumn))
    {
        VERBOSE(VB_IMPORTANT, QString("parse_settings: ") +
                QString("Parsing: %1 at line: %2 column: %3")
                .arg(filename).arg(errorLine).arg(errorColumn) +
                QString("\n\t\t\t%1").arg(errorMsg));
        f.close();
        return false;
    }
    f.close();

    settings.clear();
    return parse_dom(settings, doc.documentElement(), filename);
}

bool load_settings(MythSettingList &settings, const QString &hostname)
{
    MSqlQuery query(MSqlQuery::InitCon());

    QString list = extract_query_list(settings, MythSetting::kFile);
    if (!list.isEmpty())
    {
        DatabaseParams params;
        bool ok = MythDB::LoadDatabaseParamsFromDisk(params, true);
        if (!ok)
            return false;

        QMap<QString,QString> map;
        map["host"]                = params.dbHostName;
        map["port"] = QString::number(params.dbPort);
        map["ping"] = QString::number(params.dbHostPing);
        map["database"]            = params.dbName;
        map["user"]                = params.dbUserName;
        map["password"]            = params.dbPassword;
        map["uniqueid"]            = params.localHostName;
        map["wol_enabled"]         =
            QString::number(params.wolEnabled);
        map["wol_reconnect_count"] =
            QString::number(params.wolReconnect);
        map["wol_retry_count "]    =
            QString::number(params.wolRetry);
        map["wol_command"]         = params.wolCommand;

        MythSettingList::const_iterator it = settings.begin();
        for (; it != settings.end(); ++it)
            fill_setting(*it, map, MythSetting::kFile);
    }

    list = extract_query_list(settings, MythSetting::kHost);
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

bool check_settings(MythSettingList &database_settings,
                    const QMap<QString,QString> &params)
{
    // TODO
}
