#include <sys/stat.h>

#include <QApplication>
#include <QRegExp>
#include <QBuffer>
#include <QDir>
#include <QFileInfo>

#include "mythwizard.h"
#include "mythcontext.h"
#include "mythdb.h"
#include "mythdirs.h"
#include "mythverbose.h"
#include "httpcomms.h"
#include "importicons.h"
#include "util.h"

ImportIconsWizard::ImportIconsWizard(bool fRefresh, const QString &channelname) :
    m_strMatches(QString::null),   m_strChannelDir(QString::null),
    m_strChannelname(channelname), m_fRefresh(fRefresh),
    m_nMaxCount(0),                m_nCount(0),
    m_missingMaxCount(0),          m_missingCount(0),
    m_closeDialog(false)
{
    m_strChannelname.detach();
}

MythDialog *ImportIconsWizard::dialogWidget(MythMainWindow *parent,
                                     const char *widgetName)
{
    MythWizard *ret = (MythWizard*)ConfigurationWizard::dialogWidget(parent,widgetName);
    connect(ret->finishButton(), SIGNAL(pressed()), this, SLOT(finishButtonPressed()));
    return (MythDialog*)ret;
}

int ImportIconsWizard::exec()
{
    QString dirpath = GetConfDir();
    QDir configDir(dirpath);
    if (!configDir.exists())
    {
        if (!configDir.mkdir(dirpath))
        {
            VERBOSE(VB_IMPORTANT, QString("Could not create %1").arg(dirpath));
        }
    }

    m_strChannelDir = QString("%1/%2")
        .arg(configDir.absolutePath()).arg("/channels");
    QDir strChannelDir(m_strChannelDir);
    if (!strChannelDir.exists())
    {
        if (!strChannelDir.mkdir(m_strChannelDir))
        {
            VERBOSE(VB_IMPORTANT, QString("Could not create %1").arg(m_strChannelDir));
        }
    }
    m_strChannelDir+="/";

    if (initialLoad(m_strChannelname) > 0)
    {
        startDialog();
        m_missingIter=m_missingEntries.begin();
        doLoad();
    }
    else
        m_closeDialog=true;

    if (m_closeDialog==false) // Need this if line to exit if cancel button is pressed
        while ((ConfigurationDialog::exec() == QDialog::Accepted) && (m_closeDialog == false))  {}

    return QDialog::Rejected;
}

void ImportIconsWizard::startDialog()
{
    VerticalConfigurationGroup *manSearch =
        new VerticalConfigurationGroup(false,false,true,true);

    manSearch->addChild(m_editName = new TransLineEditSetting(false));
    m_editName->setLabel(QObject::tr("Channel Name"));
    m_editName->setHelpText(QObject::tr("Name of the icon file"));
    m_editName->setEnabled(false);

    manSearch->addChild(m_listIcons = new TransListBoxSetting());
    m_listIcons->setHelpText(QObject::tr("List of possible icon files"));

    m_editManual = new TransLineEditSetting();
    m_editManual->setHelpText(QObject::tr("Enter text here for the manual search"));

    m_buttonManual = new TransButtonSetting();
    m_buttonManual->setLabel(QObject::tr("&Search"));
    m_buttonManual->setHelpText(QObject::tr("Manually search for the text"));

    m_buttonSkip = new TransButtonSetting();
    m_buttonSkip->setLabel(QObject::tr("S&kip"));
    m_buttonSkip->setHelpText(QObject::tr("Skip this icon"));

    m_buttonSelect = new TransButtonSetting();
    m_buttonSelect->setLabel(QObject::tr("S&elect"));
    m_buttonSelect->setHelpText(QObject::tr("Select this icon"));

    HorizontalConfigurationGroup *hrz1 =
        new HorizontalConfigurationGroup(false, false, true, true);

    hrz1->addChild(m_editManual);
    hrz1->addChild(m_buttonManual);
    hrz1->addChild(m_buttonSkip);
    hrz1->addChild(m_buttonSelect);
    manSearch->addChild(hrz1);

    addChild(manSearch);

    connect(m_buttonManual, SIGNAL(pressed()), this, SLOT(manualSearch()));
    connect(m_buttonSkip, SIGNAL(pressed()), this, SLOT(skip()));
    connect(m_listIcons,SIGNAL(accepted(int)),this,
             SLOT(menuSelection(int)));
    connect(m_buttonSelect,SIGNAL(pressed()),this,
            SLOT(menuSelect()));

    enableControls(STATE_NORMAL);
}

