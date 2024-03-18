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
    MHVisible() = default;
    MHVisible(const MHVisible &ref);
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHIngredient
    void PrintMe(FILE *fd, int nTabs) const override; // MHIngredient

    void Preparation(MHEngine *engine) override; // MHIngredient
    void Destruction(MHEngine *engine) override; // MHIngredient
    void Activation(MHEngine *engine) override; // MHRoot
    void Deactivation(MHEngine *engine) override; // MHRoot

    // Actions.
    void SetPosition(int nXPosition, int nYPosition, MHEngine *engine) override; // MHRoot
    void GetPosition(MHRoot *pXPosN, MHRoot *pYPosN) override; // MHRoot
    void SetBoxSize(int nWidth, int nHeight, MHEngine *engine) override; // MHRoot
    void GetBoxSize(MHRoot *pWidthDest, MHRoot *pHeightDest) override; // MHRoot
    void SetPaletteRef(const MHObjectRef& newPalette, MHEngine *engine) override; // MHRoot
    void BringToFront(MHEngine *engine) override; // MHRoot
    void SendToBack(MHEngine *engine) override; // MHRoot
    void PutBefore(const MHRoot *pRef, MHEngine *engine) override; // MHRoot
    void PutBehind(const MHRoot *pRef, MHEngine *engine) override; // MHRoot

    // Display function.
    virtual void Display(MHEngine *) = 0;
    // Get the visible region of this visible.  This is the area that needs to be drawn.
    virtual QRegion GetVisibleArea();
    // Get the opaque area.  This is the area that this visible completely obscures and
    // is empty if the visible is drawn in a transparent or semi-transparent colour.
    virtual QRegion GetOpaqueArea() { return {}; }

    // Reset the position - used by ListGroup.
    void ResetPosition() override // MHRoot
        { m_nPosX = m_nOriginalPosX; m_nPosY = m_nOriginalPosY; }

  protected:
    // Exchange attributes
    int m_nOriginalBoxWidth  {-1};
    int m_nOriginalBoxHeight {-1};
    int m_nOriginalPosX      {0};
    int m_nOriginalPosY      {0};
    MHObjectRef m_originalPaletteRef; // Optional palette ref
    // Internal attributes
    int m_nBoxWidth          {0};
    int m_nBoxHeight         {0};
    int m_nPosX              {0};
    int m_nPosY              {0};
    MHObjectRef m_paletteRef;

    // Return the colour, looking up in the palette if necessary.
    static MHRgba GetColour(const MHColour &colour);
};

class MHLineArt : public MHVisible  
{
  public:
    MHLineArt() = default;
    MHLineArt(const MHLineArt &ref);
    const char *ClassName() override // MHRoot
        { return "LineArt"; }
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHVisible
    void PrintMe(FILE *fd, int nTabs) const override; // MHVisible
    // Only DynamicLineArt and Rectangle are supported
    void Display(MHEngine */*engine*/) override {} // MHVisible
    void Preparation(MHEngine *engine) override; // MHVisible

    // Actions on LineArt
    void SetFillColour(const MHColour &colour, MHEngine *engine) override; // MHRoot
    void SetLineColour(const MHColour &colour, MHEngine *engine) override; // MHRoot
    void SetLineWidth(int nWidth, MHEngine *engine) override; // MHRoot
    void SetLineStyle(int nStyle, MHEngine *engine) override; // MHRoot

  protected:
    // Exchanged attributes,
    bool    m_fBorderedBBox      {true}; // Does it have lines round or not?
    int     m_nOriginalLineWidth {1};
    enum : std::uint8_t { LineStyleSolid = 1, LineStyleDashed = 2, LineStyleDotted = 3 };
    int     m_originalLineStyle  {LineStyleSolid};
    MHColour    m_origLineColour, m_origFillColour;
    // Internal attributes
    int     m_nLineWidth         {0};
    int     m_lineStyle          {0};
    MHColour    m_lineColour, m_fillColour;
};

