/* Groups.cpp

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

#include "Root.h"
#include "Groups.h"

#include <algorithm>

#include "ASN1Codes.h"
#include "ParseNode.h"
#include "Ingredients.h"
#include "Programs.h"
#include "Variables.h"
#include "Presentable.h"
#include "Visible.h"
#include "Engine.h"
#include "Text.h"
#include "Bitmap.h"
#include "Stream.h"
#include "DynamicLineArt.h"
#include "Link.h"
#include "TokenGroup.h"
#include "Logging.h"

MHGroup::~MHGroup()
{
    while (!m_timers.isEmpty())
    {
        delete m_timers.takeFirst();
    }
}

void MHGroup::Initialise(MHParseNode *p, MHEngine *engine)
{
    engine->GetGroupId().Copy(""); // Set to empty before we start (just in case).
    MHRoot::Initialise(p, engine);

    // Must be an external reference with an object number of zero.
    if (m_ObjectReference.m_nObjectNo != 0 || m_ObjectReference.m_groupId.Size() == 0)
    {
        MHERROR("Object reference for a group object must be zero and external");
    }

    // Set the group id for the rest of the group to this.
    engine->GetGroupId().Copy(m_ObjectReference.m_groupId);
    // Some of the information is irrelevant.
    //  MHParseNode *pStdId = p->GetNamedArg(C_STANDARD_IDENTIFIER);
    //  MHParseNode *pStdVersion = p->GetNamedArg(C_STANDARD_VERSION);
    //  MHParseNode *pObjectInfo = p->GetNamedArg(C_OBJECT_INFORMATION);

    MHParseNode *pOnStartUp = p->GetNamedArg(C_ON_START_UP);

    if (pOnStartUp)
    {
        m_startUp.Initialise(pOnStartUp, engine);
    }

    MHParseNode *pOnCloseDown = p->GetNamedArg(C_ON_CLOSE_DOWN);

    if (pOnCloseDown)
    {
        m_closeDown.Initialise(pOnCloseDown, engine);
    }

    MHParseNode *pOriginalGCPrio = p->GetNamedArg(C_ORIGINAL_GC_PRIORITY);

    if (pOriginalGCPrio)
    {
        m_nOrigGCPriority = pOriginalGCPrio->GetArgN(0)->GetIntValue();
    }

    // Ignore the other stuff at the moment.
    MHParseNode *pItems = p->GetNamedArg(C_ITEMS);

    if (pItems == nullptr)
    {
        MHParseNode::Failure("Missing :Items block");
        return;
    }

    for (int i = 0; i < pItems->GetArgCount(); i++)
    {
        MHParseNode *pItem = pItems->GetArgN(i);
        MHIngredient *pIngredient = nullptr;

        try
        {
            // Generate the particular kind of ingredient.
            switch (pItem->GetTagNo())
            {
                case C_RESIDENT_PROGRAM:
                    pIngredient = new MHResidentProgram;
                    break;
                case C_REMOTE_PROGRAM:
                    pIngredient = new MHRemoteProgram;
                    break;
                case C_INTERCHANGED_PROGRAM:
                    pIngredient = new MHInterChgProgram;
                    break;
                case C_PALETTE:
                    pIngredient = new MHPalette;
                    break;
                case C_FONT:
                    pIngredient = new MHFont;
                    break;
                case C_CURSOR_SHAPE:
                    pIngredient = new MHCursorShape;
                    break;
                case C_BOOLEAN_VARIABLE:
                    pIngredient = new MHBooleanVar;
                    break;
                case C_INTEGER_VARIABLE:
                    pIngredient = new MHIntegerVar;
                    break;
                case C_OCTET_STRING_VARIABLE:
                    pIngredient = new MHOctetStrVar;
                    break;
                case C_OBJECT_REF_VARIABLE:
                    pIngredient = new MHObjectRefVar;
                    break;
                case C_CONTENT_REF_VARIABLE:
                    pIngredient = new MHContentRefVar;
                    break;
                case C_LINK:
                    pIngredient = new MHLink;
                    break;
                case C_STREAM:
                    pIngredient = new MHStream;
                    break;
                case C_BITMAP:
                    pIngredient = new MHBitmap;
                    break;
                case C_LINE_ART:
                    pIngredient = new MHLineArt;
                    break;
                case C_DYNAMIC_LINE_ART:
                    pIngredient = new MHDynamicLineArt;
                    break;
                case C_RECTANGLE:
                    pIngredient = new MHRectangle;
                    break;
                case C_HOTSPOT:
                    pIngredient = new MHHotSpot;
                    break;
                case C_SWITCH_BUTTON:
                    pIngredient = new MHSwitchButton;
                    break;
                case C_PUSH_BUTTON:
                    pIngredient = new MHPushButton;
                    break;
                case C_TEXT:
                    pIngredient = new MHText;
                    break;
                case C_ENTRY_FIELD:
                    pIngredient = new MHEntryField;
                    break;
                case C_HYPER_TEXT:
                    pIngredient = new MHHyperText;
                    break;
                case C_SLIDER:
                    pIngredient = new MHSlider;
                    break;
                case C_TOKEN_GROUP:
                    pIngredient = new MHTokenGroup;
                    break;
                case C_LIST_GROUP:
                    pIngredient = new MHListGroup;
                    break;
                default:
                    MHLOG(MHLogWarning, QString("Unknown ingredient %1").arg(pItem->GetTagNo()));
                    // Future proofing: ignore any ingredients that we don't know about.
                    // Obviously these can only arise in the binary coding.
            }

            if (pIngredient)
            {
                // Initialise it from its argments.
                pIngredient->Initialise(pItem, engine);

                // Remember the highest numbered ingredient
                m_nLastId = std::max(pIngredient->m_ObjectReference.m_nObjectNo, m_nLastId);

                // Add it to the ingedients of this group.
                m_items.Append(pIngredient);
            }
        }
        catch (...)
        {
            MHLOG(MHLogError, "ERROR in MHGroup::Initialise ingredient");
            if (pIngredient && gMHLogStream)
                pIngredient->PrintMe(gMHLogStream, 0);

            delete(pIngredient);
            throw;
        }
    }
}

void MHGroup::PrintMe(FILE *fd, int nTabs) const
{
    MHRoot::PrintMe(fd, nTabs);

    if (m_startUp.Size() != 0)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":OnStartUp (\n");
        m_startUp.PrintMe(fd, nTabs + 2);
        PrintTabs(fd, nTabs + 2);
        fprintf(fd, ")\n");
    }

    if (m_closeDown.Size() != 0)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":OnCloseDown (\n");
        m_closeDown.PrintMe(fd, nTabs + 2);
        PrintTabs(fd, nTabs + 2);
        fprintf(fd, ")\n");
    }

    if (m_nOrigGCPriority != 127)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":OrigGCPriority %d\n", m_nOrigGCPriority);
    }

    PrintTabs(fd, nTabs + 1);
    fprintf(fd, ":Items ( \n");

    for (int i = 0; i < m_items.Size(); i++)
    {
        m_items.GetAt(i)->PrintMe(fd, nTabs + 2);
    }

    PrintTabs(fd, nTabs + 1);
    fprintf(fd, ")\n");
}

// Preparation - sets up the run-time representation.  Sets m_fAvailable and generates IsAvailable event.
void MHGroup::Preparation(MHEngine *engine)
{
    // Prepare the ingredients first if they are initially active or are initially available programs.
    for (int i = 0; i < m_items.Size(); i++)
    {
        MHIngredient *pIngredient = m_items.GetAt(i);

        if (pIngredient->InitiallyActive() || pIngredient->InitiallyAvailable())
        {
            pIngredient->Preparation(engine);
        }
    }

    MHRoot::Preparation(engine); // Prepare the root object and send the IsAvailable event.
}

// Activation - starts running the object.  Sets m_fRunning and generates IsRunning event.
void MHGroup::Activation(MHEngine *engine)
{
    if (m_fRunning)
    {
        return;
    }

    MHRoot::Activation(engine);
    // Run any start-up actions.
    engine->AddActions(m_startUp);
    engine->RunActions();

    // Activate the ingredients in order.
    for (int i = 0; i < m_items.Size(); i++)
    {
        MHIngredient *pIngredient = m_items.GetAt(i);

        if (pIngredient->InitiallyActive())
        {
            pIngredient->Activation(engine);
        }
    }

    m_fRunning = true;
    // Start the timer here.  This is the basis for expiration..
    m_runTime.start();
    // Don't generate IsRunning here - that's done by the sub-classes.
}

// Deactivation - stops running the object.  Clears m_fRunning
void MHGroup::Deactivation(MHEngine *engine)
{
    if (! m_fRunning)
    {
        return;
    }

    // Run any close-down actions.
    engine->AddActions(m_closeDown);
    engine->RunActions();
    MHRoot::Deactivation(engine);
}

// Destruction - deletes the run-time representation.  Clears m_fAvailable.
void MHGroup::Destruction(MHEngine *engine)
{
    for (int i = m_items.Size(); i > 0; i--)
    {
        m_items.GetAt(i - 1)->Destruction(engine);
    }

    MHRoot::Destruction(engine);
}

// Return an object if its object ID matches (or if it's a stream, if it has a matching component).
MHRoot *MHGroup::FindByObjectNo(int n)
{
    if (n == m_ObjectReference.m_nObjectNo)
    {
        return this;
    }

    for (int i = m_items.Size(); i > 0; i--)
    {
        MHRoot *pResult = m_items.GetAt(i - 1)->FindByObjectNo(n);

        if (pResult)
        {
            return pResult;
        }
    }

    return nullptr;
}

// Set up a timer or cancel a timer.
void MHGroup::SetTimer(int nTimerId, bool fAbsolute, int nMilliSecs, MHEngine * /*engine*/)
{
    // First remove any existing timer with the same Id.
    for (int i = 0; i < m_timers.size(); i++)
    {
        MHTimer *pTimer = m_timers.at(i);

        if (pTimer->m_nTimerId == nTimerId)
        {
            delete m_timers.takeAt(i);
            break;
        }
    }

    // If the time has passed we don't set up a timer.
    if (nMilliSecs < 0 || (fAbsolute && m_runTime.hasExpired(nMilliSecs)))
    {
        return;
    }

    auto *pTimer = new MHTimer;
    m_timers.append(pTimer);
    pTimer->m_nTimerId = nTimerId;

    if (fAbsolute)
    {
        QTime startTime = QTime::currentTime().addMSecs(-m_runTime.elapsed());
        pTimer->m_Time = startTime.addMSecs(nMilliSecs);
    }
    else
    {
        pTimer->m_Time = QTime::currentTime().addMSecs(nMilliSecs);
    }
}

