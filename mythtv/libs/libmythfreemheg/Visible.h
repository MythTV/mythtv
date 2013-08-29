/* Visible.h

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

#if !defined(VISIBLE_H)
#define VISIBLE_H
#include <QRegion>

#include "Presentable.h"
// Dependencies
#include "Ingredients.h"
#include "Root.h"
#include "BaseClasses.h"
#include "freemheg.h"

class MHEngine;

class MHVisible : public MHPresentable  
{
  public:
    MHVisible();
    MHVisible(const MHVisible &ref);
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;

    virtual void Preparation(MHEngine *engine);
    virtual void Destruction(MHEngine *engine);
    virtual void Activation(MHEngine *engine);
    virtual void Deactivation(MHEngine *engine);

    // Actions.
    virtual void SetPosition(int nXPosition, int nYPosition, MHEngine *engine);
    virtual void GetPosition(MHRoot *pXPosN, MHRoot *pYPosN);
    virtual void SetBoxSize(int nWidth, int nHeight, MHEngine *engine);
    virtual void GetBoxSize(MHRoot *pWidthDest, MHRoot *pHeightDest);
    virtual void SetPaletteRef(const MHObjectRef newPalette, MHEngine *engine);
    virtual void BringToFront(MHEngine *engine);
    virtual void SendToBack(MHEngine *engine);
    virtual void PutBefore(const MHRoot *pRef, MHEngine *engine);
    virtual void PutBehind(const MHRoot *pRef, MHEngine *engine);

    // Display function.
    virtual void Display(MHEngine *) = 0;
    // Get the visible region of this visible.  This is the area that needs to be drawn.
    virtual QRegion GetVisibleArea();
    // Get the opaque area.  This is the area that this visible completely obscures and
    // is empty if the visible is drawn in a transparent or semi-transparent colour.
    virtual QRegion GetOpaqueArea() { return QRegion(); }

    // Reset the position - used by ListGroup.
    virtual void ResetPosition() { m_nPosX = m_nOriginalPosX; m_nPosY = m_nOriginalPosY; }

  protected:
    // Exchange attributes
    int m_nOriginalBoxWidth, m_nOriginalBoxHeight;
    int m_nOriginalPosX, m_nOriginalPosY;
    MHObjectRef m_OriginalPaletteRef; // Optional palette ref
    // Internal attributes
    int m_nBoxWidth, m_nBoxHeight;
    int m_nPosX, m_nPosY;
    MHObjectRef m_PaletteRef;

    // Return the colour, looking up in the palette if necessary.
    MHRgba GetColour(const MHColour &colour);
};

class MHLineArt : public MHVisible  
{
  public:
    MHLineArt();
    MHLineArt(const MHLineArt &ref);
    virtual const char *ClassName() { return "LineArt"; }
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;
    virtual void Display(MHEngine *) {} // Only DynamicLineArt and Rectangle are supported
    virtual void Preparation(MHEngine *engine);

    // Actions on LineArt
    virtual void SetFillColour(const MHColour &colour, MHEngine *engine);
    virtual void SetLineColour(const MHColour &colour, MHEngine *engine);
    virtual void SetLineWidth(int nWidth, MHEngine *engine);
    virtual void SetLineStyle(int nStyle, MHEngine *engine);

  protected:
    // Exchanged attributes,
    bool    m_fBorderedBBox; // Does it have lines round or not?
    int     m_nOriginalLineWidth;
    int     m_OriginalLineStyle;
    enum { LineStyleSolid = 1, LineStyleDashed = 2, LineStyleDotted = 3 };
    MHColour    m_OrigLineColour, m_OrigFillColour; 
    // Internal attributes
    int     m_nLineWidth;
    int     m_LineStyle;
    MHColour    m_LineColour, m_FillColour;
};

class MHRectangle : public MHLineArt  
{
  public:
    MHRectangle() {}
    MHRectangle(const MHRectangle &ref): MHLineArt(ref) {}
    virtual const char *ClassName() { return "Rectangle"; }
    virtual void PrintMe(FILE *fd, int nTabs) const;
    // Display function.
    virtual void Display(MHEngine *q);
    virtual QRegion GetOpaqueArea();
    virtual MHIngredient *Clone(MHEngine *) { return new MHRectangle(*this); } // Create a clone of this ingredient.
};

// The Interactible class is described as a "mix-in" class.  It is used
// in various classes which complicates inheritance.
class MHInteractible
{
  public:
    MHInteractible(MHVisible *parent);
    virtual ~MHInteractible();
    void Initialise(MHParseNode *p, MHEngine *engine);
    void PrintMe(FILE *fd, int nTabs) const;

    virtual void Interaction(MHEngine *engine);

    // This is called whenever a key is pressed while this
    // interactible is set to interactive.
    virtual void KeyEvent(MHEngine * /*engine*/, int /*nCode*/) {}
    virtual void InteractionCompleted(MHEngine * /*engine*/) {}

    void InteractSetInteractionStatus(bool newStatus, MHEngine *engine);
    bool InteractGetInteractionStatus(void) { return m_fInteractionStatus; }
    void InteractSetHighlightStatus(bool newStatus, MHEngine *engine);
    bool InteractGetHighlightStatus(void) { return m_fHighlightStatus; }
    // InteractDeactivation should be applied in every Deactivation action
    // of derived classes.
    void InteractDeactivation(void) { m_fInteractionStatus = false; }

  protected:
    // Exchanged attributes
    bool     m_fEngineResp;
    MHColour m_highlightRefColour;
    // Internal attributes
    bool     m_fHighlightStatus;
    bool     m_fInteractionStatus;

  private:
    MHVisible *m_parent;
};

