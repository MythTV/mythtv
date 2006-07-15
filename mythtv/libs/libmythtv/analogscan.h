/*
 * $Id$
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Author(s):
 *      John Pullan  (john@pullan.org)
 *
 * Description:
 *     Collection of classes to provide dvb channel scanning
 *     functionallity
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifndef ANALOGSCAN_H
#define ANALOGSCAN_H

#include <qobject.h>
#include <qstring.h>
#include <qmutex.h>
#include <qwaitcondition.h>
#include <pthread.h>
#include "frequencies.h"

/**
 * @class AnalogScan
 * @brief Facilitates the scanning of analog video sources for channels
 */
class AnalogScan : public QObject
{
   Q_OBJECT
public:
    /** 
      @brief constructs the Analog Scanner
      @param _sourceid the video source id 
      @param _cardid the card id to perform the scan on
     */
    AnalogScan(unsigned _sourceid, unsigned _cardid);

    ~AnalogScan();

    /** @brief Stops the scanning thread running */
    void stop();

    /** @brief Scans the given frequency list, implies starting the thread() */
    bool scan();

signals:
    /** @brief Indicates how far through the scan we are as a percentage
        @param pct percentage completion
    */
    void serviceScanPCTComplete(int pct);
    /** @brief Status message from the scan engine
        @param status the message
    */ 
    void serviceScanUpdateText(const QString& status);
    /** @brief signals the scan is complete */ 
    void serviceScanComplete();

protected:
    bool fRunning;
    bool fStop;

    unsigned sourceid;
    unsigned cardid;
    unsigned nTable;

    /** @brief adds a found channel to the database
        @param name name of the channel
        @param frequency freqency/frequencyid of the channel
      */
    void addChannel(int number, const QString& channum,
                    const QString& name, int frequency);
    /** @brief Scanning thread*/
    pthread_t thread;

    /** @brief Condition to signal that the scanning thread is running */
    QWaitCondition scanThreadCond;
    QMutex scanThreadCondLock;

    /** @brief Actual scanning proc */
    void doScan();
    /** @brief Actual thread proc , calls doScan*/
    static void *spawn(void *param);
};
#endif //ANALOGSCAN_H
