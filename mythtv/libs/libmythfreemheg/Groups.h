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
    int m_nTimerId;
    QTime m_Time;
};

class MHGroup : public MHRoot  
{
  public:
    MHGroup();
    virtual ~MHGroup();
    virtual void PrintMe(FILE *fd, int nTabs) const;

    virtual void Preparation(MHEngine *engine);
    virtual void Activation(MHEngine *engine);
    virtual void Deactivation(MHEngine *engine);
    virtual void Destruction(MHEngine *engine);

    virtual MHRoot *FindByObjectNo(int n);

    // Actions.
    virtual void SetTimer(int nTimerId, bool fAbsolute, int nMilliSecs, MHEngine *);
    // This isn't an MHEG action as such but is used as part of the implementation of "Clone"
    virtual void MakeClone(MHRoot *pTarget, MHRoot *pRef, MHEngine *engine);

  protected:
    void Initialise(MHParseNode *p, MHEngine *engine); // Set this up from the parse tree.
    // Standard ID, Standard version, Object information aren't recorded.
    int m_nOrigGCPriority;
    MHActionSequence m_StartUp, m_CloseDown;
    MHOwnPtrSequence <MHIngredient> m_Items; // Array of items.
    bool m_fIsApp;
    friend class MHEngine;

    // Timers are an attribute of the scene class in the standard but have been moved
    // to the group in UK MHEG.  We record the time that the group starts running so
    // we know how to calculate absolute times.
    QTime m_StartTime;
    QList<MHTimer*> m_Timers;
    int CheckTimers(MHEngine *engine); // Checks the timers and fires any relevant events.  Returns the millisecs to the
                        // next event or zero if there aren't any.
    int m_nLastId; // Highest numbered ingredient.  Used to make new ids for clones.

    friend class MHEGEngine;
};

class MHScene : public MHGroup  
{
  public:
    MHScene();
    void Initialise(MHParseNode *p, MHEngine *engine); // Set this up from the parse tree.
    virtual const char *ClassName() { return "Scene"; }
    virtual void PrintMe(FILE *fd, int nTabs) const;
    virtual void Activation(MHEngine *engine);

    // Actions.
    virtual void SetInputRegister(int nReg, MHEngine *engine);
  protected:
    int m_nEventReg;
    int m_nSceneCoordX, m_nSceneCoordY;
    int m_nAspectRatioW, m_nAspectRatioH;
    bool m_fMovingCursor;
    // We don't use the Next-Scenes info at the moment.
//  MHSceneSeq m_NextScenes; // Preload info for next scenes.
    friend class MHEngine;
};


class MHApplication : public MHGroup  
{
  public:
    MHApplication();
    virtual ~MHApplication();
    virtual const char *ClassName() { return "Application"; }
    void Initialise(MHParseNode *p, MHEngine *engine); // Set this up from the parse tree.
    virtual void PrintMe(FILE *fd, int nTabs) const;
    virtual bool IsShared() { return true; } // The application is "shared".
    virtual void Activation(MHEngine *engine);
  protected:
    MHActionSequence m_OnSpawnCloseDown, m_OnRestart;
    // Default attributes.
    int         m_nCharSet;
    MHColour    m_BGColour, m_TextColour, m_ButtonRefColour, m_HighlightRefColour, m_SliderRefColour;
    int         m_nTextCHook, m_nIPCHook, m_nStrCHook, m_nBitmapCHook, m_nLineArtCHook;
    MHFontBody  m_Font;
    MHOctetString   m_FontAttrs;
    int         m_tuneinfo;

    // Internal attributes and additional state
    int  m_nLockCount; // Count for locking the screen
    // Display stack.  Visible items with the lowest item in the stack first.
    // Later items may obscure earlier.
    MHSequence <MHVisible *> m_DisplayStack;
    int FindOnStack(const MHRoot *pVis); // Returns the index on the stack or -1 if it's not there.