class MHRectangle : public MHLineArt  
{
  public:
    MHRectangle() = default;
    MHRectangle(const MHRectangle&) = default;
    const char *ClassName() override // MHLineArt
        { return "Rectangle"; }
    void PrintMe(FILE *fd, int nTabs) const override; // MHLineArt
    // Display function.
    void Display(MHEngine *engine) override; // MHLineArt
    QRegion GetOpaqueArea() override; // MHVisible
    MHIngredient *Clone(MHEngine */*engine*/) override // MHRoot
        { return new MHRectangle(*this); } // Create a clone of this ingredient.
};

// The Interactible class is described as a "mix-in" class.  It is used
// in various classes which complicates inheritance.
class MHInteractible
{
  public:
    explicit MHInteractible(MHVisible *parent)
        : m_parent(parent) {}
    virtual ~MHInteractible() = default;
    void Initialise(MHParseNode *p, MHEngine *engine);
    void PrintMe(FILE *fd, int nTabs) const;

    virtual void Interaction(MHEngine *engine);

    // This is called whenever a key is pressed while this
    // interactible is set to interactive.
    virtual void KeyEvent(MHEngine * /*engine*/, int /*nCode*/) {}
    virtual void InteractionCompleted(MHEngine * /*engine*/) {}

    void InteractSetInteractionStatus(bool newStatus, MHEngine *engine);
    bool InteractGetInteractionStatus(void) const { return m_fInteractionStatus; }
    void InteractSetHighlightStatus(bool newStatus, MHEngine *engine);
    bool InteractGetHighlightStatus(void) const { return m_fHighlightStatus; }
    // InteractDeactivation should be applied in every Deactivation action
    // of derived classes.
    void InteractDeactivation(void) { m_fInteractionStatus = false; }

  protected:
    // Exchanged attributes
    bool     m_fEngineResp         {true};
    MHColour m_highlightRefColour;
    // Internal attributes
    bool     m_fHighlightStatus    {false};
    bool     m_fInteractionStatus  {false};

  private:
    MHVisible *m_parent;
};

class MHSlider : public MHVisible, public MHInteractible
{
  public:
    MHSlider() : MHInteractible(this) {}
    ~MHSlider() override = default;
    const char *ClassName() override // MHRoot
        { return "Slider"; }
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHVisible
    void PrintMe(FILE *fd, int nTabs) const override; // MHVisible
    void Display(MHEngine *engine) override; // MHVisible
    void Preparation(MHEngine *engine) override; // MHVisible

    void Interaction(MHEngine *engine) override; // MHInteractible
    void InteractionCompleted(MHEngine *engine) override; // MHInteractible
    void KeyEvent(MHEngine *engine, int nCode) override; // MHInteractible

    // Implement the actions in the main inheritance line.
    void SetInteractionStatus(bool newStatus, MHEngine *engine) override // MHRoot
        { InteractSetInteractionStatus(newStatus, engine); }
    bool GetInteractionStatus(void) override // MHRoot
        { return InteractGetInteractionStatus(); }
    void SetHighlightStatus(bool newStatus, MHEngine *engine) override // MHRoot
        { InteractSetHighlightStatus(newStatus, engine); }
    bool GetHighlightStatus(void) override // MHRoot
        { return InteractGetHighlightStatus(); }
    void Deactivation(MHEngine */*engine*/) override // MHVisible
        { InteractDeactivation(); }

    // Actions
    void Step(int nbSteps, MHEngine *engine) override; // MHRoot
    void SetSliderValue(int newValue, MHEngine *engine) override; // MHRoot
    int GetSliderValue(void) override // MHRoot
        { return m_sliderValue; }
    void SetPortion(int newPortion, MHEngine *engine) override; // MHRoot
    int GetPortion(void) override // MHRoot
        { return m_portion; }
    // Additional action defined in UK MHEG.
    void SetSliderParameters(int newMin, int newMax, int newStep, MHEngine *engine) override; // MHRoot

