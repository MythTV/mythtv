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

#ifndef GUISTARTUP_H_
#define GUISTARTUP_H_

// QT headers
#include <QObject>
#include <QTranslator>
#include <QEventLoop>
#include <QTimer>

// MythDB headers
#include "mythexp.h"

// MythUI headers
#include "mythscreentype.h"

class QEventLoop;
class MythUIButton;
class MythUIText;
class MythScreenStack;
class MythUIStateType;
class MythUIProgressBar;
class MythTimer;

class MPUBLIC GUIStartup : public MythScreenType
{
    Q_OBJECT

  public:

    bool m_Exit;
    bool m_Setup;
    bool m_Retry;
    bool m_Search;

    GUIStartup(MythScreenStack *parent, QEventLoop *eventLoop);
   ~GUIStartup(void);
    bool Create(void);
    bool setStatusState(const QString &name);
    bool setMessageState(const QString &name);
    void setTotal(int total);

  public slots:
    bool updateProgress(bool finished = false);

  private slots:
    void Retry(void);
    void Search(void);
    void Setup(void);
    void Close(void);
    void OnClosePromptReturn(bool submit);

  signals:
    void cancelPortCheck(void);

  private:
    MythUIButton *m_dummyButton;
    MythUIButton *m_retryButton;
    MythUIButton *m_searchButton;
    MythUIButton *m_setupButton;
    MythUIButton *m_exitButton;
    MythUIStateType *m_statusState;
    MythUIStateType *m_messageState;
    MythUIProgressBar *m_progressBar;
    MythTimer *m_progressTimer;
    QEventLoop *m_loop;
    QEventLoop m_dlgLoop;
    int m_total;
    QTimer m_timer;
};

#endif
