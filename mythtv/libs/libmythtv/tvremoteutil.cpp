#include <unistd.h>

#include <QFile>

#include "tvremoteutil.h"
#include "cardutil.h"
#include "inputinfo.h"
#include "programinfo.h"
#include "mythcorecontext.h"
#include "remoteencoder.h"
#include "tv_rec.h"

uint RemoteGetFlags(uint cardid)
{
    if (gCoreContext->IsBackend())
    {
        const TVRec *rec = TVRec::GetTVRec(cardid);
        if (rec)
            return rec->GetFlags();
    }

    QStringList strlist(QString("QUERY_REMOTEENCODER %1").arg(cardid));
    strlist << "GET_FLAGS";
    if (!gCoreContext->SendReceiveStringList(strlist) || strlist.empty())
        return 0;

    return strlist[0].toInt();
}

uint RemoteGetState(uint cardid)
{
    if (gCoreContext->IsBackend())
    {
        const TVRec *rec = TVRec::GetTVRec(cardid);
        if (rec)
            return rec->GetState();
    }

    QStringList strlist(QString("QUERY_REMOTEENCODER %1").arg(cardid));
    strlist << "GET_STATE";
    if (!gCoreContext->SendReceiveStringList(strlist) || strlist.empty())
        return kState_ChangingState;

    return strlist[0].toInt();
}


bool RemoteRecordPending(uint cardid, const ProgramInfo *pginfo,
                         int secsleft, bool hasLater)
{
    if (gCoreContext->IsBackend())
    {
        TVRec *rec = TVRec::GetTVRec(cardid);
        if (rec)
        {
            rec->RecordPending(pginfo, secsleft, hasLater);
            return true;
        }
    }

    QStringList strlist(QString("QUERY_REMOTEENCODER %1").arg(cardid));
    strlist << "RECORD_PENDING";
    strlist << QString::number(secsleft);
    strlist << QString::number(hasLater);
    pginfo->ToStringList(strlist);

    if (!gCoreContext->SendReceiveStringList(strlist) || strlist.empty())
        return false;

    return strlist[0].toUpper() == "OK";
}

bool RemoteStopLiveTV(uint cardid)
{
    if (gCoreContext->IsBackend())
    {
        TVRec *rec = TVRec::GetTVRec(cardid);
        if (rec)
        {
            rec->StopLiveTV();
            return true;
        }
    }

    QStringList strlist(QString("QUERY_REMOTEENCODER %1").arg(cardid));
    strlist << "STOP_LIVETV";

    if (!gCoreContext->SendReceiveStringList(strlist) || strlist.empty())
        return false;

    return strlist[0].toUpper() == "OK";
}