    // Enumerated type lookup functions for the text parser.
    static int GetOrientation(const QString& str);
    static int GetStyle(const QString& str);
  protected:
    void Increment(MHEngine *engine);
    void Decrement(MHEngine *engine);

    // Exchanged attributes
    // Orientation and direction of increasing value.
    enum SliderOrientation : std::uint8_t { SliderLeft = 1, SliderRight, SliderUp, SliderDown }
        m_orientation     {SliderLeft};
    int m_initialValue    {1};
    int m_initialPortion  {0};
    int m_origMaxValue    {-1};
    int m_origMinValue    {1};
    int m_origStepSize    {1};
    // Style of slider.  Normal represents a mark on a scale,
    // Thermometer a range from the start up to the mark and Proportional
    // a range from the slider to the portion.
    enum SliderStyle : std::uint8_t { SliderNormal = 1, SliderThermo, SliderProp }
        m_style           {SliderNormal};
    MHColour m_sliderRefColour;
    // Internal attributes
    // In UK MHEG min_value, max_value and step_size can be changed.
    int m_maxValue        {0};
    int m_minValue        {0};
    int m_stepSize        {0};
    int m_sliderValue     {0};
    int m_portion         {0};
};

class MHEntryField : public MHVisible, public MHInteractible
{
  public:
    MHEntryField(): MHInteractible(this) {}
    ~MHEntryField() override = default;
    const char *ClassName() override // MHRoot
        { return "EntryField"; }
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHVisible
    void PrintMe(FILE *fd, int nTabs) const override; // MHVisible
    void Display(MHEngine */*engine*/) override {} // MHVisible - Not (yet?) supported

    // Implement the actions in the main inheritance line.
    void SetInteractionStatus(bool newStatus, MHEngine *engine) override // MHRoot
    { InteractSetInteractionStatus(newStatus, engine); }
    bool GetInteractionStatus(void) override // MHRoot
        { return InteractGetInteractionStatus(); }
    void SetHighlightStatus(bool newStatus, MHEngine *engine) override // MHRoot
    { InteractSetHighlightStatus(newStatus, engine); }
    bool GetHighlightStatus(void) override // MHRoot
        { return InteractGetHighlightStatus(); }
    void Deactivation(MHEngine */*engine*/) override // MHVisible
        { InteractDeactivation(); }
};

// Button - not needed for UK MHEG.
class MHButton : public MHVisible  
{
  public:
    MHButton() = default;
    ~MHButton() override = default;
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHVisible
    void PrintMe(FILE *fd, int nTabs) const override; // MHVisible
    void Display(MHEngine */*engine*/) override {} // MHVisible - Not (yet?) supported
};

// HotSpot - not needed for UK MHEG.
class MHHotSpot : public MHButton  
{
  public:
    MHHotSpot() = default;
    ~MHHotSpot() override = default;
    const char *ClassName() override // MHRoot
        { return "HotSpot"; }
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHButton
    void PrintMe(FILE *fd, int nTabs) const override; // MHButton
    void Display(MHEngine */*engine*/) override {} // MHButton - Not supported
};

// PushButton - not needed for UK MHEG.
class MHPushButton : public MHButton  
{
  public:
    MHPushButton() = default;
    ~MHPushButton() override = default;
    const char *ClassName() override // MHRoot
        { return "PushButton"; }
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHButton
    void PrintMe(FILE *fd, int nTabs) const override; // MHButton
    void Display(MHEngine */*engine*/) override {} // MHButton - Not supported
};

