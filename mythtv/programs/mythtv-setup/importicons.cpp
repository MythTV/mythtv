#include <QCoreApplication>
#include <QRegExp>
#include <QBuffer>
#include <QDir>
#include <QFileInfo>

#include "mythdb.h"
#include "mythdirs.h"
#include "mythlogging.h"
#include "importicons.h"
#include "mythdate.h"

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#include "httpcomms.h"
#endif

// MythUI
#include "mythuitext.h"
#include "mythuibutton.h"
#include "mythuibuttonlist.h"
#include "mythuitextedit.h"
#include "mythdialogbox.h"
#include "mythprogressdialog.h"

ImportIconsWizard::ImportIconsWizard(MythScreenStack *parent, bool fRefresh,
                                     const QString &channelname)
                  :MythScreenType(parent, "ChannelIconImporter"),
    m_strChannelname(channelname), m_fRefresh(fRefresh),
    m_nMaxCount(0),                m_nCount(0),
    m_missingMaxCount(0),          m_missingCount(0),
    m_url("http://services.mythtv.org/channel-icon/"), m_progressDialog(NULL),
    m_iconsList(NULL),             m_manualEdit(NULL),
    m_nameText(NULL),              m_manualButton(NULL),
    m_skipButton(NULL),            m_statusText(NULL),
    m_preview(NULL),               m_previewtitle(NULL)
{
    m_strChannelname.detach();
    LOG(VB_GENERAL, LOG_INFO,
        QString("Fetch Icons for channel %1").arg(m_strChannelname));

    m_popupStack = GetMythMainWindow()->GetStack("popup stack");

    m_tmpDir = QDir(QString("%1/icontmp").arg(GetConfDir()));

    if (!m_tmpDir.exists())
        m_tmpDir.mkpath(m_tmpDir.absolutePath());
}

ImportIconsWizard::~ImportIconsWizard()
{
    if (m_tmpDir.exists())
    {
        QStringList files = m_tmpDir.entryList();
        for (int i = 0; i < files.size(); ++i)
        {
            m_tmpDir.remove(files.at(i));
        }
        m_tmpDir.rmpath(m_tmpDir.absolutePath());
    }
}

bool ImportIconsWizard::Create()
{
    if (!initialLoad(m_strChannelname))
        return false;


    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("config-ui.xml", "iconimport", this);

    if (!foundtheme)
        return false;

    m_iconsList = dynamic_cast<MythUIButtonList *>(GetChild("icons"));
    m_manualEdit = dynamic_cast<MythUITextEdit *>(GetChild("manualsearch"));
    m_nameText = dynamic_cast<MythUIText *>(GetChild("name"));
    m_manualButton = dynamic_cast<MythUIButton *>(GetChild("search"));
    m_skipButton = dynamic_cast<MythUIButton *>(GetChild("skip"));
    m_statusText = dynamic_cast<MythUIText *>(GetChild("status"));
    m_preview = dynamic_cast<MythUIImage *>(GetChild("preview"));
    m_previewtitle = dynamic_cast<MythUIText *>(GetChild("previewtitle"));

    if (!m_iconsList || !m_manualEdit || !m_nameText || !m_manualButton ||
        !m_skipButton || !m_statusText)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Unable to load window 'iconimport', missing required element(s)");
        return false;
    }

    m_nameText->SetEnabled(false);

    m_nameText->SetHelpText(tr("Name of the icon file"));
    m_iconsList->SetHelpText(tr("List of possible icon files"));
    m_manualEdit->SetHelpText(tr("Enter text here for the manual search"));
    m_manualButton->SetHelpText(tr("Manually search for the text"));
    m_skipButton->SetHelpText(tr("Skip this icon"));

    connect(m_manualButton, SIGNAL(Clicked()), SLOT(manualSearch()));
    connect(m_skipButton, SIGNAL(Clicked()), SLOT(skip()));
    connect(m_iconsList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            SLOT(menuSelection(MythUIButtonListItem *)));
    connect(m_iconsList, SIGNAL(itemSelected(MythUIButtonListItem *)),
            SLOT(itemChanged(MythUIButtonListItem *)));

    BuildFocusList();

    enableControls(STATE_NORMAL);

    return true;
}

