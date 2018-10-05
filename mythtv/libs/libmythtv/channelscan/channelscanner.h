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

#ifndef _CHANNEL_SCANNER_H_
#define _CHANNEL_SCANNER_H_

// Qt headers
#include <QCoreApplication>

// MythTV headers
#include "mythtvexp.h"
#include "dtvconfparser.h"
#include "iptvchannelfetcher.h"
#include "scanmonitor.h"
#include "channelscantypes.h"

#ifdef USING_VBOX
#include "vboxchannelfetcher.h"
#endif

#if !defined( USING_MINGW ) && !defined( _MSC_VER )
#include "externrecscanner.h"
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
    ChannelScanner();
    virtual ~ChannelScanner();

    void Scan(int            scantype,
              uint           cardid,
              const QString &inputname,
              uint           sourceid,
              bool           do_ignore_signal_timeout,
              bool           do_follow_nit,
              bool           do_test_decryption,
              bool           do_fta_only,
              ServiceRequirements service_requirements,
              // stuff needed for particular scans
              uint           mplexid,
              const QMap<QString,QString> &startChan,
              const QString &freq_std,
              const QString &mod,
              const QString &tbl,
              const QString &tbl_start = QString::null,
              const QString &tbl_end   = QString::null);

    virtual DTVConfParser::return_t ImportDVBUtils(
        uint sourceid, int cardtype, const QString &file);

    virtual bool ImportM3U(uint cardid, const QString &inputname,
                           uint sourceid, bool is_mpts);
    virtual bool ImportVBox(uint cardid, const QString &inputname, uint sourceid,
                            bool ftaOnly, ServiceRequirements serviceType);
    virtual bool ImportExternRecorder(uint cardid, const QString &inputname,
                                      uint sourceid);

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
    ScanMonitor        *scanMonitor;
    ChannelBase        *channel;

    // Low level channel scanners
    ChannelScanSM      *sigmonScanner;
    IPTVChannelFetcher *iptvScanner;

    /// imported channels
    DTVChannelList      channels;
    fbox_chan_map_t     iptv_channels;

    // vbox support
#ifdef USING_VBOX
    VBoxChannelFetcher *vboxScanner;
#endif
#if !defined( USING_MINGW ) && !defined( _MSC_VER )
    ExternRecChannelScanner *m_ExternRecScanner;
#endif

    /// Only fta channels desired post scan?
    bool                freeToAirOnly;
    int                 m_sourceid;

    /// Services desired post scan
    ServiceRequirements serviceRequirements;
};

#endif // _CHANNEL_SCANNER_H_
