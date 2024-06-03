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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
   Or, point your browser to http://www.gnu.org/copyleft/gpl.html

*/
#include "Visible.h"

#include <array>
#include <cstdio>

#include <QRect>
#include <QString>

#include "Presentable.h"
#include "Ingredients.h"
#include "Root.h"
#include "BaseClasses.h"
#include "ParseNode.h"
#include "ASN1Codes.h"
#include "Engine.h"
#include "Logging.h"
#include "freemheg.h"


// Copy constructor for cloning
MHVisible::MHVisible(const MHVisible &ref)
  : MHPresentable(ref),
    m_nOriginalBoxWidth(ref.m_nOriginalBoxWidth),
    m_nOriginalBoxHeight(ref.m_nOriginalBoxHeight),
    m_nOriginalPosX(ref.m_nOriginalPosX),
    m_nOriginalPosY(ref.m_nOriginalPosY),
    m_nBoxWidth(ref.m_nBoxWidth),
    m_nBoxHeight(ref.m_nBoxHeight),
    m_nPosX(ref.m_nPosX),
    m_nPosY(ref.m_nPosY)
{
    m_originalPaletteRef.Copy(ref.m_originalPaletteRef);
}


void MHVisible::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHPresentable::Initialise(p, engine);
    // Original box size - two integer arguments.
    MHParseNode *pOriginalBox = p->GetNamedArg(C_ORIGINAL_BOX_SIZE);

    if (! pOriginalBox)
    {
        MHParseNode::Failure("OriginalBoxSize missing");
    }
    else
    {
        m_nOriginalBoxWidth = pOriginalBox->GetArgN(0)->GetIntValue();
        m_nOriginalBoxHeight = pOriginalBox->GetArgN(1)->GetIntValue();
    }

    // Original position - two integer arguments.  Optional
    MHParseNode *pOriginalPos = p->GetNamedArg(C_ORIGINAL_POSITION);

    if (pOriginalPos)
    {
        m_nOriginalPosX = pOriginalPos->GetArgN(0)->GetIntValue();
        m_nOriginalPosY = pOriginalPos->GetArgN(1)->GetIntValue();
    }

    // OriginalPalette ref - optional.
    MHParseNode *pOriginalPaletteRef = p->GetNamedArg(C_ORIGINAL_PALETTE_REF);

    if (pOriginalPaletteRef)
    {
        m_originalPaletteRef.Initialise(pOriginalPaletteRef->GetArgN(0), engine);
    }
}

void MHVisible::PrintMe(FILE *fd, int nTabs) const
{
    MHPresentable::PrintMe(fd, nTabs);
    PrintTabs(fd, nTabs);
    fprintf(fd, ":OrigBoxSize %d %d\n", m_nOriginalBoxWidth, m_nOriginalBoxHeight);

    if (m_nOriginalPosX != 0 || m_nOriginalPosY != 0)
    {
        PrintTabs(fd, nTabs);
        fprintf(fd, ":OrigPosition %d %d\n", m_nOriginalPosX, m_nOriginalPosY);
    }

    if (m_originalPaletteRef.IsSet())
    {
        PrintTabs(fd, nTabs);
        fprintf(fd, ":OrigPaletteRef");
        m_originalPaletteRef.PrintMe(fd, nTabs + 1);
        fprintf(fd, "\n");
    }
}

void MHVisible::Preparation(MHEngine *engine)
{
    if (m_fAvailable)
    {
        return;    // Already prepared
    }

    m_nBoxWidth = m_nOriginalBoxWidth;
    m_nBoxHeight = m_nOriginalBoxHeight;
    m_nPosX = m_nOriginalPosX;
    m_nPosY = m_nOriginalPosY;
    m_paletteRef.Copy(m_originalPaletteRef);
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
    if (m_fRunning)
    {
        return;
    }

    MHIngredient::Activation(engine);
    m_fRunning = true;
    engine->Redraw(GetVisibleArea()); // Display the visible.
    engine->EventTriggered(this, EventIsRunning);
}

void MHVisible::Deactivation(MHEngine *engine)
{
    if (! m_fRunning)
    {
        return;
    }

    // Stop displaying.  We need to save the area before we turn off m_fRunning but
    // only call Redraw once this is no longer visible so that the area beneath will be drawn.
    QRegion region = GetVisibleArea();
    MHIngredient::Deactivation(engine);
    engine->Redraw(region); // Draw underlying object.
}

