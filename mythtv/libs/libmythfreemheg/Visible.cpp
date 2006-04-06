/* Visible.cpp

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

#include "Visible.h"
#include "Presentable.h"
#include "Ingredients.h"
#include "Root.h"
#include "BaseClasses.h"
#include "ParseNode.h"
#include "ASN1Codes.h"
#include "Engine.h"
#include "Logging.h"
#include "freemheg.h"


MHVisible::MHVisible()
{
    m_nOriginalBoxWidth = m_nOriginalBoxHeight = -1; // Should always be specified.
    m_nOriginalPosX = m_nOriginalPosY = 0; // Default values.
}

// Copy constructor for cloning
MHVisible::MHVisible(const MHVisible &ref): MHPresentable(ref)
{
    m_nOriginalBoxWidth = ref.m_nOriginalBoxWidth;
    m_nOriginalBoxHeight = ref.m_nOriginalBoxHeight;
    m_nOriginalPosX = ref.m_nOriginalPosX;
    m_nOriginalPosY = ref.m_nOriginalPosY;
    m_OriginalPaletteRef.Copy(ref.m_OriginalPaletteRef);
}


void MHVisible::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHPresentable::Initialise(p, engine);
    // Original box size - two integer arguments.
    MHParseNode *pOriginalBox = p->GetNamedArg(C_ORIGINAL_BOX_SIZE);
    if (! pOriginalBox) p->Failure("OriginalBoxSize missing");
    m_nOriginalBoxWidth = pOriginalBox->GetArgN(0)->GetIntValue();
    m_nOriginalBoxHeight = pOriginalBox->GetArgN(1)->GetIntValue();

    // Original position - two integer arguments.  Optional
    MHParseNode *pOriginalPos = p->GetNamedArg(C_ORIGINAL_POSITION);
    if (pOriginalPos) {
        m_nOriginalPosX = pOriginalPos->GetArgN(0)->GetIntValue();
        m_nOriginalPosY = pOriginalPos->GetArgN(1)->GetIntValue();
    }

    // OriginalPalette ref - optional. 
    MHParseNode *pOriginalPaletteRef = p->GetNamedArg(C_ORIGINAL_PALETTE_REF);
    if (pOriginalPaletteRef) m_OriginalPaletteRef.Initialise(pOriginalPaletteRef->GetArgN(0), engine);
}

void MHVisible::PrintMe(FILE *fd, int nTabs) const
{
    MHPresentable::PrintMe(fd, nTabs);
    PrintTabs(fd, nTabs); fprintf(fd, ":OrigBoxSize %d %d\n", m_nOriginalBoxWidth, m_nOriginalBoxHeight);
    if (m_nOriginalPosX != 0 || m_nOriginalPosY != 0) {
        PrintTabs(fd, nTabs); fprintf(fd, ":OrigPosition %d %d\n", m_nOriginalPosX, m_nOriginalPosY);
    }
    if (m_OriginalPaletteRef.IsSet()) {
        PrintTabs(fd, nTabs); fprintf(fd, ":OrigPaletteRef"); m_OriginalPaletteRef.PrintMe(fd, nTabs+1); fprintf(fd, "\n");
    }
}

void MHVisible::Preparation(MHEngine *engine)
{
    if (m_fAvailable) return; // Already prepared
    m_nBoxWidth = m_nOriginalBoxWidth;
    m_nBoxHeight = m_nOriginalBoxHeight;
    m_nPosX = m_nOriginalPosX;
    m_nPosY = m_nOriginalPosY;
    m_PaletteRef.Copy(m_OriginalPaletteRef);
    // Add a reference to this to the display stack.
    engine->AddToDisplayStack(this);
    MHIngredient::Preparation(engine);
}

void MHVisible::Destruction(MHEngine *engine)
{
    engine->RemoveFromDisplayStack(this);
    MHIngredient::Destruction(engine);
}

void MHVisible::Activation(MHEngine *engine)
{
    if (m_fRunning) return;
    MHIngredient::Activation(engine);
    m_fRunning = true;
    engine->Redraw(GetVisibleArea()); // Display the visible.
    engine->EventTriggered(this, EventIsRunning);
}

void MHVisible::Deactivation(MHEngine *engine)
{
    if (! m_fRunning) return;
    // Stop displaying.  We need to save the area before we turn off m_fRunning but
    // only call Redraw once this is no longer visible so that the area beneath will be drawn.
    QRegion region = GetVisibleArea();
    MHIngredient::Deactivation(engine);
    engine->Redraw(region); // Draw underlying object.
}

// Return the colour, looking up in the palette if necessary.  Used by the sub-classes
MHRgba MHVisible::GetColour(const MHColour &colour)
{
    ASSERT(colour.m_nColIndex < 0); // We don't support palettes.
    int red = 0, green = 0, blue = 0, alpha = 0;
    int cSize = colour.m_ColStr.Size();
    if (cSize != 4) MHLOG(MHLogWarning, QString("Colour string has length %1 not 4.").arg(cSize));
    // Just in case the length is short we handle those properly.
    if (cSize > 0) red = colour.m_ColStr.GetAt(0);
    if (cSize > 1) green = colour.m_ColStr.GetAt(1);
    if (cSize > 2) blue = colour.m_ColStr.GetAt(2);
    if (cSize > 3) alpha = 255 - colour.m_ColStr.GetAt(3); // Convert transparency to alpha
    return MHRgba(red, green, blue, alpha);
}

// Get the visible region of this visible.  This is the area that needs to be drawn.
QRegion MHVisible::GetVisibleArea()
{
    if (! m_fRunning) return QRegion(); // Not visible at all.
    else return QRegion(QRect(m_nPosX, m_nPosY, m_nBoxWidth, m_nBoxHeight));
}

// MHEG actions.
void MHVisible::SetPosition(int nXPosition, int nYPosition, MHEngine *engine)
{
    // When we move a visible we have to redraw both the old area and the new.
    // In some cases, such as moving an opaque rectangle we might be able to reduce
    // this if there is some overlap.
    QRegion drawRegion = GetVisibleArea();
    m_nPosX = nXPosition;
    m_nPosY = nYPosition;
    drawRegion += GetVisibleArea();
    engine->Redraw(drawRegion);
}

void MHVisible::GetPosition(MHRoot *pXPosN, MHRoot *pYPosN)
{
    pXPosN->SetVariableValue(m_nPosX);
    pYPosN->SetVariableValue(m_nPosY);
}

void MHVisible::SetBoxSize(int nWidth, int nHeight, MHEngine *engine)
{
    QRegion drawRegion = GetVisibleArea();
    m_nBoxWidth = nWidth;
    m_nBoxHeight = nHeight;
    drawRegion += GetVisibleArea();
    engine->Redraw(drawRegion);
}

void MHVisible::GetBoxSize(MHRoot *pWidthDest, MHRoot *pHeightDest)
{
    pWidthDest->SetVariableValue(m_nBoxWidth);
    pHeightDest->SetVariableValue(m_nBoxHeight);
}

void MHVisible::SetPaletteRef(const MHObjectRef newPalette, MHEngine *engine)
{
    m_PaletteRef.Copy(newPalette);
    engine->Redraw(GetVisibleArea());
}

void MHVisible::BringToFront(MHEngine *engine)
{
    engine->BringToFront(this);
}

void MHVisible::SendToBack(MHEngine *engine)
{
    engine->SendToBack(this);
}

void MHVisible::PutBefore(const MHRoot *pRef, MHEngine *engine)
{
    engine->PutBefore(this, pRef);
}

void MHVisible::PutBehind(const MHRoot *pRef, MHEngine *engine)
{
    engine->PutBehind(this, pRef);
}

MHLineArt::MHLineArt()
{
    m_fBorderedBBox = true;
    m_nOriginalLineWidth = 1;
    m_OriginalLineStyle = LineStyleSolid;
    // Colour defaults to empty.
}

// Copy constructor for cloning
MHLineArt::MHLineArt(const MHLineArt &ref): MHVisible(ref)
{
    m_fBorderedBBox = ref.m_fBorderedBBox;
    m_nOriginalLineWidth = ref.m_nOriginalLineWidth;
    m_OriginalLineStyle = ref.m_OriginalLineStyle;
    m_OrigLineColour = ref.m_OrigLineColour;
    m_OrigFillColour = ref.m_OrigFillColour;
}

void MHLineArt::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHVisible::Initialise(p, engine);
    // Bordered bounding box - optional
    MHParseNode *pBBBox = p->GetNamedArg(C_BORDERED_BOUNDING_BOX);
    if (pBBBox) m_fBorderedBBox = pBBBox->GetArgN(0)->GetBoolValue();
    // Original line width
    MHParseNode *pOlw = p->GetNamedArg(C_ORIGINAL_LINE_WIDTH);
    if (pOlw) m_nOriginalLineWidth = pOlw->GetArgN(0)->GetIntValue();
    // Original line style.  This is an integer not an enum.
    MHParseNode *pOls = p->GetNamedArg(C_ORIGINAL_LINE_STYLE);
    if (pOls) m_OriginalLineStyle = pOls->GetArgN(0)->GetIntValue();
    // Line colour.
    MHParseNode *pOrlc = p->GetNamedArg(C_ORIGINAL_REF_LINE_COLOUR);
    if (pOrlc) m_OrigLineColour.Initialise(pOrlc->GetArgN(0), engine);
    // Fill colour
    MHParseNode *pOrfc = p->GetNamedArg(C_ORIGINAL_REF_FILL_COLOUR);
    if (pOrfc) m_OrigFillColour.Initialise(pOrfc->GetArgN(0), engine);
}

void MHLineArt::PrintMe(FILE *fd, int nTabs) const
{
    MHVisible::PrintMe(fd, nTabs);
    if (! m_fBorderedBBox) { PrintTabs(fd, nTabs); fprintf(fd, ":BBBox false\n"); }
    if (m_nOriginalLineWidth != 1) { PrintTabs(fd, nTabs); fprintf(fd, ":OrigLineWidth %d\n", m_nOriginalLineWidth); }
    if (m_OriginalLineStyle != LineStyleSolid) {
        PrintTabs(fd, nTabs); fprintf(fd, ":OrigLineStyle %d\n", m_OriginalLineStyle);
    }
    if (m_OrigLineColour.IsSet()) {
        PrintTabs(fd, nTabs); fprintf(fd, ":OrigRefLineColour "); m_OrigLineColour.PrintMe(fd, nTabs+1); fprintf(fd, "\n");
    }
    if (m_OrigFillColour.IsSet()) {
        PrintTabs(fd, nTabs); fprintf(fd, ":OrigRefFillColour "); m_OrigFillColour.PrintMe(fd, nTabs+1); fprintf(fd, "\n");
    }
}

void MHLineArt::Preparation(MHEngine *engine)
{
    if (m_fAvailable) return; // Already prepared
    // Set up the internal attributes.
    m_nLineWidth = m_nOriginalLineWidth;
    m_LineStyle = m_OriginalLineStyle;
    if (m_OrigLineColour.IsSet()) m_LineColour.Copy(m_OrigLineColour);
    else m_LineColour.SetFromString("\000\000\000\000", 4); // Default is black
    if (m_OrigFillColour.IsSet()) m_FillColour.Copy(m_OrigFillColour);
    else m_FillColour.SetFromString("\000\000\000\377", 4); // Default is transparent

    MHVisible::Preparation(engine); // Prepare the base class.
}

// MHEG Actions.
// Set the colours
void MHLineArt::SetFillColour(const MHColour &colour, MHEngine *engine)
{
    m_FillColour.Copy(colour);
    engine->Redraw(GetVisibleArea());
}

void MHLineArt::SetLineColour(const MHColour &colour, MHEngine *engine)
{
    m_LineColour.Copy(colour);
    engine->Redraw(GetVisibleArea());
}

void MHLineArt::SetLineWidth(int nWidth, MHEngine *engine)
{
    m_nLineWidth = nWidth;
    engine->Redraw(GetVisibleArea());
}

void MHLineArt::SetLineStyle(int nStyle, MHEngine *engine)
{
    m_LineStyle = nStyle;
    engine->Redraw(GetVisibleArea());
}

// The rectangle class doesn't add much to the LineArt class
void MHRectangle::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs); fprintf(fd, "{:Rectangle ");
    MHLineArt::PrintMe(fd, nTabs+1);
    PrintTabs(fd, nTabs);fprintf(fd, "}\n");
}

// If this is opaque it obscures the underlying area.
QRegion MHRectangle::GetOpaqueArea()
{
    if (! m_fRunning) return QRegion();
    MHRgba lineColour = GetColour(m_LineColour);
    MHRgba fillColour = GetColour(m_FillColour);
    // If the fill is transparent or semi-transparent we return an empty region and
    // ignore the special case where the surrounding box is opaque.
    if (fillColour.alpha() != 255) return QRegion();
    if (lineColour.alpha() == 255 || m_nLineWidth == 0) return QRegion(QRect(m_nPosX, m_nPosY, m_nBoxWidth, m_nBoxHeight));
    if (m_nBoxWidth <= 2*m_nLineWidth || m_nBoxHeight <= 2*m_nLineWidth) return QRegion();
    else return QRegion(QRect(m_nPosX + m_nLineWidth, m_nPosY + m_nLineWidth,
                m_nBoxWidth - m_nLineWidth*2, m_nBoxHeight - m_nLineWidth*2));
}

void MHRectangle::Display(MHEngine *engine)
{
    if (! m_fRunning) return;
    if (m_nBoxWidth == 0 || m_nBoxHeight == 0) return; // Can't draw zero sized boxes.
    // The bounding box is assumed always to be True.

    MHRgba lineColour = GetColour(m_LineColour);
    MHRgba fillColour = GetColour(m_FillColour);
    MHContext *d = engine->GetContext();
    // Fill the centre.
    if (m_nBoxHeight < m_nLineWidth*2 || m_nBoxWidth < m_nLineWidth*2) {
        // If the area is very small but non-empty fill it with the line colour
        d->DrawRect(m_nPosX, m_nPosY, m_nBoxWidth, m_nBoxHeight, lineColour);
    }
    else {
        d->DrawRect(m_nPosX + m_nLineWidth, m_nPosY + m_nLineWidth,
                m_nBoxWidth - m_nLineWidth*2, m_nBoxHeight - m_nLineWidth*2, fillColour);
        // Draw the lines round the outside.  UK MHEG allows us to treat all line styles as solid.
        // It isn't clear when we draw dashed and dotted lines what colour to put in the spaces.
        d->DrawRect(m_nPosX, m_nPosY, m_nBoxWidth, m_nLineWidth, lineColour);
        d->DrawRect(m_nPosX, m_nPosY + m_nBoxHeight - m_nLineWidth, m_nBoxWidth, m_nLineWidth, lineColour);
        d->DrawRect(m_nPosX, m_nPosY + m_nLineWidth, m_nLineWidth, m_nBoxHeight - m_nLineWidth*2, lineColour);
        d->DrawRect(m_nPosX + m_nBoxWidth - m_nLineWidth, m_nPosY + m_nLineWidth,
            m_nLineWidth, m_nBoxHeight - m_nLineWidth*2, lineColour);
    }
}


MHInteractible::MHInteractible()
{

}

MHInteractible::~MHInteractible()
{

}

void MHInteractible::Initialise(MHParseNode */*p*/, MHEngine */*engine*/)
{
}