// Fire any timers that have passed.
std::chrono::milliseconds MHGroup::CheckTimers(MHEngine *engine)
{
    QTime currentTime = QTime::currentTime(); // Get current time
    QList<MHTimer *>::iterator it = m_timers.begin();
    std::chrono::milliseconds nMSecs = 0ms;

    while (it != m_timers.end())
    {
        MHTimer *pTimer = *it;

        if (pTimer->m_Time <= currentTime)   // Use <= rather than < here so we fire timers with zero time immediately.
        {
            // If the time has passed trigger the event and remove the timer from the queue.
            engine->EventTriggered(this, EventTimerFired, pTimer->m_nTimerId);
            delete pTimer;
            it = m_timers.erase(it);
        }
        else
        {
            // This has not yet expired.  Set "nMSecs" to the earliest time we have.
            auto nMSecsToGo = std::chrono::milliseconds(currentTime.msecsTo(pTimer->m_Time));

            if (nMSecs == 0ms || nMSecsToGo < nMSecs)
            {
                nMSecs = nMSecsToGo;
            }

            ++it;
        }
    }

    return nMSecs;
}

// Create a clone of the target and add it to the ingredients.
void MHGroup::MakeClone(MHRoot *pTarget, MHRoot *pRef, MHEngine *engine)
{
    MHIngredient *pClone = pTarget->Clone(engine); // Clone it.
    pClone->m_ObjectReference.m_groupId.Copy(m_ObjectReference.m_groupId); // Group id is the same as this.
    pClone->m_ObjectReference.m_nObjectNo = ++m_nLastId; // Create a new object id.
    m_items.Append(pClone);
    // Set the object reference result to the newly constructed ref.
    pRef->SetVariableValue(pClone->m_ObjectReference);
    pClone->Preparation(engine); // Prepare the clone.
}

