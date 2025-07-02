#include <chrono> // for milliseconds
#include <thread> // for sleep_for

#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/programinfo.h"

#include "cardutil.h"
#include "inputinfo.h"
#include "remoteencoder.h"
#include "tv_rec.h"
#include "tvremoteutil.h"

uint RemoteGetFlags(uint inputid)
{
    if (gCoreContext->IsBackend())
    {
        const TVRec *rec = TVRec::GetTVRec(inputid);
        if (rec)
            return rec->GetFlags();
        return 0;
    }

    QStringList strlist(QString("QUERY_REMOTEENCODER %1").arg(inputid));
    strlist << "GET_FLAGS";
    if (!gCoreContext->SendReceiveStringList(strlist) || strlist.empty())
        return 0;

    return strlist[0].toInt();
}

uint RemoteGetState(uint inputid)
{
    if (gCoreContext->IsBackend())
    {
        const TVRec *rec = TVRec::GetTVRec(inputid);
        if (rec)
            return rec->GetState();
        return kState_ChangingState;
    }

    QStringList strlist(QString("QUERY_REMOTEENCODER %1").arg(inputid));
    strlist << "GET_STATE";
    if (!gCoreContext->SendReceiveStringList(strlist) || strlist.empty())
        return kState_ChangingState;

    return strlist[0].toInt();
}


bool RemoteRecordPending(uint inputid, const ProgramInfo *pginfo,
                         std::chrono::seconds secsleft, bool hasLater)
{
    if (gCoreContext->IsBackend())
    {
        TVRec *rec = TVRec::GetTVRec(inputid);
        if (rec)
        {
            rec->RecordPending(pginfo, secsleft, hasLater);
            return true;
        }
        return false;
    }

    QStringList strlist(QString("QUERY_REMOTEENCODER %1").arg(inputid));
    strlist << "RECORD_PENDING";
    strlist << QString::number(secsleft.count());
    strlist << QString::number(static_cast<int>(hasLater));
    pginfo->ToStringList(strlist);

    if (!gCoreContext->SendReceiveStringList(strlist) || strlist.empty())
        return false;

    return strlist[0].toUpper() == "OK";
}

bool RemoteStopLiveTV(uint inputid)
{
    if (gCoreContext->IsBackend())
    {
        TVRec *rec = TVRec::GetTVRec(inputid);
        if (rec)
        {
            rec->StopLiveTV();
            return true;
        }
        return false;
    }

    QStringList strlist(QString("QUERY_REMOTEENCODER %1").arg(inputid));
    strlist << "STOP_LIVETV";

    if (!gCoreContext->SendReceiveStringList(strlist) || strlist.empty())
        return false;

    return strlist[0].toUpper() == "OK";
}

bool RemoteStopRecording(uint inputid)
{
    if (gCoreContext->IsBackend())
    {
        TVRec *rec = TVRec::GetTVRec(inputid);
        if (rec)
        {
            rec->StopRecording();
            return true;
        }
        return false;
    }

    QStringList strlist(QString("QUERY_REMOTEENCODER %1").arg(inputid));
    strlist << "STOP_RECORDING";

    if (!gCoreContext->SendReceiveStringList(strlist) || strlist.empty())
        return false;

    return strlist[0].toUpper() == "OK";
}

void RemoteStopRecording(const ProgramInfo *pginfo)
{
    QStringList strlist(QString("STOP_RECORDING"));
    pginfo->ToStringList(strlist);

    gCoreContext->SendReceiveStringList(strlist);
}

void RemoteCancelNextRecording(uint inputid, bool cancel)
{
    QStringList strlist(QString("QUERY_RECORDER %1").arg(inputid));
    strlist << "CANCEL_NEXT_RECORDING";
    strlist << QString::number((cancel) ? 1 : 0);

    gCoreContext->SendReceiveStringList(strlist);
}

std::vector<InputInfo> RemoteRequestFreeInputInfo(uint excluded_input)
{
    LOG(VB_CHANNEL, LOG_INFO,
        QString("RemoteRequestFreeInputInfo excluding input %1")
        .arg(excluded_input));

    std::vector<InputInfo> inputs;

    QStringList strlist(QString("GET_FREE_INPUT_INFO %1")
                        .arg(excluded_input));
    if (!gCoreContext->SendReceiveStringList(strlist))
        return inputs;

    QStringList::const_iterator it = strlist.cbegin();
    while (it != strlist.cend())
    {
        InputInfo info;
        if (!info.FromStringList(it, strlist.cend()))
            break;
        inputs.push_back(info);
        LOG(VB_CHANNEL, LOG_INFO,
            QString("RemoteRequestFreeInputInfo got input %1 (%2/%3)")
            .arg(info.m_inputId).arg(info.m_chanId).arg(info.m_mplexId));
    }

    LOG(VB_CHANNEL, LOG_INFO,
        QString("RemoteRequestFreeInputInfo got %1 inputs")
        .arg(inputs.size()));
    return inputs;
}