void MHInteractible::PrintMe(FILE */*fd*/, int /*nTabs*/) const
{
}

MHSlider::MHSlider()
{

}

MHSlider::~MHSlider()
{

}

void MHSlider::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHVisible::Initialise(p, engine);
    MHInteractible::Initialise(p, engine);
    //
}

void MHSlider::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs); fprintf(fd, "{:Slider ");
    MHVisible::PrintMe(fd, nTabs);
    MHInteractible::PrintMe(fd, nTabs+1);
    fprintf(fd, "****TODO\n");
    PrintTabs(fd, nTabs); fprintf(fd, "}\n");
}

MHEntryField::MHEntryField()
{

}

MHEntryField::~MHEntryField()
{

}

void MHEntryField::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHVisible::Initialise(p, engine);
    MHInteractible::Initialise(p, engine);
    //
}

void MHEntryField::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs); fprintf(fd, "{:EntryField ");
    MHVisible::PrintMe(fd, nTabs+1);
    MHInteractible::PrintMe(fd, nTabs);
    fprintf(fd, "****TODO\n");
    PrintTabs(fd, nTabs); fprintf(fd, "}\n");
}

MHButton::MHButton()
{

}

MHButton::~MHButton()
{

}

void MHButton::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHVisible::Initialise(p, engine);
    //
}

