// C++ headers
#include <iostream>
using namespace std;

// C headers
#include <cstdlib>

// POSIX headers
#include <unistd.h>

// Qt headers
#include <QCoreApplication>
#include <QDomElement>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>

// MythTV headers
#include "mythcorecontext.h"
#include "mythtimer.h"
#include "mythdb.h"

// filldata headers
#include "fillutil.h"
#include "icondata.h"

#define LOC      QString("IconData: ")
#define LOC_WARN QString("IconData, Warning: ")
#define LOC_ERR  QString("IconData, Error: ")

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
    QString getMessage() const { return message; }
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

static QDomElement nodeToElement(QDomNode &node)
{
    QDomElement retval = node.toElement();
    if (retval.isNull())
    {
        throw DOMBadElementConversion(node);
    }
    return retval;
}

static QString expandURLString(const QString &url)
{
    QRegExp expandtarget("\\[([^\\]]+)\\]");
    QString retval = url;

    int found_at = 0;
    int start_index = 0;
    while (found_at != -1)
    {
        found_at = expandtarget.indexIn(retval, start_index);
        if (found_at != -1)
        {
            QString no_mapping("no_URL_mapping");
            QString search_string = expandtarget.cap(1);
            QString expanded_text = gCoreContext->GetSetting(
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

static QString getNamedElementText(const QDomElement &e,
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

static void RunSimpleQuery(const QString &query)
{
    MSqlQuery q(MSqlQuery::InitCon());
    if (!q.exec(query))
        MythDB::DBError("RunSimpleQuery ", q);
}


IconData::~IconData()
{
    MythHttpPool::GetSingleton()->RemoveListener(this);
}

void IconData::UpdateSourceIcons(uint sourceid)
{
    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Updating icons for sourceid: %1").arg(sourceid));

    QString fileprefix = SetupIconCacheDirectory();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT ch.chanid, nim.url "
        "FROM (channel ch, callsignnetworkmap csm) "
        "RIGHT JOIN networkiconmap nim ON csm.network = nim.network "
        "WHERE ch.callsign = csm.callsign AND "
        "      icon = '' AND "
        "      ch.sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec())
    {
        MythDB::DBError("Looking for icons to fetch", query);
        return;
    }

    unsigned int count = 0;
    while (query.next())
    {
        count++;

        QString icon_url = expandURLString(query.value(1).toString());
        QFileInfo qfi(icon_url);
        QFile localfile(fileprefix + "/" + qfi.fileName());

        if (!localfile.exists() || 0 == localfile.size())
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Attempting to fetch icon at '%1'") .arg(icon_url));

            FI fi;
            fi.filename = localfile.fileName();
            fi.chanid = query.value(0).toUInt();

            bool add_request = false;
            {
                QMutexLocker locker(&m_u2fl_lock);
                add_request = m_u2fl[icon_url].empty();
                m_u2fl[icon_url].push_back(fi);
            }

            if (add_request)
                MythHttpPool::GetSingleton()->AddUrlRequest(icon_url, this);

            // HACK -- begin
            // This hack is needed because we don't enter the event loop
            // before running this code via qApp->exec()
            qApp->processEvents();
            // HACK -- end
        }
    }

    MythTimer tm; tm.start();
    while (true)
    {
        // HACK -- begin
        // This hack is needed because we don't enter the event loop
        // before running this code via qApp->exec()
        qApp->processEvents();
        // HACK -- end

        QMutexLocker locker(&m_u2fl_lock);
        if (m_u2fl.empty())
            break;

        if ((uint)tm.elapsed() > (count * 500) + 2000)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                "Timed out waiting for some icons to download, "
                "you may wish to try again later.");
            break;
        }
    }
}

void IconData::Update(
    QHttp::Error      error,
    const QString    &error_str,
    const QUrl       &url,
    uint              http_status_id,
    const QString    &http_status_str,
    const QByteArray &data)
{
    bool http_error = false;
    if (QHttp::NoError != error)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("fetching '%1'\n\t\t\t%2")
                .arg(url.toString()).arg(error_str));
        http_error = true;
    }

    if (!data.size())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Did not get any data from '%1'")
                .arg((url.toString())));
        http_error = true;
    }

    FIL fil;
    {
        QMutexLocker locker(&m_u2fl_lock);
        Url2FIL::iterator it = m_u2fl.find(url);
        if (it == m_u2fl.end())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Programmer Error, got data for '%1',"
                        "but have no record of requesting it.")
                    .arg(url.toString()));
            return;
        }

        fil = *it;
        m_u2fl.erase(it);
    }

    while (!http_error && !fil.empty())
    {
        Save(fil.back(), data);
        fil.pop_back();
    }
}

