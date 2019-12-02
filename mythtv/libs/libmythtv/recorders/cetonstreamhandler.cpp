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
#include <QUrl>
#include <QUrlQuery>

// MythTV headers
#include "cetonstreamhandler.h"
#include "streamlisteners.h"
#include "mpegstreamdata.h"
#include "cetonchannel.h"
#include "mythlogging.h"
#include "cardutil.h"
#include "mythdownloadmanager.h"

#define LOC QString("CetonSH[%1](%2): ").arg(m_inputid).arg(m_device)

QMap<QString,CetonStreamHandler*> CetonStreamHandler::s_handlers;
QMap<QString,uint>                CetonStreamHandler::s_handlers_refcnt;
QMutex                            CetonStreamHandler::s_handlers_lock;
QMap<QString, bool>               CetonStreamHandler::s_info_queried;

CetonStreamHandler *CetonStreamHandler::Get(const QString &devname,
                                            int inputid)
{
    QMutexLocker locker(&s_handlers_lock);

    QString devkey = devname.toUpper();

    QMap<QString,CetonStreamHandler*>::iterator it = s_handlers.find(devkey);

    if (it == s_handlers.end())
    {
        auto *newhandler = new CetonStreamHandler(devkey, inputid);
        newhandler->Open();
        s_handlers[devkey] = newhandler;
        s_handlers_refcnt[devkey] = 1;

        LOG(VB_RECORD, LOG_INFO,
            QString("CetonSH[%1]: Creating new stream handler %2 for %3")
            .arg(inputid).arg(devkey).arg(devname));
    }
    else
    {
        s_handlers_refcnt[devkey]++;
        uint rcount = s_handlers_refcnt[devkey];
        LOG(VB_RECORD, LOG_INFO,
            QString("CetonSH[%1]: Using existing stream handler %2 for %3")
            .arg(inputid).arg(devkey)
            .arg(devname) + QString(" (%1 in use)").arg(rcount));
    }

    return s_handlers[devkey];
}

void CetonStreamHandler::Return(CetonStreamHandler * & ref, int inputid)
{
    QMutexLocker locker(&s_handlers_lock);

    QString devname = ref->m_device;

    QMap<QString,uint>::iterator rit = s_handlers_refcnt.find(devname);
    if (rit == s_handlers_refcnt.end())
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

    s_handlers_refcnt.erase(rit);
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
    m_ip_address = parts.at(0);

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
        .arg(m_ip_address).arg(rtspPort).arg(m_tuner);
    m_tuning = IPTVTuningData(url, 0, IPTVTuningData::kNone, "", 0, "", 0);
    m_use_rtp_streaming = true;

    m_valid = true;

    QString cardstatus = GetVar("cas", "CardStatus");
    m_using_cablecard = cardstatus == "Inserted";

    if (!s_info_queried.contains(m_ip_address))
    {
        QString sernum = GetVar("diag", "Host_Serial_Number");
        QString firmware_ver = GetVar("diag", "Host_Firmware");
        QString hardware_ver = GetVar("diag", "Hardware_Revision");

        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("Ceton device %1 initialized. SN: %2, "
                    "Firmware ver. %3, Hardware ver. %4")
            .arg(m_ip_address).arg(sernum).arg(firmware_ver).arg(hardware_ver));

        if (m_using_cablecard)
        {
            QString brand = GetVar("cas", "CardManufacturer");
            QString auth = GetVar("cas", "CardAuthorization");

            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("Cable card installed (%1) - %2").arg(brand).arg(auth));
        }
        else
        {
            LOG(VB_RECORD, LOG_INFO, LOC +
                "Cable card NOT installed (operating in QAM tuner mode)");
        }

        s_info_queried.insert(m_ip_address, true);
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
    QMutexLocker locker(&m_listener_lock);

    if (!m_stream_data_list.empty())
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
        if (frequency != m_last_frequency)
        {
            LOG(VB_RECORD, LOG_WARNING, LOC +
                "VerifyTuning detected wrong frequency");
            return false;
        }

        QString modulation = GetVar("tuner", "Modulation");
        if (modulation.toUpper() != m_last_modulation.toUpper())
        {
            LOG(VB_RECORD, LOG_WARNING, LOC +
                "VerifyTuning detected wrong modulation");
            return false;
        }

        uint program = GetVar("mux", "ProgramNumber").toUInt();
        if (program != m_last_program)
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
        TuneVChannel(m_last_vchannel);
    }
    else
    {
        TuneFrequency(m_last_frequency, m_last_modulation);
        TuneProgram(m_last_program);
    }
}

