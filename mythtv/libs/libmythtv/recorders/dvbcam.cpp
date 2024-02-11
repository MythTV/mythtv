/*
 * Class DVBCam
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Author(s):
 *      Jesper Sorensen
 *          - Changed to work with Taylor Jacob's DVB rewrite
 *      Kenneth Aafloy
 *          - General Implementation
 *
 * Description:
 *      This Class has been developed from bits n' pieces of other
 *      projects.
 *
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

// C++
#include <cstdio>
#include <cstdlib>

// C
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <linux/dvb/ca.h>

// Qt
#include <QString>
#include <QList>
#include <QMap>

// MythTV
#include "libmythbase/mthread.h"
#include "libmythbase/mythlogging.h"

#include "cardutil.h"
#include "dvbcam.h"
#include "dvbchannel.h"
#include "dvbdev/dvbci.h"
#include "dvbrecorder.h"
#include "recorderbase.h"

#define LOC QString("DVBCam(%1): ").arg(m_device)

DVBCam::DVBCam(QString device)
    : m_device(std::move(device))
{
    QString dvbdev = CardUtil::GetDeviceName(DVB_DEV_CA, m_device);
    QByteArray dev = dvbdev.toLatin1();
    int cafd = open(dev.constData(), O_RDWR);
    if (cafd >= 0)
    {
        ca_caps_t caps;
        // slot_num will be uninitialised if ioctl fails
        if (ioctl(cafd, CA_GET_CAP, &caps) >= 0)
            m_numslots = caps.slot_num;
        else
            LOG(VB_GENERAL, LOG_ERR, LOC + "ioctl CA_GET_CAP failed: " + ENO);

        close(cafd);
    }
}

DVBCam::~DVBCam()
{
    Stop();
}

bool DVBCam::Start(void)
{
    if (m_numslots == 0)
        return false;

    m_havePmt    = false;
    m_pmtSent    = false;
    m_pmtUpdated = false;
    m_pmtAdded   = false;

    QString dvbdev = CardUtil::GetDeviceName(DVB_DEV_CA, m_device);
    QByteArray dev = dvbdev.toLatin1();
    m_ciHandler = cCiHandler::CreateCiHandler(dev.constData());
    if (!m_ciHandler)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialize CI handler");
        return false;
    }

    QMutexLocker locker(&m_ciHandlerLock);
    m_ciHandlerDoRun = true;
    m_ciHandlerThread = new MThread("DVBCam", this);
    m_ciHandlerThread->start();
    while (m_ciHandlerDoRun && !m_ciHandlerRunning)
        m_ciHandlerWait.wait(locker.mutex(), 1000);

    if (m_ciHandlerRunning)
        LOG(VB_DVBCAM, LOG_INFO, LOC + "CI handler successfully initialized!");

    return m_ciHandlerRunning;
}

bool DVBCam::Stop(void)
{
    {
        QMutexLocker locker(&m_ciHandlerLock);
        if (m_ciHandlerRunning)
        {
            m_ciHandlerDoRun = false;
            locker.unlock();
            m_ciHandlerThread->wait();
            locker.relock();
            delete m_ciHandlerThread;
            m_ciHandlerThread = nullptr;
        }

        if (m_ciHandler)
        {
            delete m_ciHandler;
            m_ciHandler = nullptr;
        }
    }

    QMutexLocker locker(&m_pmtLock);
    pmt_list_t::iterator it;

    for (it = m_pmtList.begin(); it != m_pmtList.end(); ++it)
        delete *it;
    m_pmtList.clear();

    for (it = m_pmtAddList.begin(); it != m_pmtAddList.end(); ++it)
        delete *it;
    m_pmtAddList.clear();

    return true;
}

void DVBCam::HandleUserIO(void)
{
    cCiEnquiry* enq = m_ciHandler->GetEnquiry();
    if (enq != nullptr)
    {
        if (enq->Text() != nullptr)
            LOG(VB_DVBCAM, LOG_INFO, LOC + QString("CAM: Received message: %1")
                    .arg(enq->Text()));
        delete enq;
    }

    cCiMenu* menu = m_ciHandler->GetMenu();
    if (menu != nullptr)
    {
        if (menu->TitleText() != nullptr)
            LOG(VB_DVBCAM, LOG_INFO, LOC + QString("CAM: Menu Title: %1")
                    .arg(menu->TitleText()));
        if (menu->SubTitleText() != nullptr)
            LOG(VB_DVBCAM, LOG_INFO, LOC + QString("CAM: Menu SubTitle: %1")
                    .arg(menu->SubTitleText()));
        if (menu->BottomText() != nullptr)
            LOG(VB_DVBCAM, LOG_INFO, LOC + QString("CAM: Menu BottomText: %1")
                    .arg(menu->BottomText()));

        for (int i=0; i<menu->NumEntries(); i++)
        {
            if (menu->Entry(i) != nullptr)
                LOG(VB_DVBCAM, LOG_INFO, LOC + QString("CAM: Menu Entry: %1")
                        .arg(menu->Entry(i)));
        }

        if (menu->Selectable())
        {
            LOG(VB_CHANNEL, LOG_INFO, LOC + "CAM: Menu is selectable");
        }

        if (menu->NumEntries() > 0)
        {
            LOG(VB_DVBCAM, LOG_INFO, LOC + "CAM: Selecting first entry");
            menu->Select(0);
        }
        else
        {
            LOG(VB_DVBCAM, LOG_INFO, LOC + "CAM: Cancelling menu");
        }

        delete menu;
    }
}

// Remove duplicate service IDs because the CAM needs to be setup only
// once for each service even if the service is recorded twice.
// This can happen with overlapping recordings on the same channel.
void DVBCam::RemoveDuplicateServices(void)
{
    QList<uint> unique_sids;
    pmt_list_t::iterator it;

    // Remove duplicates in m_pmtList
    for (it = m_pmtList.begin(); it != m_pmtList.end(); )
    {
        const ChannelBase *chan = it.key();
        uint inputId = chan->GetInputID();
        const ProgramMapTable *pmt = (*it);
        uint serviceId = pmt->ProgramNumber();
        if (unique_sids.contains(serviceId))
        {
            it = m_pmtList.erase(it);
            LOG(VB_DVBCAM, LOG_DEBUG, LOC + QString("Service [%1]%2 duplicate, removed from pmtList")
                .arg(inputId).arg(serviceId));
        }
        else
        {
            unique_sids.append(serviceId);
            ++it;
            LOG(VB_DVBCAM, LOG_DEBUG, LOC + QString("Service [%1]%2 stays in pmtList")
                .arg(inputId).arg(serviceId));
        }
    }

    // Remove duplicates in m_pmtAddList
    for (it = m_pmtAddList.begin(); it != m_pmtAddList.end(); )
    {
        const ChannelBase *chan = it.key();
        uint inputId = chan->GetInputID();
        const ProgramMapTable *pmt = (*it);
        uint serviceId = pmt->ProgramNumber();
        if (unique_sids.contains(serviceId))
        {
            it = m_pmtAddList.erase(it);
            LOG(VB_DVBCAM, LOG_DEBUG, LOC + QString("Service [%1]%2 duplicate, removed from pmtAddList")
                .arg(inputId).arg(serviceId));
        }
        else
        {
            unique_sids.append(serviceId);
            ++it;
            LOG(VB_DVBCAM, LOG_DEBUG, LOC + QString("Service [%1]%2 stays in pmtAddList")
                .arg(inputId).arg(serviceId));
        }
    }
}


void DVBCam::HandlePMT(void)
{
    LOG(VB_DVBCAM, LOG_INFO, LOC + "CiHandler needs CA_PMT");
    QMutexLocker locker(&m_pmtLock);

    RemoveDuplicateServices();

    if (m_pmtSent && m_pmtAdded && !m_pmtUpdated)
    {
        // Send added PMT
        while (!m_pmtAddList.empty())
        {
            pmt_list_t::iterator it = m_pmtAddList.begin();
            const ChannelBase *chan = it.key();
            ProgramMapTable *pmt = (*it);
            m_pmtList[chan] = pmt;
            m_pmtAddList.erase(it);
            SendPMT(*pmt, CPLM_ADD);
        }

        m_pmtUpdated = false;
        m_pmtAdded   = false;
        return;
    }

    // Grab any added PMT
    while (!m_pmtAddList.empty())
    {
        pmt_list_t::iterator it = m_pmtAddList.begin();
        const ChannelBase *chan = it.key();
        ProgramMapTable *pmt = (*it);
        m_pmtList[chan] = pmt;
        m_pmtAddList.erase(it);
    }

    uint length = m_pmtList.size();
    uint count  = 0;

    for (auto *pmt : std::as_const(m_pmtList))
    {
        uint cplm = (count     == 0)      ? CPLM_FIRST : CPLM_MORE;
        cplm      = (count + 1 == length) ? CPLM_LAST  : cplm;
        cplm      = (length    == 1)      ? CPLM_ONLY  : cplm;

        SendPMT(*pmt, cplm);

        count++;
    }

    m_pmtSent    = true;
    m_pmtUpdated = false;
    m_pmtAdded   = false;
}

void DVBCam::run(void)
{
    LOG(VB_DVBCAM, LOG_INFO, LOC + "CI handler thread running");

    QMutexLocker locker(&m_ciHandlerLock);
    m_ciHandlerRunning = true;

    while (m_ciHandlerDoRun)
    {
        locker.unlock();
        if (m_ciHandler->Process())
        {
            if (m_ciHandler->HasUserIO())
                HandleUserIO();

            bool handle_pmt  = m_pmtSent && (m_pmtUpdated || m_pmtAdded);
            handle_pmt      |= m_havePmt && m_ciHandler->NeedCaPmt();

            if (handle_pmt)
                HandlePMT();
        }
        locker.relock();
        m_ciHandlerWait.wait(locker.mutex(), 10);
    }

    m_ciHandlerRunning = false;
    LOG(VB_DVBCAM, LOG_INFO, LOC + "CiHandler thread stopped");
}

void DVBCam::SetPMT(const ChannelBase *chan, const ProgramMapTable *pmt)
{
    QMutexLocker locker(&m_pmtLock);

    pmt_list_t::iterator it = m_pmtList.find(chan);
    pmt_list_t::iterator it2 = m_pmtAddList.find(chan);
    if (!pmt && (it != m_pmtList.end()))
    {
        delete *it;
        m_pmtList.erase(it);
        m_pmtUpdated = true;
    }
    else if (!pmt && (it2 != m_pmtAddList.end()))
    {
        delete *it2;
        m_pmtAddList.erase(it2);
        m_pmtAdded = !m_pmtAddList.empty();
    }
    else if (pmt && (m_pmtList.empty() || (it != m_pmtList.end())))
    {
        if (it != m_pmtList.end())
            delete *it;
        m_pmtList[chan] = new ProgramMapTable(*pmt);
        m_havePmt    = true;
        m_pmtUpdated = true;
    }
    else if (pmt && (it == m_pmtList.end()))
    {
        m_pmtAddList[chan] = new ProgramMapTable(*pmt);
        m_pmtAdded = true;
    }
}

void DVBCam::SetTimeOffset(double offset_in_seconds)
{
    QMutexLocker locker(&m_ciHandlerLock);
    if (m_ciHandler)
        m_ciHandler->SetTimeOffset(offset_in_seconds);
}

static std::array<const std::string,6> cplm_info
{
    "CPLM_MORE",
    "CPLM_FIRST",
    "CPLM_LAST",
    "CPLM_ONLY",
    "CPLM_ADD",
    "CPLM_UPDATE"
};

cCiCaPmt CreateCAPMT(const ProgramMapTable& /*pmt*/, const dvbca_vector &/*casids*/, uint /*cplm*/);

