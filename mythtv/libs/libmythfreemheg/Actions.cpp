/* Actions.cpp

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
   Or, point your browser to http://www.gnu.org/copyleft/gpl.html

*/

#include <stdio.h>

#include "BaseActions.h"
#include "Actions.h"
#include "BaseClasses.h"
#include "ParseNode.h"
#include "ASN1Codes.h"
#include "Ingredients.h"
#include "Engine.h"
#include "Variables.h"
#include "Programs.h"
#include "Bitmap.h"
#include "Visible.h"
#include "Text.h"
#include "DynamicLineArt.h"
#include "Link.h"
#include "TokenGroup.h"
#include "Stream.h"
#include "Logging.h"


// Temporary place-holder for actions we haven't done yet.
class MHUnimplementedAction: public MHElemAction
{
  public:
    MHUnimplementedAction(int nTag): MHElemAction(""), m_nTag(nTag)
    {
        MHLOG(MHLogWarning, QString("WARN Unimplemented action %1").arg(m_nTag) );
    }
    virtual void Initialise(MHParseNode *, MHEngine *) {}
    virtual void PrintMe(FILE *fd, int /*nTabs*/) const
    {
        fprintf(fd, "****Missing action %d\n", m_nTag);
    }
    virtual void Perform(MHEngine *)
    {
        MHERROR(QString("Unimplemented action %1").arg(m_nTag));
    }
  protected:
    int m_nTag;
};

// ActionSequence.

