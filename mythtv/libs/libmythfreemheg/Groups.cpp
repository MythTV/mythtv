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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
   Or, point your browser to http://www.gnu.org/copyleft/gpl.html

*/

#include "Root.h"
#include "Groups.h"
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

MHGroup::MHGroup()
{
    m_nOrigGCPriority = 127; // Default.
    m_fIsApp = false;
    m_nLastId = 0;
}

MHGroup::~MHGroup()
{
    while (!m_Timers.isEmpty())
    {
        delete m_Timers.takeFirst();
    }
}

void MHGroup::Initialise(MHParseNode *p, MHEngine *engine)
{
    engine->GetGroupId().Copy(""); // Set to empty before we start (just in case).
    MHRoot::Initialise(p, engine);

    // Must be an external reference with an object number of zero.
    if (m_ObjectReference.m_nObjectNo != 0 || m_ObjectReference.m_GroupId.Size() == 0)
    {
        MHERROR("Object reference for a group object must be zero and external");
    }

    // Set the group id for the rest of the group to this.
    engine->GetGroupId().Copy(m_ObjectReference.m_GroupId);
    // Some of the information is irrelevant.
    //  MHParseNode *pStdId = p->GetNamedArg(C_STANDARD_IDENTIFIER);
    //  MHParseNode *pStdVersion = p->GetNamedArg(C_STANDARD_VERSION);
    //  MHParseNode *pObjectInfo = p->GetNamedArg(C_OBJECT_INFORMATION);

    MHParseNode *pOnStartUp = p->GetNamedArg(C_ON_START_UP);

    if (pOnStartUp)
    {
        m_StartUp.Initialise(pOnStartUp, engine);
    }

    MHParseNode *pOnCloseDown = p->GetNamedArg(C_ON_CLOSE_DOWN);

    if (pOnCloseDown)
    {
        m_CloseDown.Initialise(pOnCloseDown, engine);
    }

    MHParseNode *pOriginalGCPrio = p->GetNamedArg(C_ORIGINAL_GC_PRIORITY);

    if (pOriginalGCPrio)
    {
        m_nOrigGCPriority = pOriginalGCPrio->GetArgN(0)->GetIntValue();
    }

    // Ignore the other stuff at the moment.
    MHParseNode *pItems = p->GetNamedArg(C_ITEMS);

    if (pItems == NULL)
    {
        p->Failure("Missing :Items block");
        return;
    }

    for (int i = 0; i < pItems->GetArgCount(); i++)
    {
        MHParseNode *pItem = pItems->GetArgN(i);
        MHIngredient *pIngredient = NULL;

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
                if (pIngredient->m_ObjectReference.m_nObjectNo > m_nLastId)
                {
                    m_nLastId = pIngredient->m_ObjectReference.m_nObjectNo;
                }

                // Add it to the ingedients of this group.
                m_Items.Append(pIngredient);
            }
        }
        catch (...)
        {
            delete(pIngredient);
            throw;
        }
    }
}

void MHGroup::PrintMe(FILE *fd, int nTabs) const
{
    MHRoot::PrintMe(fd, nTabs);

    if (m_StartUp.Size() != 0)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":OnStartUp (\n");
        m_StartUp.PrintMe(fd, nTabs + 2);
        PrintTabs(fd, nTabs + 2);
        fprintf(fd, ")\n");
    }

    if (m_CloseDown.Size() != 0)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":OnCloseDown (\n");
        m_CloseDown.PrintMe(fd, nTabs + 2);
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

    for (int i = 0; i < m_Items.Size(); i++)
    {
        m_Items.GetAt(i)->PrintMe(fd, nTabs + 2);
    }

    PrintTabs(fd, nTabs + 1);
    fprintf(fd, ")\n");
}