/*
 * Send a CA_PMT object to the CAM (see EN50221, section 8.4.3.4)
 */
void DVBCam::SendPMT(const ProgramMapTable &pmt, uint cplm)
{
    bool success = false;

    for (uint s = 0; s < (uint)m_ciHandler->NumSlots(); s++)
    {
        dvbca_vector casids = m_ciHandler->GetCaSystemIds(s);

        if (casids.empty())
        {
            LOG(success ? VB_DVBCAM : VB_GENERAL, LOG_ERR,
                LOC + "CAM supports no CA systems! " +
                QString("(Slot #%1)").arg(s));
            continue;
        }

        LOG(VB_DVBCAM, LOG_INFO, LOC +
            QString("Creating CA_PMT, ServiceID = %1")
                .arg(pmt.ProgramNumber()));

        cCiCaPmt capmt = CreateCAPMT(pmt, casids, cplm);

        LOG(VB_DVBCAM, LOG_INFO, LOC +
            QString("Sending CA_PMT with %1 to CI slot #%2")
                .arg(QString::fromStdString(cplm_info[cplm])).arg(s));

        if (!m_ciHandler->SetCaPmt(capmt, s))
        {
            LOG(success ? VB_DVBCAM : VB_GENERAL, LOG_ERR,
                LOC + "CA_PMT send failed!");
        }
        else
        {
            success = true;
        }
    }
}