MHApplication::~MHApplication()
{
    delete(m_pCurrentScene);
}

void MHApplication::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHGroup::Initialise(p, engine);
    // OnSpawnCloseDown
    MHParseNode *pOnSpawn = p->GetNamedArg(C_ON_SPAWN_CLOSE_DOWN);

    if (pOnSpawn)
    {
        m_onSpawnCloseDown.Initialise(pOnSpawn, engine);
    }

    // OnRestart
    MHParseNode *pOnRestart = p->GetNamedArg(C_ON_RESTART);

    if (pOnRestart)
    {
        m_onRestart.Initialise(pOnRestart, engine);
    }

    // Default attributes.  These are encoded in a group in binary.
    MHParseNode *pDefattrs = p->GetNamedArg(C_DEFAULT_ATTRIBUTES);

    // but in the text form they're encoded in the Application block.
    if (pDefattrs == nullptr)
    {
        pDefattrs = p;
    }

    MHParseNode *pCharSet = pDefattrs->GetNamedArg(C_CHARACTER_SET);

    if (pCharSet)
    {
        m_nCharSet = pCharSet->GetArgN(0)->GetIntValue();
    }

    // Colours
    MHParseNode *pBGColour = pDefattrs->GetNamedArg(C_BACKGROUND_COLOUR);

    if (pBGColour)
    {
        m_bgColour.Initialise(pBGColour->GetArgN(0), engine);
    }

    MHParseNode *pTextColour = pDefattrs->GetNamedArg(C_TEXT_COLOUR);

    if (pTextColour)
    {
        m_textColour.Initialise(pTextColour->GetArgN(0), engine);
    }

    MHParseNode *pButtonRefColour = pDefattrs->GetNamedArg(C_BUTTON_REF_COLOUR);

    if (pButtonRefColour)
    {
        m_buttonRefColour.Initialise(pButtonRefColour->GetArgN(0), engine);
    }

    MHParseNode *pHighlightRefColour = pDefattrs->GetNamedArg(C_HIGHLIGHT_REF_COLOUR);

    if (pHighlightRefColour)
    {
        m_highlightRefColour.Initialise(pHighlightRefColour->GetArgN(0), engine);
    }

    MHParseNode *pSliderRefColour = pDefattrs->GetNamedArg(C_SLIDER_REF_COLOUR);

    if (pSliderRefColour)
    {
        m_sliderRefColour.Initialise(pSliderRefColour->GetArgN(0), engine);
    }

    // Content hooks
    MHParseNode *pTextCHook = pDefattrs->GetNamedArg(C_TEXT_CONTENT_HOOK);

    if (pTextCHook)
    {
        m_nTextCHook = pTextCHook->GetArgN(0)->GetIntValue();
    }

    MHParseNode *pIPCHook = pDefattrs->GetNamedArg(C_IP_CONTENT_HOOK);

    if (pIPCHook)
    {
        m_nIPCHook = pIPCHook->GetArgN(0)->GetIntValue();
    }

    MHParseNode *pStrCHook = pDefattrs->GetNamedArg(C_STREAM_CONTENT_HOOK);

    if (pStrCHook)
    {
        m_nStrCHook = pStrCHook->GetArgN(0)->GetIntValue();
    }

    MHParseNode *pBitmapCHook = pDefattrs->GetNamedArg(C_BITMAP_CONTENT_HOOK);

    if (pBitmapCHook)
    {
        m_nBitmapCHook = pBitmapCHook->GetArgN(0)->GetIntValue();
    }

    MHParseNode *pLineArtCHook = pDefattrs->GetNamedArg(C_LINE_ART_CONTENT_HOOK);

    if (pLineArtCHook)
    {
        m_nLineArtCHook = pLineArtCHook->GetArgN(0)->GetIntValue();
    }

    // Font.  This is a little tricky.  There are two attributes both called Font.
    // In the binary notation the font here is encoded as 42 whereas the text form
    // finds the first occurrence of :Font in the table and returns 13.
    MHParseNode *pFont = pDefattrs->GetNamedArg(C_FONT2);

    if (pFont == nullptr)
    {
        pFont = pDefattrs->GetNamedArg(C_FONT);
    }

    if (pFont)
    {
        m_font.Initialise(pFont->GetArgN(0), engine);
    }

    // Font attributes.
    MHParseNode *pFontAttrs = pDefattrs->GetNamedArg(C_FONT_ATTRIBUTES);

    if (pFontAttrs)
    {
        pFontAttrs->GetArgN(0)->GetStringValue(m_fontAttrs);
    }
}

