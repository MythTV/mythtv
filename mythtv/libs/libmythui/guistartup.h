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
#include "mythuiexp.h"

// MythBase headers
#include "libmythbase/mythchrono.h"

// MythUI headers
#include "libmythui/mythscreentype.h"

class QEventLoop;
class MythUIButton;
class MythUIText;
class MythScreenStack;
class MythUIStateType;
class MythUIProgressBar;
class MythTimer;

class MUI_PUBLIC GUIStartup : public MythScreenType
{
    Q_OBJECT

  public:

    bool m_Exit   {false};
    bool m_Setup  {false};
    bool m_Retry  {false};
    bool m_Search {false};

    GUIStartup(MythScreenStack *parent, QEventLoop *eventLoop);
   ~GUIStartup(void) override;
    bool Create(void) override; // MythScreenType
    bool setStatusState(const QString &name);
    bool setMessageState(const QString &name);
    void setTotal(std::chrono::seconds total);

  public slots:
    bool updateProgress(bool finished);
    void updateProgress(void);

  private slots:
    void Retry(void);
    void Search(void);
    void Setup(void);
    void Close(void) override; // MythScreenType
    void OnClosePromptReturn(bool submit);

  signals:
    void cancelPortCheck(void);

  private:
    MythUIButton      *m_dummyButton   {nullptr};
    MythUIButton      *m_retryButton   {nullptr};
    MythUIButton      *m_searchButton  {nullptr};
    MythUIButton      *m_setupButton   {nullptr};
    MythUIButton      *m_exitButton    {nullptr};
    MythUIStateType   *m_statusState   {nullptr};
    MythUIStateType   *m_messageState  {nullptr};
    MythUIProgressBar *m_progressBar   {nullptr};
    MythTimer         *m_progressTimer {nullptr};
    QEventLoop        *m_loop          {nullptr};
    QEventLoop         m_dlgLoop;
    std::chrono::milliseconds m_total  {0ms};
    QTimer             m_timer;
};

#endif