    MHScene *m_pCurrentScene;
    bool m_fRestarting;
    QString m_Path; // Path from the root directory to this application.  Either the null string or
                    // a string of the form /a/b/c .

    friend class MHEngine;
};

class MHLaunch: public MHElemAction
{
  public:
    MHLaunch(): MHElemAction(":Launch") {}
    virtual void Perform(MHEngine *engine);
};

// Quit the application.
class MHQuit: public MHElemAction
{
  public:
    MHQuit(): MHElemAction(":Quit") {}
    virtual void Perform(MHEngine *engine);
};

// SendEvent - generate an event
class MHSendEvent: public MHElemAction
{
  public:
    MHSendEvent(): MHElemAction(":SendEvent"), m_EventType(EventIsAvailable) {}
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void Perform(MHEngine *engine);
    virtual void PrintArgs(FILE *fd, int nTabs) const;
  protected:
    MHGenericObjectRef m_EventSource; // Event source
    enum EventType m_EventType; // Event type
    MHParameter m_EventData; // Optional - Null means not specified.  Can only be bool, int or string.
};

class MHSetTimer: public MHElemAction
{
  public:
    MHSetTimer(): MHElemAction(":SetTimer"), m_TimerType(ST_NoNewTimer) {}
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void Perform(MHEngine *engine);
  protected:
    virtual void PrintArgs(FILE *fd, int nTabs) const;
    MHGenericInteger m_TimerId;
    // A new timer may not be specified in which case this cancels the timer.
    // If the timer is specified the "absolute" flag is optional.
    enum { ST_NoNewTimer, ST_TimerAbsolute, ST_TimerRelative } m_TimerType;
    MHGenericInteger m_TimerValue;
    MHGenericBoolean m_AbsFlag;
};

class MHSpawn: public MHElemAction
{
  public:
    MHSpawn(): MHElemAction(":Spawn") {}
    virtual void Perform(MHEngine *engine);
};

// Read and Store persistent - read and save data to persistent store.
class MHPersistent: public MHElemAction
{
  public:
    MHPersistent(const char *name, bool fIsLoad): MHElemAction(name), m_fIsLoad(fIsLoad) {}
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void Perform(MHEngine *engine);
  protected:
    virtual void PrintArgs(FILE *fd, int nTabs) const;
    bool m_fIsLoad;
    MHObjectRef     m_Succeeded;
    MHOwnPtrSequence<MHObjectRef> m_Variables;
    MHGenericOctetString    m_FileName;
};


// TransitionTo - move to a new scene.
class MHTransitionTo: public MHElemAction
{
  public:
    MHTransitionTo();
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void Perform(MHEngine *engine);
  protected:
    virtual void PrintArgs(FILE *fd, int nTabs) const;
    bool    m_fIsTagged;
    int     m_nConnectionTag;
    int     m_nTransitionEffect;
};

// Lock and unlock functions.
class MHLockScreen: public MHElemAction
{
  public:
    MHLockScreen(): MHElemAction(":LockScreen") {}
    virtual void Perform(MHEngine *engine);
};

class MHUnlockScreen: public MHElemAction
{
  public:
    MHUnlockScreen(): MHElemAction(":UnlockScreen") {}
    virtual void Perform(MHEngine *engine);
};

class MHGetEngineSupport: public MHElemAction
{
  public:
    MHGetEngineSupport(): MHElemAction(":GetEngineSupport")  {}
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void Perform(MHEngine *engine);
  protected:
    virtual void PrintArgs(FILE *fd, int /*nTabs*/) const { m_Feature.PrintMe(fd, 0);  m_Answer.PrintMe(fd, 0); }
    MHGenericOctetString m_Feature;
    MHObjectRef m_Answer;
};

// Actions added in UK MHEG profile.
class MHSetInputRegister: public MHActionInt
{
  public:
    MHSetInputRegister(): MHActionInt(":SetInputRegister") {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg) { pTarget->SetInputRegister(nArg, engine); };
};


#endif