// SwitchButton - not needed for UK MHEG.
class MHSwitchButton : public MHPushButton  
{
  public:
    MHSwitchButton() = default;
    ~MHSwitchButton() override = default;
    const char *ClassName() override // MHPushButton
        { return "SwitchButton"; }
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHPushButton
    void PrintMe(FILE *fd, int nTabs) const override; // MHPushButton
    void Display(MHEngine */*engine*/) override {} // MHPushButton - Not supported
};

// Actions

// Set Line or Fill colour
class MHSetColour: public MHElemAction
{
  public:
    explicit MHSetColour(const char *name): MHElemAction(name) { }
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHElemAction
    void Perform(MHEngine *engine) override; // MHElemAction
  protected:
    void PrintArgs(FILE *fd, int nTabs) const override; // MHElemAction
    virtual void SetColour(const MHColour &colour, MHEngine *engine) = 0;
    // The colour can be specified as either an index or an absolute colour.
    // It's optional for the fill colour.
    enum : std::uint8_t { CT_None, CT_Indexed, CT_Absolute } m_colourType { CT_None };
    MHGenericInteger m_indexed;
    MHGenericOctetString m_absolute;
};

class MHSetLineColour: public MHSetColour {
  public:
    MHSetLineColour(): MHSetColour(":SetLineColour") { }
  protected:
    void SetColour(const MHColour &colour, MHEngine *engine) override // MHSetColour
        { Target(engine)->SetLineColour(colour, engine); }
};

class MHSetFillColour: public MHSetColour {
  public:
    MHSetFillColour(): MHSetColour(":SetFillColour") { }
  protected:
    void SetColour(const MHColour &colour, MHEngine *engine) override // MHSetColour
        { Target(engine)->SetFillColour(colour, engine); }
};

// BringToFront - bring a visible to the front of the display stack.
class MHBringToFront: public MHElemAction
{
  public:
    MHBringToFront(): MHElemAction(":BringToFront") {}
    void Perform(MHEngine *engine) override // MHElemAction
        { Target(engine)->BringToFront(engine); }
};

// PutBefore - put a visible in front of another.
class MHPutBefore: public MHActionGenericObjectRef
{
  public:
    MHPutBefore(): MHActionGenericObjectRef(":PutBefore") {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, MHRoot *pRef) override // MHActionGenericObjectRef
        { pTarget->PutBefore(pRef, engine); }
};

// PutBehind - put a visible behind another.
class MHPutBehind: public MHActionGenericObjectRef
{
  public:
    MHPutBehind(): MHActionGenericObjectRef(":PutBehind") {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, MHRoot *pRef) override // MHActionGenericObjectRef
        { pTarget->PutBehind(pRef, engine); }
};

// SendToBack - put a visible at the bottom of the display stack.
class MHSendToBack: public MHElemAction
{
  public:
    MHSendToBack(): MHElemAction(":SendToBack") {}
    void Perform(MHEngine *engine) override // MHElemAction
        { Target(engine)->SendToBack(engine); }
};

class MHSetPosition: public MHActionIntInt
{
  public:
    MHSetPosition(): MHActionIntInt(":SetPosition") {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg1, int nArg2) override // MHActionIntInt
        { pTarget->SetPosition(nArg1, nArg2, engine); };
};

class MHSetBoxSize: public MHActionIntInt
{
  public:
    MHSetBoxSize(): MHActionIntInt(":SetBoxSize") {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg1, int nArg2) override // MHActionIntInt
        { pTarget->SetBoxSize(nArg1, nArg2, engine); }
};

// Get box size of a visible
class MHGetBoxSize: public MHActionObjectRef2
{
  public:
    MHGetBoxSize(): MHActionObjectRef2(":GetBoxSize")  {}
    void CallAction(MHEngine */*engine*/, MHRoot *pTarget, MHRoot *pArg1, MHRoot *pArg2) override // MHActionObjectRef2
        { pTarget->GetBoxSize(pArg1, pArg2); }
};