class MHSlider : public MHVisible, public MHInteractible
{
  public:
    MHSlider();
    virtual ~MHSlider();
    virtual const char *ClassName() { return "Slider"; }
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;
    virtual void Display(MHEngine *);
    virtual void Preparation(MHEngine *engine);

    virtual void Interaction(MHEngine *engine);
    virtual void InteractionCompleted(MHEngine *engine);
    virtual void KeyEvent(MHEngine *engine, int nCode);

    // Implement the actions in the main inheritance line.
    virtual void SetInteractionStatus(bool newStatus, MHEngine *engine)
    { InteractSetInteractionStatus(newStatus, engine); }
    virtual bool GetInteractionStatus(void) { return InteractGetInteractionStatus(); }
    virtual void SetHighlightStatus(bool newStatus, MHEngine *engine)
    { InteractSetHighlightStatus(newStatus, engine); }
    virtual bool GetHighlightStatus(void) { return InteractGetHighlightStatus(); }
    virtual void Deactivation(MHEngine *engine) { InteractDeactivation(); }

    // Actions
    virtual void Step(int nbSteps, MHEngine *engine);
    virtual void SetSliderValue(int newValue, MHEngine *engine);
    virtual int GetSliderValue(void) { return slider_value; }
    virtual void SetPortion(int newPortion, MHEngine *engine);
    virtual int GetPortion(void) { return portion; }
    // Additional action defined in UK MHEG.
    virtual void SetSliderParameters(int newMin, int newMax, int newStep, MHEngine *engine);

    // Enumerated type lookup functions for the text parser.
    static int GetOrientation(const char *str);
    static int GetStyle(const char *str);
  protected:
    void Increment(MHEngine *engine);
    void Decrement(MHEngine *engine);

    // Exchanged attributes
    // Orientation and direction of increasing value.
    enum SliderOrientation { SliderLeft = 1, SliderRight, SliderUp, SliderDown }
        m_orientation;
    int initial_value, initial_portion;
    int orig_max_value, orig_min_value, orig_step_size;
    // Style of slider.  Normal represents a mark on a scale,
    // Thermometer a range from the start up to the mark and Proportional
    // a range from the slider to the portion.
    enum SliderStyle { SliderNormal = 1, SliderThermo, SliderProp }
        m_style;
    MHColour m_sliderRefColour;
    // Internal attributes
    // In UK MHEG min_value, max_value and step_size can be changed.
    int max_value, min_value, step_size;
    int slider_value, portion;
};

class MHEntryField : public MHVisible, public MHInteractible
{
  public:
    MHEntryField();
    virtual ~MHEntryField();
    virtual const char *ClassName() { return "EntryField"; }
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;
    virtual void Display(MHEngine *) {} // Not (yet?) supported

