// POSIX headers
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

// Qt headers
#include <qdom.h>
#include <qstring.h>
#include <qregexp.h>
#include <qfile.h>
#include <qfileinfo.h>

#include <iostream>
using namespace std;

// libmyth headers
#include "mythcontext.h"
#include "mythdbcon.h"

// filldata headers
#include "fillutil.h"
#include "icondata.h"

const char * const IM_DOC_TAG = "iconmappings";

const char * const IM_CS_TO_NET_TAG = "callsigntonetwork";
const char * const IM_CS_TAG = "callsign";

const char * const IM_NET_TAG = "network";

const char * const IM_NET_TO_URL_TAG = "networktourl";
const char * const IM_NET_URL_TAG = "url";

const char * const BASEURLMAP_START = "mythfilldatabase.urlmap.";

const char * const IM_BASEURL_TAG = "baseurl";
const char * const IM_BASE_STUB_TAG = "stub";

class DOMException
{
  private:
    QString message;

  protected:
    void setMessage(const QString &mes)
    {
        message = mes;
    }

  public:
    DOMException() : message("Unknown DOMException") {}
    virtual ~DOMException() {}
    DOMException(const QString &mes) : message(mes) {}
    QString getMessage()
    {
        return message;
    }
};

class DOMBadElementConversion : public DOMException
{
  public:
    DOMBadElementConversion()
    {
        setMessage("Unknown DOMBadElementConversion");
    }
    DOMBadElementConversion(const QString &mes) : DOMException(mes) {}
    DOMBadElementConversion(const QDomNode &node)
    {
        setMessage(QString("Unable to convert node: '%1' to QDomElement.")
                .arg(node.nodeName()));
    }
};

class DOMUnknownChildElement : public DOMException
{
  public:
    DOMUnknownChildElement()
    {
        setMessage("Unknown DOMUnknownChildElement");
    }
    DOMUnknownChildElement(const QString &mes) : DOMException(mes) {}
    DOMUnknownChildElement(const QDomElement &e, QString child_name)
    {
        setMessage(QString("Unknown child element '%1' of: '%2'")
                .arg(child_name)
                .arg(e.tagName()));
    }
};

QDomElement nodeToElement(QDomNode &node)
{
    QDomElement retval = node.toElement();
    if (retval.isNull())
    {
        throw DOMBadElementConversion(node);
    }
    return retval;
}

QString expandURLString(const QString &url)
{
    QRegExp expandtarget("\\[([^\\]]+)\\]");
    QString retval = url;

    int found_at = 0;
    int start_index = 0;
    while (found_at != -1)
    {
        found_at = expandtarget.search(retval, start_index);
        if (found_at != -1)
        {
            QString no_mapping("no_URL_mapping");
            QString search_string = expandtarget.cap(1);
            QString expanded_text = gContext->GetSetting(
                    QString(BASEURLMAP_START) + search_string, no_mapping);
            if (expanded_text != no_mapping)
            {
                retval.replace(found_at, expandtarget.matchedLength(),
                        expanded_text);
            }
            else
            {
                start_index = found_at + expandtarget.matchedLength();
            }
        }
    }

    return retval;
}

QString getNamedElementText(const QDomElement &e,
        const QString &child_element_name)
{
    QDomNode child_node = e.namedItem(child_element_name);
    if (child_node.isNull())
    {
        throw DOMUnknownChildElement(e, child_element_name);
    }
    QDomElement element = nodeToElement(child_node);
    return element.text();
}

void RunSimpleQuery(const QString &query)
{
    MSqlQuery q(MSqlQuery::InitCon());
    if (!q.exec(query))
        MythContext::DBError("RunSimpleQuery ", q);
}