const QString ImportIconsWizard::url="http://services.mythtv.org/channel-icon/";

void ImportIconsWizard::enableControls(dialogState state, bool selectEnabled)
{
    switch (state)
    {
        case STATE_NORMAL:
            if (!m_editManual->getValue().isEmpty())
                m_buttonManual->setEnabled(true);
            else
                m_buttonManual->setEnabled(false);
            if (m_missingCount < m_missingMaxCount)
            {
                if (m_missingMaxCount < 2) //When there's only one icon, nothing to skip to!
                    m_buttonSkip->setEnabled(false);
                else
                    m_buttonSkip->setEnabled(true);
                m_editName->setEnabled(true);
                m_listIcons->setEnabled(true);
                m_editManual->setEnabled(true);
                m_buttonSelect->setEnabled(selectEnabled);
            }
            else
            {
                m_buttonSkip->setEnabled(false);
                m_editName->setEnabled(false);
                m_listIcons->setEnabled(false);
                m_editManual->setEnabled(false);
                m_buttonManual->setEnabled(false);
                m_buttonSelect->setEnabled(false);
            }
            break;
        case STATE_SEARCHING:
            m_buttonSkip->setEnabled(false);
            m_buttonSelect->setEnabled(false);
            m_buttonManual->setEnabled(false);
            m_listIcons->setEnabled(false);
            m_listIcons->clearSelections();
            m_listIcons->addSelection("Please wait...");
            m_editManual->setValue("");
            break;
        case STATE_DISABLED:
            m_buttonSkip->setEnabled(false);
            m_buttonSelect->setEnabled(false);
            m_buttonManual->setEnabled(false);
            m_listIcons->setEnabled(false);
            m_listIcons->clearSelections();
            m_editName->setEnabled(false);
            m_editName->setValue("");
            m_editManual->setEnabled(false);
            m_editManual->setValue("");
            m_listIcons->setFocus();
            break;
    }
}

void ImportIconsWizard::manualSearch()
{
    QString str = m_editManual->getValue();
    search(escape_csv(str));
}

void ImportIconsWizard::skip()
{
    if (m_missingMaxCount > 1)
    {
        m_missingCount++;
        m_missingIter++;
        doLoad();
    }
}

void ImportIconsWizard::menuSelect()
{
    menuSelection(m_listIcons->currentItem());
}

void ImportIconsWizard::menuSelection(int nIndex)
{
    enableControls(STATE_SEARCHING);
    SearchEntry entry = m_listSearch[nIndex];

    CSVEntry entry2 = (*m_missingIter);
    m_strMatches += QString("%1,%2,%3,%4,%5,%6,%7,%8,%9\n").
                            arg(escape_csv(entry.strID)).
                            arg(escape_csv(entry2.strName)).
                            arg(escape_csv(entry2.strXmlTvId)).
                            arg(escape_csv(entry2.strCallsign)).
                            arg(escape_csv(entry2.strTransportId)).
                            arg(escape_csv(entry2.strAtscMajorChan)).
                            arg(escape_csv(entry2.strAtscMinorChan)).
                            arg(escape_csv(entry2.strNetworkId)).
                            arg(escape_csv(entry2.strServiceId));

    if ((!isBlocked(m_strMatches)) &&
            (checkAndDownload(entry.strLogo, entry2.strChanId)))
    {

        if (m_missingMaxCount > 1)
        {
            m_missingCount++;
            m_missingIter++;
            doLoad();
        }
        else
        {
            enableControls(STATE_DISABLED);
            m_listIcons->addSelection(QString("Channel icon for %1 was downloaded successfully.")
                                      .arg(entry2.strName));
            m_listIcons->setFocus();
            if (!m_strMatches.isEmpty())
                submit(m_strMatches);
            m_closeDialog=true;
        }
    }
    else
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                            QObject::tr("Error downloading"),
                            QObject::tr("Failed to download the icon file"));
        enableControls(STATE_DISABLED);
        m_closeDialog=true;
    }

}