// Preparation - sets up the run-time representation.  Sets m_fAvailable and generates IsAvailable event.
void MHGroup::Preparation(MHEngine *engine)
{
    // Prepare the ingredients first if they are initially active or are initially available programs.
    for (int i = 0; i < m_Items.Size(); i++)
    {
        MHIngredient *pIngredient = m_Items.GetAt(i);

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
    engine->AddActions(m_StartUp);
    engine->RunActions();

    // Activate the ingredients in order.
    for (int i = 0; i < m_Items.Size(); i++)
    {
        MHIngredient *pIngredient = m_Items.GetAt(i);

        if (pIngredient->InitiallyActive())
        {
            pIngredient->Activation(engine);
        }
    }

    m_fRunning = true;
    // Record the time here.  This is the basis for absolute times.
    m_StartTime.start();
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
    engine->AddActions(m_CloseDown);
    engine->RunActions();
    MHRoot::Deactivation(engine);
}

// Destruction - deletes the run-time representation.  Clears m_fAvailable.
void MHGroup::Destruction(MHEngine *engine)
{
    for (int i = m_Items.Size(); i > 0; i--)
    {
        m_Items.GetAt(i - 1)->Destruction(engine);
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

    for (int i = m_Items.Size(); i > 0; i--)
    {
        MHRoot *pResult = m_Items.GetAt(i - 1)->FindByObjectNo(n);

        if (pResult)
        {
            return pResult;
        }
    }

    return NULL;
}

// Set up a timer or cancel a timer.
void MHGroup::SetTimer(int nTimerId, bool fAbsolute, int nMilliSecs, MHEngine *)
{
    // First remove any existing timer with the same Id.
    for (int i = 0; i < m_Timers.size(); i++)
    {
        MHTimer *pTimer = m_Timers.at(i);

        if (pTimer->m_nTimerId == nTimerId)
        {
            delete m_Timers.takeAt(i);
            break;
        }
    }

    // If the time has passed we don't set up a timer.
    QTime currentTime;
    currentTime.start(); // Set current time

    if (nMilliSecs < 0 || (fAbsolute && m_StartTime.addMSecs(nMilliSecs) < currentTime))
    {
        return;
    }

    MHTimer *pTimer = new MHTimer;
    m_Timers.append(pTimer);
    pTimer->m_nTimerId = nTimerId;

    if (fAbsolute)
    {
        pTimer->m_Time = m_StartTime.addMSecs(nMilliSecs);
    }
    else
    {
        pTimer->m_Time = currentTime.addMSecs(nMilliSecs);
    }
}

// Fire any timers that have passed.
int MHGroup::CheckTimers(MHEngine *engine)
{
    QTime currentTime = QTime::currentTime(); // Get current time
    QList<MHTimer *>::iterator it = m_Timers.begin();
    MHTimer *pTimer;
    int nMSecs = 0;

    while (it != m_Timers.end())
    {
        pTimer = *it;

        if (pTimer->m_Time <= currentTime)   // Use <= rather than < here so we fire timers with zero time immediately.
        {
            // If the time has passed trigger the event and remove the timer from the queue.
            engine->EventTriggered(this, EventTimerFired, pTimer->m_nTimerId);
            delete pTimer;
            it = m_Timers.erase(it);
        }
        else
        {
            // This has not yet expired.  Set "nMSecs" to the earliest time we have.
            int nMSecsToGo = currentTime.msecsTo(pTimer->m_Time);

            if (nMSecs == 0 || nMSecsToGo < nMSecs)
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
    pClone->m_ObjectReference.m_GroupId.Copy(m_ObjectReference.m_GroupId); // Group id is the same as this.
    pClone->m_ObjectReference.m_nObjectNo = ++m_nLastId; // Create a new object id.
    m_Items.Append(pClone);
    // Set the object reference result to the newly constructed ref.
    pRef->SetVariableValue(pClone->m_ObjectReference);
    pClone->Preparation(engine); // Prepare the clone.
}

MHApplication::MHApplication()
{
    m_fIsApp = true;
    m_nCharSet = 0;
    m_nTextCHook = 0;
    m_nIPCHook = 0;
    m_nStrCHook = 0;
    m_nBitmapCHook = 0;
    m_nLineArtCHook = 0;
    m_tuneinfo = 0;

    m_pCurrentScene = NULL;
    m_nLockCount = 0;
    m_fRestarting = false;
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
        m_OnSpawnCloseDown.Initialise(pOnSpawn, engine);
    }

    // OnRestart
    MHParseNode *pOnRestart = p->GetNamedArg(C_ON_RESTART);

    if (pOnRestart)
    {
        m_OnRestart.Initialise(pOnRestart, engine);
    }

    // Default attributes.  These are encoded in a group in binary.
    MHParseNode *pDefattrs = p->GetNamedArg(C_DEFAULT_ATTRIBUTES);

    // but in the text form they're encoded in the Application block.
    if (pDefattrs == NULL)
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
        m_BGColour.Initialise(pBGColour->GetArgN(0), engine);
    }

    MHParseNode *pTextColour = pDefattrs->GetNamedArg(C_TEXT_COLOUR);

    if (pTextColour)
    {
        m_TextColour.Initialise(pTextColour->GetArgN(0), engine);
    }

    MHParseNode *pButtonRefColour = pDefattrs->GetNamedArg(C_BUTTON_REF_COLOUR);

    if (pButtonRefColour)
    {
        m_ButtonRefColour.Initialise(pButtonRefColour->GetArgN(0), engine);
    }

    MHParseNode *pHighlightRefColour = pDefattrs->GetNamedArg(C_HIGHLIGHT_REF_COLOUR);

    if (pHighlightRefColour)
    {
        m_HighlightRefColour.Initialise(pHighlightRefColour->GetArgN(0), engine);
    }

    MHParseNode *pSliderRefColour = pDefattrs->GetNamedArg(C_SLIDER_REF_COLOUR);

    if (pSliderRefColour)
    {
        m_SliderRefColour.Initialise(pSliderRefColour->GetArgN(0), engine);
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

    if (pFont == NULL)
    {
        pFont = pDefattrs->GetNamedArg(C_FONT);
    }

    if (pFont)
    {
        m_Font.Initialise(pFont->GetArgN(0), engine);
    }

    // Font attributes.
    MHParseNode *pFontAttrs = pDefattrs->GetNamedArg(C_FONT_ATTRIBUTES);

    if (pFontAttrs)
    {
        pFontAttrs->GetArgN(0)->GetStringValue(m_FontAttrs);
    }
}

void MHApplication::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:Application ");
    MHGroup::PrintMe(fd, nTabs);

    if (m_OnSpawnCloseDown.Size() != 0)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":OnSpawnCloseDown");
        m_OnSpawnCloseDown.PrintMe(fd, nTabs + 1);
        fprintf(fd, "\n");
    }

    if (m_OnRestart.Size() != 0)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":OnRestart");
        m_OnRestart.PrintMe(fd, nTabs + 1);
        fprintf(fd, "\n");
    }

    if (m_nCharSet > 0)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":CharacterSet %d\n", m_nCharSet);
    }

    if (m_BGColour.IsSet())
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":BackgroundColour ");
        m_BGColour.PrintMe(fd, nTabs + 1);
        fprintf(fd, "\n");
    }

    if (m_nTextCHook > 0)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":TextCHook %d\n", m_nTextCHook);
    }

    if (m_TextColour.IsSet())
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":TextColour");
        m_TextColour.PrintMe(fd, nTabs + 1);
        fprintf(fd, "\n");
    }

    if (m_Font.IsSet())
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":Font ");
        m_Font.PrintMe(fd, nTabs + 1);
        fprintf(fd, "\n");
    }

    if (m_FontAttrs.Size() > 0)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":FontAttributes ");
        m_FontAttrs.PrintMe(fd, nTabs + 1);
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

    if (m_ButtonRefColour.IsSet())
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":ButtonRefColour ");
        m_ButtonRefColour.PrintMe(fd, nTabs + 1);
        fprintf(fd, "\n");
    }

    if (m_HighlightRefColour.IsSet())
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":HighlightRefColour ");
        m_HighlightRefColour.PrintMe(fd, nTabs + 1);
        fprintf(fd, "\n");
    }

    if (m_SliderRefColour.IsSet())
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":SliderRefColour ");
        m_SliderRefColour.PrintMe(fd, nTabs + 1);
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
        engine->AddActions(m_OnRestart);
        engine->RunActions();
    }

    engine->EventTriggered(this, EventIsRunning);
}