// Process the action set and create the action values.
void MHActionSequence::Initialise(MHParseNode *p, MHEngine *engine)
{
    // Depending on the caller we may have a tagged argument list or a sequence.
    for (int i = 0; i < p->GetArgCount(); i++)
    {
        MHParseNode *pElemAction = p->GetArgN(i);
        MHElemAction *pAction;

        switch (pElemAction->GetTagNo())
        {
            case C_ACTIVATE:
                pAction = new MHActivate(":Activate", true);
                break;
            case C_ADD:
                pAction = new MHAdd;
                break;
            case C_ADD_ITEM:
                pAction = new MHAddItem;
                break;
            case C_APPEND:
                pAction = new MHAppend;
                break;
            case C_BRING_TO_FRONT:
                pAction = new MHBringToFront;
                break;
            case C_CALL:
                pAction = new MHCall(":Call", false);
                break;
            case C_CALL_ACTION_SLOT:
                pAction = new MHCallActionSlot;
                break;
            case C_CLEAR:
                pAction = new MHClear;
                break;
            case C_CLONE:
                pAction = new MHClone;
                break;
            case C_CLOSE_CONNECTION:
                pAction = new MHUnimplementedAction(pElemAction->GetTagNo());
                break; // ??
            case C_DEACTIVATE:
                pAction = new MHActivate(":Deactivate", false);
                break;
            case C_DEL_ITEM:
                pAction = new MHDelItem;
                break;
            case C_DESELECT:
                pAction = new MHUnimplementedAction(pElemAction->GetTagNo());
                break; // Button
            case C_DESELECT_ITEM:
                pAction = new MHDeselectItem;
                break;
            case C_DIVIDE:
                pAction = new MHDivide;
                break;
            case C_DRAW_ARC:
                pAction = new MHDrawArcSector(":DrawArc", false);
                break;
            case C_DRAW_LINE:
                pAction = new MHDrawLine;
                break;
            case C_DRAW_OVAL:
                pAction = new MHDrawOval;
                break;
            case C_DRAW_POLYGON:
                pAction = new MHDrawPoly(":DrawPolygon", true);
                break;
            case C_DRAW_POLYLINE:
                pAction = new MHDrawPoly(":DrawPolyline", false);
                break;
            case C_DRAW_RECTANGLE:
                pAction = new MHDrawRectangle;
                break;
            case C_DRAW_SECTOR:
                pAction = new MHDrawArcSector(":DrawSector", true);
                break;
            case C_FORK:
                pAction = new MHCall(":Fork", true);
                break;
            case C_GET_AVAILABILITY_STATUS:
                pAction = new MHGetAvailabilityStatus;
                break;
            case C_GET_BOX_SIZE:
                pAction = new MHGetBoxSize;
                break;
            case C_GET_CELL_ITEM:
                pAction = new MHGetCellItem;
                break;
            case C_GET_CURSOR_POSITION:
                pAction = new MHUnimplementedAction(pElemAction->GetTagNo());
                break; // ?
            case C_GET_ENGINE_SUPPORT:
                pAction = new MHGetEngineSupport;
                break;
            case C_GET_ENTRY_POINT:
                pAction = new MHUnimplementedAction(pElemAction->GetTagNo());
                break; // EntryField
            case C_GET_FILL_COLOUR:
                pAction = new MHGetFillColour;
                break;
            case C_GET_FIRST_ITEM:
                pAction = new MHGetFirstItem;
                break;
            case C_GET_HIGHLIGHT_STATUS:
                pAction = new MHGetHighlightStatus;
                break;
            case C_GET_INTERACTION_STATUS:
                pAction = new MHGetInteractionStatus;
                break;
            case C_GET_ITEM_STATUS:
                pAction = new MHGetItemStatus;
                break;
            case C_GET_LABEL:
                pAction = new MHUnimplementedAction(pElemAction->GetTagNo());
                break;// PushButton
            case C_GET_LAST_ANCHOR_FIRED:
                pAction = new MHUnimplementedAction(pElemAction->GetTagNo());
                break;// HyperText
            case C_GET_LINE_COLOUR:
                pAction = new MHGetLineColour;
                break;
            case C_GET_LINE_STYLE:
                pAction = new MHGetLineStyle;
                break;
            case C_GET_LINE_WIDTH:
                pAction = new MHGetLineWidth;
                break;
            case C_GET_LIST_ITEM:
                pAction = new MHGetListItem;
                break;
            case C_GET_LIST_SIZE:
                pAction = new MHGetListSize;
                break;
            case C_GET_OVERWRITE_MODE:
                pAction = new MHUnimplementedAction(pElemAction->GetTagNo());
                break;// ?
            case C_GET_PORTION:
                pAction = new MHGetPortion;
                break;
            case C_GET_POSITION:
                pAction = new MHGetPosition;
                break;
            case C_GET_RUNNING_STATUS:
                pAction = new MHGetRunningStatus;
                break;
            case C_GET_SELECTION_STATUS:
                pAction = new MHUnimplementedAction(pElemAction->GetTagNo());
                break;// ?
            case C_GET_SLIDER_VALUE:
                pAction = new MHGetSliderValue;
                break;
            case C_GET_TEXT_CONTENT:
                pAction = new MHUnimplementedAction(pElemAction->GetTagNo());
                break;// Text
            case C_GET_TEXT_DATA:
                pAction = new MHGetTextData;
                break;
            case C_GET_TOKEN_POSITION:
                pAction = new MHGetTokenPosition;
                break;
            case C_GET_VOLUME:
                pAction = new MHUnimplementedAction(pElemAction->GetTagNo());
                break; // ?
            case C_LAUNCH:
                pAction = new MHLaunch;
                break;
            case C_LOCK_SCREEN:
                pAction = new MHLockScreen;
                break;
            case C_MODULO:
                pAction = new MHModulo;
                break;
            case C_MOVE:
                pAction = new MHMove;
                break;
            case C_MOVE_TO:
                pAction = new MHMoveTo;
                break;
            case C_MULTIPLY:
                pAction = new MHMultiply;
                break;
            case C_OPEN_CONNECTION:
                pAction = new MHUnimplementedAction(pElemAction->GetTagNo());
                break; // ?
            case C_PRELOAD:
                pAction = new MHPreload;
                break;
            case C_PUT_BEFORE:
                pAction = new MHPutBefore;
                break;
            case C_PUT_BEHIND:
                pAction = new MHPutBehind;
                break;
            case C_QUIT:
                pAction = new MHQuit;
                break;
            case C_READ_PERSISTENT:
                pAction = new MHPersistent(":ReadPersistent", true);
                break;
            case C_RUN:
                pAction = new MHRun;
                break;
            case C_SCALE_BITMAP:
                pAction = new MHScaleBitmap;
                break;
            case C_SCALE_VIDEO:
                pAction = new MHScaleVideo;
                break;
            case C_SCROLL_ITEMS:
                pAction = new MHScrollItems;
                break;
            case C_SELECT:
                pAction = new MHUnimplementedAction(pElemAction->GetTagNo());
                break; // Button
            case C_SELECT_ITEM:
                pAction = new MHSelectItem;
                break;
            case C_SEND_EVENT:
                pAction = new MHSendEvent;
                break;
            case C_SEND_TO_BACK:
                pAction = new MHSendToBack;
                break;
            case C_SET_BOX_SIZE:
                pAction = new MHSetBoxSize;
                break;
            case C_SET_CACHE_PRIORITY:
                pAction = new MHUnimplementedAction(pElemAction->GetTagNo());
                break; // ?
            case C_SET_COUNTER_END_POSITION:
                pAction = new MHUnimplementedAction(pElemAction->GetTagNo());
                break; // Stream
            case C_SET_COUNTER_POSITION:
                pAction = new MHSetCounterPosition;
                break; // Stream
            case C_SET_COUNTER_TRIGGER:
                pAction = new MHUnimplementedAction(pElemAction->GetTagNo());
                break; // Stream
            case C_SET_CURSOR_POSITION:
                pAction = new MHUnimplementedAction(pElemAction->GetTagNo());
                break; // ?
            case C_SET_CURSOR_SHAPE:
                pAction = new MHUnimplementedAction(pElemAction->GetTagNo());
                break; // ?
            case C_SET_DATA:
                pAction = new MHSetData;
                break;
            case C_SET_ENTRY_POINT:
                pAction = new MHUnimplementedAction(pElemAction->GetTagNo());
                break; // EntryField
            case C_SET_FILL_COLOUR:
                pAction = new MHSetFillColour;
                break;
            case C_SET_FIRST_ITEM:
                pAction = new MHSetFirstItem;
                break;
            case C_SET_FONT_REF:
                pAction = new MHUnimplementedAction(pElemAction->GetTagNo());
                break; // Text
            case C_SET_HIGHLIGHT_STATUS:
                pAction = new MHSetHighlightStatus;
                break;
            case C_SET_INTERACTION_STATUS:
                pAction = new MHSetInteractionStatus;
                break;
            case C_SET_LABEL:
                pAction = new MHUnimplementedAction(pElemAction->GetTagNo());
                break; // PushButton
            case C_SET_LINE_COLOUR:
                pAction = new MHSetLineColour;
                break;
            case C_SET_LINE_STYLE:
                pAction = new MHSetLineStyle;
                break;
            case C_SET_LINE_WIDTH:
                pAction = new MHSetLineWidth;
                break;
            case C_SET_OVERWRITE_MODE:
                pAction = new MHUnimplementedAction(pElemAction->GetTagNo());
                break; // EntryField
            case C_SET_PALETTE_REF:
                pAction = new MHUnimplementedAction(pElemAction->GetTagNo());
                break; // Visible
            case C_SET_PORTION:
                pAction = new MHSetPortion;
                break;
            case C_SET_POSITION:
                pAction = new MHSetPosition;
                break;
            case C_SET_SLIDER_VALUE:
                pAction = new MHSetSliderValue;
                break;
            case C_SET_SPEED:
                pAction = new MHSetSpeed;
                break; // ?
            case C_SET_TIMER:
                pAction = new MHSetTimer;
                break;
            case C_SET_TRANSPARENCY:
                pAction = new MHSetTransparency;
                break;
            case C_SET_VARIABLE:
                pAction = new MHSetVariable;
                break;
            case C_SET_VOLUME:
                pAction = new MHUnimplementedAction(pElemAction->GetTagNo());
                break; // ?
            case C_SPAWN:
                pAction = new MHSpawn;
                break;
            case C_STEP:
                pAction = new MHStep;
                break;
            case C_STOP:
                pAction = new MHStop;
                break;
            case C_STORE_PERSISTENT:
                pAction = new MHPersistent(":StorePersistent", false);
                break;
            case C_SUBTRACT:
                pAction = new MHSubtract;
                break;
            case C_TEST_VARIABLE:
                pAction = new MHTestVariable;
                break;
            case C_TOGGLE:
                pAction = new MHUnimplementedAction(pElemAction->GetTagNo());
                break; // Button
            case C_TOGGLE_ITEM:
                pAction = new MHToggleItem;
                break;
            case C_TRANSITION_TO:
                pAction = new MHTransitionTo;
                break;
            case C_UNLOAD:
                pAction = new MHUnload;
                break;
            case C_UNLOCK_SCREEN:
                pAction = new MHUnlockScreen;
                break;
                // UK MHEG added actions.
            case C_SET_BACKGROUND_COLOUR:
                pAction = new MHSetBackgroundColour;
                break;
            case C_SET_CELL_POSITION:
                pAction = new MHUnimplementedAction(pElemAction->GetTagNo());
                break; // ?
            case C_SET_INPUT_REGISTER:
                pAction = new MHSetInputRegister;
                break;
            case C_SET_TEXT_COLOUR:
                pAction = new MHSetTextColour;
                break;
            case C_SET_FONT_ATTRIBUTES:
                pAction = new MHSetFontAttributes;
                break;
            case C_SET_VIDEO_DECODE_OFFSET:
                pAction = new MHSetVideoDecodeOffset;
                break;
            case C_GET_VIDEO_DECODE_OFFSET:
                pAction = new MHGetVideoDecodeOffset;
                break;
            case C_GET_FOCUS_POSITION:
                pAction = new MHUnimplementedAction(pElemAction->GetTagNo());
                break; // HyperText
            case C_SET_FOCUS_POSITION:
                pAction = new MHUnimplementedAction(pElemAction->GetTagNo());
                break; // HyperText
            case C_SET_BITMAP_DECODE_OFFSET:
                pAction = new MHSetBitmapDecodeOffset;
                break;
            case C_GET_BITMAP_DECODE_OFFSET:
                pAction = new MHGetBitmapDecodeOffset;
                break;
            case C_SET_SLIDER_PARAMETERS:
                pAction = new MHSetSliderParameters;
                break;

            // Added in ETSI ES 202 184 V2.1.1 (2010-01)
            case C_GET_COUNTER_POSITION: // Stream position
                pAction = new MHGetCounterPosition;
                break;
            case C_GET_COUNTER_MAX_POSITION: // Stream total size
                pAction = new MHGetCounterMaxPosition;
                break;

            default:
                MHLOG(MHLogWarning, QString("WARN Unknown action %1").arg(pElemAction->GetTagNo()));
                // Future proofing: ignore any actions that we don't know about.
                // Obviously these can only arise in the binary coding.
                pAction = NULL;
        }

        if (pAction)
        {
            Append(pAction); // Add to the sequence.
            pAction->Initialise(pElemAction, engine);
        }
    }
}

void MHActionSequence::PrintMe(FILE *fd, int nTabs) const
{
    for (int i = 0; i < Size(); i++)
    {
        GetAt(i)->PrintMe(fd, nTabs);
    }
}