void MHApplication::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:Application ");
    MHGroup::PrintMe(fd, nTabs);

    if (m_onSpawnCloseDown.Size() != 0)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":OnSpawnCloseDown");
        m_onSpawnCloseDown.PrintMe(fd, nTabs + 1);
        fprintf(fd, "\n");
    }

    if (m_onRestart.Size() != 0)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":OnRestart");
        m_onRestart.PrintMe(fd, nTabs + 1);
        fprintf(fd, "\n");
    }

    if (m_nCharSet > 0)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":CharacterSet %d\n", m_nCharSet);
    }

    if (m_bgColour.IsSet())
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":BackgroundColour ");
        m_bgColour.PrintMe(fd, nTabs + 1);
        fprintf(fd, "\n");
    }

    if (m_nTextCHook > 0)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":TextCHook %d\n", m_nTextCHook);
    }

    if (m_textColour.IsSet())
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":TextColour");
        m_textColour.PrintMe(fd, nTabs + 1);
        fprintf(fd, "\n");
    }

    if (m_font.IsSet())
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":Font ");
        m_font.PrintMe(fd, nTabs + 1);
        fprintf(fd, "\n");
    }

    if (m_fontAttrs.Size() > 0)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":FontAttributes ");
        m_fontAttrs.PrintMe(fd, nTabs + 1);
        fprintf(fd, "\n");
    }

    if (m_nIPCHook > 0)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":InterchgPrgCHook %d\n", m_nIPCHook);
    }

    if (m_nStrCHook > 0)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":StreamCHook %d\n", m_nStrCHook);
    }

    if (m_nBitmapCHook > 0)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":BitmapCHook %d\n", m_nBitmapCHook);
    }

    if (m_nLineArtCHook > 0)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":LineArtCHook %d\n", m_nLineArtCHook);
    }

    if (m_buttonRefColour.IsSet())
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":ButtonRefColour ");
        m_buttonRefColour.PrintMe(fd, nTabs + 1);
        fprintf(fd, "\n");
    }

    if (m_highlightRefColour.IsSet())
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":HighlightRefColour ");
        m_highlightRefColour.PrintMe(fd, nTabs + 1);
        fprintf(fd, "\n");
    }

    if (m_sliderRefColour.IsSet())
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":SliderRefColour ");
        m_sliderRefColour.PrintMe(fd, nTabs + 1);
        fprintf(fd, "\n");
    }

    fprintf(fd, "}\n");
}

