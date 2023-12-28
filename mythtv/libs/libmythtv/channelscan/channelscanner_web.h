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

#ifndef CHANNEL_SCANNER_WEB_H
#define CHANNEL_SCANNER_WEB_H

// Qt headers
#include <QString>

// MythTV headers
#include "libmythtv/mythtvexp.h"
#include "channelscanner.h"

// class RunScanner : public QRunnable
// {
//   public:
//     virtual void run();
// };

class MTV_PUBLIC ChannelScannerWeb : public ChannelScanner
{
private:
  ChannelScannerWeb(void) = default;

public:
  ~ChannelScannerWeb() override = default;
  void HandleEvent(const ScannerEvent *scanEvent) override; // ChannelScanner
  static ChannelScannerWeb * getInstance();
  void ResetStatus();
  void setupScan(int CardId);
  void stopScan();
  void stopMon(void);
  void log(const QString &msg);

  bool StartScan (uint CardId,
                            const QString &DesiredServices,
                            bool FreeToAirOnly,
                            bool ChannelNumbersOnly,
                            bool CompleteChannelsOnly,
                            bool FullChannelSearch,
                            bool RemoveDuplicates,
                            bool AddFullTS,
                            bool TestDecryptable,
                            const QString &ScanType,
                            const QString &FreqTable,
                            QString Modulation,
                            const QString &FirstChan,
                            const QString &LastChan,
                            uint ScanId,
                            bool IgnoreSignalTimeout,
                            bool FollowNITSetting,
                            uint MplexId,
                            const QString &Frequency,
                            const QString &Bandwidth,
                            const QString &Polarity,
                            const QString &SymbolRate,
                            const QString &Inversion,
                            const QString &Constellation,
                            const QString &ModSys,
                            const QString &CodeRate_LP,
                            const QString &CodeRate_HP,
                            const QString &FEC,
                            const QString &Trans_Mode,
                            const QString &Guard_Interval,
                            const QString &Hierarchy,
                            const QString &RollOff);
protected:
  void InformUser(const QString &error) override; // ChannelScanner
  void Process(const ScanDTVTransportList &_transports, bool success = false);
  void MonitorProgress(bool lock, bool strength,
                       bool snr, bool rotor) override; // ChannelScanner

private:
  static ChannelScannerWeb *s_Instance;
  int m_runType {0};
  bool m_onlysavescan {false};
  bool m_interactive {false};
  ScanDTVTransportList m_transports;
  int m_scantype {-1};
  int m_scanId {0};

public:
  QMutex m_mutex;
  QWaitCondition m_waitCondition;
  uint m_cardid {0};
  // Status values
  // "IDLE", "RINNUNG"
  QString m_status{"IDLE"};
  bool m_statusLock{false};
  uint m_statusProgress{0};
  int m_statusSnr{0};
  QString m_statusText;
  QString m_statusLog;
  QString m_statusTitleText;
  int m_statusRotorPosition {0};
  int m_statusSignalStrength {0};
  QString m_dlgMsg;
  QStringList m_dlgButtons;
  bool m_dlgInputReq {false};
  int m_dlgButton {-1};
  QString m_dlgString;

  // Monitor Settings
  bool m_showSignalLock{false};
  bool m_showSignalStrength{false};
  bool m_showSignalNoise{false};
  bool m_showRotorPos{false};

  MThread * m_monitorThread {nullptr};
};

#endif // CHANNEL_SCANNER_WEB_H