    // Implement the actions in the main inheritance line.
    virtual void SetInteractionStatus(bool newStatus, MHEngine *engine)
    { InteractSetInteractionStatus(newStatus, engine); }
    virtual bool GetInteractionStatus(void) { return InteractGetInteractionStatus(); }
    virtual void SetHighlightStatus(bool newStatus, MHEngine *engine)
    { InteractSetHighlightStatus(newStatus, engine); }
    virtual bool GetHighlightStatus(void) { return InteractGetHighlightStatus(); }
    virtual void Deactivation(MHEngine *engine) { InteractDeactivation(); }
};

// Button - not needed for UK MHEG.
class MHButton : public MHVisible  
{
  public:
    MHButton();
    virtual ~MHButton();
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;
    virtual void Display(MHEngine *) {} // Not (yet?) supported
};

// HotSpot - not needed for UK MHEG.
class MHHotSpot : public MHButton  
{
  public:
    MHHotSpot();
    virtual ~MHHotSpot();
    virtual const char *ClassName() { return "HotSpot"; }
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;
    virtual void Display(MHEngine *) {} // Not supported
};

// PushButton - not needed for UK MHEG.
class MHPushButton : public MHButton  
{
  public:
    MHPushButton();
    virtual ~MHPushButton();
    virtual const char *ClassName() { return "PushButton"; }
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;
    virtual void Display(MHEngine *) {} // Not supported
};

// SwitchButton - not needed for UK MHEG.
class MHSwitchButton : public MHPushButton  
{
  public:
    MHSwitchButton();
    virtual ~MHSwitchButton();
    virtual const char *ClassName() { return "SwitchButton"; }
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;
    virtual void Display(MHEngine *) {} // Not supported
};

// Actions

// Set Line or Fill colour
class MHSetColour: public MHElemAction
{
  public:
    MHSetColour(const char *name): MHElemAction(name), m_ColourType(CT_None) { }
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void Perform(MHEngine *engine);
  protected:
    virtual void PrintArgs(FILE *fd, int nTabs) const;
    virtual void SetColour(const MHColour &colour, MHEngine *engine) = 0;
    // The colour can be specified as either an index or an absolute colour.
    // It's optional for the fill colour.
    enum { CT_None, CT_Indexed, CT_Absolute } m_ColourType;
    MHGenericInteger m_Indexed;
    MHGenericOctetString m_Absolute;
};

class MHSetLineColour: public MHSetColour {
  public:
    MHSetLineColour(): MHSetColour(":SetLineColour") { }
  protected:
    virtual void SetColour(const MHColour &colour, MHEngine *engine) { Target(engine)->SetLineColour(colour, engine); }
};

class MHSetFillColour: public MHSetColour {
  public:
    MHSetFillColour(): MHSetColour(":SetFillColour") { }
  protected:
    virtual void SetColour(const MHColour &colour, MHEngine *engine) { Target(engine)->SetFillColour(colour, engine); }
};

// BringToFront - bring a visible to the front of the display stack.
class MHBringToFront: public MHElemAction
{
  public:
    MHBringToFront(): MHElemAction(":BringToFront") {}
    virtual void Perform(MHEngine *engine) { Target(engine)->BringToFront(engine); }
};

// PutBefore - put a visible in front of another.
class MHPutBefore: public MHActionGenericObjectRef
{
  public:
    MHPutBefore(): MHActionGenericObjectRef(":PutBefore") {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, MHRoot *pRef) { pTarget->PutBefore(pRef, engine); }
};

// PutBehind - put a visible behind another.
class MHPutBehind: public MHActionGenericObjectRef
{
  public:
    MHPutBehind(): MHActionGenericObjectRef(":PutBehind") {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, MHRoot *pRef) { pTarget->PutBehind(pRef, engine); }
};

// SendToBack - put a visible at the bottom of the display stack.
class MHSendToBack: public MHElemAction
{
  public:
    MHSendToBack(): MHElemAction(":SendToBack") {}
    virtual void Perform(MHEngine *engine) { Target(engine)->SendToBack(engine); }
};

class MHSetPosition: public MHActionIntInt
{
  public:
    MHSetPosition(): MHActionIntInt(":SetPosition") {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg1, int nArg2) { pTarget->SetPosition(nArg1, nArg2, engine); };
};