// Return the colour, looking up in the palette if necessary.  Used by the sub-classes
MHRgba MHVisible::GetColour(const MHColour &colour)
{
    int red = 0;
    int green = 0;
    int blue = 0;
    int alpha = 0;
    if (colour.IsSet())
    {
        int cSize = colour.m_colStr.Size();

        if (cSize != 4)
        {
            MHLOG(MHLogWarning, QString("Colour string has length %1 not 4.").arg(cSize));
        }

        // Just in case the length is short we handle those properly.
        if (cSize > 0)
        {
            red = colour.m_colStr.GetAt(0);
        }

        if (cSize > 1)
        {
            green = colour.m_colStr.GetAt(1);
        }

        if (cSize > 2)
        {
            blue = colour.m_colStr.GetAt(2);
        }

        if (cSize > 3)
        {
            alpha = 255 - colour.m_colStr.GetAt(3);    // Convert transparency to alpha
        }
    }

    return {red, green, blue, alpha};
}

// Get the visible region of this visible.  This is the area that needs to be drawn.
QRegion MHVisible::GetVisibleArea()
{
    if (! m_fRunning)
    {
        return {};    // Not visible at all.
    }
    return {QRect(m_nPosX, m_nPosY, m_nBoxWidth, m_nBoxHeight)};
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

void MHVisible::SetPaletteRef(const MHObjectRef& newPalette, MHEngine *engine)
{
    m_paletteRef.Copy(newPalette);
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

// Copy constructor for cloning
MHLineArt::MHLineArt(const MHLineArt &ref)
  : MHVisible(ref),
    m_fBorderedBBox(ref.m_fBorderedBBox),
    m_nOriginalLineWidth(ref.m_nOriginalLineWidth),
    m_originalLineStyle(ref.m_originalLineStyle),
    m_nLineWidth(ref.m_nLineWidth),
    m_lineStyle(ref.m_lineStyle)
{
    m_origLineColour.Copy(ref.m_origLineColour);
    m_origFillColour.Copy(ref.m_origFillColour);
}

void MHLineArt::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHVisible::Initialise(p, engine);
    // Bordered bounding box - optional
    MHParseNode *pBBBox = p->GetNamedArg(C_BORDERED_BOUNDING_BOX);

    if (pBBBox)
    {
        m_fBorderedBBox = pBBBox->GetArgN(0)->GetBoolValue();
    }

    // Original line width
    MHParseNode *pOlw = p->GetNamedArg(C_ORIGINAL_LINE_WIDTH);

    if (pOlw)
    {
        m_nOriginalLineWidth = pOlw->GetArgN(0)->GetIntValue();
    }

    // Original line style.  This is an integer not an enum.
    MHParseNode *pOls = p->GetNamedArg(C_ORIGINAL_LINE_STYLE);

    if (pOls)
    {
        m_originalLineStyle = pOls->GetArgN(0)->GetIntValue();
    }

    // Line colour.
    MHParseNode *pOrlc = p->GetNamedArg(C_ORIGINAL_REF_LINE_COLOUR);

    if (pOrlc)
    {
        m_origLineColour.Initialise(pOrlc->GetArgN(0), engine);
    }

    // Fill colour
    MHParseNode *pOrfc = p->GetNamedArg(C_ORIGINAL_REF_FILL_COLOUR);

    if (pOrfc)
    {
        m_origFillColour.Initialise(pOrfc->GetArgN(0), engine);
    }
}

void MHLineArt::PrintMe(FILE *fd, int nTabs) const
{
    MHVisible::PrintMe(fd, nTabs);

    if (! m_fBorderedBBox)
    {
        PrintTabs(fd, nTabs);
        fprintf(fd, ":BBBox false\n");
    }

    if (m_nOriginalLineWidth != 1)
    {
        PrintTabs(fd, nTabs);
        fprintf(fd, ":OrigLineWidth %d\n", m_nOriginalLineWidth);
    }

    if (m_originalLineStyle != LineStyleSolid)
    {
        PrintTabs(fd, nTabs);
        fprintf(fd, ":OrigLineStyle %d\n", m_originalLineStyle);
    }

    if (m_origLineColour.IsSet())
    {
        PrintTabs(fd, nTabs);
        fprintf(fd, ":OrigRefLineColour ");
        m_origLineColour.PrintMe(fd, nTabs + 1);
        fprintf(fd, "\n");
    }

    if (m_origFillColour.IsSet())
    {
        PrintTabs(fd, nTabs);
        fprintf(fd, ":OrigRefFillColour ");
        m_origFillColour.PrintMe(fd, nTabs + 1);
        fprintf(fd, "\n");
    }
}

void MHLineArt::Preparation(MHEngine *engine)
{
    if (m_fAvailable)
    {
        return;    // Already prepared
    }

    // Set up the internal attributes.
    m_nLineWidth = m_nOriginalLineWidth;
    m_lineStyle = m_originalLineStyle;

    if (m_origLineColour.IsSet())
    {
        m_lineColour.Copy(m_origLineColour);
    }
    else
    {
        m_lineColour.SetFromString("\000\000\000\000", 4);    // Default is black
    }

    if (m_origFillColour.IsSet())
    {
        m_fillColour.Copy(m_origFillColour);
    }
    else
    {
        m_fillColour.SetFromString("\000\000\000\377", 4);    // Default is transparent
    }

    MHVisible::Preparation(engine); // Prepare the base class.
}

// MHEG Actions.
// Set the colours
void MHLineArt::SetFillColour(const MHColour &colour, MHEngine *engine)
{
    m_fillColour.Copy(colour);
    engine->Redraw(GetVisibleArea());
}

void MHLineArt::SetLineColour(const MHColour &colour, MHEngine *engine)
{
    m_lineColour.Copy(colour);
    engine->Redraw(GetVisibleArea());
}

void MHLineArt::SetLineWidth(int nWidth, MHEngine *engine)
{
    m_nLineWidth = nWidth;
    engine->Redraw(GetVisibleArea());
}

void MHLineArt::SetLineStyle(int nStyle, MHEngine *engine)
{
    m_lineStyle = nStyle;
    engine->Redraw(GetVisibleArea());
}

// The rectangle class doesn't add much to the LineArt class
void MHRectangle::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:Rectangle ");
    MHLineArt::PrintMe(fd, nTabs + 1);
    PrintTabs(fd, nTabs);
    fprintf(fd, "}\n");
}