void IconData::UpdateSourceIcons(int sourceid)
{
    VERBOSE(VB_GENERAL,
            QString("Updating icons for sourceid: %1").arg(sourceid));

    QString fileprefix = SetupIconCacheDirectory();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT ch.chanid, nim.url "
            "FROM (channel ch, callsignnetworkmap csm) "
            "RIGHT JOIN networkiconmap nim ON csm.network = nim.network "
            "WHERE ch.callsign = csm.callsign AND "
            "(icon = :NOICON OR icon = '') AND ch.sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);
    query.bindValue(":NOICON", "none");

    if (!query.exec())
        MythContext::DBError("Looking for icons to fetch", query);

    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            QString icon_url = expandURLString(query.value(1).toString());
            QFileInfo qfi(icon_url);
            QFile localfile(fileprefix + "/" + qfi.fileName());
            if (!localfile.exists())
            {
                QString icon_get_command =
                    QString("wget --timestamping --directory-prefix=%1 '%2'")
                            .arg(fileprefix).arg(icon_url);

                if ((print_verbose_messages & VB_GENERAL) == 0)
                    icon_get_command += " > /dev/null 2> /dev/null";
                VERBOSE(VB_GENERAL,
                        QString("Attempting to fetch icon with: %1")
                                .arg(icon_get_command));

                system(icon_get_command);
            }

            if (localfile.exists())
            {
                int chanid = query.value(0).toInt();
                if (!quiet)
                {
                    QString m = QString("Updating channel icon for chanid: %1")
                        .arg(chanid);
                    cout << m << endl;
                }
                MSqlQuery icon_update_query(MSqlQuery::InitCon());
                icon_update_query.prepare("UPDATE channel SET icon = :ICON "
                        "WHERE chanid = :CHANID AND sourceid = :SOURCEID");
                icon_update_query.bindValue(":ICON", localfile.name());
                icon_update_query.bindValue(":CHANID", query.value(0).toInt());
                icon_update_query.bindValue(":SOURCEID", sourceid);

                if (!icon_update_query.exec())
                    MythContext::DBError("Setting the icon file name",
                            icon_update_query);
            }
            else
            {
                cerr << QString(
                        "Error retrieving icon from '%1' to file '%2'")
                        .arg(icon_url)
                        .arg(localfile.name())
                     << endl;
            }
        }
    }
}

void IconData::ImportIconMap(const QString &filename)
{
    if (!quiet)
    {
        QString msg = QString("Importing icon mapping from %1...")
                .arg(filename);
        cout << msg << endl;
    }
    QFile xml_file;

    if (dash_open(xml_file, filename, IO_ReadOnly))
    {
        QDomDocument doc;
        QString de_msg;
        int de_ln = 0;
        int de_column = 0;
        if (doc.setContent(&xml_file, false, &de_msg, &de_ln, &de_column))
        {
            MSqlQuery nm_query(MSqlQuery::InitCon());
            nm_query.prepare("REPLACE INTO networkiconmap(network, url) "
                    "VALUES(:NETWORK, :URL)");
            MSqlQuery cm_query(MSqlQuery::InitCon());
            cm_query.prepare("REPLACE INTO callsignnetworkmap(callsign, "
                    "network) VALUES(:CALLSIGN, :NETWORK)");
            MSqlQuery su_query(MSqlQuery::InitCon());
            su_query.prepare("UPDATE settings SET data = :URL "
                    "WHERE value = :STUBNAME");
            MSqlQuery si_query(MSqlQuery::InitCon());
            si_query.prepare("INSERT INTO settings(value, data) "
                    "VALUES(:STUBNAME, :URL)");

            QDomElement element = doc.documentElement();

            QDomNode node = element.firstChild();
            while (!node.isNull())
            {
                try
                {
                    QDomElement e = nodeToElement(node);
                    if (e.tagName() == IM_NET_TO_URL_TAG)
                    {
                        QString net = getNamedElementText(e, IM_NET_TAG);
                        QString u = getNamedElementText(e, IM_NET_URL_TAG);

                        nm_query.bindValue(":NETWORK", net.stripWhiteSpace());
                        nm_query.bindValue(":URL", u.stripWhiteSpace());
                        if (!nm_query.exec())
                            MythContext::DBError(
                                    "Inserting network->url mapping", nm_query);
                    }
                    else if (e.tagName() == IM_CS_TO_NET_TAG)
                    {
                        QString cs = getNamedElementText(e, IM_CS_TAG);
                        QString net = getNamedElementText(e, IM_NET_TAG);

                        cm_query.bindValue(":CALLSIGN", cs.stripWhiteSpace());
                        cm_query.bindValue(":NETWORK", net.stripWhiteSpace());
                        if (!cm_query.exec())
                            MythContext::DBError("Inserting callsign->network "
                                    "mapping", cm_query);
                    }
                    else if (e.tagName() == IM_BASEURL_TAG)
                    {
                        MSqlQuery *qr = &si_query;

                        QString st(BASEURLMAP_START);
                        st += getNamedElementText(e, IM_BASE_STUB_TAG);
                        QString u = getNamedElementText(e, IM_NET_URL_TAG);

                        MSqlQuery qc(MSqlQuery::InitCon());
                        qc.prepare("SELECT COUNT(*) FROM settings "
                                "WHERE value = :STUBNAME");
                        qc.bindValue(":STUBNAME", st);
                        qc.exec();
                        if (qc.isActive() && qc.size() > 0)
                        {
                            qc.first();
                            if (qc.value(0).toInt() != 0)
                            {
                                qr = &su_query;
                            }
                        }

                        qr->bindValue(":STUBNAME", st);
                        qr->bindValue(":URL", u);

                        if (!qr->exec())
                            MythContext::DBError(
                                    "Inserting callsign->network mapping", *qr);
                    }
                }
                catch (DOMException &e)
                {
                    cerr << QString("Error while processing %1: %2")
                            .arg(node.nodeName())
                            .arg(e.getMessage())
                         << endl;
                }
                node = node.nextSibling();
            }
        }
        else
        {
            cerr << QString(
                    "Error unable to set document content: %1:%2c%3 %4")
                    .arg(filename)
                    .arg(de_ln)
                    .arg(de_column)
                    .arg(de_msg)
                 << endl;
        }
    }
    else
    {
        cerr << QString("Error unable to open '%1' for reading.")
                .arg(filename)
             << endl;
    }
}