class MHSetBoxSize: public MHActionIntInt
{
  public:
    MHSetBoxSize(): MHActionIntInt(":SetBoxSize") {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg1, int nArg2) { pTarget->SetBoxSize(nArg1, nArg2, engine); }
};

// Get box size of a visible
class MHGetBoxSize: public MHActionObjectRef2
{
  public:
    MHGetBoxSize(): MHActionObjectRef2(":GetBoxSize")  {}
    virtual void CallAction(MHEngine *, MHRoot *pTarget, MHRoot *pArg1, MHRoot *pArg2) { pTarget->GetBoxSize(pArg1, pArg2); }
};

// GetPosition of a visible
class MHGetPosition: public MHActionObjectRef2
{
  public:
    MHGetPosition(): MHActionObjectRef2(":GetPosition")  {}
    virtual void CallAction(MHEngine *, MHRoot *pTarget, MHRoot *pArg1, MHRoot *pArg2) { pTarget->GetPosition(pArg1, pArg2); }
};

class MHSetLineWidth: public MHActionInt
{
  public:
    MHSetLineWidth(): MHActionInt(":SetLineWidth") {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg) { pTarget->SetLineWidth(nArg, engine); };
};

class MHSetLineStyle: public MHActionInt
{
  public:
    MHSetLineStyle(): MHActionInt(":SetLineStyle") {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg) { pTarget->SetLineStyle(nArg, engine); };
};

class MHSetInteractionStatus: public MHActionBool
{
  public:
    MHSetInteractionStatus(): MHActionBool("SetInteractionStatus") {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, bool newStatus)
    { Target(engine)->SetInteractionStatus(newStatus, engine); }
};

class MHGetInteractionStatus: public MHActionObjectRef
{
  public:
    MHGetInteractionStatus(): MHActionObjectRef(":GetInteractionStatus")  {}
    virtual void CallAction(MHEngine *, MHRoot *pTarget, MHRoot *pResult)
        { pResult->SetVariableValue(pTarget->GetInteractionStatus());}
};

class MHSetHighlightStatus: public MHActionBool
{
  public:
    MHSetHighlightStatus(): MHActionBool("SetHighlightStatus") {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, bool newStatus)
    { Target(engine)->SetHighlightStatus(newStatus, engine); }
};

class MHGetHighlightStatus: public MHActionObjectRef
{
  public:
    MHGetHighlightStatus(): MHActionObjectRef(":GetHighlightStatus")  {}
    virtual void CallAction(MHEngine *, MHRoot *pTarget, MHRoot *pResult)
        { pResult->SetVariableValue(pTarget->GetHighlightStatus());}
};

class MHStep: public MHActionInt
{
  public:
    MHStep(): MHActionInt(":Step") {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg) { pTarget->Step(nArg, engine); };
};

class MHSetSliderValue: public MHActionInt
{
  public:
    MHSetSliderValue(): MHActionInt(":SetSliderValue") {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg) { pTarget->SetSliderValue(nArg, engine); };
};

class MHGetSliderValue: public MHActionObjectRef
{
  public:
    MHGetSliderValue(): MHActionObjectRef(":GetSliderValue")  {}
    virtual void CallAction(MHEngine *, MHRoot *pTarget, MHRoot *pResult)
        { pResult->SetVariableValue(pTarget->GetSliderValue());}
};

class MHSetPortion: public MHActionInt
{
  public:
    MHSetPortion(): MHActionInt(":SetPortion") {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg) { pTarget->SetPortion(nArg, engine); };
};

class MHGetPortion: public MHActionObjectRef
{
  public:
    MHGetPortion(): MHActionObjectRef(":GetPortion")  {}
    virtual void CallAction(MHEngine *, MHRoot *pTarget, MHRoot *pResult)
        { pResult->SetVariableValue(pTarget->GetPortion());}
};

class MHSetSliderParameters: public MHActionInt3
{
  public:
    MHSetSliderParameters(): MHActionInt3(":SetSliderParameters") {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, int newMin, int newMax, int newStep)
        { pTarget->SetSliderParameters(newMin, newMax, newStep, engine); };
};

#endif
