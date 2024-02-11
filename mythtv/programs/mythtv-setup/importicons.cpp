// Qt
#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythdownloadmanager.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/remotefile.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythprogressdialog.h"
#include "libmythui/mythuibutton.h"
#include "libmythui/mythuibuttonlist.h"
#include "libmythui/mythuiimage.h"
#include "libmythui/mythuitext.h"
#include "libmythui/mythuitextedit.h"

// MythTV Setup
#include "importicons.h"

ImportIconsWizard::ImportIconsWizard(MythScreenStack *parent, bool fRefresh,
                                     QString channelname)
                  :MythScreenType(parent, "ChannelIconImporter"),
    m_strChannelname(std::move(channelname)), m_fRefresh(fRefresh)
{
    if (!m_strChannelname.isEmpty())
    {
        LOG(VB_GENERAL, LOG_INFO,
            QString("Fetching icon for channel %1").arg(m_strChannelname));
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, "Fetching icons for multiple channels");
    }

    m_popupStack = GetMythMainWindow()->GetStack("popup stack");

    m_tmpDir = QDir(QString("%1/tmp/icon").arg(GetConfDir()));

    if (!m_tmpDir.exists())
        m_tmpDir.mkpath(m_tmpDir.absolutePath());
}

ImportIconsWizard::~ImportIconsWizard()
{
    if (m_tmpDir.exists())
    {
        QStringList files = m_tmpDir.entryList();
        for (const QString &file : std::as_const(files))
            m_tmpDir.remove(file);
        m_tmpDir.rmpath(m_tmpDir.absolutePath());
    }
}

bool ImportIconsWizard::Create()
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("config-ui.xml", "iconimport", this);
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

    connect(m_manualButton, &MythUIButton::Clicked, this, &ImportIconsWizard::manualSearch);
    connect(m_skipButton, &MythUIButton::Clicked, this, &ImportIconsWizard::skip);
    connect(m_iconsList, &MythUIButtonList::itemClicked,
            this, &ImportIconsWizard::menuSelection);
    connect(m_iconsList, &MythUIButtonList::itemSelected,
            this, &ImportIconsWizard::itemChanged);

    BuildFocusList();

    enableControls(STATE_NORMAL);

    return true;
}

void ImportIconsWizard::Load()
{
    if (!initialLoad(m_strChannelname))
        Close();
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

void ImportIconsWizard::enableControls(dialogState state)
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
    if (!search(str))
        m_statusText->SetText(tr("No matches found for \"%1\"").arg(str));
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

    auto entry = item->GetData().value<SearchEntry>();

    LOG(VB_GENERAL, LOG_INFO, QString("Menu Selection: %1 %2 %3")
            .arg(entry.strID, entry.strName, entry.strLogo));

    enableControls(STATE_SEARCHING);

    CSVEntry entry2 = (*m_missingIter);
    m_strMatches += QString("%1,%2,%3,%4,%5,%6,%7,%8,%9\n")
                            .arg(escape_csv(entry.strID),
                                 escape_csv(entry2.strName),
                                 escape_csv(entry2.strXmlTvId),
                                 escape_csv(entry2.strCallsign),
                                 escape_csv(entry2.strTransportId),
                                 escape_csv(entry2.strAtscMajorChan),
                                 escape_csv(entry2.strAtscMinorChan),
                                 escape_csv(entry2.strNetworkId),
                                 escape_csv(entry2.strServiceId));

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

bool ImportIconsWizard::initialLoad(const QString& name)
{

    QString dirpath = GetConfDir();
    QDir configDir(dirpath);
    if (!configDir.exists() && !configDir.mkdir(dirpath))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Could not create %1").arg(dirpath));
    }

    m_strChannelDir = QString("%1/%2").arg(configDir.absolutePath(),
                                           "/channels");
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
                        "WHERE deleted IS NULL AND ");
    if (!name.isEmpty())
        querystring.append("name=\"" + name + "\"");
    else
        querystring.append("channel.visible > 0");
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
            QCoreApplication::processEvents();
        }
        else
        {
            delete m_progressDialog;
            m_progressDialog = nullptr;
        }

        while(query.next())
        {
            CSVEntry entry;
            QString relativeIconPath = query.value(11).toString();
            QString absoluteIconPath = QString("%1%2").arg(m_strChannelDir,
                                                           relativeIconPath);

            if (m_fRefresh && !relativeIconPath.isEmpty() &&
                QFile(absoluteIconPath).exists() &&
                !QImage(absoluteIconPath).isNull())
            {
                LOG(VB_GENERAL, LOG_NOTICE, QString("Icon already exists, skipping (%1)").arg(absoluteIconPath));
            }
            else
            {
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
                                arg(escape_csv(entry.strChanId),
                                    escape_csv(entry.strName),
                                    escape_csv(entry.strXmlTvId),
                                    escape_csv(entry.strCallsign),
                                    escape_csv(entry.strTransportId),
                                    escape_csv(entry.strAtscMajorChan),
                                    escape_csv(entry.strAtscMinorChan),
                                    escape_csv(entry.strNetworkId),
                                    escape_csv(entry.strServiceId));
                entry.strNameCSV=escape_csv(entry.strName);
                LOG(VB_CHANNEL, LOG_INFO,
                    QString("chanid %1").arg(entry.strIconCSV));

                m_listEntries.append(entry);
            }

            m_nMaxCount++;
            if (m_progressDialog)
            {
                m_progressDialog->SetProgress(m_nMaxCount);
                QCoreApplication::processEvents();
            }
        }

        if (m_progressDialog)
        {
            m_progressDialog->Close();
            m_progressDialog = nullptr;
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
        QCoreApplication::processEvents();
    }
    else
    {
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }

    /*: %1 is the current channel position,
     *  %2 is the total number of channels,
     */
    QString downloadMessage = tr("Downloading %1 of %2");

    while (!closeDialog && (m_iter != m_listEntries.end()))
    {
        QString message = downloadMessage.arg(m_nCount+1)
                                         .arg(m_listEntries.size());

        LOG(VB_GENERAL, LOG_NOTICE, message);

        if (!m_missingEntries.empty())
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
            QCoreApplication::processEvents();
        }
    }

    if (m_progressDialog)
    {
        m_progressDialog->Close();
        m_progressDialog = nullptr;
    }

    if (m_missingEntries.empty() || closeDialog)
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
    if (!search((*m_missingIter).strName))
    {
        m_statusText->SetText(tr("No matches found for %1")
                              .arg((*m_missingIter).strName));
    }
    else
    {
        m_statusText->Reset();
    }

    return true;
}