// If this is opaque it obscures the underlying area.
QRegion MHRectangle::GetOpaqueArea()
{
    if (! m_fRunning)
    {
        return {};
    }

    MHRgba lineColour = GetColour(m_lineColour);
    MHRgba fillColour = GetColour(m_fillColour);

    // If the fill is transparent or semi-transparent we return an empty region and
    // ignore the special case where the surrounding box is opaque.
    if (fillColour.alpha() != 255)
    {
        return {};
    }

    if (lineColour.alpha() == 255 || m_nLineWidth == 0)
    {
        return {QRect(m_nPosX, m_nPosY, m_nBoxWidth, m_nBoxHeight)};
    }

    if (m_nBoxWidth <= 2 * m_nLineWidth || m_nBoxHeight <= 2 * m_nLineWidth)
    {
        return {};
    }
    return {QRect(m_nPosX + m_nLineWidth, m_nPosY + m_nLineWidth,
                  m_nBoxWidth - (m_nLineWidth * 2), m_nBoxHeight - (m_nLineWidth * 2))};
}

void MHRectangle::Display(MHEngine *engine)
{
    if (! m_fRunning)
    {
        return;
    }

    if (m_nBoxWidth == 0 || m_nBoxHeight == 0)
    {
        return;    // Can't draw zero sized boxes.
    }

    // The bounding box is assumed always to be True.

    MHRgba lineColour = GetColour(m_lineColour);
    MHRgba fillColour = GetColour(m_fillColour);
    MHContext *d = engine->GetContext();

    // Fill the centre.
    if (m_nBoxHeight < m_nLineWidth * 2 || m_nBoxWidth < m_nLineWidth * 2)
    {
        // If the area is very small but non-empty fill it with the line colour
        d->DrawRect(m_nPosX, m_nPosY, m_nBoxWidth, m_nBoxHeight, lineColour);
    }
    else
    {
        d->DrawRect(m_nPosX + m_nLineWidth, m_nPosY + m_nLineWidth,
                    m_nBoxWidth - (m_nLineWidth * 2), m_nBoxHeight - (m_nLineWidth * 2), fillColour);
        // Draw the lines round the outside.  UK MHEG allows us to treat all line styles as solid.
        // It isn't clear when we draw dashed and dotted lines what colour to put in the spaces.
        d->DrawRect(m_nPosX, m_nPosY, m_nBoxWidth, m_nLineWidth, lineColour);
        d->DrawRect(m_nPosX, m_nPosY + m_nBoxHeight - m_nLineWidth, m_nBoxWidth, m_nLineWidth, lineColour);
        d->DrawRect(m_nPosX, m_nPosY + m_nLineWidth, m_nLineWidth, m_nBoxHeight - (m_nLineWidth * 2), lineColour);
        d->DrawRect(m_nPosX + m_nBoxWidth - m_nLineWidth, m_nPosY + m_nLineWidth,
                    m_nLineWidth, m_nBoxHeight - (m_nLineWidth * 2), lineColour);
    }
}


