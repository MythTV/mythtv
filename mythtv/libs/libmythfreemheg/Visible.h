/* Visible.h

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

#if !defined(VISIBLE_H)
#define VISIBLE_H
#include <qregion.h>

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

class MHInteractible
{
public:
    MHInteractible();
    virtual ~MHInteractible();
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;
};

class MHSlider : public MHVisible, public MHInteractible
{
public:
    MHSlider();
    virtual ~MHSlider();
    virtual const char *ClassName() { return "Slider"; }
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;
    virtual void Display(MHEngine *) {} // Not (yet?) supported
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

#endif