static void process_desc(cCiCaPmt &capmt,
                  const dvbca_vector &casids,
                  const desc_list_t &desc)
{
    desc_list_t::const_iterator it;
    for (it = desc.begin(); it != desc.end(); ++it)
    {
        ConditionalAccessDescriptor cad(*it);
        for (auto id : casids)
        {
            if (!cad.IsValid() || cad.SystemID() != id)
                continue;

            LOG(VB_DVBCAM, LOG_INFO, QString("DVBCam: Adding CA descriptor: "
                                             "CASID(0x%2), ECM PID(0x%3)")
                    .arg(cad.SystemID(),0,16).arg(cad.PID(),0,16));

            capmt.AddCaDescriptor(cad.SystemID(), cad.PID(),
                                  cad.DataSize(), cad.Data());
        }
    }

}

cCiCaPmt CreateCAPMT(const ProgramMapTable &pmt,
                     const dvbca_vector &casids,
                     uint cplm)
{
    cCiCaPmt capmt(pmt.ProgramNumber(), cplm);

    // Add CA descriptors for the service
    desc_list_t gdesc = MPEGDescriptor::ParseOnlyInclude(
        pmt.ProgramInfo(), pmt.ProgramInfoLength(),
        DescriptorID::conditional_access);

    process_desc(capmt, casids, gdesc);

    // Add elementary streams + CA descriptors
    for (uint i = 0; i < pmt.StreamCount(); i++)
    {
        LOG(VB_DVBCAM, LOG_INFO,
            QString("DVBCam: Adding elementary stream: %1, pid(0x%2)")
                .arg(pmt.StreamDescription(i, "dvb"))
                .arg(pmt.StreamPID(i),0,16));

        capmt.AddElementaryStream(pmt.StreamType(i), pmt.StreamPID(i));

        desc_list_t desc = MPEGDescriptor::ParseOnlyInclude(
            pmt.StreamInfo(i), pmt.StreamInfoLength(i),
            DescriptorID::conditional_access);

        process_desc(capmt, casids, desc);
    }
    return capmt;
}