void MHInteractible::Initialise(MHParseNode *p, MHEngine *engine)
{
    // Engine Resp - optional
    MHParseNode *pEngineResp = p->GetNamedArg(C_ENGINE_RESP);

    if (pEngineResp)
    {
        m_fEngineResp = pEngineResp->GetArgN(0)->GetBoolValue();
    }

    // Highlight colour.
    MHParseNode *phlCol = p->GetNamedArg(C_HIGHLIGHT_REF_COLOUR);

    if (phlCol)
    {
        m_highlightRefColour.Initialise(phlCol->GetArgN(0), engine);
    }
    else
    {
        engine->GetDefaultHighlightRefColour(m_highlightRefColour);
    }

    m_fHighlightStatus = false;
    m_fInteractionStatus = false;
}

void MHInteractible::PrintMe(FILE *fd, int nTabs) const
{
    if (! m_fEngineResp)
    {
        PrintTabs(fd, nTabs);
        fprintf(fd, ":EngineResp false\n");
    }

    if (m_highlightRefColour.IsSet())
    {
        PrintTabs(fd, nTabs);
        fprintf(fd, ":HighlightRefColour ");
        m_highlightRefColour.PrintMe(fd, nTabs + 1);
        fprintf(fd, "\n");
    }
}

void MHInteractible::Interaction(MHEngine *engine)
{
    m_fInteractionStatus = true;
    engine->SetInteraction(this);
    // The MHEG standard says: "generate visual feedback" here
    // but it appears that any visual feedback is controlled only
    // by the highlight status combined with engine-resp.
}

void MHInteractible::InteractSetInteractionStatus(bool newStatus, MHEngine *engine)
{
    if (newStatus)   // Turning interaction on.
    {
        if (engine->GetInteraction() == nullptr) // No current interactible
        {
            Interaction(engine);    // virtual function
        }
    }
    else   // Turning interaction off.
    {
        if (m_fInteractionStatus)
        {
            m_fInteractionStatus = false;
            engine->SetInteraction(nullptr);
            InteractionCompleted(engine); // Interaction is interrupted.
            engine->EventTriggered(m_parent, EventInteractionCompleted);
        }
    }
}

void MHInteractible::InteractSetHighlightStatus(bool newStatus, MHEngine *engine)
{
    if (newStatus == m_fHighlightStatus)
    {
        return;
    }

    m_fHighlightStatus = newStatus;

    // If active redraw to show change of status.
    if (m_parent->GetRunningStatus() && m_fEngineResp)
    {
        engine->Redraw(m_parent->GetVisibleArea());
    }

    // Generate the event for the change of highlight status.
    engine->EventTriggered(m_parent, m_fHighlightStatus ? EventHighlightOn : EventHighlightOff);
}

