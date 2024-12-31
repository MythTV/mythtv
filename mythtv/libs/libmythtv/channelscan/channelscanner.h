/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Copyright (c) 2004, 2005 John Pullan <john@pullan.org>
 * Copyright (c) 2005 - 2007 Daniel Kristjansson
 *
 * Description:
 *     Collection of classes to provide channel scanning functionallity
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

#ifndef CHANNEL_SCANNER_H
#define CHANNEL_SCANNER_H

// Qt headers
#include <QCoreApplication>

// MythTV headers
#include "libmythtv/cardutil.h"
#include "libmythtv/dtvconfparser.h"
#include "libmythtv/mythtvexp.h"

#include "iptvchannelfetcher.h"
#include "scanmonitor.h"
#include "channelscantypes.h"

#ifdef USING_VBOX
#include "vboxchannelfetcher.h"
#endif

#if !defined( USING_MINGW ) && !defined( _MSC_VER )
#include "externrecscanner.h"
#endif

#ifdef USING_HDHOMERUN
#include "hdhrchannelfetcher.h"
#endif

class ScanMonitor;
class IPTVChannelFetcher;
class ExternRecChannelFetcher;
class ChannelScanSM;
class ChannelBase;

// Not (yet?) implemented from old scanner
// do_delete_channels, do_rename_channels, atsc_format
// TODO implement deletion of stale channels..

class MTV_PUBLIC ChannelScanner
{
    Q_DECLARE_TR_FUNCTIONS(ChannelScanner)

    friend class ScanMonitor;

  public:
    ChannelScanner() = default;
    virtual ~ChannelScanner();

    void Scan(int            scantype,
              uint           cardid,
              const QString &inputname,
              uint           sourceid,
              bool           do_ignore_signal_timeout,
              bool           do_follow_nit,
              bool           do_test_decryption,
              bool           do_fta_only,
              bool           do_lcn_only,
              bool           do_complete_only,
              bool           do_full_channel_search,
              bool           do_remove_duplicates,
              bool           do_add_full_ts,
              ServiceRequirements service_requirements,
              // stuff needed for particular scans
              uint           mplexid,
              const QMap<QString,QString> &startChan,
              const QString &freq_std,
              const QString &mod,
              const QString &tbl,
              const QString &tbl_start = QString(),
              const QString &tbl_end   = QString());

    virtual DTVConfParser::return_t ImportDVBUtils(
        uint sourceid, CardUtil::INPUT_TYPES cardtype, const QString &file);

    virtual bool ImportM3U(uint cardid, const QString &inputname,
                           uint sourceid, bool is_mpts);
    virtual bool ImportVBox(uint cardid, const QString &inputname, uint sourceid,
                            bool ftaOnly, ServiceRequirements serviceType);
    virtual bool ImportExternRecorder(uint cardid, const QString &inputname,
                                      uint sourceid);
    virtual bool ImportHDHR(uint cardid, const QString &inputname, uint sourceid,
                            ServiceRequirements serviceType);

  protected:
    virtual void Teardown(void);

    virtual void PreScanCommon(
        int scantype, uint cardid,
        const QString &inputname,
        uint sourceid, bool do_ignore_signal_timeout,
        bool do_test_decryption);

    virtual void MonitorProgress(
        bool /*lock*/, bool /*strength*/, bool /*snr*/, bool /*rotor*/) { }

    virtual void HandleEvent(const ScannerEvent*) = 0;
    virtual void InformUser(const QString &/*error*/) = 0;

  protected:
    ScanMonitor             *m_scanMonitor         {nullptr};
    ChannelBase             *m_channel             {nullptr};

    // Low level channel scanners
    ChannelScanSM           *m_sigmonScanner       {nullptr};
    IPTVChannelFetcher      *m_iptvScanner         {nullptr};

    /// imported channels
    DTVChannelList           m_channels;
    fbox_chan_map_t          m_iptvChannels;

    // vbox support
#ifdef USING_VBOX
    VBoxChannelFetcher      *m_vboxScanner         {nullptr};
#endif
#if !defined( USING_MINGW ) && !defined( _MSC_VER )
    ExternRecChannelScanner *m_externRecScanner    {nullptr};
#endif
    // HDHomeRun channel list import
#ifdef USING_HDHOMERUN
    HDHRChannelFetcher      *m_hdhrScanner         {nullptr};
#endif

    /// Only fta channels desired post scan?
    bool                     m_freeToAirOnly       {false};

    /// Only channels with logical channel numbers desired post scan?
    bool                     m_channelNumbersOnly  {false};

    /// Only complete channels desired post scan?
    bool                     m_completeOnly        {false};

    /// Extended search for old channels post scan?
    bool                     m_fullSearch          {false};

    /// Remove duplicate transports and channels?
    bool                     m_removeDuplicates    {false};

    /// Add MPTS "full transport stream" channels
    bool                     m_addFullTS           {false};

    int                      m_sourceid            {-1};

    /// Services desired post scan
    ServiceRequirements      m_serviceRequirements {kRequireAV};
};

#endif // CHANNEL_SCANNER_H