int RemoteGetFreeRecorderCount(void)
{
    LOG(VB_CHANNEL, LOG_INFO, QString("RemoteGetFreeRecorderCount"));

    std::vector<InputInfo> inputs =
        RemoteRequestFreeInputInfo(0);

    LOG(VB_CHANNEL, LOG_INFO, QString("RemoteGetFreeRecorderCount got %1")
        .arg(inputs.size()));
    return inputs.size();
}

RemoteEncoder *RemoteRequestNextFreeRecorder(int inputid)
{
    LOG(VB_CHANNEL, LOG_INFO,
        QString("RemoteRequestNextFreeRecorder after input %1)")
        .arg(inputid));

    std::vector<InputInfo> inputs =
        RemoteRequestFreeInputInfo(inputid);

    if (inputs.empty())
    {
        LOG(VB_CHANNEL, LOG_INFO,
            QString("RemoteRequestNextFreeRecorder got no input (after input %1)")
            .arg(inputid));
        return nullptr;
    }

    size_t i = 0;
    for ( ; i < inputs.size(); ++i)
        if (inputs[i].m_inputId == (uint)inputid)
            break;

    if (i >= inputs.size())
    {
        // We should always find the referenced input.  If we don't,
        // just return the first one.
        i = 0;
    }
    else
    {
        // Try to find the next input with a different name.  If one
        // doesn't exist, just return the current one.
        size_t j = i;
        i = (i + 1) % inputs.size();
        while (i != j && inputs[i].m_displayName == inputs[j].m_displayName)
        {
            i = (i + 1) % inputs.size();
        }
    }

    LOG(VB_CHANNEL, LOG_INFO,
        QString("RemoteRequestNextFreeRecorder got input %1")
        .arg(inputs[i].m_inputId));

    return RemoteGetExistingRecorder(inputs[i].m_inputId);
}

std::vector<uint> RemoteRequestFreeRecorderList(uint excluded_input)
{
    LOG(VB_CHANNEL, LOG_INFO,
        QString("RemoteRequestFreeRecorderList excluding input %1")
        .arg(excluded_input));

    std::vector<InputInfo> inputs =
        RemoteRequestFreeInputInfo(excluded_input);

    std::vector<uint> inputids;
    std::transform(inputs.cbegin(), inputs.cend(), std::back_inserter(inputids),
                   [](const auto & input){ return input.m_inputId; } );

    LOG(VB_CHANNEL, LOG_INFO,
        QString("RemoteRequestFreeRecorderList got inputs"));
    return inputids;
}

std::vector<uint> RemoteRequestFreeInputList(uint excluded_input)
{
    LOG(VB_CHANNEL, LOG_INFO,
        QString("RemoteRequestFreeInputList excluding input %1")
        .arg(excluded_input));

    std::vector<InputInfo> inputs =
        RemoteRequestFreeInputInfo(excluded_input);

    std::vector<uint> inputids;
    std::transform(inputs.cbegin(), inputs.cend(), std::back_inserter(inputids),
                   [](const auto & input){ return input.m_inputId; } );

    LOG(VB_CHANNEL, LOG_INFO,
        QString("RemoteRequestFreeInputList got inputs"));
    return inputids;
}

RemoteEncoder *RemoteRequestFreeRecorderFromList
(const QStringList &qualifiedRecorders, uint excluded_input)
{
    LOG(VB_CHANNEL, LOG_INFO,
        QString("RemoteRequestFreeRecorderFromList excluding input %1")
        .arg(excluded_input));

    std::vector<InputInfo> inputs =
        RemoteRequestFreeInputInfo(excluded_input);

    for (const auto & recorder : std::as_const(qualifiedRecorders))
    {
        uint inputid = recorder.toUInt();
        auto sameinput = [inputid](const auto & input){ return input.m_inputId == inputid; };
        if (std::any_of(inputs.cbegin(), inputs.cend(), sameinput))
        {
            LOG(VB_CHANNEL, LOG_INFO,
                QString("RemoteRequestFreeRecorderFromList got input %1")
                .arg(inputid));
            return RemoteGetExistingRecorder(inputid);
        }
    }

    LOG(VB_CHANNEL, LOG_INFO,
        QString("RemoteRequestFreeRecorderFromList got no input"));
    return nullptr;
}

RemoteEncoder *RemoteRequestRecorder(void)
{
    LOG(VB_CHANNEL, LOG_INFO,
        QString("RemoteRequestRecorder entered"));

    std::vector<InputInfo> inputs =
        RemoteRequestFreeInputInfo(0);

    if (inputs.empty())
    {
        LOG(VB_CHANNEL, LOG_INFO,
            QString("RemoteRequestRecorder got no input"));
        return nullptr;
    }

    LOG(VB_CHANNEL, LOG_INFO,
        QString("RemoteRequestRecorder got input %1")
        .arg(inputs[0].m_inputId));
    return RemoteGetExistingRecorder(inputs[0].m_inputId);
}