void IconData::ExportIconMap(const QString &filename)
{
    if (!quiet)
    {
        cout << QString("Exporting icon mapping to %1...").arg(filename)
             << endl;
    }
    QFile xml_file(filename);
    if (dash_open(xml_file, filename, IO_WriteOnly))
    {
        QTextStream os(&xml_file);
        os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        os << "<!-- generated by mythfilldatabase -->\n";

        QDomDocument iconmap;
        QDomElement roote = iconmap.createElement(IM_DOC_TAG);

        MSqlQuery query(MSqlQuery::InitCon());
        query.exec("SELECT * FROM callsignnetworkmap ORDER BY callsign;");

        if (query.isActive() && query.size() > 0)
        {
            while (query.next())
            {
                QDomElement cs2nettag = iconmap.createElement(IM_CS_TO_NET_TAG);
                QDomElement cstag = iconmap.createElement(IM_CS_TAG);
                QDomElement nettag = iconmap.createElement(IM_NET_TAG);
                QDomText cs_text = iconmap.createTextNode(
                        query.value(1).toString());
                QDomText net_text = iconmap.createTextNode(
                        query.value(2).toString());

                cstag.appendChild(cs_text);
                nettag.appendChild(net_text);

                cs2nettag.appendChild(cstag);
                cs2nettag.appendChild(nettag);

                roote.appendChild(cs2nettag);
            }
        }

        query.exec("SELECT * FROM networkiconmap ORDER BY network;");
        if (query.isActive() && query.size() > 0)
        {
            while (query.next())
            {
                QDomElement net2urltag = iconmap.createElement(
                        IM_NET_TO_URL_TAG);
                QDomElement nettag = iconmap.createElement(IM_NET_TAG);
                QDomElement urltag = iconmap.createElement(IM_NET_URL_TAG);
                QDomText net_text = iconmap.createTextNode(
                        query.value(1).toString());
                QDomText url_text = iconmap.createTextNode(
                        query.value(2).toString());

                nettag.appendChild(net_text);
                urltag.appendChild(url_text);

                net2urltag.appendChild(nettag);
                net2urltag.appendChild(urltag);

                roote.appendChild(net2urltag);
            }
        }

        query.prepare("SELECT value,data FROM settings WHERE value "
                "LIKE :URLMAP");
        query.bindValue(":URLMAP", QString(BASEURLMAP_START) + "%");
        query.exec();
        if (query.isActive() && query.size() > 0)
        {
            QRegExp baseax("\\.([^\\.]+)$");
            while (query.next())
            {
                QString base_stub = query.value(0).toString();
                if (baseax.search(base_stub) != -1)
                {
                    base_stub = baseax.cap(1);
                }

                QDomElement baseurltag = iconmap.createElement(IM_BASEURL_TAG);
                QDomElement stubtag = iconmap.createElement(
                        IM_BASE_STUB_TAG);
                QDomElement urltag = iconmap.createElement(IM_NET_URL_TAG);
                QDomText base_text = iconmap.createTextNode(base_stub);
                QDomText url_text = iconmap.createTextNode(
                        query.value(1).toString());

                stubtag.appendChild(base_text);
                urltag.appendChild(url_text);

                baseurltag.appendChild(stubtag);
                baseurltag.appendChild(urltag);

                roote.appendChild(baseurltag);
            }
        }

        iconmap.appendChild(roote);
        iconmap.save(os, 4);
    }
    else
    {
        cerr << QString("Error unable to open '%1' for writing.") << endl;
    }
}

void IconData::ResetIconMap(bool reset_icons)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM settings WHERE value LIKE :URLMAPLIKE");
    query.bindValue(":URLMAPLIKE", QString(BASEURLMAP_START) + '%');
    if (!query.exec())
        MythContext::DBError("ResetIconMap", query);

    RunSimpleQuery("TRUNCATE TABLE callsignnetworkmap;");
    RunSimpleQuery("TRUNCATE TABLE networkiconmap");

    if (reset_icons)
    {
        RunSimpleQuery("UPDATE channel SET icon = 'none'");
    }
}
