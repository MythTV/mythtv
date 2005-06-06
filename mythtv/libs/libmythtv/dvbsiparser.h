/*
 * dvbsections.h
 *
 * Digital Video Broadcast Section/Table Parser
 *
 * Author(s):  Kenneth Aafloy
 *                - Wrote original code
 *             Taylor Jacob (rtjacob@earthlink.net)
 *                - Added NIT/EIT/SDT Scaning Code
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

#ifndef DVBSIPARSER_H
#define DVBSIPARSER_H

#include <qobject.h>
#include <qvaluelist.h>
#include "dvbtypes.h"
#include "siparser.h"

/*
 *  Class used for PID Filter Management.
 */
class PIDFilterManager
{

    public:
        PIDFilterManager()
        {
           pid = 0;
           running = false;
        }
        PIDFilterManager(const int inputPID)
        {
            pid = inputPID;
            running = true;
        }
        uint16_t pid;
        bool running;
};

class DVBSIParser : public SIParser
{
public:
    DVBSIParser(int cardnum, bool start_thread = false);
    ~DVBSIParser();

    /* Control PIDs */
    void AddPid(uint16_t pid,uint8_t mask = 0x00,uint8_t filter = 0xFF, bool CheckCRC = true,
                int bufferFactor = 10);
    void DelPid(int pid);
    void DelAllPids();

    /* Thread control */
    void StartSectionReader();
    void StopSectionReader();

private:
    /// System information thread thunk, runs DVBSIParser::StartSectionReader()
    static void *SystemInfoThread(void *param);

    int                                cardnum;

    /* Thread related */
    bool                               exitSectionThread;
    bool                               sectionThreadRunning;
    bool                               selfStartedThread;
    pthread_mutex_t                    poll_lock;
    pthread_t                          siparser_thread;

    /* Filter / fd management */
    typedef QMap<int,PIDFilterManager> PIDFDMap;
    PIDFDMap                           PIDfilterManager;
    int                                pollLength;
    pollfd                            *pollArray;
    bool                               filterChange;
};

#endif //DVBSIPARSER_H