bool IconData::Save(const FI &fi, const QByteArray &data)
{
    QFile file(fi.filename);
    if (!file.open(QIODevice::WriteOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
             QString("Failed to open '%1' for writing") .arg(fi.filename));
        return false;
    }

    if (file.write(data) < data.size())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to write icon data to '%1'") .arg(fi.filename));
        file.remove();
        return false;
    }

    file.flush();

    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Updating channel icon for chanid: %1").arg(fi.chanid));

    MSqlQuery update(MSqlQuery::InitCon());
    update.prepare(
        "UPDATE channel SET icon = :ICON "
        "WHERE chanid = :CHANID");

    update.bindValue(":ICON",     fi.filename);
    update.bindValue(":CHANID",   fi.chanid);

    if (!update.exec())
    {
        MythDB::DBError("Setting the icon file name", update);
        return false;
    }

    return true;
}

void IconData::ImportIconMap(const QString &filename)
{
    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Importing icon mapping from %1...").arg(filename));

    QFile xml_file;

    if (dash_open(xml_file, filename, QIODevice::ReadOnly))
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

                        nm_query.bindValue(":NETWORK", net.trimmed());
                        nm_query.bindValue(":URL", u.trimmed());
                        if (!nm_query.exec())
                            MythDB::DBError(
                                    "Inserting network->url mapping", nm_query);
                    }
                    else if (e.tagName() == IM_CS_TO_NET_TAG)
                    {
                        QString cs = getNamedElementText(e, IM_CS_TAG);
                        QString net = getNamedElementText(e, IM_NET_TAG);

                        cm_query.bindValue(":CALLSIGN", cs.trimmed());
                        cm_query.bindValue(":NETWORK", net.trimmed());
                        if (!cm_query.exec())
                            MythDB::DBError("Inserting callsign->network "
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
                        if (qc.exec() && qc.next())
                        {
                            if (qc.value(0).toInt() != 0)
                            {
                                qr = &su_query;
                            }
                        }

                        qr->bindValue(":STUBNAME", st);
                        qr->bindValue(":URL", u);

                        if (!qr->exec())
                            MythDB::DBError(
                                    "Inserting callsign->network mapping", *qr);
                    }
                }
                catch (DOMException &e)
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC +
                        QString("while processing %1: %2")
                            .arg(node.nodeName()).arg(e.getMessage()));
                }
                node = node.nextSibling();
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("unable to set document content: %1:%2c%3 %4")
                    .arg(filename).arg(de_ln).arg(de_column).arg(de_msg));
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("unable to open '%1' for reading.").arg(filename));
    }
}

void IconData::ExportIconMap(const QString &filename)
{
    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Exporting icon mapping to '%1'").arg(filename));

    QFile xml_file(filename);
    if (dash_open(xml_file, filename, QIODevice::WriteOnly))
    {
        QTextStream os(&xml_file);
        os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        os << "<!-- generated by mythfilldatabase -->\n";

        QDomDocument iconmap;
        QDomElement roote = iconmap.createElement(IM_DOC_TAG);

        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("SELECT * FROM callsignnetworkmap ORDER BY callsign;");
        if (query.exec())
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

        query.prepare("SELECT * FROM networkiconmap ORDER BY network;");
        if (query.exec())
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
        if (query.exec())
        {
            QRegExp baseax("\\.([^\\.]+)$");
            while (query.next())
            {
                QString base_stub = query.value(0).toString();
                if (baseax.indexIn(base_stub) != -1)
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
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("unable to open '%1' for writing.").arg(filename));
    }
}

void IconData::ResetIconMap(bool reset_icons)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM settings WHERE value LIKE :URLMAPLIKE");
    query.bindValue(":URLMAPLIKE", QString(BASEURLMAP_START) + '%');
    if (!query.exec())
        MythDB::DBError("ResetIconMap", query);

    RunSimpleQuery("TRUNCATE TABLE callsignnetworkmap;");
    RunSimpleQuery("TRUNCATE TABLE networkiconmap");

    if (reset_icons)
    {
        RunSimpleQuery("UPDATE channel SET icon = ''");
    }
}