void MHSlider::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHVisible::Initialise(p, engine);
    MHInteractible::Initialise(p, engine);
    //
    MHParseNode *pOrientation = p->GetNamedArg(C_ORIENTATION);

    if (pOrientation)
    {
        m_orientation = (enum SliderOrientation)pOrientation->GetArgN(0)->GetEnumValue();
    }

    // This is not optional.

    MHParseNode *pMin = p->GetNamedArg(C_MIN_VALUE);

    if (pMin)
    {
        m_origMinValue = pMin->GetArgN(0)->GetIntValue();
    }
    else
    {
        m_origMinValue = 1;
    }

    MHParseNode *pMax = p->GetNamedArg(C_MAX_VALUE);

    if (pMax)
    {
        m_origMaxValue = pMax->GetArgN(0)->GetIntValue();
    }
    else
    {
        m_origMaxValue = m_origMinValue - 1;    // Unset
    }

    MHParseNode *pInit = p->GetNamedArg(C_INITIAL_VALUE);

    if (pInit)
    {
        m_initialValue = pInit->GetArgN(0)->GetIntValue();
    }
    else
    {
        m_initialValue = m_origMinValue;    // Default is m_minValue
    }

    MHParseNode *pPortion = p->GetNamedArg(C_INITIAL_PORTION);

    if (pPortion)
    {
        m_initialPortion = pPortion->GetArgN(0)->GetIntValue();
    }
    else
    {
        m_initialPortion = m_origMinValue - 1;    // Unset
    }

    MHParseNode *pStep = p->GetNamedArg(C_STEP_SIZE);

    if (pStep)
    {
        m_origStepSize = pStep->GetArgN(0)->GetIntValue();
    }
    else
    {
        m_origStepSize = 1;    // Unset
    }

    MHParseNode *pStyle = p->GetNamedArg(C_SLIDER_STYLE);

    if (pStyle)
    {
        m_style = (enum SliderStyle)pStyle->GetArgN(0)->GetEnumValue();
    }
    else
    {
        m_style = SliderNormal;
    }

    MHParseNode *pslCol = p->GetNamedArg(C_SLIDER_REF_COLOUR);

    if (pslCol)
    {
        m_sliderRefColour.Initialise(pslCol->GetArgN(0), engine);
    }
    else
    {
        engine->GetDefaultSliderRefColour(m_sliderRefColour);
    }
}

static const std::array<const QString,4> rchOrientation
{
    "left", // 1
    "right",
    "up",
    "down" // 4
};

// Look up the Orientation. Returns zero if it doesn't match.  Used in the text parser only.
int MHSlider::GetOrientation(const QString& str)
{
    for (size_t i = 0; i < rchOrientation.size(); i++)
    {
        if (str.compare(rchOrientation[i], Qt::CaseInsensitive) == 0)
        {
            return (i + 1);    // Numbered from 1
        }
    }

    return 0;
}

static const std::array<const QString,3> rchStyle
{
    "normal", // 1
    "thermometer",
    "proportional" // 3
};

int MHSlider::GetStyle(const QString& str)
{
    for (size_t i = 0; i < rchStyle.size(); i++)
    {
        if (str.compare(rchStyle[i], Qt::CaseInsensitive) == 0)
        {
            return (i + 1);    // Numbered from 1
        }
    }

    return 0;
}

void MHSlider::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:Slider ");
    MHVisible::PrintMe(fd, nTabs + 1);
    MHInteractible::PrintMe(fd, nTabs + 1);

    PrintTabs(fd, nTabs);
    fprintf(fd, ":Orientation %s\n", qPrintable(rchOrientation[m_orientation-1]));

    if (m_initialValue >= m_origMinValue)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":InitialValue %d\n", m_initialValue);
    }

    if (m_origMinValue != 1)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":MinValue %d\n", m_origMinValue);
    }

    if (m_origMaxValue > m_origMinValue)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":MaxValue %d\n", m_origMaxValue);
    }

    if (m_initialPortion >= m_origMinValue)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":InitialPortion %d\n", m_initialPortion);
    }

    if (m_origStepSize != 1)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":StepSize %d\n", m_origStepSize);
    }

    if (m_style != SliderNormal)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":SliderStyle %s\n", qPrintable(rchStyle[m_style-1]));
    }

    if (m_sliderRefColour.IsSet())
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":SliderRefColour ");
        m_sliderRefColour.PrintMe(fd, nTabs + 2);
        fprintf(fd, "\n");
    }

    PrintTabs(fd, nTabs);
    fprintf(fd, "}\n");
}

// The MHEG standard doesn't define where the internal values are
// initialised.  Assume it's during preparation.
void MHSlider::Preparation(MHEngine *engine)
{
    MHVisible::Preparation(engine);
    m_maxValue = m_origMaxValue;
    m_minValue = m_origMinValue;
    m_stepSize = m_origStepSize;
    m_sliderValue = m_initialValue;
    m_portion = m_initialPortion;
}