// This is a bit messy.  The MHEG corrigendum says that we need to process OnRestart actions
// BEFORE generating IsRunning.  That's important because TransitionTo etc are ignored during
// OnRestart but allowed in events triggered by IsRunning.
void MHApplication::Activation(MHEngine *engine)
{
    if (m_fRunning)
    {
        return;
    }

    MHGroup::Activation(engine);

    if (m_fRestarting)   // Set by Quit
    {
        engine->AddActions(m_onRestart);
        engine->RunActions();
    }

    engine->EventTriggered(this, EventIsRunning);
}

int MHApplication::FindOnStack(const MHRoot *pVis) // Returns the index on the stack or -1 if it's not there.
{
    for (int i = 0; i < m_displayStack.Size(); i++)
    {
        if (m_displayStack.GetAt(i) == pVis)
        {
            return i;
        }
    }

    return -1; // Not there
}

void MHScene::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHGroup::Initialise(p, engine);
    // Event register.
    MHParseNode *pInputEventReg = p->GetNamedArg(C_INPUT_EVENT_REGISTER);

    if (pInputEventReg)
    {
        m_nEventReg = pInputEventReg->GetArgN(0)->GetIntValue();
    }

    // Co-ordinate system
    MHParseNode *pSceneCoords = p->GetNamedArg(C_SCENE_COORDINATE_SYSTEM);

    if (pSceneCoords)
    {
        m_nSceneCoordX = pSceneCoords->GetArgN(0)->GetIntValue();
        m_nSceneCoordY = pSceneCoords->GetArgN(1)->GetIntValue();
    }

    // Aspect ratio
    MHParseNode *pAspectRatio = p->GetNamedArg(C_ASPECT_RATIO);

    if (pAspectRatio)
    {
        // Is the binary encoded as a sequence or a pair of arguments?
        m_nAspectRatioW = pAspectRatio->GetArgN(0)->GetIntValue();
        m_nAspectRatioH = pAspectRatio->GetArgN(1)->GetIntValue();
    }

    // Moving cursor
    MHParseNode *pMovingCursor = p->GetNamedArg(C_MOVING_CURSOR);

    if (pMovingCursor)
    {
        pMovingCursor->GetArgN(0)->GetBoolValue();
    }

    // Next scene sequence: this is just a hint and isn't implemented
}