uint ImportIconsWizard::initialLoad(QString name)
{
    // Do not try and access dialog within this function
    QString querystring=("SELECT chanid, name, xmltvid, callsign,"
                  "dtv_multiplex.transportid, atsc_major_chan, "
                  "atsc_minor_chan, dtv_multiplex.networkid, "
                  "channel.serviceid, channel.mplexid,"
                  "dtv_multiplex.mplexid, channel.icon, channel.visible "
                  "FROM channel LEFT JOIN dtv_multiplex "
                  "ON channel.mplexid = dtv_multiplex.mplexid "
                  "WHERE");
    if (name.isEmpty()==false)
        querystring+=" name=\"" + name + "\"";
    else
        querystring+=" channel.visible";
    querystring+=" ORDER BY name";

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(querystring);

    m_listEntries.clear();
    m_nCount=0;
    m_nMaxCount=0;
    m_missingMaxCount=0;

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        MythProgressDialog *progressDialog = new MythProgressDialog("Initialising, please wait...", query.size());

        while(query.next())
        {
            CSVEntry entry;

            if (m_fRefresh)
            {
                QFileInfo file(query.value(11).toString());
                if (file.exists())
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
            VERBOSE(VB_CHANNEL,QString("chanid %1").arg(entry.strIconCSV));

            m_listEntries.append(entry);
            m_nMaxCount++;
            progressDialog->setProgress(m_nMaxCount);
        }

        progressDialog->Close();
        progressDialog->deleteLater();
    }

    m_iter = m_listEntries.begin();

    MythProgressDialog *progressDialog = new MythProgressDialog("Downloading, please wait...",
                            m_listEntries.size(), true, this, SLOT(cancelPressed()));
    while (!m_closeDialog && (m_iter != m_listEntries.end()))
    {
        QString message = QString("Downloading %1 / %2 : ").arg(m_nCount+1)
            .arg(m_listEntries.size()) + (*m_iter).strName;
        if (m_missingEntries.size() > 0)
            message.append(QString("\nCould not find %1 icons.").arg(m_missingEntries.size()));
        progressDialog->setLabel(message);
        if (!findmissing((*m_iter).strIconCSV))
        {
            m_missingEntries.append((*m_iter));
            m_missingMaxCount++;
        }

        m_nCount++;
        m_iter++;
        progressDialog->setProgress(m_nCount);
    }
    progressDialog->Close();
    progressDialog->deleteLater();

    if (m_missingEntries.size() == 0)
        return 0;

    //Comment below for debugging - cancel button will continue to dialog
    if (m_closeDialog)
        m_nMaxCount=0;
    return m_nMaxCount;
}

bool ImportIconsWizard::doLoad()
{
    VERBOSE(VB_CHANNEL, QString("Icons: Found %1 / Missing %2")
                                    .arg(m_missingCount)
                                    .arg(m_missingMaxCount));
    if (m_missingCount >= m_missingMaxCount)
    {
        VERBOSE(VB_CHANNEL, "doLoad Icon search complete");
        enableControls(STATE_DISABLED);
        if (!m_strMatches.isEmpty())
            submit(m_strMatches);
        m_closeDialog=true;
        return false;
    }
    else
    {
        // Look for the next missing icon
        m_editName->setValue((*m_missingIter).strName);
        search((*m_missingIter).strNameCSV);
        return true;
    }
}

QString ImportIconsWizard::escape_csv(const QString& str)
{
    QRegExp rxDblForEscape("\"");
    QString str2 = str;
    str2.replace(rxDblForEscape,"\\\"");
    return "\""+str2+"\"";
}

QStringList ImportIconsWizard::extract_csv(const QString& strLine)
{
    QStringList ret;
    //Clean up the line and split out the fields
    QString str = strLine;

    int pos = 0;
    bool fFinish = false;
    while(!fFinish)
    {
        str = str.trimmed();
        while(!fFinish)
        {
            QString strLeft;
            switch (str.at(pos).unicode())
            {
            case '\\':
                if (pos>=1)
                    str.left(pos-1)+str.mid(pos+1);
                else
                    str=str.mid(pos+1);
                pos+=2;
                if (pos > str.length())
                {
                    strLeft = str.left(pos);
                    if (strLeft.startsWith("\"") && strLeft.endsWith("\""))
                        strLeft=strLeft.mid(1,strLeft.length()-2);
                    ret.append(strLeft);
                    fFinish = true;
                }
                break;
            case ',':
                strLeft = str.left(pos);
                if (strLeft.startsWith("\"") && strLeft.endsWith("\""))
                    strLeft=strLeft.mid(1,strLeft.length()-2);
                ret.append(strLeft);
                if ((pos+1) > str.length())
                   fFinish = true;
                str=str.mid(pos+1);
                pos=0;
                break;
            default:
                pos++;
                if (pos >= str.length())
                {
                    strLeft = str.left(pos);
                    if (strLeft.startsWith("\"") && strLeft.endsWith("\""))
                        strLeft=strLeft.mid(1,strLeft.length()-2);
                    ret.append(strLeft);
                    fFinish = true;
                }
            }
        }
    }

    // This is just to avoid segfaulting, we should add some error recovery
    while (ret.size() < 5)
        ret.push_back("");

    return ret;
}