void MHButton::PrintMe(FILE *fd, int nTabs) const
{
    MHVisible::PrintMe(fd, nTabs);
    //
}

MHHotSpot::MHHotSpot()
{

}

MHHotSpot::~MHHotSpot()
{

}

void MHHotSpot::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHButton::Initialise(p, engine);
    //
}

void MHHotSpot::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs); fprintf(fd, "{:Hotspot ");
    MHButton::PrintMe(fd, nTabs+1);
    fprintf(fd, "****TODO\n");
    PrintTabs(fd, nTabs); fprintf(fd, "}\n");
}

MHPushButton::MHPushButton()
{

}

MHPushButton::~MHPushButton()
{

}

void MHPushButton::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHButton::Initialise(p, engine);
    //
}

void MHPushButton::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs); fprintf(fd, "{:PushButton ");
    MHButton::PrintMe(fd, nTabs+1);
    fprintf(fd, "****TODO\n");
    PrintTabs(fd, nTabs); fprintf(fd, "}\n");
}

MHSwitchButton::MHSwitchButton()
{

}

MHSwitchButton::~MHSwitchButton()
{

}

void MHSwitchButton::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHPushButton::Initialise(p, engine);
    //
}

void MHSwitchButton::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs); fprintf(fd, "{:SwitchButton ");
    MHPushButton::PrintMe(fd, nTabs+1);
    fprintf(fd, "****TODO\n");
    PrintTabs(fd, nTabs); fprintf(fd, "}\n");
}


