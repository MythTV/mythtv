/* Engine.cpp

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

#include <QStringList>
#include <QRegularExpression>
#include <QThread>

#include "Engine.h"
#include "ParseNode.h"
#include "ParseBinary.h"
#include "ParseText.h"
#include "Root.h"
#include "Groups.h"
#include "ASN1Codes.h"
#include "Logging.h"
#include "freemheg.h"
#include "Visible.h"  // For MHInteractible
#include "Stream.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <memory>

// External creation function.
MHEG *MHCreateEngine(MHContext *context)
{
    return new MHEngine(context);
}

MHEngine::MHEngine(MHContext *context): m_context(context)
{
    // Required for BBC Freeview iPlayer
    auto *pEntry = new MHPSEntry;
    pEntry->m_FileName.Copy("ram://bbcipstr");
    pEntry->m_Data.Append(new MHUnion(true)); // Default true
    // The next value must be true to enable Freeview interaction channel
    pEntry->m_Data.Append(new MHUnion(true)); // Default false
    m_persistentStore.Append(pEntry);
}

MHEngine::~MHEngine()
{
    while (!m_applicationStack.isEmpty())
    {
        delete m_applicationStack.pop();
    }

    while (!m_eventQueue.isEmpty())
    {
        delete m_eventQueue.dequeue();
    }

    while (!m_externContentTable.isEmpty())
    {
        delete m_externContentTable.takeFirst();
    }
}

// Check for external content every 2 seconds.
static constexpr std::chrono::milliseconds CONTENT_CHECK_TIME { 2s };

// This is the main loop of the engine.
std::chrono::milliseconds MHEngine::RunAll()
{
    // Request to boot or reboot
    if (m_fBooting)
    {
        // Reset everything
        while (!m_applicationStack.isEmpty())
        {
            delete m_applicationStack.pop();
        }

        while (!m_eventQueue.isEmpty())
        {
            delete m_eventQueue.dequeue();
        }

        while (!m_externContentTable.isEmpty())
        {
            delete m_externContentTable.takeFirst();
        }

        m_linkTable.clear();

        // UK MHEG applications boot from ~//a or ~//startup.  Actually the initial
        // object can also be explicitly given in the
        MHObjectRef startObj;
        startObj.m_nObjectNo = 0;
        startObj.m_groupId.Copy(MHOctetString("~//a"));

        // Launch will block until either it finds the appropriate object and
        // begins the application or discovers that the file definitely isn't
        // present in the carousel.  It is possible that the object might appear
        // if one of the containing directories is updated.
        if (! Launch(startObj))
        {
            startObj.m_groupId.Copy(MHOctetString("~//startup"));

            if (! Launch(startObj))
            {
                MHLOG(MHLogNotifications, "NOTE Engine auto-boot failed");
                return -1ms;
            }
        }

        m_fBooting = false;
    }

    std::chrono::milliseconds nNextTime = 0ms;

    bool at_least_once {true} ;
    while (!m_eventQueue.isEmpty() || ! m_actionStack.isEmpty() || at_least_once)
    {
        at_least_once = false;

        // Check to see if we need to close.
        if (m_context->CheckStop())
        {
            return 0ms;
        }

        // Run queued actions.
        RunActions();
        // Now the action stack is empty process the next asynchronous event.
        // Processing one event may affect how subsequent events are handled.

        // Check to see if some files we're waiting for have arrived.
        // This could result in ContentAvailable events.
        CheckContentRequests();

        // Check the timers.  This may result in timer events being raised.
        nNextTime = CurrentScene() ? CurrentScene()->CheckTimers(this) : 0ms;

        if (CurrentApp())
        {
            // The UK MHEG profile allows applications to have timers.
            std::chrono::milliseconds nAppTime = CurrentApp()->CheckTimers(this);

            if (nAppTime != 0ms && (nNextTime == 0ms || nAppTime < nNextTime))
            {
                nNextTime = nAppTime;
            }
        }

        if (! m_externContentTable.isEmpty())
        {
            // If we have an outstanding request for external content we need to set a timer.
            if (nNextTime == 0ms || nNextTime > CONTENT_CHECK_TIME)
            {
                nNextTime = CONTENT_CHECK_TIME;
            }
        }

        if (! m_eventQueue.isEmpty())
        {
            MHAsynchEvent *pEvent = m_eventQueue.dequeue();
            MHLOG(MHLogLinks, QString("Asynchronous event dequeued - %1 from %2")
                  .arg(MHLink::EventTypeToString(pEvent->m_eventType),
                       pEvent->m_pEventSource->m_ObjectReference.Printable()));
            CheckLinks(pEvent->m_pEventSource->m_ObjectReference,
                       pEvent->m_eventType, pEvent->m_eventData);
            delete pEvent;
        }
    }

    // Redraw the display if necessary.
    if (! m_redrawRegion.isEmpty())
    {
        m_context->RequireRedraw(m_redrawRegion);
        m_redrawRegion = QRegion();
    }

    return nNextTime;
}


// Convert the parse tree for an application or scene into an object node.
MHGroup *MHEngine::ParseProgram(QByteArray &text)
{
    if (text.size() == 0)
    {
        return nullptr;
    }

    // Look at the first byte to decide whether this is text or binary.  Binary
    // files will begin with 0xA0 or 0xA1, text files with white space, comment ('/')
    // or curly bracket.
    // This is only there for testing: all downloaded objects will be in ASN1
    unsigned char ch = text[0];
    std::unique_ptr<MHParseBase> parser;
    std::unique_ptr<MHGroup> pRes;

    if (ch >= 128)
    {
        parser = std::make_unique<MHParseBinary>(text);
    }
    else
    {
        parser = std::make_unique<MHParseText>(text);
    }

    // Parse the binary or text.
    std::unique_ptr<MHParseNode> pTree ( parser->Parse() );

    switch (pTree->GetTagNo())   // The parse node should be a tagged item.
    {
        case C_APPLICATION:
            pRes = std::make_unique<MHApplication>();
            break;
        case C_SCENE:
            pRes = std::make_unique<MHScene>();
            break;
        default:
            MHParseNode::Failure("Expected Application or Scene"); // throws exception.
    }

    pRes->Initialise(pTree.get(), this); // Convert the parse tree.
    return pRes.release();
}

// Determine protocol for a file
enum EProtocol : std::uint8_t {kProtoUnknown, kProtoDSM, kProtoCI, kProtoHTTP, kProtoHybrid};
static EProtocol PathProtocol(const QString& csPath)
{
    if (csPath.isEmpty() || csPath.startsWith("DSM:") || csPath.startsWith("~"))
        return kProtoDSM;
    if (csPath.startsWith("hybrid:"))
        return kProtoHybrid;
    if (csPath.startsWith("http:") || csPath.startsWith("https:"))
        return kProtoHTTP;
    if (csPath.startsWith("CI:"))
        return kProtoCI;

    int firstColon = csPath.indexOf(':');
    int firstSlash = csPath.indexOf('/');
    if (firstColon > 0 && firstSlash > 0 && firstColon < firstSlash)
        return kProtoUnknown;

    return kProtoDSM;
}

// Launch and Spawn
bool MHEngine::Launch(const MHObjectRef &target, bool fIsSpawn)
{
    if (m_fInTransition)
    {
        MHLOG(MHLogWarning, "WARN Launch during transition - ignoring");
        return false;
    }

    if (target.m_groupId.Size() == 0) return false; // No file name.
    QString csPath = GetPathName(target.m_groupId); // Get path relative to root.
    MHLOG(MHLogNotifications, "NOTE Launching " + csPath);

    // Check that the file exists before we commit to the transition.
    // This may block if we cannot be sure whether the object is present.
    QByteArray text;
    if (! m_context->GetCarouselData(csPath, text))
    {
        if (!m_fBooting)
            EngineEvent(2); // GroupIDRefError
        MHLOG(MHLogWarning, "WARN Launch not found " + csPath);
        return false;
    }

    auto *pProgram = dynamic_cast<MHApplication*>(ParseProgram(text));
    if (! pProgram)
    {
        MHLOG(MHLogWarning, "Empty application");
        return false;
    }
    if (! pProgram->m_fIsApp)
    {
        MHLOG(MHLogWarning, "Expected an application");
        delete pProgram;
        return false;
    }
    if ((gMHLogoptions & MHLogScenes) && gMHLogStream != nullptr)   // Print it so we know what's going on.
    {
        pProgram->PrintMe(gMHLogStream, 0);
    }

    // Clear the action queue of anything pending.
    m_actionStack.clear();

    m_fInTransition = true; // Starting a transition

    try
    {
        if (CurrentApp())
        {
            if (fIsSpawn)   // Run the CloseDown actions.
            {
                AddActions(CurrentApp()->m_closeDown);
                RunActions();
            }

            if (CurrentScene())
            {
                CurrentScene()->Destruction(this);
            }

            CurrentApp()->Destruction(this);

            if (!fIsSpawn)
            {
                delete m_applicationStack.pop();    // Pop and delete the current app.
            }
        }

        // Save the path we use for this app.
        pProgram->m_path = csPath; // Record the path
        int nPos = pProgram->m_path.lastIndexOf('/');

        if (nPos < 0)
        {
            pProgram->m_path = "";
        }
        else
        {
            pProgram->m_path = pProgram->m_path.left(nPos);
        }

        // Have now got the application.
        m_applicationStack.push(pProgram);

        // This isn't in the standard as far as I can tell but we have to do this because
        // we may have events referring to the old application.
        while (!m_eventQueue.isEmpty())
        {
            delete m_eventQueue.dequeue();
        }

        // Activate the application. ....
        CurrentApp()->Activation(this);
        m_fInTransition = false; // The transition is complete
        MHLOG(MHLogNotifications, "NOTE Launch completed OK");
        return true;
    }
    catch (...)
    {
        m_fInTransition = false; // The transition is complete
        return false;
    }
}

void MHEngine::Quit()
{
    if (m_fInTransition)
    {
        MHLOG(MHLogWarning, "WARN Quit during transition - ignoring");
        return;
    }

    m_fInTransition = true; // Starting a transition

    MHScene *scene = CurrentScene();
    if (scene)
        scene->Destruction(this);

    MHApplication *pApp = CurrentApp();
    if (nullptr != pApp)
        pApp->Destruction(this);

    // This isn't in the standard as far as I can tell but we have to do this because
    // we may have events referring to the old application.
    while (!m_eventQueue.isEmpty())
    {
        delete m_eventQueue.dequeue();
    }

    delete m_applicationStack.pop();

    // If the stack is now empty we return to boot mode.
    if (m_applicationStack.isEmpty())
    {
        m_fBooting = true;
    }
    else if (nullptr != pApp)
    {
        pApp->m_fRestarting = true;
        pApp->Activation(this); // This will do any OnRestart actions.
        // Note - this doesn't activate the previously active scene.
    }

    m_fInTransition = false; // The transition is complete
}

void MHEngine::TransitionToScene(const MHObjectRef &target)
{
    if (m_fInTransition)
    {
        // TransitionTo is not allowed in OnStartUp or OnCloseDown actions.
        MHLOG(MHLogWarning, "WARN TransitionTo during transition - ignoring");
        return;
    }

    if (target.m_groupId.Size() == 0)
    {
        return;    // No file name.
    }

    QString csPath = GetPathName(target.m_groupId);

    // Check that the file exists before we commit to the transition.
    // This may block if we cannot be sure whether the object is present.
    QByteArray text;
    if (! m_context->GetCarouselData(csPath, text)) {
        EngineEvent(2); // GroupIDRefError
        return;
    }

    // Parse and run the file.
    MHGroup *pProgram = ParseProgram(text);

    if (!pProgram )
        MHERROR("Empty scene");

    if (pProgram->m_fIsApp)
    {
        delete pProgram;
        MHERROR("Expected a scene");
    }

    // Clear the action queue of anything pending.
    m_actionStack.clear();

    // At this point we have managed to load the scene.
    // Deactivate any non-shared ingredients in the application.
    MHApplication *pApp = CurrentApp();
    if (nullptr == pApp)
    {
        delete pProgram;
        MHERROR("Application disappeared");
    }

    for (int i = pApp->m_items.Size(); i > 0; i--)
    {
        MHIngredient *pItem = pApp->m_items.GetAt(i - 1);

        if (! pItem->IsShared())
        {
            pItem->Deactivation(this);    // This does not remove them from the display stack.
        }
    }

    m_fInTransition = true; // TransitionTo etc are not allowed.

    if (pApp->m_pCurrentScene)
    {
        pApp->m_pCurrentScene->Deactivation(this); // This may involve a call to RunActions
        pApp->m_pCurrentScene->Destruction(this);
    }

    // Everything that belongs to the previous scene should have been removed from the display stack.

    // At this point we may have added actions to the queue as a result of synchronous
    // events during the deactivation.

    // Remove any events from the asynch event queue unless they derive from
    // the application itself or a shared ingredient.

    // This was causing crashes with leftover events being invalid.
    // Changed to clear all events at this point.

    while (!m_eventQueue.isEmpty())
        delete m_eventQueue.dequeue();

    // Can now actually delete the old scene.
    if (pApp->m_pCurrentScene)
    {
        delete(pApp->m_pCurrentScene);
        pApp->m_pCurrentScene = nullptr;
    }

    m_interacting = nullptr;

    // Switch to the new scene.
    pApp->m_pCurrentScene = dynamic_cast< MHScene* >(pProgram);
    MHScene *pScene = CurrentScene();
    if (nullptr != pScene)
    {
        SetInputRegister(pScene->m_nEventReg);
        m_redrawRegion = QRegion(0, 0, pScene->m_nSceneCoordX, pScene->m_nSceneCoordY); // Redraw the whole screen
    }

    if ((gMHLogoptions & MHLogScenes) && gMHLogStream != nullptr)   // Print it so we know what's going on.
    {
        pProgram->PrintMe(gMHLogStream, 0);
    }

    pProgram->Preparation(this);
    pProgram->Activation(this);
    m_fInTransition = false; // The transition is complete
}

void MHEngine::SetInputRegister(int nReg)
{
    m_context->SetInputRegister(nReg); // Enable the appropriate buttons
}

// Create a canonical path name.  The rules are given in the UK MHEG document.
QString MHEngine::GetPathName(const MHOctetString &str)
{
    if (str.Size() == 0)
        return {};

    QString csPath = QString::fromUtf8((const char *)str.Bytes(), str.Size());
    switch (PathProtocol(csPath))
    {
    default:
    case kProtoUnknown:
    case kProtoHybrid:
    case kProtoHTTP:
    case kProtoCI:
        return csPath;
    case kProtoDSM:
        break;
    }

    if (csPath.startsWith("DSM:"))
        csPath = csPath.mid(4); // Remove DSM:
    else if (csPath.startsWith("~"))
        csPath = csPath.mid(1); // Remove ~
    if (!csPath.startsWith("//"))
    {
        // Add the current application's path name
        MHApplication *pApp = CurrentApp();
        if (nullptr != pApp)
        {
            csPath = pApp->m_path + csPath;
        }
    }

    // Remove any occurrences of x/../
    int nPos = -1;

    while ((nPos = csPath.indexOf("/../")) >= 0)
    {
        int nEnd = nPos + 4;

        while (nPos >= 1 && csPath[nPos-1] != '/')
        {
            nPos--;
        }

        csPath = csPath.left(nPos) + csPath.mid(nEnd);
    }

    return csPath;
}

// Look up an object. In most cases we just want to fail if the object isn't found.
MHRoot *MHEngine::FindObject(const MHObjectRef &oRef, bool failOnNotFound)
{
    // It should match either the application or the scene.
    MHGroup *pSearch = nullptr;
    MHGroup *pScene = CurrentScene();
    MHGroup *pApp = CurrentApp();

    if (pScene && GetPathName(pScene->m_ObjectReference.m_groupId) == GetPathName(oRef.m_groupId))
    {
        pSearch = pScene;
    }
    else if (pApp && GetPathName(pApp->m_ObjectReference.m_groupId) == GetPathName(oRef.m_groupId))
    {
        pSearch = pApp;
    }

    if (pSearch)
    {
        MHRoot *pItem = pSearch->FindByObjectNo(oRef.m_nObjectNo);

        if (pItem)
        {
            return pItem;
        }
    }

    if (failOnNotFound)
    {
        // I've seen at least one case where MHEG code has quite deliberately referred to
        // an object that may or may not exist at a particular time.
        // Another case was a call to CallActionSlot with an object reference variable
        // that had been initialised to zero.
        MHLOG(MHLogWarning, QString("WARN Reference %1 not found").arg(oRef.m_nObjectNo));
        throw "FindObject failed";
    }

    return nullptr; // If we don't generate an error.
}

// Run queued actions.
void MHEngine::RunActions()
{
    while (! m_actionStack.isEmpty())
    {
        // Remove the first action.
        MHElemAction *pAction = m_actionStack.pop();

        // Run it.  If it fails and throws an exception catch it and continue with the next.
        try
        {
            if ((gMHLogoptions & MHLogActions) && gMHLogStream != nullptr)   // Debugging
            {
                fprintf(gMHLogStream, "[freemheg] Action - ");
                pAction->PrintMe(gMHLogStream, 0);
                fflush(gMHLogStream);
            }

            pAction->Perform(this);
        }
        catch(const std::exception& ex)
        {
            MHLOG(MHLogDetail, QString("Action - threw %1").arg(ex.what()));
        }
        catch(...)
        {
            MHLOG(MHLogDetail, QString("Action - threw unknown"));
        }
    }
}

// Called when an event is triggered.  Either queues the event or finds a link that matches.
void MHEngine::EventTriggered(MHRoot *pSource, enum EventType ev, const MHUnion &evData)
{
    MHLOG(MHLogLinks, QString("Event - %1 from %2")
          .arg(MHLink::EventTypeToString(ev), pSource->m_ObjectReference.Printable()));

    switch (ev)
    {
        case EventFirstItemPresented:
        case EventHeadItems:
        case EventHighlightOff:
        case EventHighlightOn:
        case EventIsAvailable:
        case EventIsDeleted:
        case EventIsDeselected:
        case EventIsRunning:
        case EventIsSelected:
        case EventIsStopped:
        case EventItemDeselected:
        case EventItemSelected:
        case EventLastItemPresented:
        case EventTailItems:
        case EventTestEvent:
        case EventTokenMovedFrom:
        case EventTokenMovedTo:
            // Synchronous events.  Fire any links that are waiting.
            // The UK MHEG describes this as the preferred interpretation.  We are checking the link
            // at the time we generate the event rather than queuing the synchronous events until
            // this elementary action is complete.  That matters if we are processing an elementary action
            // which will activate or deactivate links.
            CheckLinks(pSource->m_ObjectReference, ev, evData);
            break;
        case EventAnchorFired:
        case EventAsyncStopped:
        case EventContentAvailable:
        case EventCounterTrigger:
        case EventCursorEnter:
        case EventCursorLeave:
        case EventEngineEvent:
        case EventEntryFieldFull:
        case EventInteractionCompleted:
        case EventStreamEvent:
        case EventStreamPlaying:
        case EventStreamStopped:
        case EventTimerFired:
        case EventUserInput:
        case EventFocusMoved: // UK MHEG.  Generated by HyperText class
        case EventSliderValueChanged: // UK MHEG.  Generated by Slider class
        default:
        {
            // Asynchronous events.  Add to the event queue.
            auto *pEvent = new MHAsynchEvent;
            pEvent->m_pEventSource = pSource;
            pEvent->m_eventType = ev;
            pEvent->m_eventData = evData;
            m_eventQueue.enqueue(pEvent);
        }
        break;
    }
}


// TO CHECK: If two actions both depend on the same event is the order in which the links are
// searched defined?  This processes items in the order in which they were activated rather
// than their static position in the group.

// Check all the links in the application and scene and fire any that match this event.
void MHEngine::CheckLinks(const MHObjectRef &sourceRef, enum EventType ev, const MHUnion &un)
{
    for (auto *link : std::as_const(m_linkTable))
        link->MatchEvent(sourceRef, ev, un, this);
}

// Add and remove links to and from the active link table.
void MHEngine::AddLink(MHLink *pLink)
{
    m_linkTable.append(pLink);
}

void MHEngine::RemoveLink(MHLink *pLink)
{
    m_linkTable.removeAll(pLink);
}

// Called when a link fires to add the actions to the action stack.
void MHEngine::AddActions(const MHActionSequence &actions)
{
    // Put them on the stack in reverse order so that we will pop the first.
    for (int i = actions.Size(); i > 0; i--)
    {
        m_actionStack.push(actions.GetAt(i - 1));
    }
}

// Add a visible to the display stack if it isn't already there.
void MHEngine::AddToDisplayStack(MHVisible *pVis)
{
    MHApplication *pApp = CurrentApp();
    if (nullptr == pApp)
        return;
    if (pApp->FindOnStack(pVis) != -1)
    {
        return;    // Return if it's already there.
    }

    pApp->m_displayStack.Append(pVis);
    Redraw(pVis->GetVisibleArea()); // Request a redraw
}

// Remove a visible from the display stack if it is there.
void MHEngine::RemoveFromDisplayStack(MHVisible *pVis)
{
    MHApplication *pApp = CurrentApp();
    if (nullptr == pApp)
        return;
    int nPos = pApp->FindOnStack(pVis);

    if (nPos == -1)
    {
        return;
    }

    pApp->m_displayStack.RemoveAt(nPos);
    Redraw(pVis->GetVisibleArea()); // Request a redraw
}

// Functions to alter the Z-order.
void MHEngine::BringToFront(const MHRoot *p)
{
    MHApplication *pApp = CurrentApp();
    if (nullptr == pApp)
        return;
    int nPos = pApp->FindOnStack(p);

    if (nPos == -1)
    {
        return;    // If it's not there do nothing
    }

    auto *pVis = (MHVisible *)p; // Can now safely cast it.
    pApp->m_displayStack.RemoveAt(nPos); // Remove it from its present posn
    pApp->m_displayStack.Append(pVis); // Push it on the top.
    Redraw(pVis->GetVisibleArea()); // Request a redraw
}

void MHEngine::SendToBack(const MHRoot *p)
{
    MHApplication *pApp = CurrentApp();
    if (nullptr == pApp)
        return;
    int nPos = pApp->FindOnStack(p);

    if (nPos == -1)
    {
        return;    // If it's not there do nothing
    }

    auto *pVis = (MHVisible *)p; // Can now safely cast it.
    pApp->m_displayStack.RemoveAt(nPos); // Remove it from its present posn
    pApp->m_displayStack.InsertAt(pVis, 0); // Put it on the bottom.
    Redraw(pVis->GetVisibleArea()); // Request a redraw
}

void MHEngine::PutBefore(const MHRoot *p, const MHRoot *pRef)
{
    MHApplication *pApp = CurrentApp();
    if (nullptr == pApp)
        return;
    int nPos = pApp->FindOnStack(p);

    if (nPos == -1)
    {
        return;    // If it's not there do nothing
    }

    auto *pVis = (MHVisible *)p; // Can now safely cast it.
    int nRef = pApp->FindOnStack(pRef);

    if (nRef == -1)
    {
        return;    // If the reference visible isn't there do nothing.
    }

    pApp->m_displayStack.RemoveAt(nPos);

    if (nRef >= nPos)
    {
        nRef--;    // The position of the reference may have shifted
    }

    pApp->m_displayStack.InsertAt(pVis, nRef + 1);
    // Redraw the area occupied by the moved item.  We might be able to reduce
    // the area to be redrawn by looking at the way it is affected by other items
    // in the stack.  We could also see whether it's currently active.
    Redraw(pVis->GetVisibleArea()); // Request a redraw
}

void MHEngine::PutBehind(const MHRoot *p, const MHRoot *pRef)
{
    MHApplication *pApp = CurrentApp();
    if (nullptr == pApp)
        return;
    int nPos = pApp->FindOnStack(p);

    if (nPos == -1)
    {
        return;    // If it's not there do nothing
    }

    int nRef = pApp->FindOnStack(pRef);

    if (nRef == -1)
    {
        return;    // If the reference visible isn't there do nothing.
    }

    auto *pVis = (MHVisible *)p; // Can now safely cast it.
    pApp->m_displayStack.RemoveAt(nPos);

    if (nRef >= nPos)
    {
        nRef--;    // The position of the reference may have shifted
    }

    pApp->m_displayStack.InsertAt(pVis, nRef); // Shift the reference and anything above up.
    Redraw(pVis->GetVisibleArea()); // Request a redraw
}

// Draw a region of the screen.  This attempts to minimise the drawing by eliminating items
// that are completely obscured by items above them.  We have to take into account the
// transparency of items since items higher up the stack may be semi-transparent.
void MHEngine::DrawRegion(const QRegion& toDraw, int nStackPos)
{
    MHApplication *pApp = CurrentApp();
    if (nullptr == pApp)
        return;
    if (toDraw.isEmpty())
    {
        return;    // Nothing left to draw.
    }

    while (nStackPos >= 0)
    {
        MHVisible *pItem = pApp->m_displayStack.GetAt(nStackPos);
        // Work out how much of the area we want to draw is included in this visible.
        // The visible area will be empty if the item is transparent or not active.
        QRegion drawArea = pItem->GetVisibleArea() & toDraw;

        if (! drawArea.isEmpty())   // It contributes something.
        {
            // Remove the opaque area of this item from the region we have left.
            // If this item is (semi-)transparent this will not remove anything.
            QRegion newDraw = toDraw - pItem->GetOpaqueArea();
            DrawRegion(newDraw, nStackPos - 1); // Do the items further down if any.
            // Now we've drawn anything below this we can draw this item on top.
            pItem->Display(this);
            return;
        }

        nStackPos--;
    }

    // We've drawn all the visibles and there's still some undrawn area.
    // Fill it with black.
    m_context->DrawBackground(toDraw);
}

// Redraw an area of the display.  This will be called via the context from Redraw.
void MHEngine::DrawDisplay(const QRegion& toDraw)
{
    if (m_fBooting)
    {
        return;
    }

    MHApplication *pApp = CurrentApp();
    int nTopStack = (pApp == nullptr) ? -1 : pApp->m_displayStack.Size() - 1;
    DrawRegion(toDraw, nTopStack);
}

// An area of the screen needs to be redrawn.  We simply remember this and redraw it
// in one go when the timer expires.
void MHEngine::Redraw(const QRegion& region)
{
    m_redrawRegion += region;
}

// Called to decrement the lock count.
void MHEngine::UnlockScreen()
{
    MHApplication *pApp = CurrentApp();
    if (nullptr == pApp)
        return;
    if (pApp->m_nLockCount > 0)
    {
        pApp->m_nLockCount--;
    }
}


// Called from the windowing application, this generates a user event as the result of a button push.
void MHEngine::GenerateUserAction(int nCode)
{
    MHScene *pScene = CurrentScene();

    if (! pScene)
    {
        return;
    }

    // Various keys generate engine events as well as user events.
    // These are generated before the user events and even if there
    // is an interactible.
    switch (nCode)
    {
        case 104:
        case 105: // Text key
            EngineEvent(4);
            break;
        case 16: // Text Exit/Cancel key
        case 100: // Red
        case 101: // Green
        case 102: // Yellow
        case 103: // Blue
        case 300: // EPG
            EngineEvent(nCode);
            break;
    }

    // If we are interacting with an interactible send the key
    // there otherwise generate a user event.
    if (m_interacting)
    {
        m_interacting->KeyEvent(this, nCode);
    }
    else
    {
        EventTriggered(pScene, EventUserInput, nCode);
    }
}

void MHEngine::EngineEvent(int nCode)
{
    if (CurrentApp())
        EventTriggered(CurrentApp(), EventEngineEvent, nCode);
    else if (!m_fBooting)
        MHLOG(MHLogWarning, QString("WARN EngineEvent %1 but no app").arg(nCode));
}

void MHEngine::StreamStarted(MHStream *stream, bool bStarted)
{
    EventTriggered(stream, bStarted ? EventStreamPlaying : EventStreamStopped);
}

// Called by an ingredient wanting external content.
void MHEngine::RequestExternalContent(MHIngredient *pRequester)
{
    // It seems that some MHEG applications contain active ingredients with empty contents
    // This isn't correct but we simply ignore that.
    if (! pRequester->m_contentRef.IsSet())
    {
        return;
    }

    // Remove any existing content requests for this ingredient.
    CancelExternalContentRequest(pRequester);

    QString csPath = GetPathName(pRequester->m_contentRef.m_contentRef);

    if (csPath.isEmpty())
    {
        MHLOG(MHLogWarning, "RequestExternalContent empty path");
        return;
    }
    
    if (m_context->CheckCarouselObject(csPath))
    {
        // Available now - pass it to the ingredient.
        QByteArray text;
        if (m_context->GetCarouselData(csPath, text))
        {
            // If the content is not recognized catch the exception and continue
            try
            {
                pRequester->ContentArrived(
                    reinterpret_cast< const unsigned char * >(text.constData()),
                    text.size(), this);
            }
            catch(const std::exception& ex)
            {
                MHLOG(MHLogDetail, QString("ExternalContent - threw %1").arg(ex.what()));
            }
            catch(...)
            {
                MHLOG(MHLogDetail, QString("ExternalContent - threw unknown"));
            }
        }
        else
        {
            MHLOG(MHLogWarning, QString("WARN No file content %1 <= %2")
                .arg(pRequester->m_ObjectReference.Printable(), csPath));
            if (kProtoHTTP == PathProtocol(csPath))
                EngineEvent(203); // 203=RemoteNetworkError if 404 reply
            EngineEvent(3); // ContentRefError
        }
    }
    else
    {
        // Need to record this and check later.
        MHLOG(MHLogNotifications, QString("Waiting for %1 <= %2")
            .arg(pRequester->m_ObjectReference.Printable(), csPath.left(128)) );
        auto *pContent = new MHExternContent;
        pContent->m_FileName = csPath;
        pContent->m_pRequester = pRequester;
        pContent->m_time.start();
        m_externContentTable.append(pContent);
    }
}

// Remove any pending requests from the queue.
void MHEngine::CancelExternalContentRequest(MHIngredient *pRequester)
{
    QList<MHExternContent *>::iterator it = m_externContentTable.begin();

    while (it != m_externContentTable.end())
    {
        MHExternContent *pContent = *it;

        if (pContent->m_pRequester == pRequester)
        {
            MHLOG(MHLogNotifications, QString("Cancelled wait for %1")
                .arg(pRequester->m_ObjectReference.Printable()) );
            it = m_externContentTable.erase(it);
            delete pContent;
            return;
        }
        ++it;
    }
}

// See if we can satisfy any of the outstanding requests.
void MHEngine::CheckContentRequests()
{
    QList<MHExternContent*>::iterator it = m_externContentTable.begin();
    while (it != m_externContentTable.end())
    {
        MHExternContent *pContent = *it;
        if (m_context->CheckCarouselObject(pContent->m_FileName))
        {
            // Remove from the list.
            it = m_externContentTable.erase(it);

            QByteArray text;
            if (m_context->GetCarouselData(pContent->m_FileName, text))
            {
                MHLOG(MHLogNotifications, QString("Received %1 len %2")
                    .arg(pContent->m_pRequester->m_ObjectReference.Printable())
                    .arg(text.size()) );
                // If the content is not recognized catch the exception and continue
                try
                {
                    pContent->m_pRequester->ContentArrived(
                        reinterpret_cast< const unsigned char * >(text.constData()),
                        text.size(), this);
                }
                catch(const std::exception& ex)
                {
                    MHLOG(MHLogDetail, QString("ContentRequest - threw %1").arg(ex.what()));
                }
                catch(...)
                {
                    MHLOG(MHLogDetail, QString("ContentRequest - threw unknown"));
                }
            }
            else
            {
                MHLOG(MHLogWarning, QString("WARN No file content %1 <= %2")
                    .arg(pContent->m_pRequester->m_ObjectReference.Printable(),
                         pContent->m_FileName));
                if (kProtoHTTP == PathProtocol(pContent->m_FileName))
                    EngineEvent(203); // 203=RemoteNetworkError if 404 reply
                EngineEvent(3); // ContentRefError
            }

            delete pContent;
        }
        else if (pContent->m_time.elapsed() > 60000) // TODO Get this from carousel
        {
            // Remove from the list.
            it = m_externContentTable.erase(it);

            MHLOG(MHLogWarning, QString("WARN File timed out %1 <= %2")
                .arg(pContent->m_pRequester->m_ObjectReference.Printable(),
                     pContent->m_FileName));

            if (kProtoHTTP == PathProtocol(pContent->m_FileName))
                EngineEvent(203); // 203=RemoteNetworkError if 404 reply
            EngineEvent(3); // ContentRefError

            delete pContent;
        }
        else
        {
            ++it;
        }
    }
}

bool MHEngine::LoadStorePersistent(bool fIsLoad, const MHOctetString &fileName, const MHSequence<MHObjectRef *> &variables)
{
    QString const csFile = QString::fromUtf8(
            (const char *)fileName.Bytes(), fileName.Size() );

    // See if there is an entry there already.
    MHPSEntry *pEntry = nullptr;
    int i = 0;

    for (i = 0; i < m_persistentStore.Size(); i++)
    {
        pEntry = m_persistentStore.GetAt(i);

        if (pEntry->m_FileName.Equal(fileName))
        {
            break;
        }
    }

    if (i == m_persistentStore.Size())   // Not there.
    {
        // If we're loading then we've failed.
        if (fIsLoad)
        {
            MHLOG(MHLogNotifications, QString(
                "Load Persistent(%1) #%2: no such file")
                .arg(csFile).arg(variables.Size()) );
            return false;
        }

        // If we're storing we make a new entry.
        pEntry = new MHPSEntry;
        pEntry->m_FileName.Copy(fileName);
        m_persistentStore.Append(pEntry);
    }

    // This should never happen
    if (pEntry == nullptr)
        return false;

    if (fIsLoad)   // Copy the data into the variables.
    {
        // Check that we have sufficient data before we continue?
        if (pEntry->m_Data.Size() < variables.Size())
        {
            MHLOG(MHLogWarning, QString(
                "Load Persistent(%1): size mismatch").arg(csFile));
            return false;
        }

        for (i = 0; i < variables.Size(); i++)
        {
            MHUnion *pValue = pEntry->m_Data.GetAt(i);
            MHLOG(MHLogNotifications, QString("Load Persistent(%1) #%2=%3")
                .arg(csFile).arg(i).arg(pValue->Printable()) );
            FindObject(*(variables.GetAt(i)))->SetVariableValue(*pValue);
        }
    }

    else   // Get the data from the variables into the store.
    {
        // Remove any existing data.
        while (pEntry->m_Data.Size() != 0)
        {
            pEntry->m_Data.RemoveAt(0);
        }

        // Set the store to the values.
        for (i = 0; i < variables.Size(); i++)
        {
            auto *pValue = new MHUnion;
            pEntry->m_Data.Append(pValue);
            FindObject(*(variables.GetAt(i)))->GetVariableValue(*pValue, this);
            MHLOG(MHLogNotifications, QString("Store Persistent(%1) %2=>#%3")
                .arg(csFile, pValue->Printable(), QString::number(i)) );
        }
    }

    return true;
}

// Find out what we support.
bool MHEngine::GetEngineSupport(const MHOctetString &feature)
{
    QString csFeat = QString::fromUtf8((const char *)feature.Bytes(), feature.Size());
    static const QRegularExpression kSeparatorRE { R"([\(\,\)])" };
    QStringList strings = csFeat.split(kSeparatorRE);

    MHLOG(MHLogNotifications, "NOTE GetEngineSupport " + csFeat);

    if (strings[0] == "ApplicationStacking" || strings[0] == "ASt")
    {
        return true;
    }

    // We're required to support cloning for Text, Bitmap and Rectangle.
    if (strings[0] == "Cloning" || strings[0] == "Clo")
    {
        return true;
    }

    if (strings[0] == "SceneCoordinateSystem" || strings[0] == "SCS")
    {
        return strings.count() >= 3 && strings[1] == "720" && strings[2] == "576";

        // I've also seen SceneCoordinateSystem(1,1)
    }

    if (strings[0] == "MultipleAudioStreams" || strings[0] == "MAS")
    {
        return strings.count() >= 2 && (strings[1] == "0" || strings[1] == "1");
    }

    if (strings[0] == "MultipleVideoStreams" || strings[0] == "MVS")
    {
        return strings.count() >= 2 && (strings[1] == "0" || strings[1] == "1");
    }

    // We're supposed to return true for all values of N
    if (strings[0] == "OverlappingVisibles" || strings[0] == "OvV")
    {
        return true;
    }

    if (strings[0] == "SceneAspectRatio" || strings[0] == "SAR")
    {
        if (strings.count() < 3)
        {
            return false;
        }
        return (strings[1] == "4" && strings[2] == "3") || (strings[1] == "16" && strings[2] == "9");
    }

    // We're supposed to support these at least.  May also support(10,1440,1152)
    if (strings[0] == "VideoScaling" || strings[0] == "VSc")
    {
        if (strings.count() < 4 || strings[1] != "10")
        {
            return false;
        }
        return (strings[2] == "720" && strings[3] == "576") || (strings[2] == "360" && strings[3] == "288");
    }

    if (strings[0] == "BitmapScaling" || strings[0] == "BSc")
    {
        if (strings.count() < 4 || strings[1] != "2")
        {
            return false;
        }
        return (strings[2] == "720" && strings[3] == "576") || (strings[2] == "360" && strings[3] == "288");
    }

    // I think we only support the video fully on screen
    if (strings[0] == "VideoDecodeOffset" || strings[0] == "VDO")
    {
        return strings.count() >= 3 && strings[1] == "10" && strings[1] == "0";
    }

    // We support bitmaps that are partially off screen (don't we?)
    if (strings[0] == "BitmapDecodeOffset" || strings[0] == "BDO")
    {
        if (strings.count() >= 3 && strings[1] == "2" && (strings[2] == "0" || strings[2] == "1"))
        {
            return true;
        }
        if (strings.count() >= 2 && (strings[1] == "4" || strings[1] == "6"))
        {
            return true;
        }
        return false;
    }

    if (strings[0] == "UKEngineProfile" || strings[0] == "UniversalEngineProfile" || strings[0] == "UEP")
    {
        if (strings.count() < 2)
        {
            return false;
        }

        if (strings[1] == MHEGEngineProviderIdString)
        {
            return true;
        }

        if (strings[1] == m_context->GetReceiverId())
        {
            return true;
        }

        if (strings[1] == m_context->GetDSMCCId())
        {
            return true;
        }

        // The UK profile 1.06 seems a bit confused on this point.  It is not clear whether
        // we are supposed to return true for UKEngineProfile(2) or not.
        if (strings[1] == "2")
            return true;
        if (strings[1] == "1")
            return true;
        // 'The Space' on Freeview checks this...
        if (strings[1] == "PANT11001")
            return true;
	// Irish DTT expects "1285". From ETSI ES 202 184: UEP(1285) means the receiver has been verified as fully conformant.
        if (strings[1] == "1285")
            return true;
        
        return false;
    }

    // InteractionChannelExtension.
    if (strings[0] == "ICProfile" || strings[0] == "ICP") {
        if (strings.count() < 2) return false;
        if (strings[1] == "0")
            return true; // InteractionChannelExtension.
        if (strings[1] == "1")
            return true; // ICStreamingExtension. This is a deliberate lie
        return false;
    }

    if (strings[0] == "HDExtension" || strings[0] == "HDE") {
        if (strings.count() < 2) return false;
        if (strings[1] == "0")
            return false; // HDVideoExtension.
        if (strings[1] == "1")
            return false; // HDGraphicsPlaneExtension
        return false;
    }
    if (strings[0] == "HDGraphicsPlaneExtension" || strings[0] == "HDG") {
        if (strings.count() < 2) return false;
        // true if HDGraphicsPlaneExtension
        return strings[1] == "0";
    }

    // Otherwise return false.
    return false;
}

// Get the various defaults.  These are extracted from the current app or the (UK) MHEG defaults.
int MHEngine::GetDefaultCharSet()
{
    MHApplication *pApp = CurrentApp();

    if (pApp && pApp->m_nCharSet > 0)
    {
        return pApp->m_nCharSet;
    }
    return 10;    // UK MHEG default.
}

void MHEngine::GetDefaultBGColour(MHColour &colour)
{
    MHApplication *pApp = CurrentApp();

    if (pApp && pApp->m_bgColour.IsSet())
    {
        colour.Copy(pApp->m_bgColour);
    }
    else
    {
        colour.SetFromString("\000\000\000\377", 4);    // '=00=00=00=FF' Default - transparent
    }
}

void MHEngine::GetDefaultTextColour(MHColour &colour)
{
    MHApplication *pApp = CurrentApp();

    if (pApp && pApp->m_textColour.IsSet())
    {
        colour.Copy(pApp->m_textColour);
    }
    else
    {
        colour.SetFromString("\377\377\377\000", 4);    // '=FF=FF=FF=00' UK MHEG Default - white
    }
}

void MHEngine::GetDefaultButtonRefColour(MHColour &colour)
{
    MHApplication *pApp = CurrentApp();

    if (pApp && pApp->m_buttonRefColour.IsSet())
    {
        colour.Copy(pApp->m_buttonRefColour);
    }
    else
    {
        colour.SetFromString("\377\377\377\000", 4);    // '=FF=FF=FF=00' ??? Not specified in UK MHEG
    }
}

void MHEngine::GetDefaultHighlightRefColour(MHColour &colour)
{
    MHApplication *pApp = CurrentApp();

    if (pApp && pApp->m_highlightRefColour.IsSet())
    {
        colour.Copy(pApp->m_highlightRefColour);
    }
    else
    {
        colour.SetFromString("\377\377\377\000", 4);    // '=FF=FF=FF=00' UK MHEG Default - white
    }
}

void MHEngine::GetDefaultSliderRefColour(MHColour &colour)
{
    MHApplication *pApp = CurrentApp();

    if (pApp && pApp->m_sliderRefColour.IsSet())
    {
        colour.Copy(pApp->m_sliderRefColour);
    }
    else
    {
        colour.SetFromString("\377\377\377\000", 4);    // '=FF=FF=FF=00' UK MHEG Default - white
    }
}

int MHEngine::GetDefaultTextCHook()
{
    MHApplication *pApp = CurrentApp();

    if (pApp && pApp->m_nTextCHook > 0)
    {
        return pApp->m_nTextCHook;
    }
    return 10;    // UK MHEG default.
}

int MHEngine::GetDefaultStreamCHook()
{
    MHApplication *pApp = CurrentApp();

    if (pApp && pApp->m_nStrCHook > 0)
    {
        return pApp->m_nStrCHook;
    }
    return 10;    // UK MHEG default.
}

int MHEngine::GetDefaultBitmapCHook()
{
    MHApplication *pApp = CurrentApp();

    if (pApp && pApp->m_nBitmapCHook > 0)
    {
        return pApp->m_nBitmapCHook;
    }
    return 4;    // UK MHEG default - PNG bitmap
}

void MHEngine::GetDefaultFontAttrs(MHOctetString &str)
{
    MHApplication *pApp = CurrentApp();

    if (pApp && pApp->m_fontAttrs.Size() > 0)
    {
        str.Copy(pApp->m_fontAttrs);
    }
    else
    {
        str.Copy("plain.24.24.0");    // TODO: Check this.
    }
}

// An identifier string required by the UK profile.  The "manufacturer" is GNU.
const char *MHEngine::MHEGEngineProviderIdString = "MHGGNU001";

// Define the logging function and settings
int gMHLogoptions = MHLogError;

FILE *gMHLogStream = nullptr;

// The MHEG engine calls this when it needs to log something.
void mhlog_fn(const QString& logtext)
{
    QByteArray tmp = logtext.toLatin1();
    fprintf(gMHLogStream, "[freemheg] %s\n", tmp.constData());
}

// Called from the user of the library to set the logging.
void MHSetLogging(FILE *logStream, unsigned int logLevel)
{
    gMHLogStream = logStream;
    gMHLogoptions = logLevel;
}