QString ImportIconsWizard::wget(QUrl& url,const QString& strParam )
{
    QByteArray raw(strParam.toAscii());
    QBuffer data(&raw);
    QHttpRequestHeader header;

    header.setContentType(QString("application/x-www-form-urlencoded"));
    header.setContentLength(raw.size());

    header.setValue("User-Agent", "MythTV Channel Icon lookup bot");

    QString str = HttpComms::postHttp(url,&header,&data);

    return str;
}

bool ImportIconsWizard::checkAndDownload(const QString& str, const QString& localChanId)
{
    // Do not try and access dialog within this function

    int iIndex = str.lastIndexOf('/');
    QString str2;
    if (iIndex < 0)
        str2=str;
    else
        str2=str.mid(iIndex+1);

    QString str3 = str;
    QFileInfo file(m_strChannelDir+str2);

    bool fRet;
    if (!file.exists())
        fRet = HttpComms::getHttpFile(m_strChannelDir+str2,str3);
    else
        fRet = true;

    if (fRet)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        QString  qstr = "UPDATE channel SET icon = :ICON "
                        "WHERE chanid = :CHANID";

        query.prepare(qstr);
        query.bindValue(":ICON", m_strChannelDir+str2);
        query.bindValue(":CHANID", localChanId);

        if (!query.exec())
        {
            MythDB::DBError("Error inserting channel icon", query);
            return false;
        }

    }

    return fRet;
}

bool ImportIconsWizard::isBlocked(const QString& strParam)
{
    QString strParam1 = QUrl::toPercentEncoding(strParam);
    QUrl url(ImportIconsWizard::url+"/checkblock");
    QString str = wget(url,"csv="+strParam1);

    if (str.startsWith("Error", Qt::CaseInsensitive))
    {
        VERBOSE(VB_IMPORTANT, QString("Error from isBlocked : %1").arg(str));
        return true;
    }
    else if (str.isEmpty() || str.startsWith("\r\n") || str.startsWith("#"))
        return false;
    else
    {
        VERBOSE(VB_IMPORTANT, QString("isBlocked Error: %1").arg(str));
        VERBOSE(VB_CHANNEL, QString("Icon Import: Working isBlocked"));
        int nVal = MythPopupBox::showOkCancelPopup(gContext->GetMainWindow(),
                            QObject::tr("Icon is blocked"),
                            QObject::tr("This combination of channel and icon "
                                        "has been blocked by the MythTV "
                                        "admins. The most common reason for "
                                        "this is that there is a better match "
                                        "available.\n "
                                        "Are you still sure that you want to "
                                        "use this icon?"),
                          true);
        if (nVal == 1) // Use the icon
            return false;
        else // Don't use the icon
            return true;
    }
}


bool ImportIconsWizard::lookup(const QString& strParam)
{
    QString strParam1 = QUrl::toPercentEncoding("callsign="+strParam);
    QUrl url(ImportIconsWizard::url+"/lookup");

    QString str = wget(url,strParam1);
    if (str.isEmpty() || str.startsWith("Error", Qt::CaseInsensitive))
    {
        VERBOSE(VB_IMPORTANT, QString("Error from icon lookup : %1").arg(str));
        return true;
    }
    else
    {
        VERBOSE(VB_CHANNEL, QString("Icon Import: Working lookup : %1").arg(str));
        return false;
    }
}