RemoteEncoder *RemoteGetExistingRecorder(const ProgramInfo *pginfo)
{
    LOG(VB_CHANNEL, LOG_INFO,
        QString("RemoteGetExistingRecorder program %1")
        .arg(pginfo->GetTitle()));

    QStringList strlist( "GET_RECORDER_NUM" );
    pginfo->ToStringList(strlist);

    if (!gCoreContext->SendReceiveStringList(strlist))
    {
        LOG(VB_CHANNEL, LOG_INFO,
            QString("RemoteGetExistingRecorder got no input"));
        return nullptr;
    }

    int num = strlist[0].toInt();
    QString hostname = strlist[1];
    int port = strlist[2].toInt();

    LOG(VB_CHANNEL, LOG_INFO,
        QString("RemoteGetExistingRecorder got input %1").arg(num));
    return new RemoteEncoder(num, hostname, port);
}

RemoteEncoder *RemoteGetExistingRecorder(int recordernum)
{
    LOG(VB_CHANNEL, LOG_INFO,
        QString("RemoteGetExistingRecorder input %1")
        .arg(recordernum));

    QStringList strlist( "GET_RECORDER_FROM_NUM" );
    strlist << QString("%1").arg(recordernum);

    if (!gCoreContext->SendReceiveStringList(strlist))
    {
        LOG(VB_CHANNEL, LOG_INFO,
            QString("RemoteGetExistingRecorder got no input"));
        return nullptr;
    }

    QString hostname = strlist[0];
    int port = strlist[1].toInt();

    LOG(VB_CHANNEL, LOG_INFO,
        QString("RemoteGetExistingRecorder got input %1")
        .arg(recordernum));
    return new RemoteEncoder(recordernum, hostname, port);
}

bool RemoteIsBusy(uint inputid, InputInfo &busy_input)
{
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("RemoteIsBusy(%1) %2")
            .arg(inputid).arg(gCoreContext->IsBackend() ? "be" : "fe"));
#endif

    busy_input.Clear();

    if (gCoreContext->IsBackend())
    {
        const TVRec *rec = TVRec::GetTVRec(inputid);
        if (rec)
            return rec->IsBusy(&busy_input);

        // Note this value is intentionally different than the
        // non-backend, error value below.  There is a small
        // window when adding an input where an input can exist in
        // the database, but not yet have a TVRec.  In such cases,
        // we don't want it to be considered busy and block other
        // actions.
        return false;
    }

    QStringList strlist(QString("QUERY_REMOTEENCODER %1").arg(inputid));
    strlist << "IS_BUSY";
    if (!gCoreContext->SendReceiveStringList(strlist) || strlist.empty())
        return true;

    QStringList::const_iterator it = strlist.cbegin();
    bool state = (*it).toInt() != 0;
    ++it;
    if (!busy_input.FromStringList(it, strlist.cend()))
        state = true; // if there was an error pretend that the input is busy.

    return state;
}

bool RemoteGetRecordingStatus(
    std::vector<TunerStatus> *tunerList, bool list_inactive)
{
    bool isRecording = false;
    std::vector<uint> inputlist = CardUtil::GetInputList();

    if (tunerList)
        tunerList->clear();

    for (uint inputid : inputlist)
    {
        int         state       = kState_ChangingState;
        QString     channelName = "";
        QString     title       = "";
        QString     subtitle    = "";
        QDateTime   dtStart     = QDateTime();
        QDateTime   dtEnd       = QDateTime();
        QStringList strlist;

        QString cmd = QString("QUERY_REMOTEENCODER %1").arg(inputid);

        while (state == kState_ChangingState)
        {
            strlist = QStringList(cmd);
            strlist << "GET_STATE";
            gCoreContext->SendReceiveStringList(strlist);

            if (strlist.empty())
                break;

            state = strlist[0].toInt();
            if (kState_ChangingState == state)
                std::this_thread::sleep_for(5ms);
        }

        if (kState_RecordingOnly == state || kState_WatchingRecording == state)
        {
            isRecording = true;

            if (!tunerList)
                break;

            strlist = QStringList(QString("QUERY_RECORDER %1").arg(inputid));
            strlist << "GET_RECORDING";
            gCoreContext->SendReceiveStringList(strlist);

            ProgramInfo progInfo(strlist);

            title       = progInfo.GetTitle();
            subtitle    = progInfo.GetSubtitle();
            channelName = progInfo.GetChannelName();
            dtStart     = progInfo.GetScheduledStartTime();
            dtEnd       = progInfo.GetScheduledEndTime();
        }
        else if (!list_inactive)
        {
            continue;
        }

        if (tunerList)
        {
            TunerStatus tuner;
            tuner.id          = inputid;
            tuner.isRecording = ((kState_RecordingOnly     == state) ||
                                  (kState_WatchingRecording == state));
            tuner.channame    = channelName;
            tuner.title       = (kState_ChangingState == state) ?
                QObject::tr("Error querying recorder state") : title;
            tuner.subtitle    = subtitle;
            tuner.startTime   = dtStart;
            tuner.endTime     = dtEnd;
            tunerList->push_back(tuner);
        }
    }

    return isRecording;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
