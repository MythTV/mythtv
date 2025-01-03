/* Engine.h

   Copyright (C)  David C. J. Matthews 2004, 2008  dm at prolingua.co.uk

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

#if !defined(ENGINE_H)
#define ENGINE_H

#include "freemheg.h"
#include "Root.h"
#include "Ingredients.h"
#include "BaseClasses.h"
#include "Groups.h"
#include "Visible.h"
#include "Actions.h"
#include "Link.h"
#include <QString>
#include <QRect>
#include <QRegion>
#include <QElapsedTimer>
#include <QList>
#include <QQueue>
#include <QStack>
#include <QTime>

class MHDLADisplay;

// Asynchronous event data.
class MHAsynchEvent {
  public:
    MHRoot *m_pEventSource {nullptr};
    enum EventType m_eventType {EventIsAvailable};
    MHUnion m_eventData;
};

// Entry in the "persistent" store.  At the moment it's not really persistent.
// The UK MHEG profile says we shouldn't store type information.  That complicates
// the code so for the moment we do.
class MHPSEntry {
  public:
    MHPSEntry() = default;
    MHOctetString m_FileName;
    MHOwnPtrSequence <MHUnion> m_Data;
};

// Entry in the pending external content list.
class MHExternContent {
  public:
    QString m_FileName;
    MHIngredient *m_pRequester {nullptr}; 
    QElapsedTimer m_time;
};

class MHInteractible;

class MHEngine: public MHEG {
  public:
    explicit MHEngine(MHContext *context);
    ~MHEngine() override;

    void SetBooting() override // MHEG
        { m_fBooting = true; }

    void DrawDisplay(const QRegion& toDraw) override; // MHEG

    void BootApplication(const char *fileName);
    void TransitionToScene(const MHObjectRef &target);
    bool Launch(const MHObjectRef &target, bool fIsSpawn=false);
    void Spawn(const MHObjectRef &target) { Launch(target, true); }
    void Quit();

    // Look up an object by its object reference.  In nearly all cases we want to throw
    // an exception if it isn't found.  In a very few cases where we don't fail this
    // returns nullptr if it isn't there.
    MHRoot *FindObject(const MHObjectRef &oRef, bool failOnNotFound = true);

    // Called when an event is triggered.  Either queues the event or finds a link that matches.
    void EventTriggered(MHRoot *pSource, enum EventType ev)
    { EventTriggered(pSource, ev , MHUnion()); }
    void EventTriggered(MHRoot *pSource, enum EventType ev, const MHUnion &evData);

    // Called when a link fires to add the actions to the action stack.
    void AddActions(const MHActionSequence &actions);

    // Display stack and draw functions.
    void AddToDisplayStack(MHVisible *pVis);
    void RemoveFromDisplayStack(MHVisible *pVis);
    void Redraw(const QRegion& region); // Request a redraw.
    // Functions to alter the Z-order.
    void BringToFront(const MHRoot *pVis);
    void SendToBack(const MHRoot *pVis);
    void PutBefore(const MHRoot *pVis, const MHRoot *pRef);
    void PutBehind(const MHRoot *pVis, const MHRoot *pRef);
    void LockScreen() { MHApplication *app = CurrentApp(); if (app) app->m_nLockCount++; }
    void UnlockScreen();

    // Run synchronous actions and process any asynchronous events until the queues are empty.
    // Returns the number of milliseconds until wake-up or 0 if none.
    std::chrono::milliseconds RunAll(void) override; // MHEG

    // Run synchronous actions.
    void RunActions();
    // Generate a UserAction event i.e. a key press.
    void GenerateUserAction(int nCode) override; // MHEG
    void EngineEvent(int nCode) override; // MHEG
    void StreamStarted(MHStream *stream, bool bStarted) override; // MHEG

    // Called from an ingredient to request a load of external content.
    void RequestExternalContent(MHIngredient *pRequester);
    void CancelExternalContentRequest(MHIngredient *pRequester);

    // Load from or store to the persistent store.
    bool LoadStorePersistent(bool fIsLoad, const MHOctetString &fileName, const MHSequence<MHObjectRef *> &variables);

    // Add and remove links to and from the active link table.
    void AddLink(MHLink *pLink);
    void RemoveLink(MHLink *pLink);

    bool InTransition() const { return m_fInTransition; }

    bool GetEngineSupport(const MHOctetString &feature);

    // Get the various defaults.  These are extracted from the current app or the (UK) MHEG defaults.
    int GetDefaultCharSet();
    void GetDefaultBGColour(MHColour &colour);
    void GetDefaultTextColour(MHColour &colour);
    void GetDefaultButtonRefColour(MHColour &colour);
    void GetDefaultHighlightRefColour(MHColour &colour);
    void GetDefaultSliderRefColour(MHColour &colour);
    int GetDefaultTextCHook();
    int GetDefaultStreamCHook();
    int GetDefaultBitmapCHook();
//  void GetDefaultFont(MHFontBody &font); // Not currently implemented
    void GetDefaultFontAttrs(MHOctetString &str);
    void SetInputRegister(int nReg);

    MHOctetString &GetGroupId() { return m_currentGroupId; }
    MHContext *GetContext() { return m_context; }

    QString GetPathName(const MHOctetString &str); // Return a path relative to the home directory

    static const char *MHEGEngineProviderIdString;

    // Interaction: Set if an Interactible has the focus and is receiving key presses.
    MHInteractible *GetInteraction(void) { return m_interacting; }
    void SetInteraction(MHInteractible *p) { m_interacting = p; }

    int GetTuneInfo() { MHApplication *app = CurrentApp(); return app ? app->m_tuneInfo : 0; }
    void SetTuneInfo(int tuneinfo) { MHApplication *app = CurrentApp(); if (app) app->m_tuneInfo = tuneinfo; }

  protected:
    void CheckLinks(const MHObjectRef &sourceRef, enum EventType ev, const MHUnion &un);
    MHGroup *ParseProgram(QByteArray &text);
    void DrawRegion(const QRegion& toDraw, int nStackPos);

    QRegion m_redrawRegion; // The accumulation of repaints when the screen is locked.

    // Application stack and functions to get the current application and scene.
    QStack<MHApplication*> m_applicationStack;
    MHApplication *CurrentApp() {
        if (m_applicationStack.isEmpty())
            return nullptr;
        return m_applicationStack.top();
    }
    MHScene *CurrentScene() {
        MHApplication *app = CurrentApp();
        return app ? app->m_pCurrentScene : nullptr;
    }

    // Action stack.  Actions may generate synchronous events which fire links and add
    // new actions.  These new actions have to be processed before we continue with other
    // actions.
    QStack<MHElemAction*> m_actionStack;

    // Asynchronous event queue.  Asynchronous events are added to this queue and handled
    // once the action stack is empty.
    QQueue<MHAsynchEvent*> m_eventQueue;

    // Active Link set.  Active links are included in this table.
    QList<MHLink*> m_linkTable;

    // Pending external content.  If we have requested external content that has not yet arrived
    // we make an entry in this table.
    QList<MHExternContent*> m_externContentTable;
    void CheckContentRequests();

    MHOwnPtrSequence <MHPSEntry> m_persistentStore;

    bool m_fInTransition {false}; // If we get a TransitionTo, Quit etc during OnStartUp and OnCloseDown we ignore them.

    // To canonicalise the object ids we set this to the group id of the current scene or app
    // and use that wherever we get an object id without a group id.
    MHOctetString   m_currentGroupId;

    MHContext       *m_context {nullptr}; // Pointer to the context providing drawing and other operations
    bool            m_fBooting {true};

    MHInteractible  *m_interacting {nullptr}; // Set to current interactive object if any.
};

#endif