void MHScene::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:Scene ");
    MHGroup::PrintMe(fd, nTabs);
    PrintTabs(fd, nTabs + 1);
    fprintf(fd, ":InputEventReg %d\n", m_nEventReg);
    PrintTabs(fd, nTabs + 1);
    fprintf(fd, ":SceneCS %d %d\n", m_nSceneCoordX, m_nSceneCoordY);

    if (m_nAspectRatioW != 4 || m_nAspectRatioH != 3)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":AspectRatio %d %d\n", m_nAspectRatioW, m_nAspectRatioH);
    }

    if (m_fMovingCursor)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":MovingCursor true\n");
    }

    fprintf(fd, "}\n");
}

void MHScene::Activation(MHEngine *engine)
{
    if (m_fRunning)
    {
        return;
    }

    MHGroup::Activation(engine);
    engine->EventTriggered(this, EventIsRunning);
}

// Action added in UK MHEG profile.  It doesn't define a new internal attribute for this.
void MHScene::SetInputRegister(int nReg, MHEngine *engine)
{
    m_nEventReg = nReg;
    engine->SetInputRegister(nReg);
}


void MHSendEvent::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHElemAction::Initialise(p, engine); // Target
    m_eventSource.Initialise(p->GetArgN(1), engine);
    m_eventType = (enum EventType)p->GetArgN(2)->GetEnumValue();

    if (p->GetArgCount() >= 4)
    {
        // TODO: We could check here that we only have bool, int or string and not object ref or content ref.
        m_eventData.Initialise(p->GetArgN(3), engine);
    }
}

void MHSendEvent::PrintArgs(FILE *fd, int /*nTabs*/) const
{
    m_eventSource.PrintMe(fd, 0);
    QByteArray tmp = MHLink::EventTypeToString(m_eventType).toLatin1();
    fprintf(fd, "%s", tmp.constData());
    fprintf(fd, " ");

    if (m_eventData.m_Type != MHParameter::P_Null)
    {
        m_eventData.PrintMe(fd, 0);
    }
}

void MHSendEvent::Perform(MHEngine *engine)
{
    // The target is always the current scene so we ignore it here.
    MHObjectRef target;
    MHObjectRef source;
    m_target.GetValue(target, engine); // TODO: Check this is the scene?
    m_eventSource.GetValue(source, engine);

    // Generate the event.
    if (m_eventData.m_Type == MHParameter::P_Null)
    {
        engine->EventTriggered(engine->FindObject(source), m_eventType);
    }
    else
    {
        MHUnion data;
        data.GetValueFrom(m_eventData, engine);
        engine->EventTriggered(engine->FindObject(source), m_eventType, data);
    }
}

void MHSetTimer::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHElemAction::Initialise(p, engine);
    m_timerId.Initialise(p->GetArgN(1), engine); // The timer id

    if (p->GetArgCount() > 2)
    {
        MHParseNode *pNewTimer = p->GetArgN(2);
        m_timerValue.Initialise(pNewTimer->GetSeqN(0), engine);

        if (pNewTimer->GetSeqCount() > 1)
        {
            m_timerType = ST_TimerAbsolute; // May be absolute - depends on the value.
            m_absFlag.Initialise(pNewTimer->GetSeqN(1), engine);
        }
        else
        {
            m_timerType = ST_TimerRelative;
        }
    }
}

void MHSetTimer::PrintArgs(FILE *fd, int /*nTabs*/) const
{
    m_timerId.PrintMe(fd, 0);

    if (m_timerType != ST_NoNewTimer)
    {
        fprintf(fd, "( ");
        m_timerValue.PrintMe(fd, 0);

        if (m_timerType == ST_TimerAbsolute)
        {
            m_absFlag.PrintMe(fd, 0);
        }

        fprintf(fd, ") ");
    }
}