void ImportIconsWizard::Init()
{
    if (m_nMaxCount > 0)
    {
        m_missingIter = m_missingEntries.begin();
        if (!doLoad())
        {
            if (!m_strMatches.isEmpty())
                askSubmit(m_strMatches);
        }
    }
}

void ImportIconsWizard::enableControls(dialogState state, bool selectEnabled)
{
    switch (state)
    {
        case STATE_NORMAL:
            if (m_missingCount + 1 == m_missingMaxCount)
                m_skipButton->SetText(tr("Finish"));
            else
                m_skipButton->SetText(tr("Skip"));
            m_skipButton->SetEnabled(true);
            m_nameText->SetEnabled(true);
            m_iconsList->SetEnabled(true);
            m_manualEdit->SetEnabled(true);
            m_manualButton->SetEnabled(true);
            break;
        case STATE_SEARCHING:
            m_skipButton->SetEnabled(false);
            m_manualButton->SetEnabled(false);
            m_iconsList->SetEnabled(false);
            m_iconsList->Reset();
            m_manualEdit->Reset();
            break;
        case STATE_DISABLED:
            m_skipButton->SetEnabled(false);
            m_manualButton->SetEnabled(false);
            m_iconsList->SetEnabled(false);
            m_iconsList->Reset();
            m_nameText->SetEnabled(false);
            m_nameText->Reset();
            m_manualEdit->SetEnabled(false);
            m_manualEdit->Reset();
            SetFocusWidget(m_iconsList);
            break;
    }
}

void ImportIconsWizard::manualSearch()
{
    QString str = m_manualEdit->GetText();
    if (!search(escape_csv(str)))
        m_statusText->SetText(tr("No matches found for %1") .arg(str));
    else
        m_statusText->Reset();
}

void ImportIconsWizard::skip()
{
    m_missingCount++;
    m_missingIter++;

    if (!doLoad())
    {
        if (!m_strMatches.isEmpty())
            askSubmit(m_strMatches);
        else
            Close();
    }
}

void ImportIconsWizard::menuSelection(MythUIButtonListItem *item)
{
    if (!item)
        return;

    SearchEntry entry = qVariantValue<SearchEntry>(item->GetData());

    LOG(VB_GENERAL, LOG_INFO, QString("Menu Selection: %1 %2 %3")
            .arg(entry.strID) .arg(entry.strName) .arg(entry.strLogo));

    enableControls(STATE_SEARCHING);

    CSVEntry entry2 = (*m_missingIter);
    m_strMatches += QString("%1,%2,%3,%4,%5,%6,%7,%8,%9\n")
                            .arg(escape_csv(entry.strID))
                            .arg(escape_csv(entry2.strName))
                            .arg(escape_csv(entry2.strXmlTvId))
                            .arg(escape_csv(entry2.strCallsign))
                            .arg(escape_csv(entry2.strTransportId))
                            .arg(escape_csv(entry2.strAtscMajorChan))
                            .arg(escape_csv(entry2.strAtscMinorChan))
                            .arg(escape_csv(entry2.strNetworkId))
                            .arg(escape_csv(entry2.strServiceId));

    if (checkAndDownload(entry.strLogo, entry2.strChanId))
    {
        m_statusText->SetText(tr("Icon for %1 was downloaded successfully.")
                                    .arg(entry2.strName));
    }
    else
    {
        m_statusText->SetText(tr("Failed to download the icon for %1.")
                                    .arg(entry2.strName));
    }

    if (m_missingMaxCount > 1)
    {
        m_missingCount++;
        m_missingIter++;
        if (!doLoad())
        {
            if (!m_strMatches.isEmpty())
                askSubmit(m_strMatches);
            else
                Close();
        }
    }
    else
    {
        enableControls(STATE_DISABLED);

        SetFocusWidget(m_iconsList);
        if (!m_strMatches.isEmpty())
            askSubmit(m_strMatches);
        else
            Close();
    }

}

void ImportIconsWizard::itemChanged(MythUIButtonListItem *item)
{
    if (!item)
        return;

    if (m_preview)
    {
        m_preview->Reset();
        QString iconpath = item->GetImageFilename("icon");
        if (!iconpath.isEmpty())
        {
            m_preview->SetFilename(iconpath);
            m_preview->Load();
        }
    }

    if (m_previewtitle)
        m_previewtitle->SetText(item->GetText("iconname"));
}

