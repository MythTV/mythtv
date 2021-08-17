/*  -*- Mode: c++ -*-
 *   Class ExternalFetcher
 *
 *   Copyright (C) John Poet 2018
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software Foundation,
 *   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

// Qt includes
#include <QString>

class ExternalStreamHandler;

class ExternalRecChannelFetcher
{
  public:
    ExternalRecChannelFetcher(int cardid, QString cmd);
    ~ExternalRecChannelFetcher(void);

    bool Valid(void) const;

    int LoadChannels(void);
    bool FirstChannel(QString & channum,
                      QString & name,
                      QString & callsign,
                      QString & xmltvid,
                      QString & icon)
    {
        return FetchChannel("FirstChannel", channum, name, callsign,
                            xmltvid, icon);
    }
    bool NextChannel(QString & channum,
                     QString & name,
                     QString & callsign,
                     QString & xmltvid,
                     QString & icon)
    {
        return FetchChannel("NextChannel", channum, name, callsign,
                            xmltvid, icon);

    }

  protected:
    void Close(void);
    bool FetchChannel(const QString & cmd,
                      QString & channum,
                      QString & name,
                      QString & callsign,
                      QString & xmltvid,
                      QString & icon);


  private:
    int     m_cardid;
    QString m_command;

    ExternalStreamHandler *m_streamHandler {nullptr};
};