bool RemoteStopRecording(uint cardid)
{
    if (gCoreContext->IsBackend())
    {
        TVRec *rec = TVRec::GetTVRec(cardid);
        if (rec)
        {
            rec->StopRecording();
            return true;
        }
    }

    QStringList strlist(QString("QUERY_REMOTEENCODER %1").arg(cardid));
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

void RemoteCancelNextRecording(uint cardid, bool cancel)
{
    QStringList strlist(QString("QUERY_RECORDER %1").arg(cardid));
    strlist << "CANCEL_NEXT_RECORDING";
    strlist << QString::number((cancel) ? 1 : 0);

    gCoreContext->SendReceiveStringList(strlist);
}

RemoteEncoder *RemoteRequestNextFreeRecorder(int curr)
{
    QStringList strlist( "GET_NEXT_FREE_RECORDER" );
    strlist << QString("%1").arg(curr);

    if (!gCoreContext->SendReceiveStringList(strlist, true))
        return NULL;

    int num = strlist[0].toInt();
    QString hostname = strlist[1];
    int port = strlist[2].toInt();

    return new RemoteEncoder(num, hostname, port);
}

static void
RemoteRequestFreeRecorderInputList(const vector<uint> &excluded_cardids,
                                   vector<uint> &cardids,
                                   vector<uint> &inputids)
{
#if 0
    vector<uint> list;

    QStringList strlist("GET_FREE_RECORDER_LIST");

    if (!gCoreContext->SendReceiveStringList(strlist, true))
        return list;

    QStringList::const_iterator it = strlist.begin();
    for (; it != strlist.end(); ++it) 
        list.push_back((*it).toUInt());

    return list;
#endif
    vector<uint> cards = CardUtil::GetLiveTVCardList();
    for (uint i = 0; i < cards.size(); i++)
    {
        vector<InputInfo> inputs =
            RemoteRequestFreeInputList(cards[i], excluded_cardids);
        for (uint j = 0; j < inputs.size(); j++)
        {
            if (find(cardids.begin(),
                     cardids.end(),
                     inputs[j].cardid) == cardids.end())
            {
                cardids.push_back(inputs[j].cardid);
                inputids.push_back(inputs[j].inputid);
            }
        }
    }
    QString msg("RemoteRequestFreeRecorderInputList returned {");
    for (uint k = 0; k < cardids.size(); k++)
        msg += QString(" %1:%2").arg(cardids[k]).arg(inputids[k]);
    msg += "}";
    LOG(VB_CHANNEL, LOG_INFO, msg);
}

/**
 * Given a list of recorder numbers, return the first available.
 */
vector<uint> RemoteRequestFreeRecorderList(const vector<uint> &excluded_cardids)
{
    vector<uint> cardids, inputids;
    RemoteRequestFreeRecorderInputList(excluded_cardids, cardids, inputids);
    return cardids;
}

vector<uint> RemoteRequestFreeInputList(const vector<uint> &excluded_cardids)
{
    vector<uint> cardids, inputids;
    RemoteRequestFreeRecorderInputList(excluded_cardids, cardids, inputids);
    return inputids;
}

RemoteEncoder *RemoteRequestFreeRecorderFromList
(const QStringList &qualifiedRecorders, const vector<uint> &excluded_cardids)
{
#if 0
    QStringList strlist( "GET_FREE_RECORDER_LIST" );

    if (!gCoreContext->SendReceiveStringList(strlist, true))
        return NULL;

    for (QStringList::const_iterator recIter = qualifiedRecorders.begin();
         recIter != qualifiedRecorders.end(); ++recIter)
    {
        if (!strlist.contains(*recIter))
        {
            // did not find it in the free recorder list. We
            // move on to check the next recorder
            continue;
        }
        // at this point we found a free recorder that fulfills our request
        return RemoteGetExistingRecorder((*recIter).toInt());
    }
    // didn't find anything. just return NULL.
    return NULL;
#endif
    vector<uint> freeRecorders =
        RemoteRequestFreeRecorderList(excluded_cardids);
    for (QStringList::const_iterator recIter = qualifiedRecorders.begin();
         recIter != qualifiedRecorders.end(); ++recIter)
    {
        if (find(freeRecorders.begin(),
                 freeRecorders.end(),
                 (*recIter).toUInt()) != freeRecorders.end())
            return RemoteGetExistingRecorder((*recIter).toInt());
    }
    return NULL;
}

RemoteEncoder *RemoteRequestRecorder(void)
{
    QStringList strlist( "GET_FREE_RECORDER" );

    if (!gCoreContext->SendReceiveStringList(strlist, true))
        return NULL;

    int num = strlist[0].toInt();
    QString hostname = strlist[1];
    int port = strlist[2].toInt();

    return new RemoteEncoder(num, hostname, port);
}

RemoteEncoder *RemoteGetExistingRecorder(const ProgramInfo *pginfo)
{
    QStringList strlist( "GET_RECORDER_NUM" );
    pginfo->ToStringList(strlist);

    if (!gCoreContext->SendReceiveStringList(strlist))
        return NULL;

    int num = strlist[0].toInt();
    QString hostname = strlist[1];
    int port = strlist[2].toInt();

    return new RemoteEncoder(num, hostname, port);
}

RemoteEncoder *RemoteGetExistingRecorder(int recordernum)
{
    QStringList strlist( "GET_RECORDER_FROM_NUM" );
    strlist << QString("%1").arg(recordernum);

    if (!gCoreContext->SendReceiveStringList(strlist))
        return NULL;

    QString hostname = strlist[0];
    int port = strlist[1].toInt();

    return new RemoteEncoder(recordernum, hostname, port);
}

vector<InputInfo> RemoteRequestFreeInputList(
    uint cardid, const vector<uint> &excluded_cardids)
{
    vector<InputInfo> list;

    QStringList strlist(QString("QUERY_RECORDER %1").arg(cardid));
    strlist << "GET_FREE_INPUTS";
    for (uint i = 0; i < excluded_cardids.size(); i++)
        strlist << QString::number(excluded_cardids[i]);

    if (!gCoreContext->SendReceiveStringList(strlist))
        return list;

    QStringList::const_iterator it = strlist.begin();
    if ((it == strlist.end()) || (*it == "EMPTY_LIST"))
        return list;

    while (it != strlist.end())
    {
        InputInfo info;
        if (!info.FromStringList(it, strlist.end()))
            break;
        list.push_back(info);
    }

    return list;
}

InputInfo RemoteRequestBusyInputID(uint cardid)
{
    InputInfo blank;

    QStringList strlist(QString("QUERY_RECORDER %1").arg(cardid));
    strlist << "GET_BUSY_INPUT";

    if (!gCoreContext->SendReceiveStringList(strlist))
        return blank;

    QStringList::const_iterator it = strlist.begin();
    if ((it == strlist.end()) || (*it == "EMPTY_LIST"))
        return blank;

    InputInfo info;
    if (info.FromStringList(it, strlist.end()))
        return info;

    return blank;
}

bool RemoteIsBusy(uint cardid, TunedInputInfo &busy_input)
{
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("RemoteIsBusy(%1) %2")
            .arg(cardid).arg(gCoreContext->IsBackend() ? "be" : "fe"));
#endif

    busy_input.Clear();

    if (gCoreContext->IsBackend())
    {
        const TVRec *rec = TVRec::GetTVRec(cardid);
        if (rec)
            return rec->IsBusy(&busy_input);
    }

    QStringList strlist(QString("QUERY_REMOTEENCODER %1").arg(cardid));
    strlist << "IS_BUSY";
    if (!gCoreContext->SendReceiveStringList(strlist) || strlist.empty())
        return true;

    QStringList::const_iterator it = strlist.begin();
    bool state = (*it).toInt();
    ++it;
    if (!busy_input.FromStringList(it, strlist.end()))
        state = true; // if there was an error pretend that the input is busy.

    return state;
}