void MHSlider::Display(MHEngine *engine)
{
    MHContext *d = engine->GetContext();
    MHRgba colour;

    if (m_fHighlightStatus && m_fEngineResp)
    {
        colour = GetColour(m_highlightRefColour);
    }
    else
    {
        colour = GetColour(m_sliderRefColour);
    }

    int major = m_nBoxHeight; // Direction of change.
    if (m_orientation == SliderLeft || m_orientation == SliderRight)
        major = m_nBoxWidth;

    if (m_maxValue <= m_minValue)
    {
        return;    // Avoid divide by zero if error.
    }

    if (m_style == SliderNormal)
    {
        // This is drawn as a 9 pixel wide "thumb" at the position.
        major -= 9; // Width of "thumb"
        int posn = major * (m_sliderValue - m_minValue) / (m_maxValue - m_minValue);

        switch (m_orientation)
        {
            case SliderLeft:
                d->DrawRect(m_nPosX + posn, m_nPosY, 9, m_nBoxHeight, colour);
                break;
            case SliderRight:
                d->DrawRect(m_nPosX + m_nBoxWidth - posn - 9, m_nPosY, 9, m_nBoxHeight, colour);
                break;
            case SliderUp:
                d->DrawRect(m_nPosX, m_nPosY + m_nBoxHeight - posn - 9, m_nBoxWidth, 9, colour);
                break;
            case SliderDown:
                d->DrawRect(m_nPosX, m_nPosY + posn, m_nBoxWidth, 9, colour);
                break;
        }
    }
    else
    {
        // Thermometer and proportional sliders are drawn as bars.  Thermometers
        // run from the start to the position, proportional sliders from the
        // position for the "portion".
        int start = 0;
        int end = major * (m_sliderValue - m_minValue) / (m_maxValue - m_minValue);

        if (m_style == SliderProp)
        {
            start = end;
            end = major * (m_sliderValue + m_portion - m_minValue) / (m_maxValue - m_minValue);
        }

        switch (m_orientation)
        {
            case SliderLeft:
                d->DrawRect(m_nPosX + start, m_nPosY, end - start, m_nBoxHeight, colour);
                break;
            case SliderRight:
                d->DrawRect(m_nPosX + m_nBoxWidth - end, m_nPosY, end - start, m_nBoxHeight, colour);
                break;
            case SliderUp:
                d->DrawRect(m_nPosX, m_nPosY + m_nBoxHeight - end, m_nBoxWidth, end - start, colour);
                break;
            case SliderDown:
                d->DrawRect(m_nPosX, m_nPosY + start, m_nBoxWidth, end - start, colour);
                break;
        }

    }
}

void MHSlider::Interaction(MHEngine *engine)
{
    MHInteractible::Interaction(engine);
    // All the interaction is handled by KeyEvent.
}

// Called when the interaction has been terminated and we need
// to restore the state to non-interacting.
void MHSlider::InteractionCompleted(MHEngine *engine)
{
    MHInteractible::InteractionCompleted(engine);
    // Redraw with the interaction highlighting turned off
    engine->Redraw(GetVisibleArea());
}

// Called when a key is pressed.  The only keys that have an effect are
// the Select and Cancel keys which both terminate the action and the
// arrow keys.  The effect of the arrow keys depends on the orientation of
// the slider.
void MHSlider::KeyEvent(MHEngine *engine, int nCode)
{
    switch (nCode)
    {
        case 15: // Select key
        case 16: // Cancel key
            m_fInteractionStatus = false;
            engine->SetInteraction(nullptr);
            InteractionCompleted(engine); // Interaction is interrupted.
            engine->EventTriggered(this, EventInteractionCompleted);
            break;

        case 1: // Up

            if (m_orientation == SliderUp)
            {
                Increment(engine);
            }
            else if (m_orientation == SliderDown)
            {
                Decrement(engine);
            }

            break;

        case 2: // Down

            if (m_orientation == SliderUp)
            {
                Decrement(engine);
            }
            else if (m_orientation == SliderDown)
            {
                Increment(engine);
            }

            break;

        case 3: // Left

            if (m_orientation == SliderLeft)
            {
                Increment(engine);
            }
            else if (m_orientation == SliderRight)
            {
                Decrement(engine);
            }

            break;

        case 4: // Right

            if (m_orientation == SliderLeft)
            {
                Decrement(engine);
            }
            else if (m_orientation == SliderRight)
            {
                Increment(engine);
            }

            break;

    }
}

void MHSlider::Increment(MHEngine *engine)
{
    if (m_sliderValue + m_stepSize <= m_maxValue)
    {
        m_sliderValue += m_stepSize;
        engine->Redraw(GetVisibleArea());
        engine->EventTriggered(this, EventSliderValueChanged);
    }
}

