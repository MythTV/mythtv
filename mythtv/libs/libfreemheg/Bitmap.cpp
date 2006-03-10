/* Bitmap.cpp

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

#include "Bitmap.h"
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

#include "inttypes.h"

/*
UK MHEG content hook values: 2 => MPEG I-frame, 4 => PNG bitmap
*/

MHBitmap::MHBitmap()
{
    m_fTiling = false;
    m_nOrigTransparency = 0;
    m_nXDecodeOffset = 0;
    m_nYDecodeOffset = 0;
    m_pContent = NULL;
}

MHBitmap::MHBitmap(const MHBitmap &ref): MHVisible(ref)
{
    m_fTiling = ref.m_fTiling;
    m_nOrigTransparency = ref.m_nOrigTransparency;
    m_nXDecodeOffset = 0;
    m_nYDecodeOffset = 0;
    m_pContent = NULL;
}

MHBitmap::~MHBitmap()
{
    delete(m_pContent);
}

void MHBitmap::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHVisible::Initialise(p, engine);
    // Tiling - optional
    MHParseNode *pTiling = p->GetNamedArg(C_TILING);
    if (pTiling) m_fTiling = pTiling->GetArgN(0)->GetBoolValue();
    // Transparency - optional
    MHParseNode *pTransparency = p->GetNamedArg(C_ORIGINAL_TRANSPARENCY);
    if (pTransparency) m_nOrigTransparency = pTransparency->GetArgN(0)->GetIntValue();
    m_pContent = engine->GetContext()->CreateBitmap(m_fTiling);
}

void MHBitmap::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs); fprintf(fd, "{:Bitmap ");
    MHVisible::PrintMe(fd, nTabs+1);
    if (m_fTiling) { PrintTabs(fd, nTabs+1); fprintf(fd, ":Tiling true\n"); }
    if (m_nOrigTransparency != 0) { PrintTabs(fd, nTabs+1); fprintf(fd, ":OrigTransparency %d\n", m_nOrigTransparency); }
    PrintTabs(fd, nTabs); fprintf(fd, "}\n");
}

void MHBitmap::Preparation(MHEngine *engine)
{
    if (m_fAvailable) return;
    m_nTransparency = m_nOrigTransparency; // Transparency isn't used in UK MHEG
    MHVisible::Preparation(engine);
}

// 
void MHBitmap::ContentPreparation(MHEngine *engine)
{
    MHVisible::ContentPreparation(engine);
    ASSERT(m_ContentType != IN_NoContent);
    ASSERT(m_ContentType != IN_IncludedContent); // We can't handle included content at the moment.
//  if (m_ContentType == IN_IncludedContent) CreateContent(m_IncludedContent.Bytes(), m_IncludedContent.Size());
}

// Decode the content.
void MHBitmap::ContentArrived(const unsigned char *data, int length, MHEngine *engine)
{
    QRegion updateArea = GetVisibleArea(); // If there's any content already we have to redraw it.
    if (! m_pContent) return; // Shouldn't happen.

    int nCHook = m_nContentHook;
    if (nCHook == 0) nCHook = engine->GetDefaultBitmapCHook();

    // TODO: What if we can't convert it?
    if (nCHook == 4) { // PNG.
        m_pContent->CreateFromPNG(data, length);
    }
    else if (nCHook == 2) { // MPEG I-frame.
        m_pContent->CreateFromMPEG(data, length);
    }

    else MHERROR(QString("Unknown bitmap content hook %1").arg(nCHook));

    updateArea += GetVisibleArea(); // Redraw this bitmap.
    engine->Redraw(updateArea); // Mark for redrawing

    // Now signal that the content is available.
    engine->EventTriggered(this, EventContentAvailable);
}



// Set the transparency.
void MHBitmap::SetTransparency(int nTransPerCent, MHEngine *)
{
    // The object transparency isn't actually used in UK MHEG.
    // We want a value between 0 and 255
    ASSERT(nTransPerCent >= 0 && nTransPerCent <= 100); // This should really be a check.
    m_nTransparency = ((nTransPerCent * 255) + 50) / 100;
}

// Scale a bitmap.  In UK MHEG this is only used for MPEG I-frames, not PNG.
void MHBitmap::ScaleBitmap(int xScale, int yScale, MHEngine *engine)
{
    QRegion updateArea = GetVisibleArea(); // If there's any content already we have to redraw it.
    m_pContent->ScaleImage(xScale, yScale);
    updateArea += GetVisibleArea(); // Redraw this bitmap.
    engine->Redraw(updateArea); // Mark for redrawing
}

// Added action in UK MHEG.
void MHBitmap::SetBitmapDecodeOffset(int newXOffset, int newYOffset, MHEngine *engine)
{
    QRegion updateArea = GetVisibleArea(); // Redraw the area before the offset
    m_nXDecodeOffset = newXOffset;
    m_nYDecodeOffset = newYOffset;
    updateArea += GetVisibleArea(); // Redraw this bitmap.
    engine->Redraw(updateArea); // Mark for redrawing
}

// Added action in UK MHEG.
void MHBitmap::GetBitmapDecodeOffset(MHRoot *pXOffset, MHRoot *pYOffset)
{
    pXOffset->SetVariableValue(m_nXDecodeOffset);
    pYOffset->SetVariableValue(m_nYDecodeOffset);
}

void MHBitmap::Display(MHEngine *)
{
    if (! m_fRunning || ! m_pContent || m_nBoxWidth == 0 || m_nBoxHeight == 0) return; // Can't draw zero sized boxes.

    m_pContent->Draw(m_nPosX+m_nXDecodeOffset, m_nPosY+m_nYDecodeOffset,
        QRect(m_nPosX, m_nPosY, m_nBoxWidth, m_nBoxHeight), m_fTiling);
}

// Return the region drawn by the bitmap.
QRegion MHBitmap::GetVisibleArea()
{
    if (! m_fRunning || m_pContent == NULL) return QRegion();
    // The visible area is the intersection of the containing box with the, possibly offset,
    // bitmap.
    QSize imageSize = m_pContent->GetSize();
    QRegion boxRegion = QRegion(m_nPosX, m_nPosY, m_nBoxWidth, m_nBoxHeight);
    QRegion bitmapRegion = QRegion(m_nPosX+m_nXDecodeOffset, m_nPosY+m_nYDecodeOffset,
                                imageSize.width(), imageSize.height());
    return boxRegion & bitmapRegion;
}

// Return the area actually obscured by the bitmap.
QRegion MHBitmap::GetOpaqueArea()
{
    // The area is empty unless the bitmap is opaque.
    if (! m_fRunning || m_pContent == NULL || ! m_pContent->IsOpaque()) return QRegion();
    else return GetVisibleArea();
}