bool RemoteGetRecordingStatus(
    vector<TunerStatus> *tunerList, bool list_inactive)
{
    bool isRecording = false;
    vector<uint> cardlist = CardUtil::GetCardList();

    if (tunerList)
        tunerList->clear();

    for (uint i = 0; i < cardlist.size(); i++)
    {
        QString     status      = "";
        uint        cardid      = cardlist[i];
        int         state       = kState_ChangingState;
        QString     channelName = "";
        QString     title       = "";
        QString     subtitle    = "";
        QDateTime   dtStart     = QDateTime();
        QDateTime   dtEnd       = QDateTime();
        QStringList strlist;

        QString cmd = QString("QUERY_REMOTEENCODER %1").arg(cardid);

        while (state == kState_ChangingState)
        {
            strlist = QStringList(cmd);
            strlist << "GET_STATE";
            gCoreContext->SendReceiveStringList(strlist);

            if (strlist.empty())
                break;

            state = strlist[0].toInt();
            if (kState_ChangingState == state)
                usleep(5000);
        }

        if (kState_RecordingOnly == state || kState_WatchingRecording == state)
        {
            isRecording |= true;

            if (!tunerList)
                break;

            strlist = QStringList(QString("QUERY_RECORDER %1").arg(cardid));
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
            continue;

        if (tunerList)
        {
            TunerStatus tuner;
            tuner.id          = cardid;
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