bool ImportIconsWizard::initialLoad(QString name)
{
    QString dirpath = GetConfDir();
    QDir configDir(dirpath);
    if (!configDir.exists() && !configDir.mkdir(dirpath))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Could not create %1").arg(dirpath));
    }

    m_strChannelDir = QString("%1/%2").arg(configDir.absolutePath())
                                      .arg("/channels");
    QDir strChannelDir(m_strChannelDir);
    if (!strChannelDir.exists() && !strChannelDir.mkdir(m_strChannelDir))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Could not create %1").arg(m_strChannelDir));
    }
    m_strChannelDir += "/";

    bool closeDialog = false;

    QString querystring("SELECT chanid, name, xmltvid, callsign,"
                        "dtv_multiplex.transportid, atsc_major_chan, "
                        "atsc_minor_chan, dtv_multiplex.networkid, "
                        "channel.serviceid, channel.mplexid,"
                        "dtv_multiplex.mplexid, channel.icon, channel.visible "
                        "FROM channel LEFT JOIN dtv_multiplex "
                        "ON channel.mplexid = dtv_multiplex.mplexid "
                        "WHERE ");
    if (!name.isEmpty())
        querystring.append("name=\"" + name + "\"");
    else
        querystring.append("channel.visible");
    querystring.append(" ORDER BY name");

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(querystring);

    m_listEntries.clear();
    m_nCount=0;
    m_nMaxCount=0;
    m_missingMaxCount=0;

    if (query.exec() && query.size() > 0)
    {
        m_progressDialog =
                new MythUIProgressDialog(tr("Initializing, please wait..."),
                                        m_popupStack, "IconImportInitProgress");

        if (m_progressDialog->Create())
        {
            m_popupStack->AddScreen(m_progressDialog);
            m_progressDialog->SetTotal(query.size());
        }
        else
        {
            delete m_progressDialog;
            m_progressDialog = NULL;
        }

        while(query.next())
        {
            CSVEntry entry;

            if (m_fRefresh)
            {
                if (QFile(query.value(11).toString()).exists())
                    continue;
            }

            entry.strChanId=query.value(0).toString();
            entry.strName=query.value(1).toString();
            entry.strXmlTvId=query.value(2).toString();
            entry.strCallsign=query.value(3).toString();
            entry.strTransportId=query.value(4).toString();
            entry.strAtscMajorChan=query.value(5).toString();
            entry.strAtscMinorChan=query.value(6).toString();
            entry.strNetworkId=query.value(7).toString();
            entry.strServiceId=query.value(8).toString();
            entry.strIconCSV= QString("%1,%2,%3,%4,%5,%6,%7,%8,%9\n").
                              arg(escape_csv(entry.strChanId)).
                              arg(escape_csv(entry.strName)).
                              arg(escape_csv(entry.strXmlTvId)).
                              arg(escape_csv(entry.strCallsign)).
                              arg(escape_csv(entry.strTransportId)).
                              arg(escape_csv(entry.strAtscMajorChan)).
                              arg(escape_csv(entry.strAtscMinorChan)).
                              arg(escape_csv(entry.strNetworkId)).
                              arg(escape_csv(entry.strServiceId));
            entry.strNameCSV=escape_csv(entry.strName);
            LOG(VB_CHANNEL, LOG_INFO,
                QString("chanid %1").arg(entry.strIconCSV));

            m_listEntries.append(entry);
            m_nMaxCount++;
            if (m_progressDialog)
                m_progressDialog->SetProgress(m_nMaxCount);
        }

        if (m_progressDialog)
        {
            m_progressDialog->Close();
            m_progressDialog = NULL;
        }
    }

    m_iter = m_listEntries.begin();

    m_progressDialog = new MythUIProgressDialog(
        tr("Downloading, please wait..."), m_popupStack,
           "IconImportInitProgress");

    if (m_progressDialog->Create())
    {
        m_popupStack->AddScreen(m_progressDialog);
        m_progressDialog->SetTotal(m_listEntries.size());
    }
    else
    {
        delete m_progressDialog;
        m_progressDialog = NULL;
    }

    while (!closeDialog && (m_iter != m_listEntries.end()))
    {
        /*: %1 is the current channel position,
         *  %2 is the total number of channels,
         *  %3 is the channel name
         */
        QString message = tr("Downloading %1 / %2 : %3")
                            .arg(m_nCount+1)
                            .arg(m_listEntries.size())
                            .arg((*m_iter).strName);

        if (m_missingEntries.size() > 0)
        {
            message.append("\n");
            message.append(tr("Could not find %n icon(s).", "", 
                              m_missingEntries.size()));
        }

        if (!findmissing((*m_iter).strIconCSV))
        {
            m_missingEntries.append((*m_iter));
            m_missingMaxCount++;
        }

        m_nCount++;
        m_iter++;
        if (m_progressDialog)
        {
            m_progressDialog->SetMessage(message);
            m_progressDialog->SetProgress(m_nCount);
        }
    }

    if (m_progressDialog)
    {
        m_progressDialog->Close();
        m_progressDialog = NULL;
    }

    if (m_missingEntries.size() == 0 || closeDialog)
        return false;

    if (m_nMaxCount <= 0)
        return false;

    return true;
}