bool ImportIconsWizard::search(const QString& strParam)
{
    QString strParam1 = QUrl::toPercentEncoding(strParam);
    bool retVal = false;
    enableControls(STATE_SEARCHING);
    QUrl url(ImportIconsWizard::url+"/search");

    QString str = wget(url,"s="+strParam1);
    m_listSearch.clear();
    m_listIcons->clearSelections();

    if (str.isEmpty() || str.startsWith("#") ||
        str.startsWith("Error", Qt::CaseInsensitive))
    {
        VERBOSE(VB_IMPORTANT, QString("Error from search : %1").arg(str));
        retVal = false;
    }
    else
    {
        VERBOSE(VB_CHANNEL, QString("Icon Import: Working search : %1").arg(str));
        QStringList strSplit = str.split("\n");

        QString prevIconName = "";
        int namei = 1;

        for (int x = 0; x < strSplit.size(); x++)
        {
            QString row = strSplit[x];
            if (row != "#" )
            {
                QStringList ret = extract_csv(row);
                VERBOSE(VB_CHANNEL, QString("Icon Import: search : %1 %2 %3")
                            .arg(ret[0]).arg(ret[1]).arg(ret[2]));
                SearchEntry entry;
                entry.strID = ret[0];
                entry.strName = ret[1];
                entry.strLogo = ret[2];
                m_listSearch.append(entry);
                if (prevIconName == entry.strName)
                {
                    QString newname = QString("%1 (%2)").arg(entry.strName)
                                                        .arg(namei);
                    m_listIcons->addSelection(newname);
                    namei++;
                }
                else
                {
                    m_listIcons->addSelection(entry.strName);
                    namei=1;
                }
                prevIconName = entry.strName;
            }
        }
        retVal = true;
    }
    enableControls(STATE_NORMAL, retVal);
    return retVal;
}

bool ImportIconsWizard::findmissing(const QString& strParam)
{
    QString strParam1 = QUrl::toPercentEncoding(strParam);
    QUrl url(ImportIconsWizard::url+"/findmissing");

    QString str = wget(url,"csv="+strParam1);
    VERBOSE(VB_CHANNEL, QString("Icon Import: findmissing : strParam1 = %1. str = %2").arg(strParam1).arg(str));
    if (str.isEmpty() || str.startsWith("#") ||
        str.startsWith("Error", Qt::CaseInsensitive))
    {
        VERBOSE(VB_IMPORTANT, QString("Error from findmissing : %1").arg(str));
        return false;
    }
    else
    {
        VERBOSE(VB_CHANNEL, QString("Icon Import: Working findmissing : %1")
                .arg(str));
        QStringList strSplit = str.split("\n", QString::SkipEmptyParts);
        for (QStringList::const_iterator it = strSplit.begin();
             it != strSplit.end(); ++it)
        {
            if (*it != "#")
            {
                const QStringList ret = extract_csv(*it);
                VERBOSE(VB_CHANNEL, QString(
                            "Icon Import: findmissing : %1 %2 %3 %4 %5")
                        .arg(ret[0]).arg(ret[1]).arg(ret[2])
                        .arg(ret[3]).arg(ret[4]));
                checkAndDownload(ret[4], (*m_iter).strChanId);
            }
        }
        return true;
    }
}

bool ImportIconsWizard::submit(const QString& strParam)
{
    int nVal = MythPopupBox::showOkCancelPopup(
                            gContext->GetMainWindow(),
                            QObject::tr("Submit information"),
                            QObject::tr("You now have the opportunity to "
                                        "transmit your choices  back to "
                                        "mythtv.org so that others can "
                                        "benefit from your selections."),
                            true);
    if (nVal == 0) // If cancel is pressed exit
    {
        m_listIcons->addSelection("Icon choices not submitted.");
        return false;
    }

    QString strParam1 = QUrl::toPercentEncoding(strParam);
    QUrl url(ImportIconsWizard::url+"/submit");

    QString str = wget(url,"csv="+strParam1);
    if (str.isEmpty() || str.startsWith("#") ||
        str.startsWith("Error", Qt::CaseInsensitive))
    {
        VERBOSE(VB_IMPORTANT, QString("Error from submit : %1").arg(str));
        m_listIcons->addSelection("Failed to submit icon choices.");
        return false;
    }
    else
    {
        VERBOSE(VB_CHANNEL, QString("Icon Import: Working submit : %1")
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
        VERBOSE(VB_CHANNEL, QString("Icon Import: working submit : atsc=%1 callsign=%2 dvb=%3 tv=%4 xmltvid=%5")
                                              .arg(atsc).arg(callsign).arg(dvb).arg(tv).arg(xmltvid));
        m_listIcons->addSelection("Icon choices submitted successfully.");
        return true;
    }
}

void ImportIconsWizard::cancelPressed()
{
    m_closeDialog=true;
}

void ImportIconsWizard::finishButtonPressed()
{
    m_closeDialog = true;
}