void MHSlider::Decrement(MHEngine *engine)
{
    if (m_sliderValue - m_stepSize >= m_minValue)
    {
        m_sliderValue -= m_stepSize;
        engine->Redraw(GetVisibleArea());
        engine->EventTriggered(this, EventSliderValueChanged);
    }
}

void MHSlider::Step(int nbSteps, MHEngine *engine)
{
    m_stepSize = nbSteps;

    if (m_fRunning)
    {
        engine->Redraw(GetVisibleArea());
    }

    engine->EventTriggered(this, EventSliderValueChanged);
}

void MHSlider::SetSliderValue(int newValue, MHEngine *engine)
{
    m_sliderValue = newValue;

    if (m_fRunning)
    {
        engine->Redraw(GetVisibleArea());
    }

    engine->EventTriggered(this, EventSliderValueChanged);
}

void MHSlider::SetPortion(int newPortion, MHEngine *engine)
{
    m_portion = newPortion;

    if (m_fRunning)
    {
        engine->Redraw(GetVisibleArea());
    }

    engine->EventTriggered(this, EventSliderValueChanged);
}

// Additional action defined in UK MHEG.
void MHSlider::SetSliderParameters(int newMin, int newMax, int newStep, MHEngine *engine)
{
    m_minValue = newMin;
    m_maxValue = newMax;
    m_stepSize = newStep;
    m_sliderValue = newMin;

    if (m_fRunning)
    {
        engine->Redraw(GetVisibleArea());
    }

    engine->EventTriggered(this, EventSliderValueChanged);
}


void MHEntryField::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHVisible::Initialise(p, engine);
    MHInteractible::Initialise(p, engine);
    //
}

void MHEntryField::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:EntryField ");
    MHVisible::PrintMe(fd, nTabs + 1);
    MHInteractible::PrintMe(fd, nTabs);
    fprintf(fd, "****TODO\n");
    PrintTabs(fd, nTabs);
    fprintf(fd, "}\n");
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

void MHHotSpot::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHButton::Initialise(p, engine);
    //
}

void MHHotSpot::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:Hotspot ");
    MHButton::PrintMe(fd, nTabs + 1);
    fprintf(fd, "****TODO\n");
    PrintTabs(fd, nTabs);
    fprintf(fd, "}\n");
}

void MHPushButton::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHButton::Initialise(p, engine);
    //
}

void MHPushButton::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:PushButton ");
    MHButton::PrintMe(fd, nTabs + 1);
    fprintf(fd, "****TODO\n");
    PrintTabs(fd, nTabs);
    fprintf(fd, "}\n");
}

void MHSwitchButton::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHPushButton::Initialise(p, engine);
    //
}

void MHSwitchButton::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:SwitchButton ");
    MHPushButton::PrintMe(fd, nTabs + 1);
    fprintf(fd, "****TODO\n");
    PrintTabs(fd, nTabs);
    fprintf(fd, "}\n");
}


void MHSetColour::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHElemAction::Initialise(p, engine);

    if (p->GetArgCount() > 1)
    {
        MHParseNode *pIndexed = p->GetNamedArg(C_NEW_COLOUR_INDEX);
        MHParseNode *pAbsolute = p->GetNamedArg(C_NEW_ABSOLUTE_COLOUR);

        if (pIndexed)
        {
            m_colourType = CT_Indexed;
            m_indexed.Initialise(pIndexed->GetArgN(0), engine);
        }
        else if (pAbsolute)
        {
            m_colourType = CT_Absolute;
            m_absolute.Initialise(pAbsolute->GetArgN(0), engine);
        }
    }
}

void MHSetColour::PrintArgs(FILE *fd, int /*nTabs*/) const
{
    if (m_colourType == CT_Indexed)
    {
        fprintf(fd, ":NewColourIndex ");
        m_indexed.PrintMe(fd, 0);
    }
    else if (m_colourType == CT_Absolute)
    {
        fprintf(fd, ":NewAbsoluteColour ");
        m_absolute.PrintMe(fd, 0);
    }
}

void MHSetColour::Perform(MHEngine *engine)
{
    MHObjectRef target;
    m_target.GetValue(target, engine); // Get the item to set.
    MHColour newColour;

    switch (m_colourType)
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
            m_absolute.GetValue(colour, engine);
            newColour.m_colStr.Copy(colour);
            break;
        }
        case CT_Indexed:
            newColour.m_nColIndex = m_indexed.GetValue(engine);
    }

    SetColour(newColour, engine); // Set the colour of the appropriate portion of the visible
}