bool CetonStreamHandler::TunerOff(void)
{
    bool result;
    if (m_using_cablecard)
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

    QString modulation_id = (modulation == "qam_256") ? "2" :
                            (modulation == "qam_64")  ? "0" :
                            (modulation == "ntsc-m") ? "4" :
                            (modulation == "8vsb")   ? "6" :
                                                       "";
    if (modulation_id == "")
        return false;

    m_last_frequency = frequency;
    m_last_modulation = modulation;

    QUrlQuery params;
    params.addQueryItem("instance_id", QString::number(m_tuner));
    params.addQueryItem("frequency", QString::number(frequency));
    params.addQueryItem("modulation",modulation_id);
    params.addQueryItem("tuner","1");
    params.addQueryItem("demod","1");
    params.addQueryItem("rst_chnl","0");
    params.addQueryItem("force_tune","0");

    QString response;
    uint status;
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


    m_last_program = program;

    QUrlQuery params;
    params.addQueryItem("instance_id", QString::number(m_tuner));
    params.addQueryItem("program", QString::number(program));

    QString response;
    uint status;
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
    uint status;
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

    if ((vchannel != "0") && (m_last_vchannel != "0"))
        ClearProgramNumber();

    LOG(VB_RECORD, LOG_INFO, LOC + QString("TuneVChannel(%1)").arg(vchannel));

    m_last_vchannel = vchannel;

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
        usleep(20000);
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

        usleep(100000);
    };

    LOG(VB_GENERAL, LOG_ERR, LOC +
        "GetProgramNumber() failed to get a non-zero program number");

    return 0;
}

QString CetonStreamHandler::GetVar(
    const QString &section, const QString &variable) const
{
    QString loc = LOC + QString("DoGetVar(%1,%2,%3,%4) - ")
        .arg(m_ip_address).arg(m_tuner).arg(section,variable);

    QUrlQuery params;
    params.addQueryItem("i", QString::number(m_tuner));
    params.addQueryItem("s", section);
    params.addQueryItem("v", variable);

    QString response;
    uint status;
    if (!HttpRequest("GET", "/get_var.json", params, response, status))
    {
        LOG(VB_GENERAL, LOG_ERR, loc +
            QString("HttpRequest failed - %1").arg(response));
        return QString();
    }

    QRegExp regex("^\\{ \"?result\"?: \"(.*)\" \\}$");
    if (regex.indexIn(response) == -1)
    {
        LOG(VB_GENERAL, LOG_ERR, loc +
            QString("unexpected http response: -->%1<--").arg(response));
        return QString();
    }

    QString result = regex.cap(1);
    LOG(VB_RECORD, LOG_DEBUG, loc + QString("got: -->%1<--").arg(result));
    return result;
}

QStringList CetonStreamHandler::GetProgramList()
{
    QString loc = LOC + QString("CetonHTTP: DoGetProgramList(%1,%2) - ")
        .arg(m_ip_address).arg(m_tuner);

    QUrlQuery params;
    params.addQueryItem("i", QString::number(m_tuner));

    QString response;
    uint status;
    if (!HttpRequest("GET", "/get_pat.json", params, response, status))
    {
        LOG(VB_GENERAL, LOG_ERR,
            loc + QString("HttpRequest failed - %1").arg(response));
        return QStringList();
    }

    QRegExp regex(
        "^\\{ \"?length\"?: \\d+(, \"?results\"?: \\[ (.*) \\])? \\}$");

    if (regex.indexIn(response) == -1)
    {
        LOG(VB_GENERAL, LOG_ERR,
            loc + QString("returned unexpected output: -->%1<--")
            .arg(response));
        return QStringList();
    }

    LOG(VB_RECORD, LOG_DEBUG, loc + QString("got: -->%1<--").arg(regex.cap(2)));
    return regex.cap(2).split(", ");
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
    url.setHost(m_ip_address);
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
