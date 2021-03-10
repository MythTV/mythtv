//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017 MythTV Developers <mythtv-dev@mythtv.org>
//
// This is part of MythTV (https://www.mythtv.org)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef PORTCHECKER_H_
#define PORTCHECKER_H_

#include <QObject>
#include <QString>

#include "mythbaseexp.h"
#include "mythchrono.h"

/** 
 * Small class to handle TCP port checking and finding link-local context.
 *
 * Create an automatic object when needed to perform port checks.
 * It is not thread safe and must not be called recursively.
 * Using an automatic object is the safest way. You can use
 * the same object again to perform a second, third, ..., check.
 *
 */

class MBASE_PUBLIC PortChecker : public QObject
{
    Q_OBJECT

  public:
    PortChecker() = default;
    ~PortChecker() override = default;
    bool checkPort(QString &host, int port, std::chrono::milliseconds timeLimit=30s,
      bool linkLocalOnly=false);

    static bool resolveLinkLocal(QString &host, int port,
      std::chrono::milliseconds timeLimit=30s);

  public slots:
    void cancelPortCheck(void);

  private:
    bool m_cancelCheck {false};
    static void processEvents(void);
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
