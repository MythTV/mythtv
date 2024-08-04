/** -*- Mode: c++ -*-
 *  CetonStreamHandler
 *  Copyright (c) 2011 Ronald Frazier
 *  Copyright (c) 2009-2011 Daniel Kristjansson
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// POSIX headers
#include <fcntl.h>
#include <unistd.h>
#ifndef _WIN32
#include <sys/select.h>
#include <sys/ioctl.h>
#endif

// Qt headers
#include <QCoreApplication>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

// MythTV headers
#include "libmythbase/mythdownloadmanager.h"
#include "libmythbase/mythlogging.h"

#include "cardutil.h"
#include "cetonchannel.h"
#include "cetonstreamhandler.h"
#include "mpeg/mpegstreamdata.h"
#include "mpeg/streamlisteners.h"

#define LOC QString("CetonSH[%1](%2): ").arg(m_inputId).arg(m_device)

QMap<QString,CetonStreamHandler*> CetonStreamHandler::s_handlers;
QMap<QString,uint>                CetonStreamHandler::s_handlersRefCnt;
QMutex                            CetonStreamHandler::s_handlersLock;
QMap<QString, bool>               CetonStreamHandler::s_infoQueried;

CetonStreamHandler *CetonStreamHandler::Get(const QString &devname,
                                            int inputid)
{
    QMutexLocker locker(&s_handlersLock);

    QString devkey = devname.toUpper();

    QMap<QString,CetonStreamHandler*>::iterator it = s_handlers.find(devkey);

    if (it == s_handlers.end())
    {
        auto *newhandler = new CetonStreamHandler(devkey, inputid);
        newhandler->Open();
        s_handlers[devkey] = newhandler;
        s_handlersRefCnt[devkey] = 1;

        LOG(VB_RECORD, LOG_INFO,
            QString("CetonSH[%1]: Creating new stream handler %2 for %3")
            .arg(QString::number(inputid), devkey, devname));
    }
    else
    {
        s_handlersRefCnt[devkey]++;
        uint rcount = s_handlersRefCnt[devkey];
        LOG(VB_RECORD, LOG_INFO,
            QString("CetonSH[%1]: Using existing stream handler %2 for %3")
                .arg(QString::number(inputid), devkey, devname) +
            QString(" (%1 in use)").arg(rcount));
    }

    return s_handlers[devkey];
}

void CetonStreamHandler::Return(CetonStreamHandler * & ref, int inputid)
{
    QMutexLocker locker(&s_handlersLock);

    QString devname = ref->m_device;

    QMap<QString,uint>::iterator rit = s_handlersRefCnt.find(devname);
    if (rit == s_handlersRefCnt.end())
        return;

    QMap<QString,CetonStreamHandler*>::iterator it = s_handlers.find(devname);

    if (*rit > 1)
    {
        ref = nullptr;
        (*rit)--;
        return;
    }

    if ((it != s_handlers.end()) && (*it == ref))
    {
        LOG(VB_RECORD, LOG_INFO, QString("CetonSH[%1]: Closing handler for %2")
            .arg(inputid).arg(devname));
        ref->Close();
        delete *it;
        s_handlers.erase(it);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("CetonSH[%1] Error: Couldn't find handler for %2")
            .arg(inputid).arg(devname));
    }

    s_handlersRefCnt.erase(rit);
    ref = nullptr;
}

CetonStreamHandler::CetonStreamHandler(const QString &device, int inputid)
    : IPTVStreamHandler(IPTVTuningData("", 0, IPTVTuningData::kNone,
                                       "", 0, "", 0), inputid)
{
    setObjectName("CetonStreamHandler");

    QStringList parts = device.split("-");
    if (parts.size() != 2)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Invalid device id %1").arg(m_device));
        return;
    }
    m_ipAddress = parts.at(0);

    QStringList tuner_parts = parts.at(1).split(".");
    if (tuner_parts.size() == 2)
    {
        m_card = tuner_parts.at(0).toUInt();
        m_tuner = tuner_parts.at(1).toUInt();
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Invalid device id %1").arg(m_device));
        return;
    }

    if (GetVar("diag", "Host_IP_Address") == "")
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Ceton tuner does not seem to be available at IP");
        return;
    }

    int rtspPort = 8554;
    QString url = QString("rtsp://%1:%2/cetonmpeg%3")
        .arg(m_ipAddress).arg(rtspPort).arg(m_tuner);
    m_tuning = IPTVTuningData(url, 0, IPTVTuningData::kNone, "", 0, "", 0);
    m_useRtpStreaming = true;

    m_valid = true;

    QString cardstatus = GetVar("cas", "CardStatus");
    m_usingCablecard = cardstatus == "Inserted";

    if (!s_infoQueried.contains(m_ipAddress))
    {
        QString sernum = GetVar("diag", "Host_Serial_Number");
        QString firmware_ver = GetVar("diag", "Host_Firmware");
        QString hardware_ver = GetVar("diag", "Hardware_Revision");

        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("Ceton device %1 initialized. SN: %2, "
                    "Firmware ver. %3, Hardware ver. %4")
            .arg(m_ipAddress, sernum, firmware_ver, hardware_ver));

        if (m_usingCablecard)
        {
            QString brand = GetVar("cas", "CardManufacturer");
            QString auth = GetVar("cas", "CardAuthorization");

            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("Cable card installed (%1) - %2").arg(brand, auth));
        }
        else
        {
            LOG(VB_RECORD, LOG_INFO, LOC +
                "Cable card NOT installed (operating in QAM tuner mode)");
        }

        s_infoQueried.insert(m_ipAddress, true);
    }
}

bool CetonStreamHandler::Open(void)
{
    return Connect();
}

void CetonStreamHandler::Close(void)
{
    if (m_connected)
    {
        TunerOff();
        m_connected = false;
    }
}

bool CetonStreamHandler::Connect(void)
{
    if (!m_valid)
        return false;

    m_connected = true;
    return true;
}

bool CetonStreamHandler::EnterPowerSavingMode(void)
{
    QMutexLocker locker(&m_listenerLock);

    if (!m_streamDataList.empty())
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            "Ignoring request - video streaming active");
        return false;
    }

    locker.unlock(); // _listener_lock
    TunerOff();
    return true;
}

bool CetonStreamHandler::IsConnected(void) const
{
    return m_connected;
}

bool CetonStreamHandler::VerifyTuning(void)
{
    if (IsCableCardInstalled())
    {
        uint prog = GetVar("mux", "ProgramNumber").toUInt();
        if (prog == 0)
        {
            LOG(VB_RECORD, LOG_WARNING, LOC +
                "VerifyTuning detected program = 0");
            return false;
        }
    }
    else
    {
        uint frequency = GetVar("tuner", "Frequency").toUInt();
        if (frequency != m_lastFrequency)
        {
            LOG(VB_RECORD, LOG_WARNING, LOC +
                "VerifyTuning detected wrong frequency");
            return false;
        }

        QString modulation = GetVar("tuner", "Modulation");
        if (modulation.toUpper() != m_lastModulation.toUpper())
        {
            LOG(VB_RECORD, LOG_WARNING, LOC +
                "VerifyTuning detected wrong modulation");
            return false;
        }

        uint program = GetVar("mux", "ProgramNumber").toUInt();
        if (program != m_lastProgram)
        {
            LOG(VB_RECORD, LOG_WARNING, LOC +
                "VerifyTuning detected wrong program");
            return false;
        }
    }

    QString carrier_lock = GetVar("tuner", "CarrierLock");
    if (carrier_lock != "1")
    {
        LOG(VB_RECORD, LOG_WARNING, LOC +
            "VerifyTuning detected no carrier lock");
        return false;
    }

    QString pcr_lock = GetVar("tuner", "PCRLock");
    if (pcr_lock != "1")
    {
        LOG(VB_RECORD, LOG_WARNING, LOC +
            "VerifyTuning detected no PCR lock");
        return false;
    }

    return true;
}

void CetonStreamHandler::RepeatTuning(void)
{
    if (IsCableCardInstalled())
    {
        TuneVChannel(m_lastVchannel);
    }
    else
    {
        TuneFrequency(m_lastFrequency, m_lastModulation);
        TuneProgram(m_lastProgram);
    }
}

bool CetonStreamHandler::TunerOff(void)
{
    bool result = false;
    if (m_usingCablecard)
        result = TuneVChannel("0");
    else
        result = TuneFrequency(0, "qam_256");

    return result;
}

bool CetonStreamHandler::TuneFrequency(
    uint frequency, const QString &modulation)
{
    LOG(VB_RECORD, LOG_INFO, LOC + QString("TuneFrequency(%1, %2)")
        .arg(frequency).arg(modulation));

    if (frequency >= 100000000)
        frequency /= 1000;

    QString modulation_id;
    if (modulation == "qam_256")
        modulation_id = "2";
    else if (modulation == "qam_64")
        modulation_id = "0";
    else if (modulation == "ntsc-m")
        modulation_id = "4";
    else if (modulation == "8vsb")
        modulation_id = "6";
    else
        return false;

    m_lastFrequency = frequency;
    m_lastModulation = modulation;

    QUrlQuery params;
    params.addQueryItem("instance_id", QString::number(m_tuner));
    params.addQueryItem("frequency", QString::number(frequency));
    params.addQueryItem("modulation",modulation_id);
    params.addQueryItem("tuner","1");
    params.addQueryItem("demod","1");
    params.addQueryItem("rst_chnl","0");
    params.addQueryItem("force_tune","0");

    QString response;
    uint status = 0;
    bool result =  HttpRequest(
        "POST", "/tune_request.cgi", params, response, status);

    if (!result)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("TuneFrequency() - HTTP status = %1 - response = %2")
            .arg(status).arg(response));
    }

    return result;
}

bool CetonStreamHandler::TuneProgram(uint program)
{
    LOG(VB_RECORD, LOG_INFO, LOC + QString("TuneProgram(%1)").arg(program));

    QStringList program_list = GetProgramList();
    if (!program_list.contains(QString::number(program)))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
        QString("TuneProgram(%1) - Requested program not in the program list")
            .arg(program));
        return false;
    };


    m_lastProgram = program;

    QUrlQuery params;
    params.addQueryItem("instance_id", QString::number(m_tuner));
    params.addQueryItem("program", QString::number(program));

    QString response;
    uint status = 0;
    bool result = HttpRequest(
        "POST", "/program_request.cgi", params, response, status);

    if (!result)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("TuneProgram() - HTTP status = %1 - response = %2")
            .arg(status).arg(response));
    }

    return result;
}

bool CetonStreamHandler::PerformTuneVChannel(const QString &vchannel)
{
    LOG(VB_RECORD, LOG_INFO, LOC + QString("PerformTuneVChannel(%1)")
        .arg(vchannel));

    QUrlQuery params;
    params.addQueryItem("instance_id", QString::number(m_tuner));
    params.addQueryItem("channel", vchannel);

    QString response;
    uint status = 0;
    bool result = HttpRequest(
        "POST", "/channel_request.cgi", params, response, status);

    if (!result)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("PerformTuneVChannel() - HTTP status = %1 - response = %2")
            .arg(status).arg(response));
    }

    return result;
}


bool CetonStreamHandler::TuneVChannel(const QString &vchannel)
{
    if (GetVar("cas", "VirtualChannelNumber") == vchannel)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + QString("Not Re-Tuning channel %1")
            .arg(vchannel));
        return true;
    }

    if ((vchannel != "0") && (m_lastVchannel != "0"))
        ClearProgramNumber();

    LOG(VB_RECORD, LOG_INFO, LOC + QString("TuneVChannel(%1)").arg(vchannel));

    m_lastVchannel = vchannel;

    return PerformTuneVChannel(vchannel);
}

void CetonStreamHandler::ClearProgramNumber(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + QString("ClearProgramNumber()"));
    PerformTuneVChannel("0");
    for(int i=0; i<50;  i++)
    {
        if (GetVar("mux", "ProgramNumber") == "0")
            return;
        usleep(20ms);
    };

    LOG(VB_GENERAL, LOG_ERR, LOC + "Program number failed to clear");
}

uint CetonStreamHandler::GetProgramNumber(void) const
{
    for(int i = 1; i <= 30; i++)
    {
        QString prog = GetVar("mux", "ProgramNumber");
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("GetProgramNumber() got %1 on attempt %2")
            .arg(prog).arg(i));

        uint prognum = prog.toUInt();
        if (prognum != 0)
            return prognum;

        usleep(100ms);
    };

    LOG(VB_GENERAL, LOG_ERR, LOC +
        "GetProgramNumber() failed to get a non-zero program number");

    return 0;
}

QString CetonStreamHandler::GetVar(
    const QString &section, const QString &variable) const
{
    QString loc = LOC + QString("DoGetVar(%1,%2,%3,%4) - ")
        .arg(m_ipAddress).arg(m_tuner).arg(section,variable);

    QUrlQuery params;
    params.addQueryItem("i", QString::number(m_tuner));
    params.addQueryItem("s", section);
    params.addQueryItem("v", variable);

    QString response;
    uint status = 0;
    if (!HttpRequest("GET", "/get_var.json", params, response, status))
    {
        LOG(VB_GENERAL, LOG_ERR, loc +
            QString("HttpRequest failed - %1").arg(response));
        return {};
    }

    static const QRegularExpression regex { "^\\{ \"?result\"?: \"(.*)\" \\}$"};
    auto match = regex.match(response);
    if (!match.hasMatch())
    {
        LOG(VB_GENERAL, LOG_ERR, loc +
            QString("unexpected http response: -->%1<--").arg(response));
        return {};
    }

    QString result = match.captured(1);
    LOG(VB_RECORD, LOG_DEBUG, loc + QString("got: -->%1<--").arg(result));
    return result;
}

QStringList CetonStreamHandler::GetProgramList()
{
    QString loc = LOC + QString("CetonHTTP: DoGetProgramList(%1,%2) - ")
        .arg(m_ipAddress).arg(m_tuner);

    QUrlQuery params;
    params.addQueryItem("i", QString::number(m_tuner));

    QString response;
    uint status = 0;
    if (!HttpRequest("GET", "/get_pat.json", params, response, status))
    {
        LOG(VB_GENERAL, LOG_ERR,
            loc + QString("HttpRequest failed - %1").arg(response));
        return {};
    }

    static const QRegularExpression regex(
        R"(^\{ "?length"?: \d+(, "?results"?: \[ (.*) \])? \}$)");

    auto match = regex.match(response);
    if (!match.hasMatch())
    {
        LOG(VB_GENERAL, LOG_ERR,
            loc + QString("returned unexpected output: -->%1<--")
            .arg(response));
        return {};
    }

    LOG(VB_RECORD, LOG_DEBUG, loc + QString("got: -->%1<--")
        .arg(match.captured(2)));
    return match.captured(2).split(", ");
}

bool CetonStreamHandler::HttpRequest(
    const QString &method, const QString &script,
    const QUrlQuery &params,
    QString &response, uint &status_code) const
{
    QUrl url;
    auto *request = new QNetworkRequest();
    QByteArray data;
    MythDownloadManager *manager = GetMythDownloadManager();

    url.setScheme("http");
    url.setHost(m_ipAddress);
    url.setPath(script);

    // Specify un-cached access to the device
    // The next two lines are automagically added by Qt
    // request->setRawHeader("Cache-Control", "no-cache");
    // request->setRawHeader("Pragma", "no-cache");
    request->setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                          QNetworkRequest::AlwaysNetwork);

    if ("GET" == method)
    {
        url.setQuery(params);
        request->setUrl(url);
        if (manager->download(request, &data))
        {
            response = QString(data);
            status_code = 200;
            return true;
        }

        response = "Download failed";
        status_code = 500;
        return false;
    }
    if ("POST" == method)
    {
        request->setUrl(url);
        request->setHeader(QNetworkRequest::ContentTypeHeader,
                           "application/x-www-form-urlencoded");
        data = params.query(QUrl::FullyEncoded).toUtf8();
        // Next line automagically added by Qt
        // request->setHeader(QNetworkRequest::ContentLengthHeader, data.size());
        if (manager->post(request, &data))
        {
            response = QString(data);
            status_code = 200;
            return true;
        }

        response = "Download failed";
        status_code = 500;
        return false;
    }

    delete request;

    response = "Unsupported HttpRequest method";
    status_code = 500;
    return false;
}