int MHApplication::FindOnStack(const MHRoot *pVis) // Returns the index on the stack or -1 if it's not there.
{
    for (int i = 0; i < m_DisplayStack.Size(); i++)
    {
        if (m_DisplayStack.GetAt(i) == pVis)
        {
            return i;
        }
    }

    return -1; // Not there
}

MHScene::MHScene()
{
    m_fIsApp = false;
    // TODO: In UK MHEG 1.06 the aspect ratio is optional and if not specified "the
    // scene has no aspect ratio".
    m_nAspectRatioW = 4;
    m_nAspectRatioH = 3;
    m_fMovingCursor = false;
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
    m_EventSource.Initialise(p->GetArgN(1), engine);
    m_EventType = (enum EventType)p->GetArgN(2)->GetEnumValue();

    if (p->GetArgCount() >= 4)
    {
        // TODO: We could check here that we only have bool, int or string and not object ref or content ref.
        m_EventData.Initialise(p->GetArgN(3), engine);
    }
}

void MHSendEvent::PrintArgs(FILE *fd, int) const
{
    m_EventSource.PrintMe(fd, 0);
    QByteArray tmp = MHLink::EventTypeToString(m_EventType).toLatin1();
    fprintf(fd, "%s", tmp.constData());
    fprintf(fd, " ");

    if (m_EventData.m_Type != MHParameter::P_Null)
    {
        m_EventData.PrintMe(fd, 0);
    }
}