QString ImportIconsWizard::escape_csv(const QString& str)
{
    static const QRegularExpression rxDblForEscape("\"");
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
    for (const auto& cur : std::as_const(line))
    {
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

QString ImportIconsWizard::wget(QUrl& url, const QString& strParam )
{
    QByteArray data(strParam.toLatin1());

    auto *req = new QNetworkRequest(url);
    req->setHeader(QNetworkRequest::ContentTypeHeader, QString("application/x-www-form-urlencoded"));
    req->setHeader(QNetworkRequest::ContentLengthHeader, data.size());

    LOG(VB_CHANNEL, LOG_DEBUG, QString("ImportIconsWizard: posting to: %1, params: %2")
                                       .arg(url.toString(), strParam));

    if (GetMythDownloadManager()->post(req, &data))
    {
        LOG(VB_CHANNEL, LOG_DEBUG, QString("ImportIconsWizard: result: %1").arg(QString(data)));
        return {data};
    }

    return {};
}

#include <QTemporaryFile>
bool ImportIconsWizard::checkAndDownload(const QString& url, const QString& localChanId)
{
    QString filename = url.section('/', -1);
    QString filePath = m_strChannelDir + filename;

    // If we get to this point we've already checked whether the icon already
    // exist locally, we want to download anyway to fix a broken image or
    // get the latest version of the icon

    QTemporaryFile tmpFile(filePath);
    if (!tmpFile.open())
    {
        LOG(VB_GENERAL, LOG_INFO, "Icon Download: Couldn't create temporary file");
        return false;
    }

    bool fRet = GetMythDownloadManager()->download(url, tmpFile.fileName());

    if (!fRet)
    {
        LOG(VB_GENERAL, LOG_INFO,
            QString("Download for icon %1 failed").arg(filename));
        return false;
    }

    QImage icon(tmpFile.fileName());
    if (icon.isNull())
    {
        LOG(VB_GENERAL, LOG_INFO,
            QString("Downloaded icon for %1 isn't a valid image").arg(filename));
        return false;
    }

    // Remove any existing icon
    QFile file(filePath);
    file.remove();

    // Rename temporary file & prevent it being cleaned up
    tmpFile.rename(filePath);
    tmpFile.setAutoRemove(false);

    MSqlQuery query(MSqlQuery::InitCon());
    QString  qstr = "UPDATE channel SET icon = :ICON "
                    "WHERE chanid = :CHANID";

    query.prepare(qstr);
    query.bindValue(":ICON", filename);
    query.bindValue(":CHANID", localChanId);

    if (!query.exec())
    {
        MythDB::DBError("Error inserting channel icon", query);
        return false;
    }

    return fRet;
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
    LOG(VB_CHANNEL, LOG_INFO,
        QString("Icon Import: Working lookup : %1").arg(str));
    return false;
}

bool ImportIconsWizard::search(const QString& strParam)
{

    QString strParam1 = QUrl::toPercentEncoding(strParam);
    bool retVal = false;
    enableControls(STATE_SEARCHING);
    QUrl url(m_url+"/search");

    CSVEntry entry2 = (*m_missingIter);
    QString channelcsv = QString("%1,%2,%3,%4,%5,%6,%7,%8\n")
                                .arg(escape_csv(QUrl::toPercentEncoding(entry2.strName)),
                                     escape_csv(QUrl::toPercentEncoding(entry2.strXmlTvId)),
                                     escape_csv(QUrl::toPercentEncoding(entry2.strCallsign)),
                                     escape_csv(entry2.strTransportId),
                                     escape_csv(entry2.strAtscMajorChan),
                                     escape_csv(entry2.strAtscMinorChan),
                                     escape_csv(entry2.strNetworkId),
                                     escape_csv(entry2.strServiceId));

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
        if (strSplit.size() > 150)
        {
            LOG(VB_GENERAL, LOG_WARNING,
                QString("Warning: Result set contains %1 items, "
                        "truncating to the first %2 results")
                    .arg(strSplit.size()).arg(150));
            while (strSplit.size() > 150)
                strSplit.removeLast();
        }
        // HACK HACK HACK -- end

        QString prevIconName;
        int namei = 1;

        for (const QString& row : std::as_const(strSplit))
        {
            if (row != "#" )
            {
                QStringList ret = extract_csv(row);
                LOG(VB_CHANNEL, LOG_INFO,
                    QString("Icon Import: search : %1 %2 %3")
                        .arg(ret[0], ret[1], ret[2]));
                SearchEntry entry;
                entry.strID = ret[0];
                entry.strName = ret[1];
                entry.strLogo = ret[2];
                m_listSearch.append(entry);

                MythUIButtonListItem *item = nullptr;
                if (prevIconName == entry.strName)
                {
                    QString newname = QString("%1 (%2)").arg(entry.strName)
                                                        .arg(namei);
                    item = new MythUIButtonListItem(m_iconsList, newname,
                                             QVariant::fromValue(entry));
                    namei++;
                }
                else
                {
                    item = new MythUIButtonListItem(m_iconsList, entry.strName,
                                             QVariant::fromValue(entry));
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
    enableControls(STATE_NORMAL);
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
            .arg(strParam1, str));
    if (str.isEmpty() || str.startsWith("#"))
    {
        return false;
    }
    if (str.startsWith("Error", Qt::CaseInsensitive))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Error from findmissing : %1").arg(str));
        return false;
    }

    LOG(VB_CHANNEL, LOG_INFO,
        QString("Icon Import: Working findmissing : %1") .arg(str));
    QStringList strSplit = str.split("\n", Qt::SkipEmptyParts);
    for (const auto& line : std::as_const(strSplit))
    {
        if (line[0] == QChar('#'))
            continue;

        const QStringList ret = extract_csv(line);
        LOG(VB_CHANNEL, LOG_INFO,
            QString("Icon Import: findmissing : %1 %2 %3 %4 %5")
            .arg(ret[0], ret[1], ret[2], ret[3], ret[4]));
        checkAndDownload(ret[4], (*m_iter).strChanId);
    }
    return true;
}

void ImportIconsWizard::askSubmit(const QString& strParam)
{
    m_strParam = strParam;
    QString message = tr("You now have the opportunity to transmit your "
                         "choices back to mythtv.org so that others can "
                         "benefit from your selections.");

    auto *confirmPopup = new MythConfirmationDialog(m_popupStack, message);

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

    LOG(VB_CHANNEL, LOG_INFO, QString("Icon Import: Working submit : %1")
        .arg(str));
    QStringList strSplit = str.split("\n", Qt::SkipEmptyParts);
    unsigned atsc = 0;
    unsigned dvb = 0;
    unsigned callsign = 0;
    unsigned tv = 0;
    unsigned xmltvid = 0;
    for (const auto& line : std::as_const(strSplit))
    {
        if (line[0] == QChar('#'))
            continue;

        QStringList strSplit2=(line).split(":", Qt::SkipEmptyParts);
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

void ImportIconsWizard::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = (DialogCompletionEvent*)(event);

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
