/* Groups.h

   Copyright (C)  David C. J. Matthews 2004  dm at prolingua.co.uk

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
   Or, point your browser to http://www.gnu.org/copyleft/gpl.html

*/


#if !defined(GROUPCLASS_H)
#define GROUPCLASS_H
#include <QString>
#include <QDateTime>
#include <QElapsedTimer>
#include <QList>

#include "Root.h"
#include "Ingredients.h"
#include "BaseClasses.h"
#include "BaseActions.h"

class MHVisible;

// Group Class and the two derived classes.

// 
class MHTimer {
  public:
    int   m_nTimerId {0};
    QTime m_Time;
};

class MHGroup : public MHRoot  
{
  public:
    MHGroup() = default;
    ~MHGroup() override;
    void PrintMe(FILE *fd, int nTabs) const override; // MHRoot

    void Preparation(MHEngine *engine) override; // MHRoot
    void Activation(MHEngine *engine) override; // MHRoot
    void Deactivation(MHEngine *engine) override; // MHRoot
    void Destruction(MHEngine *engine) override; // MHRoot

    MHRoot *FindByObjectNo(int n) override; // MHRoot

    // Actions.
    void SetTimer(int nTimerId, bool fAbsolute, int nMilliSecs, MHEngine *engine) override; // MHRoot
    // This isn't an MHEG action as such but is used as part of the implementation of "Clone"
    void MakeClone(MHRoot *pTarget, MHRoot *pRef, MHEngine *engine) override; // MHRoot

  protected:
    // Set this up from the parse tree.
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHRoot
    // Standard ID, Standard version, Object information aren't recorded.
    int m_nOrigGCPriority {127};
    MHActionSequence m_startUp, m_closeDown;
    MHOwnPtrSequence <MHIngredient> m_items; // Array of items.
    bool m_fIsApp {false};
    friend class MHEngine;

    // Timers are an attribute of the scene class in the standard but have been moved
    // to the group in UK MHEG.  We start this timer when that the group starts
    // running so we know how to calculate expiration times.
    QElapsedTimer m_runTime;
    QList<MHTimer*> m_timers;
    // Checks the timers and fires any relevant events.  Returns the
    // millisecs to the next event or zero if there aren't any.
    std::chrono::milliseconds CheckTimers(MHEngine *engine);
    int m_nLastId {0}; // Highest numbered ingredient.  Used to make new ids for clones.

    friend class MHEGEngine;
};

class MHScene : public MHGroup  
{
  public:
    MHScene() = default;
     // Set this up from the parse tree.
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHGroup
    const char *ClassName() override // MHRoot
        { return "Scene"; }
    void PrintMe(FILE *fd, int nTabs) const override; // MHGroup
    void Activation(MHEngine *engine) override; // MHGroup

    // Actions.
    void SetInputRegister(int nReg, MHEngine *engine) override; // MHRoot
  protected:
    int m_nEventReg      {0};
    int m_nSceneCoordX   {0};
    int m_nSceneCoordY   {0};

    // TODO: In UK MHEG 1.06 the aspect ratio is optional and if not specified "the
    // scene has no aspect ratio".
    int m_nAspectRatioW  {4};
    int m_nAspectRatioH  {3};
    bool m_fMovingCursor {false};
    // We don't use the Next-Scenes info at the moment.
//  MHSceneSeq m_NextScenes; // Preload info for next scenes.
    friend class MHEngine;
};


class MHApplication : public MHGroup  
{
  public:
    MHApplication() { m_fIsApp = true; }
    ~MHApplication() override;
    const char *ClassName() override // MHRoot
        { return "Application"; }
    // Set this up from the parse tree.
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHGroup
    void PrintMe(FILE *fd, int nTabs) const override; // MHGroup
    bool IsShared() override // MHRoot
        { return true; } // The application is "shared".
    void Activation(MHEngine *engine) override; // MHGroup
  protected:
    MHActionSequence m_onSpawnCloseDown, m_onRestart;
    // Default attributes.
    int         m_nCharSet      {0};
    MHColour    m_bgColour, m_textColour, m_buttonRefColour, m_highlightRefColour, m_sliderRefColour;
    int         m_nTextCHook    {0};
    int         m_nIPCHook      {0};
    int         m_nStrCHook     {0};
    int         m_nBitmapCHook  {0};
    int         m_nLineArtCHook {0};
    MHFontBody  m_font;
    MHOctetString   m_fontAttrs;
    int         m_tuneInfo      {0};