bool ImportIconsWizard::doLoad()
{
    LOG(VB_CHANNEL, LOG_INFO, QString("Icons: Found %1 / Missing %2")
            .arg(m_missingCount).arg(m_missingMaxCount));

    // skip over empty entries
    while (m_missingIter != m_missingEntries.end() &&
           (*m_missingIter).strName.isEmpty())
    {
        m_missingCount++;
        m_missingIter++;
    }

    if (m_missingIter == m_missingEntries.end())
    {
        LOG(VB_CHANNEL, LOG_INFO, "doLoad Icon search complete");
        enableControls(STATE_DISABLED);
        return false;
    }

    // Look for the next missing icon
    m_nameText->SetText(tr("Choose icon for channel %1")
                        .arg((*m_missingIter).strName));
    m_manualEdit->SetText((*m_missingIter).strName);
    if (!search((*m_missingIter).strNameCSV))
        m_statusText->SetText(tr("No matches found for %1")
                              .arg((*m_missingIter).strName));
    else
        m_statusText->Reset();

    return true;
}

QString ImportIconsWizard::escape_csv(const QString& str)
{
    QRegExp rxDblForEscape("\"");
    QString str2 = str;
    str2.replace(rxDblForEscape,"\\\"");
    return "\""+str2+"\"";
}

QStringList ImportIconsWizard::extract_csv(const QString &line)
{
    QStringList ret;
    QString str;
    bool in_comment = false;
    bool in_escape = false;
    int comma_count = 0;
    for (int i = 0; i < line.length(); i++)
    {
        QChar cur = line[i];
        if (in_escape)
        {
            str += cur;
            in_escape = false;
        }
        else if (cur == '"')
        {
            in_comment = !in_comment;
        }
        else if (cur == '\\')
        {
            in_escape = true;
        }
        else if (!in_comment && (cur == ','))
        {
            ret += str;
            str.clear();
            ++comma_count;
        }
        else
        {
            str += cur;
        }
    }
    if (comma_count)
        ret += str;

    // This is just to avoid segfaulting, we should add some error recovery
    while (ret.size() < 5)
        ret.push_back("");

    return ret;
}

QString ImportIconsWizard::wget(QUrl& url,const QString& strParam )
{
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    QByteArray raw(strParam.toLatin1());
    QBuffer data(&raw);
    QHttpRequestHeader header;

    header.setContentType(QString("application/x-www-form-urlencoded"));
    header.setContentLength(raw.size());

    header.setValue("User-Agent", "MythTV Channel Icon lookup bot");

    QString str = HttpComms::postHttp(url,&header,&data);

    return str;
#else
#warning ImportIconsWizard::wget() not ported to Qt5
    return QString();
#endif
}

bool ImportIconsWizard::checkAndDownload(const QString& url, const QString& localChanId)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    QString iconUrl = url;
    QString filename = url.section('/', -1);
    QFileInfo file(m_strChannelDir+filename);

    bool fRet;
    // Since DNS for lyngsat-logos.com times out, set a 20s timeout
    if (!file.exists())
        fRet = HttpComms::getHttpFile(file.absoluteFilePath(),iconUrl,20000);
    else
        fRet = true;

    if (fRet)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        QString  qstr = "UPDATE channel SET icon = :ICON "
                        "WHERE chanid = :CHANID";

        query.prepare(qstr);
        query.bindValue(":ICON", file.absoluteFilePath());
        query.bindValue(":CHANID", localChanId);

        if (!query.exec())
        {
            MythDB::DBError("Error inserting channel icon", query);
            return false;
        }

    }

    return fRet;