void MHSetTimer::Perform(MHEngine *engine)
{
    int nTimerId = m_timerId.GetValue(engine);
    bool fAbsolute = false; // Defaults to relative time.
    int newTime = -1;

    switch (m_timerType)
    {
        case ST_NoNewTimer:
            fAbsolute = true;
            newTime = -1;
            break; // We treat an absolute time of -1 as "cancel"
        case ST_TimerAbsolute:
            fAbsolute = m_absFlag.GetValue(engine);
            [[fallthrough]];
        case ST_TimerRelative:
            newTime = m_timerValue.GetValue(engine);
    }

    Target(engine)->SetTimer(nTimerId, fAbsolute, newTime, engine);
}

void MHPersistent::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHElemAction::Initialise(p, engine); // Target
    m_succeeded.Initialise(p->GetArgN(1), engine);
    MHParseNode *pVarSeq = p->GetArgN(2);

    for (int i = 0; i < pVarSeq->GetSeqCount(); i++)
    {
        auto *pVar = new MHObjectRef;
        m_variables.Append(pVar);
        pVar->Initialise(pVarSeq->GetSeqN(i), engine);
    }

    m_fileName.Initialise(p->GetArgN(3), engine);
}

void MHPersistent::PrintArgs(FILE *fd, int nTabs) const
{
    m_succeeded.PrintMe(fd, nTabs);
    fprintf(fd, " ( ");

    for (int i = 0; i < m_variables.Size(); i++)
    {
        m_variables.GetAt(i)->PrintMe(fd, 0);
    }

    fprintf(fd, " ) ");
    m_fileName.PrintMe(fd, nTabs);
}

void MHPersistent::Perform(MHEngine *engine)
{
    MHObjectRef target;
    m_target.GetValue(target, engine); // Get the target - this should always be the application
    MHOctetString fileName;
    m_fileName.GetValue(fileName, engine);
    bool fResult = engine->LoadStorePersistent(m_fIsLoad, fileName, m_variables);
    engine->FindObject(m_succeeded)->SetVariableValue(fResult);
}

MHTransitionTo::MHTransitionTo()
  : MHElemAction(":TransitionTo")
    {}

void MHTransitionTo::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHElemAction::Initialise(p, engine); // Target
    // The next one may be present but NULL in binary.
    if (p->GetArgCount() > 1)
    {
        MHParseNode *pCtag = p->GetArgN(1);

        if (pCtag->m_nNodeType == MHParseNode::PNInt)
        {
            m_fIsTagged = true;
            m_nConnectionTag = pCtag->GetIntValue();
        }
    }

    if (p->GetArgCount() > 2)
    {
        MHParseNode *pTrEff = p->GetArgN(2);
        m_nTransitionEffect = pTrEff->GetIntValue();
    }
}

void MHTransitionTo::PrintArgs(FILE *fd, int /*nTabs*/) const
{
    if (m_fIsTagged)
    {
        fprintf(fd, " %d ", m_nConnectionTag);
    }
    else if (m_nTransitionEffect >= 0)
    {
        fprintf(fd, " NULL ");
    }

    if (m_nTransitionEffect >= 0)
    {
        fprintf(fd, " %d", m_nTransitionEffect);
    }
}

// Do the action - Transition to a new scene.
void MHTransitionTo::Perform(MHEngine *engine)
{
    MHObjectRef target;
    m_target.GetValue(target, engine); // Get the target
    engine->TransitionToScene(target);
}

void MHLaunch::Perform(MHEngine *engine)
{
    MHObjectRef target;
    m_target.GetValue(target, engine);
    engine->Launch(target);
}
void MHQuit::Perform(MHEngine *engine)
{
    engine->Quit();
}
void MHSpawn::Perform(MHEngine *engine)
{
    MHObjectRef target;
    m_target.GetValue(target, engine);
    engine->Spawn(target);
}
void MHLockScreen::Perform(MHEngine *engine)
{
    engine->LockScreen();
}
void MHUnlockScreen::Perform(MHEngine *engine)
{
    engine->UnlockScreen();
}

void MHGetEngineSupport::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHElemAction::Initialise(p, engine);
    m_feature.Initialise(p->GetArgN(1), engine);
    m_answer.Initialise(p->GetArgN(2), engine);
}

void MHGetEngineSupport::Perform(MHEngine *engine)
{
    // Ignore the target which isn't used.
    MHOctetString feature;
    m_feature.GetValue(feature, engine);
    engine->FindObject(m_answer)->SetVariableValue(engine->GetEngineSupport(feature));
}