    // Internal attributes and additional state
    int         m_nLockCount    {0}; // Count for locking the screen
    // Display stack.  Visible items with the lowest item in the stack first.
    // Later items may obscure earlier.
    MHSequence <MHVisible *> m_displayStack;
    int FindOnStack(const MHRoot *pVis); // Returns the index on the stack or -1 if it's not there.

    MHScene    *m_pCurrentScene {nullptr};
    bool        m_fRestarting   {false};
    QString m_path; // Path from the root directory to this application.  Either the null string or
                    // a string of the form /a/b/c .

    friend class MHEngine;
};

class MHLaunch: public MHElemAction
{
  public:
    MHLaunch(): MHElemAction(":Launch") {}
    void Perform(MHEngine *engine) override; // MHElemAction
};

// Quit the application.
class MHQuit: public MHElemAction
{
  public:
    MHQuit(): MHElemAction(":Quit") {}
    void Perform(MHEngine *engine) override; // MHElemAction
};

// SendEvent - generate an event
class MHSendEvent: public MHElemAction
{
  public:
    MHSendEvent(): MHElemAction(":SendEvent") {}
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHElemAction
    void Perform(MHEngine *engine) override; // MHElemAction
    void PrintArgs(FILE *fd, int nTabs) const override; // MHElemAction
  protected:
    MHGenericObjectRef m_eventSource; // Event source
    enum EventType m_eventType { EventIsAvailable }; // Event type
    MHParameter m_eventData; // Optional - Null means not specified.  Can only be bool, int or string.
};

class MHSetTimer: public MHElemAction
{
  public:
    MHSetTimer(): MHElemAction(":SetTimer") {}
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHElemAction
    void Perform(MHEngine *engine) override; // MHElemAction
  protected:
    void PrintArgs(FILE *fd, int nTabs) const override; // MHElemAction
    MHGenericInteger m_timerId;
    // A new timer may not be specified in which case this cancels the timer.
    // If the timer is specified the "absolute" flag is optional.
    enum : std::uint8_t {
        ST_NoNewTimer,
        ST_TimerAbsolute,
        ST_TimerRelative
    } m_timerType { ST_NoNewTimer };
    MHGenericInteger m_timerValue;
    MHGenericBoolean m_absFlag;
};

class MHSpawn: public MHElemAction
{
  public:
    MHSpawn(): MHElemAction(":Spawn") {}
    void Perform(MHEngine *engine) override; // MHElemAction
};

// Read and Store persistent - read and save data to persistent store.
class MHPersistent: public MHElemAction
{
  public:
    MHPersistent(const char *name, bool fIsLoad): MHElemAction(name), m_fIsLoad(fIsLoad) {}
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHElemAction
    void Perform(MHEngine *engine) override; // MHElemAction
  protected:
    void PrintArgs(FILE *fd, int nTabs) const override; // MHElemAction
    bool m_fIsLoad;
    MHObjectRef     m_succeeded;
    MHOwnPtrSequence<MHObjectRef> m_variables;
    MHGenericOctetString    m_fileName;
};


// TransitionTo - move to a new scene.
class MHTransitionTo: public MHElemAction
{
  public:
    MHTransitionTo();
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHElemAction
    void Perform(MHEngine *engine) override; // MHElemAction
  protected:
    void PrintArgs(FILE *fd, int nTabs) const override; // MHElemAction
    bool    m_fIsTagged         { false };
    int     m_nConnectionTag    {     0 };
    int     m_nTransitionEffect {    -1 };
};

// Lock and unlock functions.
class MHLockScreen: public MHElemAction
{
  public:
    MHLockScreen(): MHElemAction(":LockScreen") {}
    void Perform(MHEngine *engine) override; // MHElemAction
};

class MHUnlockScreen: public MHElemAction
{
  public:
    MHUnlockScreen(): MHElemAction(":UnlockScreen") {}
    void Perform(MHEngine *engine) override; // MHElemAction
};

class MHGetEngineSupport: public MHElemAction
{
  public:
    MHGetEngineSupport(): MHElemAction(":GetEngineSupport")  {}
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHElemAction
    void Perform(MHEngine *engine) override; // MHElemAction
  protected:
    void PrintArgs(FILE *fd, int /*nTabs*/) const override // MHElemAction
        { m_feature.PrintMe(fd, 0);  m_answer.PrintMe(fd, 0); }
    MHGenericOctetString m_feature;
    MHObjectRef m_answer;
};

// Actions added in UK MHEG profile.
class MHSetInputRegister: public MHActionInt
{
  public:
    MHSetInputRegister(): MHActionInt(":SetInputRegister") {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg) override // MHActionInt
        { pTarget->SetInputRegister(nArg, engine); };
};


#endif
