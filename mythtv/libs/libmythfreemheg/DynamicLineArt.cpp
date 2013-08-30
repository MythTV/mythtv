/* DynamicLineArt.cpp

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

#include "DynamicLineArt.h"
#include "Visible.h"
#include "Presentable.h"
#include "Ingredients.h"
#include "Root.h"
#include "BaseClasses.h"
#include "ParseNode.h"
#include "ASN1Codes.h"
#include "Engine.h"
#include "freemheg.h"

MHDynamicLineArt::MHDynamicLineArt()
{
    m_picture = NULL;
}

MHDynamicLineArt::~MHDynamicLineArt()
{
    delete(m_picture);
}


void MHDynamicLineArt::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHLineArt::Initialise(p, engine);
    m_picture =
        engine->GetContext()->CreateDynamicLineArt(m_fBorderedBBox, GetColour(m_OrigLineColour), GetColour(m_OrigFillColour));
}

void MHDynamicLineArt::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:DynamicLineArt ");
    MHLineArt::PrintMe(fd, nTabs + 1);
    PrintTabs(fd, nTabs);
    fprintf(fd, "}\n");
}

void MHDynamicLineArt::Preparation(MHEngine *engine)
{
    MHLineArt::Preparation(engine);
    m_picture->SetSize(m_nBoxWidth, m_nBoxHeight);
    m_picture->SetLineSize(m_nLineWidth);
    m_picture->SetLineColour(GetColour(m_LineColour));
    m_picture->SetFillColour(GetColour(m_FillColour));
}

void MHDynamicLineArt::Display(MHEngine *)
{
    m_picture->Draw(m_nPosX, m_nPosY);
}

// Get the opaque area.  This is only opaque if the background is opaque.
QRegion MHDynamicLineArt::GetOpaqueArea()
{
    if ((GetColour(m_OrigFillColour)).alpha() == 255)
    {
        return GetVisibleArea();
    }
    else
    {
        return QRegion();
    }
}

// Reset the picture.
void MHDynamicLineArt::Clear()
{
    m_picture->Clear();
}

// As well as the general action this also clears the drawing.
void MHDynamicLineArt::SetBoxSize(int nWidth, int nHeight, MHEngine *engine)
{
    MHLineArt::SetBoxSize(nWidth, nHeight, engine);
    m_picture->SetSize(nWidth, nHeight);
    Clear();
}

// SetPosition, BringToFront, SendToBack, PutBefore and PutBehind were defined in the original
// MHEG standard to clear the drawing.  This was removed in the MHEG Corrigendum.

void MHDynamicLineArt::SetFillColour(const MHColour &colour, MHEngine *)
{
    m_FillColour.Copy(colour);
    m_picture->SetFillColour(GetColour(m_FillColour));
}

void MHDynamicLineArt::SetLineColour(const MHColour &colour, MHEngine *)
{
    m_LineColour.Copy(colour);
    m_picture->SetLineColour(GetColour(m_LineColour));
}

void MHDynamicLineArt::SetLineWidth(int nWidth, MHEngine *)
{
    m_nLineWidth = nWidth;
    m_picture->SetLineSize(m_nLineWidth);
}

// We don't actually use this at the moment.
void MHDynamicLineArt::SetLineStyle(int nStyle, MHEngine *)
{
    m_LineStyle = nStyle;
}


void MHDynamicLineArt::GetLineColour(MHRoot *pResult)
{
    // Returns the palette index as an integer if it is an index or the colour as a string if not.
    if (m_LineColour.m_nColIndex >= 0)
    {
        pResult->SetVariableValue(m_LineColour.m_nColIndex);
    }
    else
    {
        pResult->SetVariableValue(m_LineColour.m_ColStr);
    }
}

void MHDynamicLineArt::GetFillColour(MHRoot *pResult)
{
    if (m_FillColour.m_nColIndex >= 0)
    {
        pResult->SetVariableValue(m_FillColour.m_nColIndex);
    }
    else
    {
        pResult->SetVariableValue(m_FillColour.m_ColStr);
    }
}

void MHDynamicLineArt::DrawLine(int x1, int y1, int x2, int y2, MHEngine *engine)
{
    m_picture->DrawLine(x1, y1, x2, y2);
    engine->Redraw(GetVisibleArea());
}

void MHDynamicLineArt::DrawRectangle(int x1, int y1, int x2, int y2, MHEngine *engine)
{
    m_picture->DrawBorderedRectangle(x1, y1, x2 - x1, y2 - y1);
    engine->Redraw(GetVisibleArea());
}

void MHDynamicLineArt::DrawOval(int x, int y, int width, int height, MHEngine *engine)
{
    m_picture->DrawOval(x, y, width, height);
    engine->Redraw(GetVisibleArea());
}

void MHDynamicLineArt::DrawArcSector(bool fIsSector, int x, int y, int width, int height,
                                     int start, int arc, MHEngine *engine)
{
    m_picture->DrawArcSector(x, y, width, height, start, arc, fIsSector);
    engine->Redraw(GetVisibleArea());
}

void MHDynamicLineArt::DrawPoly(bool fIsPolygon, int nPoints, const int xArray[], const int yArray[], MHEngine *engine)
{
    m_picture->DrawPoly(fIsPolygon, nPoints, xArray, yArray);
    engine->Redraw(GetVisibleArea());
}

// Actions.

// Polygons and Polylines have the same arguments: a list of points.
void MHDrawPoly::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHElemAction::Initialise(p, engine);
    MHParseNode *args = p->GetArgN(1);

    for (int i = 0; i < args->GetSeqCount(); i++)
    {
        MHPointArg *pPoint = new MHPointArg;
        m_Points.Append(pPoint);
        pPoint->Initialise(args->GetSeqN(i), engine);
    }
}

void MHDrawPoly::Perform(MHEngine *engine)
{
    int nPoints = m_Points.Size();
    int *xArray = new int[nPoints];
    int *yArray = new int[nPoints];

    for (int i = 0; i < nPoints; i++)
    {
        MHPointArg *pPoint = m_Points[i];
        xArray[i] = pPoint->x.GetValue(engine);
        yArray[i] = pPoint->y.GetValue(engine);
    }

    Target(engine)->DrawPoly(m_fIsPolygon, nPoints, xArray, yArray, engine);
    delete[](xArray);
    delete[](yArray);
}

void MHDrawPoly::PrintArgs(FILE *fd, int /*nTabs*/) const
{
    fprintf(fd, " ( ");

    for (int i = 0; i < m_Points.Size(); i++)
    {
        m_Points[i]->PrintMe(fd, 0);
    }

    fprintf(fd, " )\n");
}