void MHSendEvent::Perform(MHEngine *engine)
{
    // The target is always the current scene so we ignore it here.
    MHObjectRef target, source;
    m_Target.GetValue(target, engine); // TODO: Check this is the scene?
    m_EventSource.GetValue(source, engine);

    // Generate the event.
    if (m_EventData.m_Type == MHParameter::P_Null)
    {
        engine->EventTriggered(engine->FindObject(source), m_EventType);
    }
    else
    {
        MHUnion data;
        data.GetValueFrom(m_EventData, engine);
        engine->EventTriggered(engine->FindObject(source), m_EventType, data);
    }
}

void MHSetTimer::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHElemAction::Initialise(p, engine);
    m_TimerId.Initialise(p->GetArgN(1), engine); // The timer id

    if (p->GetArgCount() > 2)
    {
        MHParseNode *pNewTimer = p->GetArgN(2);
        m_TimerValue.Initialise(pNewTimer->GetSeqN(0), engine);

        if (pNewTimer->GetSeqCount() > 1)
        {
            m_TimerType = ST_TimerAbsolute; // May be absolute - depends on the value.
            m_AbsFlag.Initialise(pNewTimer->GetSeqN(1), engine);
        }
        else
        {
            m_TimerType = ST_TimerRelative;
        }
    }
}

void MHSetTimer::PrintArgs(FILE *fd, int /*nTabs*/) const
{
    m_TimerId.PrintMe(fd, 0);

    if (m_TimerType != ST_NoNewTimer)
    {
        fprintf(fd, "( ");
        m_TimerValue.PrintMe(fd, 0);

        if (m_TimerType == ST_TimerAbsolute)
        {
            m_AbsFlag.PrintMe(fd, 0);
        }

        fprintf(fd, ") ");
    }
}

void MHSetTimer::Perform(MHEngine *engine)
{
    int nTimerId = m_TimerId.GetValue(engine);
    bool fAbsolute = false; // Defaults to relative time.
    int newTime = -1;

    switch (m_TimerType)
    {
        case ST_NoNewTimer:
            fAbsolute = true;
            newTime = -1;
            break; // We treat an absolute time of -1 as "cancel"
        case ST_TimerAbsolute:
            fAbsolute = m_AbsFlag.GetValue(engine); // And drop to the next
        case ST_TimerRelative:
            newTime = m_TimerValue.GetValue(engine);
    }

    Target(engine)->SetTimer(nTimerId, fAbsolute, newTime, engine);
}

void MHPersistent::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHElemAction::Initialise(p, engine); // Target
    m_Succeeded.Initialise(p->GetArgN(1), engine);
    MHParseNode *pVarSeq = p->GetArgN(2);

    for (int i = 0; i < pVarSeq->GetSeqCount(); i++)
    {
        MHObjectRef *pVar = new MHObjectRef;
        m_Variables.Append(pVar);
        pVar->Initialise(pVarSeq->GetSeqN(i), engine);
    }

    m_FileName.Initialise(p->GetArgN(3), engine);
}

void MHPersistent::PrintArgs(FILE *fd, int nTabs) const
{
    m_Succeeded.PrintMe(fd, nTabs);
    fprintf(fd, " ( ");

    for (int i = 0; i < m_Variables.Size(); i++)
    {
        m_Variables.GetAt(i)->PrintMe(fd, 0);
    }

    fprintf(fd, " ) ");
    m_FileName.PrintMe(fd, nTabs);
}

void MHPersistent::Perform(MHEngine *engine)
{
    MHObjectRef target;
    m_Target.GetValue(target, engine); // Get the target - this should always be the application
    MHOctetString fileName;
    m_FileName.GetValue(fileName, engine);
    bool fResult = engine->LoadStorePersistent(m_fIsLoad, fileName, m_Variables);
    engine->FindObject(m_Succeeded)->SetVariableValue(fResult);
}

MHTransitionTo::MHTransitionTo(): MHElemAction(":TransitionTo")
{
    m_fIsTagged = false;
    m_nConnectionTag = 0;
    m_nTransitionEffect = -1;
}

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
    m_Target.GetValue(target, engine); // Get the target
    engine->TransitionToScene(target);
}

void MHLaunch::Perform(MHEngine *engine)
{
    MHObjectRef target;
    m_Target.GetValue(target, engine);
    engine->Launch(target);
}
void MHQuit::Perform(MHEngine *engine)
{
    engine->Quit();
}
void MHSpawn::Perform(MHEngine *engine)
{
    MHObjectRef target;
    m_Target.GetValue(target, engine);
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
    m_Feature.Initialise(p->GetArgN(1), engine);
    m_Answer.Initialise(p->GetArgN(2), engine);
}

void MHGetEngineSupport::Perform(MHEngine *engine)
{
    // Ignore the target which isn't used.
    MHOctetString feature;
    m_Feature.GetValue(feature, engine);
    engine->FindObject(m_Answer)->SetVariableValue(engine->GetEngineSupport(feature));
}