#else
#warning ImportIconsWizard::checkAndDownload() not ported to Qt5
    return false;
#endif
}

bool ImportIconsWizard::lookup(const QString& strParam)
{
    QString strParam1 = QUrl::toPercentEncoding("callsign="+strParam);
    QUrl url(m_url+"/lookup");

    QString str = wget(url,strParam1);
    if (str.isEmpty() || str.startsWith("Error", Qt::CaseInsensitive))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Error from icon lookup : %1").arg(str));
        return true;
    }
    else
    {
        LOG(VB_CHANNEL, LOG_INFO,
            QString("Icon Import: Working lookup : %1").arg(str));
        return false;
    }
}

bool ImportIconsWizard::search(const QString& strParam)
{

    QString strParam1 = QUrl::toPercentEncoding(strParam);
    bool retVal = false;
    enableControls(STATE_SEARCHING);
    QUrl url(m_url+"/search");

    CSVEntry entry2 = (*m_missingIter);
    QString channelcsv = QString("%1,%2,%3,%4,%5,%6,%7,%8\n")
                                .arg(escape_csv(entry2.strName))
                                .arg(escape_csv(entry2.strXmlTvId))
                                .arg(escape_csv(entry2.strCallsign))
                                .arg(escape_csv(entry2.strTransportId))
                                .arg(escape_csv(entry2.strAtscMajorChan))
                                .arg(escape_csv(entry2.strAtscMinorChan))
                                .arg(escape_csv(entry2.strNetworkId))
                                .arg(escape_csv(entry2.strServiceId));

    QString message = QObject::tr("Searching for icons for channel %1")
                                                    .arg(entry2.strName);

    OpenBusyPopup(message);

    QString str = wget(url,"s="+strParam1+"&csv="+channelcsv);
    m_listSearch.clear();
    m_iconsList->Reset();

    if (str.isEmpty() || str.startsWith("#") ||
        str.startsWith("Error", Qt::CaseInsensitive))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error from search : %1").arg(str));
        retVal = false;
    }
    else
    {
        LOG(VB_CHANNEL, LOG_INFO,
            QString("Icon Import: Working search : %1").arg(str));
        QStringList strSplit = str.split("\n");

        // HACK HACK HACK -- begin
        // This is needed since the user can't escape out of the progress dialog
        // and the result set may contain thousands of channels.
        if (strSplit.size() > 36*3)
        {
            LOG(VB_GENERAL, LOG_WARNING,
                QString("Warning: Result set contains %1 items, "
                        "truncating to the first %2 results")
                    .arg(strSplit.size()).arg(18*3));
            while (strSplit.size() > 18*3) strSplit.removeLast();
        }
        // HACK HACK HACK -- end

        QString prevIconName;
        int namei = 1;

        for (int x = 0; x < strSplit.size(); ++x)
        {
            QString row = strSplit[x];
            if (row != "#" )
            {
                QStringList ret = extract_csv(row);
                LOG(VB_CHANNEL, LOG_INFO,
                    QString("Icon Import: search : %1 %2 %3")
                        .arg(ret[0]).arg(ret[1]).arg(ret[2]));
                SearchEntry entry;
                entry.strID = ret[0];
                entry.strName = ret[1];
                entry.strLogo = ret[2];
                m_listSearch.append(entry);

                MythUIButtonListItem *item;
                if (prevIconName == entry.strName)
                {
                    QString newname = QString("%1 (%2)").arg(entry.strName)
                                                        .arg(namei);
                    item = new MythUIButtonListItem(m_iconsList, newname,
                                             qVariantFromValue(entry));
                    namei++;
                }
                else
                {
                    item = new MythUIButtonListItem(m_iconsList, entry.strName,
                                             qVariantFromValue(entry));
                    namei=1;
                }

                QString iconname = entry.strName;

                item->SetImage(entry.strLogo, "icon", false);
                item->SetText(iconname, "iconname");

                prevIconName = entry.strName;
            }
        }

        retVal = true;
    }
    enableControls(STATE_NORMAL, retVal);
    CloseBusyPopup();
    return retVal;
}

