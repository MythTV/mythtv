/*
 * dvbsections.h
 *
 * Digital Video Broadcast Section/Table Parser
 *
 * Author:          Kenneth Aafloy
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 * The author can be reached at ke-aa@frisurf.no
 *
 * The project's page is at http://www.mythtv.org
 *
 */

#ifndef DVBSECTIONS_H
#define DVBSECTIONS_H

#include <qobject.h>
#include "dvbtypes.h"

typedef struct tablehead
{
    uint8_t    table_id;
    uint16_t   section_length;
    uint16_t   table_id_ext;
    bool        current_next;
    uint8_t    version;
    uint8_t    section_number;
    uint8_t    section_last;
} tablehead_t;

typedef struct program
{
    uint8_t    version;

    uint16_t   pmt_pid;
    uint16_t   pcr_pid;
} program_t;

typedef map<uint16_t, program_t> programs_t;

typedef struct program_map
{
    uint8_t     pat_version;
    programs_t  programs;
} program_map_t;

class DVBSections: public QObject
{
    Q_OBJECT
public:
    DVBSections(int cardnum);
    ~DVBSections();

    void AddPid(int pid);
    void DelPid(int pid);

    void Start();
    void Stop();

    static void *ThreadHelper(void*);
    void ThreadLoop();

public slots:
    void ChannelChanged(dvb_channel_t& chan);

signals:
    void ChannelChanged(dvb_channel_t& chan, uint8_t* pmt, int len);

private:
    int cardnum;
    bool exitSectionThread;
    bool sectionThreadRunning;

    pthread_t       thread;

    int             pollLength;
    pollfd         *pollArray;
    pthread_mutex_t poll_lock;

    dvb_channel_t  chan_opts;

    uint8_t        curpmtsize;
    uint8_t        *curpmtbuf;
    uint16_t       curprogram;

    pthread_mutex_t pmap_lock;
    program_map_t   pmap;

    void ParseTable(tablehead_t* head, uint8_t* buffer, int size);

    void ParsePAT(tablehead_t* head, uint8_t* buffer, int size);
    void ParsePMT(tablehead_t* head, uint8_t* buffer, int size);
};

#endif //DVBSECTIONS_H