// GetPosition of a visible
class MHGetPosition: public MHActionObjectRef2
{
  public:
    MHGetPosition(): MHActionObjectRef2(":GetPosition")  {}
    void CallAction(MHEngine */*engine*/, MHRoot *pTarget, MHRoot *pArg1, MHRoot *pArg2) override // MHActionObjectRef2
        { pTarget->GetPosition(pArg1, pArg2); }
};

class MHSetLineWidth: public MHActionInt
{
  public:
    MHSetLineWidth(): MHActionInt(":SetLineWidth") {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg) override // MHActionInt
        { pTarget->SetLineWidth(nArg, engine); };
};

class MHSetLineStyle: public MHActionInt
{
  public:
    MHSetLineStyle(): MHActionInt(":SetLineStyle") {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg) override // MHActionInt
        { pTarget->SetLineStyle(nArg, engine); };
};

class MHSetInteractionStatus: public MHActionBool
{
  public:
    MHSetInteractionStatus(): MHActionBool("SetInteractionStatus") {}
    void CallAction(MHEngine *engine, MHRoot */*pTarget*/, bool newStatus) override // MHActionBool
        { Target(engine)->SetInteractionStatus(newStatus, engine); }
};

class MHGetInteractionStatus: public MHActionObjectRef
{
  public:
    MHGetInteractionStatus(): MHActionObjectRef(":GetInteractionStatus")  {}
    void CallAction(MHEngine */*engine*/, MHRoot *pTarget, MHRoot *pResult) override // MHActionObjectRef
        { pResult->SetVariableValue(pTarget->GetInteractionStatus());}
};

class MHSetHighlightStatus: public MHActionBool
{
  public:
    MHSetHighlightStatus(): MHActionBool("SetHighlightStatus") {}
    void CallAction(MHEngine *engine, MHRoot */*pTarget*/, bool newStatus) override // MHActionBool
        { Target(engine)->SetHighlightStatus(newStatus, engine); }
};

class MHGetHighlightStatus: public MHActionObjectRef
{
  public:
    MHGetHighlightStatus(): MHActionObjectRef(":GetHighlightStatus")  {}
    void CallAction(MHEngine */*engine*/, MHRoot *pTarget, MHRoot *pResult) override // MHActionObjectRef
        { pResult->SetVariableValue(pTarget->GetHighlightStatus());}
};

class MHStep: public MHActionInt
{
  public:
    MHStep(): MHActionInt(":Step") {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg) override // MHActionInt
        { pTarget->Step(nArg, engine); };
};

class MHSetSliderValue: public MHActionInt
{
  public:
    MHSetSliderValue(): MHActionInt(":SetSliderValue") {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg) override // MHActionInt
        { pTarget->SetSliderValue(nArg, engine); };
};

class MHGetSliderValue: public MHActionObjectRef
{
  public:
    MHGetSliderValue(): MHActionObjectRef(":GetSliderValue")  {}
    void CallAction(MHEngine */*engine*/, MHRoot *pTarget, MHRoot *pResult) override // MHActionObjectRef
        { pResult->SetVariableValue(pTarget->GetSliderValue());}
};

class MHSetPortion: public MHActionInt
{
  public:
    MHSetPortion(): MHActionInt(":SetPortion") {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg) override // MHActionInt
        { pTarget->SetPortion(nArg, engine); };
};

class MHGetPortion: public MHActionObjectRef
{
  public:
    MHGetPortion(): MHActionObjectRef(":GetPortion")  {}
    void CallAction(MHEngine */*engine*/, MHRoot *pTarget, MHRoot *pResult) override // MHActionObjectRef
        { pResult->SetVariableValue(pTarget->GetPortion());}
};

class MHSetSliderParameters: public MHActionInt3
{
  public:
    MHSetSliderParameters(): MHActionInt3(":SetSliderParameters") {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, int newMin, int newMax, int newStep) override // MHActionInt3
        { pTarget->SetSliderParameters(newMin, newMax, newStep, engine); };
};

#endif