void MHSetColour::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHElemAction::Initialise(p, engine);
    if (p->GetArgCount() > 1) {
        MHParseNode *pIndexed = p->GetNamedArg(C_NEW_COLOUR_INDEX);
        MHParseNode *pAbsolute = p->GetNamedArg(C_NEW_ABSOLUTE_COLOUR);
        if (pIndexed) { m_ColourType = CT_Indexed; m_Indexed.Initialise(pIndexed->GetArgN(0), engine); }
        else if (pAbsolute) { m_ColourType = CT_Absolute; m_Absolute.Initialise(pAbsolute->GetArgN(0), engine); }
    }
}

void MHSetColour::PrintArgs(FILE *fd, int) const
{
    if (m_ColourType == CT_Indexed) { fprintf(fd, ":NewColourIndex "); m_Indexed.PrintMe(fd, 0); }
    else if (m_ColourType == CT_Absolute) { fprintf(fd, ":NewAbsoluteColour "); m_Absolute.PrintMe(fd, 0); }
}

void MHSetColour::Perform(MHEngine *engine)
{
    MHObjectRef target;
    m_Target.GetValue(target, engine); // Get the item to set.
    MHColour newColour;
    switch (m_ColourType)
    {
    case CT_None:
        {
            // If the colour is not specified use "transparent".
            newColour.SetFromString("\000\000\000\377", 4);
            break;
        }
    case CT_Absolute:
        {
            MHOctetString colour;
            m_Absolute.GetValue(colour, engine);
            newColour.m_ColStr.Copy(colour);
            break;
        }
    case CT_Indexed:
        newColour.m_nColIndex = m_Indexed.GetValue(engine);
    }
    SetColour(newColour, engine); // Set the colour of the appropriate portion of the visible
}
