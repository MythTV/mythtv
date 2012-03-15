/** -*- Mode: c++ -*-
 *  CetonStreamHandler
 *  Copyright (c) 2011 Ronald Frazier
 *  Copyright (c) 2009-2011 Daniel Kristjansson
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// POSIX headers
#include <fcntl.h>
#include <unistd.h>
#ifndef USING_MINGW
#include <sys/select.h>
#include <sys/ioctl.h>
#endif

// Qt headers
#include <QCoreApplication>
#include <QBuffer>
#include <QFile>
#include <QHttp>
#include <QUrl>

// MythTV headers
#include "cetonstreamhandler.h"
#include "streamlisteners.h"
#include "mpegstreamdata.h"
#include "cetonchannel.h"
#include "mythlogging.h"
#include "cetonrtp.h"
#include "cardutil.h"

#define LOC QString("CetonSH(%1): ").arg(_device)

QMap<QString,CetonStreamHandler*> CetonStreamHandler::_handlers;
QMap<QString,uint>                CetonStreamHandler::_handlers_refcnt;
QMutex                            CetonStreamHandler::_handlers_lock;
QMap<QString, bool>               CetonStreamHandler::_info_queried;

CetonStreamHandler *CetonStreamHandler::Get(const QString &devname)
{
    QMutexLocker locker(&_handlers_lock);

    QString devkey = devname.toUpper();

    QMap<QString,CetonStreamHandler*>::iterator it = _handlers.find(devkey);

    if (it == _handlers.end())
    {
        CetonStreamHandler *newhandler = new CetonStreamHandler(devkey);
        newhandler->Open();
        _handlers[devkey] = newhandler;
        _handlers_refcnt[devkey] = 1;

        LOG(VB_RECORD, LOG_INFO,
            QString("CetonSH: Creating new stream handler %1 for %2")
                .arg(devkey).arg(devname));
    }
    else
    {
        _handlers_refcnt[devkey]++;
        uint rcount = _handlers_refcnt[devkey];
        LOG(VB_RECORD, LOG_INFO,
            QString("CetonSH: Using existing stream handler %1 for %2")
                .arg(devkey)
                .arg(devname) + QString(" (%1 in use)").arg(rcount));
    }

    return _handlers[devkey];
}

void CetonStreamHandler::Return(CetonStreamHandler * & ref)
{
    QMutexLocker locker(&_handlers_lock);

    QString devname = ref->_device;

    QMap<QString,uint>::iterator rit = _handlers_refcnt.find(devname);
    if (rit == _handlers_refcnt.end())
        return;

    if (*rit > 1)
    {
        ref = NULL;
        (*rit)--;
        return;
    }

    QMap<QString,CetonStreamHandler*>::iterator it = _handlers.find(devname);
    if ((it != _handlers.end()) && (*it == ref))
    {
        LOG(VB_RECORD, LOG_INFO, QString("CetonSH: Closing handler for %1")
                           .arg(devname));
        ref->Close();
        delete *it;
        _handlers.erase(it);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("CetonSH Error: Couldn't find handler for %1")
                .arg(devname));
    }

    _handlers_refcnt.erase(rit);
    ref = NULL;
}

CetonStreamHandler::CetonStreamHandler(const QString &device) :
    StreamHandler(device),
    _connected(false),
    _valid(false)
{
    setObjectName("CetonStreamHandler");

    QStringList parts = device.split("-");
    if (parts.size() != 2)
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("Invalid device id %1").arg(_device));
        return;
    }
    _ip_address = parts.at(0);

    QStringList tuner_parts = parts.at(1).split(".");
    if (tuner_parts.size() == 2)
    {
        _using_rtp = (tuner_parts.at(0) == "RTP");
        _card = tuner_parts.at(0).toUInt();
        _tuner = tuner_parts.at(1).toUInt();
    }
    else
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("Invalid device id %1").arg(_device));
        return;
    }

    if (GetVar("diag", "Host_IP_Address") == "")
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("Ceton tuner does not seem to be available at IP"));
        return;
    }

    if (!_using_rtp)
    {
        _device_path = QString("/dev/ceton/ctn91xx_mpeg%1_%2")
            .arg(_card).arg(_tuner);

        if (!QFile(_device_path).exists())
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                QString("Tuner device unavailable"));
            return;
        }
    }

    _valid = true;

    QString cardstatus = GetVar("cas", "CardStatus");
    _using_cablecard = cardstatus == "Inserted";

    if (!_info_queried.contains(_ip_address))
    {
        QString sernum = GetVar("diag", "Host_Serial_Number");
        QString firmware_ver = GetVar("diag", "Host_Firmware");
        QString hardware_ver = GetVar("diag", "Hardware_Revision");

        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("Ceton device %1 initialized. SN: %2, "
                    "Firmware ver. %3, Hardware ver. %4")
            .arg(_ip_address).arg(sernum).arg(firmware_ver).arg(hardware_ver));

        if (_using_cablecard)
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

        _info_queried.insert(_ip_address, true);
    }
}

void CetonStreamHandler::run(void)
{
    RunProlog();
    bool _error = false;

    QFile file(_device_path);
    CetonRTP rtp(_ip_address, _tuner);

    if (_using_rtp)
    {
        if (!(rtp.Init() && rtp.StartStreaming()))
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                "Starting recording (RTP initialization failed). Aborting.");
            _error = true;
        }
    }
    else
    {
        if (!file.open(QIODevice::ReadOnly))
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                "Starting recording (file open failed). Aborting.");
            _error = true;
        }

        int flags = fcntl(file.handle(), F_GETFL, 0);
        if (flags == -1) flags = 0;
        fcntl(file.handle(), F_SETFL, flags | O_NONBLOCK);
    }

    if (_error)
    {
        RunEpilog();
        return;
    }

    SetRunning(true, false, false);

    int buffer_size = (64 * 1024); // read about 64KB
    buffer_size /= TSPacket::kSize;
    buffer_size *= TSPacket::kSize;
    char *buffer = new char[buffer_size];

    LOG(VB_RECORD, LOG_INFO, LOC + "RunTS(): begin");

    _read_timer.start();

    int remainder = 0;
    while (_running_desired && !_error)
    {
        int bytes_read;
        if (_using_rtp)
            bytes_read = rtp.Read(buffer, buffer_size);
        else
            bytes_read = file.read(buffer, buffer_size);

        if (bytes_read <= 0)
        {
            if (_read_timer.elapsed() >= 5000)
            {
                LOG(VB_RECORD, LOG_WARNING, LOC +
                    "No data received for 5 seconds...checking tuning");
                if (!VerifyTuning())
                    RepeatTuning();
                _read_timer.start();
            }

            usleep(5000);
            continue;
        }

        _read_timer.start();

        _listener_lock.lock();

        if (_stream_data_list.empty())
        {
            _listener_lock.unlock();
            continue;
        }

        StreamDataList::const_iterator sit = _stream_data_list.begin();
        for (; sit != _stream_data_list.end(); ++sit)
            remainder = sit.key()->ProcessData(
                reinterpret_cast<unsigned char*>(buffer), bytes_read);

        _listener_lock.unlock();
        if (remainder != 0)
        {
            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("RunTS(): bytes_read = %1 remainder = %2")
                    .arg(bytes_read).arg(remainder));
        }
    }
    LOG(VB_RECORD, LOG_INFO, LOC + "RunTS(): " + "shutdown");

    if (_using_rtp)
        rtp.StopStreaming();
    else
        file.close();

    delete[] buffer;

    LOG(VB_RECORD, LOG_INFO, LOC + "RunTS(): " + "end");

    SetRunning(false, false, false);
    RunEpilog();
}

bool CetonStreamHandler::Open(void)
{
    return Connect();
}

void CetonStreamHandler::Close(void)
{
    if (_connected)
    {
        TunerOff();
        _connected = false;
    }
}

bool CetonStreamHandler::Connect(void)
{
    if (!_valid)
        return false;

    _connected = true;
    return true;
}

bool CetonStreamHandler::EnterPowerSavingMode(void)
{
    QMutexLocker locker(&_listener_lock);

    if (!_stream_data_list.empty())
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            "Ignoring request - video streaming active");
        return false;
    }
    else
    {
        locker.unlock(); // _listener_lock
        TunerOff();
        return true;
    }
}

bool CetonStreamHandler::IsConnected(void) const
{
    return _connected;
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
        if (frequency != _last_frequency)
        {
            LOG(VB_RECORD, LOG_WARNING, LOC +
                "VerifyTuning detected wrong frequency");
            return false;
        }

        QString modulation = GetVar("tuner", "Modulation");
        if (modulation.toUpper() != _last_modulation.toUpper())
        {
            LOG(VB_RECORD, LOG_WARNING, LOC +
                "VerifyTuning detected wrong modulation");
            return false;
        }

        uint program = GetVar("mux", "ProgramNumber").toUInt();
        if (program != _last_program)
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
        TuneVChannel(_last_vchannel);
    }
    else
    {
        TuneFrequency(_last_frequency, _last_modulation);
        TuneProgram(_last_program);
    }
}

bool CetonStreamHandler::TunerOff(void)
{
    bool result = TuneFrequency(0, "qam_256");
    if (result && _using_cablecard)
        result = TuneVChannel("0");

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

    _last_frequency = frequency;
    _last_modulation = modulation;

    QUrl params;
    params.addQueryItem("instance_id", QString::number(_tuner));
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
        LOG(VB_RECORD, LOG_ERR, LOC +
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
        LOG(VB_RECORD, LOG_ERR, LOC + 
        QString("TuneProgram(%1) - Requested program not in the program list").arg(program));
        return false;
    };


    _last_program = program;

    QUrl params;
    params.addQueryItem("instance_id", QString::number(_tuner));
    params.addQueryItem("program", QString::number(program));

    QString response;
    uint status;
    bool result = HttpRequest(
        "POST", "/program_request.cgi", params, response, status);

    if (!result)
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("TuneProgram() - HTTP status = %1 - response = %2")
            .arg(status).arg(response));
    }

    return result;
}

bool CetonStreamHandler::TuneVChannel(const QString &vchannel)
{
    if ((vchannel != "0") && (_last_vchannel != "0"))
        ClearProgramNumber();

    LOG(VB_RECORD, LOG_INFO, LOC + QString("TuneVChannel(%1)").arg(vchannel));

    _last_vchannel = vchannel;

    QUrl params;
    params.addQueryItem("instance_id", QString::number(_tuner));
    params.addQueryItem("channel", vchannel);

    QString response;
    uint status;
    bool result = HttpRequest(
        "POST", "/channel_request.cgi", params, response, status);

    if (!result)
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("TuneVChannel() - HTTP status = %1 - response = %2")
            .arg(status).arg(response));
    }

    return result;
}

void CetonStreamHandler::ClearProgramNumber(void)
{
    TuneVChannel("0");
    for(int i=0; i<50;  i++)
    {
        if (GetVar("mux", "ProgramNumber") == "0")
            return;
        usleep(20000);
    };

    LOG(VB_RECORD, LOG_ERR, LOC + QString("Program number failed to clear"));
}

uint CetonStreamHandler::GetProgramNumber(void) const
{
    for(int i = 1; i <= 30; i++)
    {
        QString prog = GetVar("mux", "ProgramNumber");
        LOG(VB_RECORD, LOG_INFO, LOC + QString("GetProgramNumber() got %1 on attempt %2").arg(prog).arg(i));

        uint prognum = prog.toUInt();
        if (prognum != 0) 
            return prognum;

        usleep(100000);
    };

    LOG(VB_RECORD, LOG_ERR, LOC + QString("Error: GetProgramNumber() failed to get a non-zero program number"));
    return 0;
}

QString CetonStreamHandler::GetVar(
    const QString &section, const QString &variable) const
{
    QString loc = LOC + QString("DoGetVar(%1,%2,%3,%4) - ")
        .arg(_ip_address).arg(_tuner).arg(section,variable);

    QUrl params;
    params.addQueryItem("i", QString::number(_tuner));
    params.addQueryItem("s", section);
    params.addQueryItem("v", variable);

    QString response;
    uint status;
    if (!HttpRequest("GET", "/get_var.json", params, response, status))
    {
        LOG(VB_RECORD, LOG_ERR, loc +
            QString("HttpRequest failed - %1").arg(response));
        return QString();
    }

    QRegExp regex("^\\{ \"?result\"?: \"(.*)\" \\}$");
    if (regex.indexIn(response) == -1)
    {
        LOG(VB_RECORD, LOG_ERR, loc +
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
        .arg(_ip_address).arg(_tuner);

    QUrl params;
    params.addQueryItem("i", QString::number(_tuner));

    QString response;
    uint status;
    if (!HttpRequest("GET", "/get_pat.json", params, response, status))
    {
        LOG(VB_RECORD, LOG_ERR,
            loc + QString("HttpRequest failed - %1").arg(response));
        return QStringList();
    }

    QRegExp regex(
        "^\\{ \"?length\"?: \\d+(, \"?results\"?: \\[ (.*) \\])? \\}$");

    if (regex.indexIn(response) == -1)
    {
        LOG(VB_RECORD, LOG_ERR,
            loc + QString("returned unexpected output: -->%1<--")
            .arg(response));
        return QStringList();
    }

    LOG(VB_RECORD, LOG_DEBUG, loc + QString("got: -->%1<--").arg(regex.cap(2)));
    return regex.cap(2).split(", ");
}

bool CetonStreamHandler::HttpRequest(
    const QString &method, const QString &script, const QUrl &params,
    QString &response, uint &status_code) const
{
    QHttp http;
    http.setHost(_ip_address);

    QByteArray request_params(params.encodedQuery());

    if (method == "GET")
    {
        QString path = script + "?" + QString(request_params);
        QHttpRequestHeader header(method, path);
        header.setValue("Host", _ip_address);
        http.request(header);
    }
    else
    {
        QHttpRequestHeader header(method, script);
        header.setValue("Host", _ip_address);
        header.setContentType("application/x-www-form-urlencoded");
        http.request(header, request_params);
    }

    while (http.hasPendingRequests() || http.currentId())
    {
        usleep(5000);
        qApp->processEvents();
    }

    if (http.error() != QHttp::NoError)
    {
        status_code = 0;
        response = http.errorString();
        return false;
    }

    QHttpResponseHeader resp_header = http.lastResponse();
    if (!resp_header.isValid())
    {
        status_code = 0;
        response = "Completed but response object was not valid";
        return false;
    }

    status_code = resp_header.statusCode();
    response = QString(http.readAll());
    return true;
}