bool ImportIconsWizard::findmissing(const QString& strParam)
{
    QString strParam1 = QUrl::toPercentEncoding(strParam);
    QUrl url(m_url+"/findmissing");

    QString str = wget(url,"csv="+strParam1);
    LOG(VB_CHANNEL, LOG_INFO,
        QString("Icon Import: findmissing : strParam1 = %1. str = %2")
            .arg(strParam1).arg(str));
    if (str.isEmpty() || str.startsWith("#"))
    {
        return false;
    }
    else if (str.startsWith("Error", Qt::CaseInsensitive))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Error from findmissing : %1").arg(str));
        return false;
    }
    else
    {
        LOG(VB_CHANNEL, LOG_INFO,
            QString("Icon Import: Working findmissing : %1") .arg(str));
        QStringList strSplit = str.split("\n", QString::SkipEmptyParts);
        for (QStringList::const_iterator it = strSplit.begin();
             it != strSplit.end(); ++it)
        {
            if (*it != "#")
            {
                const QStringList ret = extract_csv(*it);
                LOG(VB_CHANNEL, LOG_INFO,
                    QString("Icon Import: findmissing : %1 %2 %3 %4 %5")
                        .arg(ret[0]).arg(ret[1]).arg(ret[2])
                        .arg(ret[3]).arg(ret[4]));
                checkAndDownload(ret[4], (*m_iter).strChanId);
            }
        }
        return true;
    }
}

void ImportIconsWizard::askSubmit(const QString& strParam)
{
    m_strParam = strParam;
    QString message = tr("You now have the opportunity to transmit your "
                         "choices back to mythtv.org so that others can "
                         "benefit from your selections.");

    MythConfirmationDialog *confirmPopup =
            new MythConfirmationDialog(m_popupStack, message);

    confirmPopup->SetReturnEvent(this, "submitresults");

    if (confirmPopup->Create())
        m_popupStack->AddScreen(confirmPopup, false);
}

bool ImportIconsWizard::submit()
{
    QString strParam1 = QUrl::toPercentEncoding(m_strParam);
    QUrl url(m_url+"/submit");

    QString str = wget(url,"csv="+strParam1);
    if (str.isEmpty() || str.startsWith("#") ||
        str.startsWith("Error", Qt::CaseInsensitive))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error from submit : %1").arg(str));
        m_statusText->SetText(tr("Failed to submit icon choices."));
        return false;
    }
    else
    {
        LOG(VB_CHANNEL, LOG_INFO, QString("Icon Import: Working submit : %1")
                .arg(str));
        QStringList strSplit = str.split("\n", QString::SkipEmptyParts);
        unsigned atsc = 0, dvb = 0, callsign = 0, tv = 0, xmltvid = 0;
        for (QStringList::const_iterator it = strSplit.begin();
             it != strSplit.end(); ++it)
        {
            if (*it == "#")
                continue;

            QStringList strSplit2=(*it).split(":", QString::SkipEmptyParts);
            if (strSplit2.size() < 2)
                continue;

            QString s = strSplit2[0].trimmed();
            if (s == "a")
                atsc = strSplit2[1].toUInt();
            else if (s == "c")
                callsign = strSplit2[1].toUInt();
            else if (s == "d")
                dvb = strSplit2[1].toUInt();
            else if (s == "t")
                tv = strSplit2[1].toUInt();
            else if (s == "x")
                xmltvid = strSplit2[1].toUInt();
        }
        LOG(VB_CHANNEL, LOG_INFO,
            QString("Icon Import: working submit : atsc=%1 callsign=%2 "
                    "dvb=%3 tv=%4 xmltvid=%5")
                .arg(atsc).arg(callsign).arg(dvb).arg(tv).arg(xmltvid));
        m_statusText->SetText(tr("Icon choices submitted successfully."));
        return true;
    }
}

void ImportIconsWizard::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        QString resultid  = dce->GetId();
        int     buttonnum = dce->GetResult();

        if (resultid == "submitresults")
        {
            switch (buttonnum)
            {
                case 0 :
                    Close();
                    break;
                case 1 :
                    submit();
                    Close();
                    break;
            }
        }
    }
}

void ImportIconsWizard::Close()
{
    MythScreenType::Close();
}
