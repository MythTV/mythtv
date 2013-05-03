/* Root.h

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


#if !defined(ROOTCLASS_H)
#define ROOTCLASS_H

#include "BaseClasses.h"
#include "BaseActions.h"
class MHParseNode;
class MHLink;
class MHIngredient;
class MHEngine;

enum EventType { EventIsAvailable = 1, EventContentAvailable, EventIsDeleted, EventIsRunning, EventIsStopped,
       EventUserInput, EventAnchorFired, EventTimerFired, EventAsyncStopped, EventInteractionCompleted,
       EventTokenMovedFrom, EventTokenMovedTo, EventStreamEvent, EventStreamPlaying, EventStreamStopped,
       EventCounterTrigger, EventHighlightOn, EventHighlightOff, EventCursorEnter, EventCursorLeave,
       EventIsSelected, EventIsDeselected, EventTestEvent, EventFirstItemPresented, EventLastItemPresented,
       EventHeadItems, EventTailItems, EventItemSelected, EventItemDeselected, EventEntryFieldFull,
       EventEngineEvent,
       // The next two events are added in UK MHEG.
       EventFocusMoved, EventSliderValueChanged };

class MHRoot  
{
  public:
    MHRoot(): m_fAvailable(false), m_fRunning(false) {}
    MHRoot(const MHRoot &/*ref*/): m_fAvailable(false), m_fRunning(false) {}
    virtual ~MHRoot() {}
    // Initialise - set up the item from the parse tree.
    virtual void Initialise(MHParseNode *p, MHEngine *engine); // Set this up from the parse tree.
    // Print this item.
    virtual void PrintMe(FILE *fd, int nTabs) const;
    // Test for shared status.
    virtual bool IsShared() { return false; }

    // MHEG Actions.
    // Preparation - sets up the run-time representation.  Sets m_fAvailable and generates IsAvailable event.
    virtual void Preparation(MHEngine *engine);
    // Activation - starts running the object.  Sets m_fRunning and generates IsRunning event.
    virtual void Activation(MHEngine *engine);
    // Deactivation - stops running the object.  Clears m_fRunning
    virtual void Deactivation(MHEngine *engine);
    // Destruction - deletes the run-time representation.  Clears m_fAvailable.
    virtual void Destruction(MHEngine *engine);
    // Prepare the content - This action is added in the corrigendum to the standard.
    virtual void ContentPreparation(MHEngine *) {}

    // Return an object with a given object number.  In the root class this returns this object
    // if it matches.  Group and Stream classes also search their components.
    virtual MHRoot *FindByObjectNo(int n);

    // Actions.  The default behaviour if a sub-class doesn't override them is to fail.

    // Actions on Root class
    virtual bool GetAvailabilityStatus() { return m_fAvailable; }
    virtual bool GetRunningStatus() { return m_fRunning; }

    // Actions on Groups
    virtual void SetTimer(int/*nTimerId*/, bool/*fAbsolute*/, int /*nMilliSecs*/, MHEngine *) { InvalidAction("SetTimer"); }
    // This isn't an MHEG action as such but is used as part of the implementation of "Clone"
    virtual void MakeClone(MHRoot* /*pTarget*/, MHRoot* /*pRef*/, MHEngine *) { InvalidAction("MakeClone"); }
    virtual void SetInputRegister(int /*nReg*/, MHEngine *) { InvalidAction("SetInputRegister"); }

    // Actions on Ingredients.
    virtual void SetData(const MHOctetString &/*included*/, MHEngine *) { InvalidAction("SetData"); }
    virtual void SetData(const MHContentRef &/*referenced*/, bool /*fSizeGiven*/, int /*size*/, bool /*fCCGiven*/, int /*cc*/, MHEngine *)
         { InvalidAction("SetData"); }
    virtual void Preload(MHEngine *) { InvalidAction("Preload"); }
    virtual void Unload(MHEngine *) { InvalidAction("Unload"); }
    // The UK MHEG profile only requires cloning for Text, Bitmap and Rectangle.
    virtual MHIngredient *Clone(MHEngine *) { InvalidAction("Clone"); return NULL; }

    // Actions on Presentables.  The Run/Stop actions on presentables and the Activate/Deactivate actions
    // on Links have identical effects.  They could be merged.
    virtual void Run(MHEngine *) { InvalidAction("Run"); }
    virtual void Stop(MHEngine *) { InvalidAction("Stop"); }

    // Actions on variables.
    virtual void TestVariable(int /*nOp*/, const MHUnion &/*parm*/, MHEngine *)  { InvalidAction("TestVariable"); }
    virtual void GetVariableValue(MHUnion &, MHEngine *) { InvalidAction("GetVariableValue"); }
    virtual void SetVariableValue(const MHUnion &) { InvalidAction("SetVariableValue"); }

    // Actions on Text objects
    virtual void GetTextData(MHRoot * /*pDestination*/, MHEngine *) { InvalidAction("GetTextData"); }
    virtual void SetBackgroundColour(const MHColour &/*colour*/, MHEngine *) { InvalidAction("SetBackgroundColour"); }
    virtual void SetTextColour(const MHColour &/*colour*/, MHEngine *) { InvalidAction("SetTextColour"); }
    virtual void SetFontAttributes(const MHOctetString &/*fontAttrs*/, MHEngine *) { InvalidAction("SetFontAttributes"); }

    // Actions on Links
    virtual void Activate(bool /*f*/, MHEngine *) { InvalidAction("Activate/Deactivate"); } // Activate/Deactivate

    // Actions on Programs.
    virtual void CallProgram(bool /*fIsFork*/, const MHObjectRef &/*success*/,
        const MHSequence<MHParameter *> &/*args*/, MHEngine *) { InvalidAction("CallProgram"); }

    // Actions on TokenGroups
    virtual void CallActionSlot(int, MHEngine *) { InvalidAction("CallActionSlot"); }
    virtual void Move(int, MHEngine *) { InvalidAction("Move"); }
    virtual void MoveTo(int, MHEngine *) { InvalidAction("MoveTo"); }
    virtual void GetTokenPosition(MHRoot * /*pResult*/, MHEngine *) { InvalidAction("GetTokenPosition"); }

    // Actions on ListGroups
    virtual void AddItem(int /*nIndex*/, MHRoot * /*pItem*/, MHEngine *) { InvalidAction("GetCellItem"); }
    virtual void DelItem(MHRoot * /*pItem*/, MHEngine *) { InvalidAction("GetCellItem"); }
    virtual void GetCellItem(int /*nCell*/, const MHObjectRef &/*itemDest*/, MHEngine *) { InvalidAction("GetCellItem"); }
    virtual void GetListItem(int /*nCell*/, const MHObjectRef &/*itemDest*/, MHEngine *) { InvalidAction("GetCellItem"); }
    virtual void GetItemStatus(int /*nCell*/, const MHObjectRef &/*itemDest*/, MHEngine *) { InvalidAction("GetItemStatus"); }
    virtual void SelectItem(int /*nCell*/, MHEngine *) { InvalidAction("SelectItem"); }
    virtual void DeselectItem(int /*nCell*/, MHEngine *) { InvalidAction("DeselectItem"); }
    virtual void ToggleItem(int /*nCell*/, MHEngine *) { InvalidAction("ToggleItem"); }
    virtual void ScrollItems(int /*nCell*/, MHEngine *) { InvalidAction("ScrollItems"); }
    virtual void SetFirstItem(int /*nCell*/, MHEngine *) { InvalidAction("SetFirstItem"); }
    virtual void GetFirstItem(MHRoot * /*pResult*/, MHEngine *) { InvalidAction("GetFirstItem"); }
    virtual void GetListSize(MHRoot * /*pResult*/, MHEngine *) { InvalidAction("GetListSize"); }

    // Actions on Visibles.
    virtual void SetPosition(int /*nXPosition*/, int /*nYPosition*/, MHEngine *) { InvalidAction("SetPosition"); }
    virtual void GetPosition(MHRoot * /*pXPosN*/, MHRoot * /*pYPosN*/) { InvalidAction("GetPosition"); }
    virtual void SetBoxSize(int /*nWidth*/, int /*nHeight*/, MHEngine *) { InvalidAction("SetBoxSize"); }
    virtual void GetBoxSize(MHRoot * /*pWidthDest*/, MHRoot * /*HeightDest*/) { InvalidAction("GetBoxSize"); }
    virtual void SetPaletteRef(const MHObjectRef /*newPalette*/, MHEngine *) { InvalidAction("SetPaletteRef"); }
    virtual void BringToFront(MHEngine *) { InvalidAction("BringToFront"); }
    virtual void SendToBack(MHEngine *) { InvalidAction("SendToBack"); }
    virtual void PutBefore(const MHRoot * /*pRef*/, MHEngine *) { InvalidAction("PutBefore"); }
    virtual void PutBehind(const MHRoot * /*pRef*/, MHEngine *) { InvalidAction("PutBehind"); }
    virtual void ResetPosition()  { InvalidAction("ResetPosition"); } // Used internally by ListGroup

    // Actions on LineArt
    virtual void SetFillColour(const MHColour &/*colour*/, MHEngine *) { InvalidAction("SetFillColour"); }
    virtual void SetLineColour(const MHColour &/*colour*/, MHEngine *) { InvalidAction("SetLineColour"); }
    virtual void SetLineWidth(int /*nWidth*/, MHEngine *) { InvalidAction("SetLineWidth"); }
    virtual void SetLineStyle(int /*nStyle*/, MHEngine *) { InvalidAction("SetLineStyle"); }

    // Actions on Bitmaps
    virtual void SetTransparency(int /*nTransPerCent*/, MHEngine *) { InvalidAction("SetTransparency"); }
    virtual void ScaleBitmap(int /*xScale*/, int /*yScale*/, MHEngine *) { InvalidAction("ScaleBitmap"); } 
    virtual void SetBitmapDecodeOffset(int /*newXOffset*/, int /*newYOffset*/, MHEngine *) { InvalidAction("SetBitmapDecodeOffset"); }
    virtual void GetBitmapDecodeOffset(MHRoot * /*pXOffset*/, MHRoot * /*pYOffset*/) { InvalidAction("GetBitmapDecodeOffset"); }

    // Actions on Dynamic Line Art
    virtual void Clear() { InvalidAction(""); }
    virtual void GetLineWidth(MHRoot * /*pResult*/) { InvalidAction("GetLineWidth"); }
    virtual void GetLineStyle(MHRoot * /*pResult*/) { InvalidAction("GetLineStyle"); }
    virtual void GetLineColour(MHRoot * /*pResult*/) { InvalidAction("GetLineColour"); }
    virtual void GetFillColour(MHRoot * /*pResult*/) { InvalidAction("GetFillColour"); }
    virtual void DrawArcSector(bool /*fIsSector*/, int /*x*/, int /*y*/, int /*width*/, int /*height*/, int /*start*/,
        int /*arc*/, MHEngine *) { InvalidAction("DrawArc/Sector"); }
    virtual void DrawLine(int /*x1*/, int /*y1*/, int /*x2*/, int /*y2*/, MHEngine *) { InvalidAction("DrawLine"); }
    virtual void DrawOval(int /*x1*/, int /*y1*/, int /*width*/, int /*height*/, MHEngine *) { InvalidAction("DrawOval"); }
    virtual void DrawRectangle(int /*x1*/, int /*y1*/, int /*x2*/, int /*y2*/, MHEngine *) { InvalidAction("DrawRectangle"); }
    virtual void DrawPoly(bool /*fIsPolygon*/, int /*nPoints*/, const int * /*xArray*/, const int * /*yArray*/, MHEngine *)
         { InvalidAction("DrawPoly(gon/line)"); }

    // Actions on Video streams.
    virtual void ScaleVideo(int /*xScale*/, int /*yScale*/, MHEngine *) { InvalidAction("ScaleVideo"); }
    virtual void SetVideoDecodeOffset(int /*newXOffset*/, int /*newYOffset*/, MHEngine *) { InvalidAction("SetVideoDecodeOffset"); }
    virtual void GetVideoDecodeOffset(MHRoot * /*pXOffset*/, MHRoot * /*pYOffset*/, MHEngine *) { InvalidAction("GetVideoDecodeOffset"); }
    virtual void GetCounterPosition(MHRoot * /*pPos*/, MHEngine *) { InvalidAction("GetCounterPosition"); }
    virtual void GetCounterMaxPosition(MHRoot * /*pPos*/, MHEngine *) { InvalidAction("GetCounterMaxPosition"); }
    virtual void SetCounterPosition(int /*pos*/, MHEngine *) { InvalidAction("SetCounterPosition"); }
    virtual void SetSpeed(int /*speed 0=stop*/, MHEngine *) { InvalidAction("SetSpeed"); }

    // Actions on Interactibles.
    virtual void SetInteractionStatus(bool /*newStatus*/, MHEngine *) { InvalidAction("SetInteractionStatus"); }
    virtual bool GetInteractionStatus(void) { InvalidAction("GetInteractionStatus"); return false; }
    virtual void SetHighlightStatus(bool /*newStatus*/, MHEngine *engine) { InvalidAction("SetHighlightStatus"); }
    virtual bool GetHighlightStatus(void) { InvalidAction("GetHighlightStatus"); return false; }

    // Actions on Sliders.
    virtual void Step(int /*nbSteps*/, MHEngine * /*engine*/) { InvalidAction("Step"); }
    virtual void SetSliderValue(int /*nbSteps*/, MHEngine * /*engine*/) { InvalidAction("SetSliderValue"); }
    virtual int GetSliderValue(void) { InvalidAction("GetSliderValue"); return 0; }
    virtual void SetPortion(int /*newPortion*/, MHEngine * /*engine*/) { InvalidAction("SetPortion"); }
    virtual int GetPortion(void) { InvalidAction("GetPortion"); return 0; }
    // Additional action defined in UK MHEG.
    virtual void SetSliderParameters(int /*newMin*/, int /*newMax*/, int /*newStep*/, MHEngine * /*engine*/)
         { InvalidAction("SetSliderParameters"); }

  protected:

    void InvalidAction(const char *actionName);
  public: 
    MHObjectRef m_ObjectReference; // Identifier of this object.

    virtual const char *ClassName() = 0; // For debugging messages.
  protected:
    bool m_fAvailable; // Set once Preparation has completed.
    bool m_fRunning; // Set once Activation has completed.

    friend class MHEngine;
};

// Get Availability Status - Does the object exist and is it available?.
class MHGetAvailabilityStatus: public MHElemAction
{
  public:
    MHGetAvailabilityStatus(): MHElemAction(":GetAvailabilityStatus")  {}
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void Perform(MHEngine *engine);
  protected:
    virtual void PrintArgs(FILE *fd, int /*nTabs*/) const { m_ResultVar.PrintMe(fd, 0); }
    MHObjectRef m_ResultVar;
};

// Get Running Status - Is the object running?.
class MHGetRunningStatus: public MHActionObjectRef
{
  public:
    MHGetRunningStatus(): MHActionObjectRef(":GetRunningStatus")  {}
    virtual void CallAction(MHEngine *, MHRoot *pTarget, MHRoot *pResult) { pResult->SetVariableValue(pTarget->GetRunningStatus());}
};

#endif
