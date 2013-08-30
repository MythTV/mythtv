/* Bitmap.h

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


#if !defined(BITMAP_H)
#define BITMAP_H

#include "Visible.h"
#include "BaseActions.h"
// Dependencies
#include "Presentable.h"
#include "Ingredients.h"
#include "Root.h"
#include "BaseClasses.h"

class MHBitmapDisplay;

class MHBitmap : public MHVisible  
{
  public:
    MHBitmap();
    MHBitmap(const MHBitmap &ref);
    virtual const char *ClassName() { return "Bitmap"; }
    virtual ~MHBitmap();
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;

    virtual void Preparation(MHEngine *engine);
    virtual void ContentPreparation(MHEngine *engine);
    virtual void ContentArrived(const unsigned char *data, int length, MHEngine *engine);

    // Actions.
    virtual void SetTransparency(int nTransPerCent, MHEngine *);
    virtual void ScaleBitmap(int xScale, int yScale, MHEngine *engine);
    virtual void SetBitmapDecodeOffset(int newXOffset, int newYOffset, MHEngine *engine);
    virtual void GetBitmapDecodeOffset(MHRoot *pXOffset, MHRoot *pYOffset);
    virtual MHIngredient *Clone(MHEngine *) { return new MHBitmap(*this); } // Create a clone of this ingredient.

    // Display function.
    virtual void Display(MHEngine *d);
    virtual QRegion GetVisibleArea();
    virtual QRegion GetOpaqueArea();

  protected:
    bool    m_fTiling;
    int     m_nOrigTransparency;

    // Internal attributes
    int     m_nTransparency;
    // Added in UK MHEG
    int     m_nXDecodeOffset, m_nYDecodeOffset;

    MHBitmapDisplay  *m_pContent; // Pointer to current image if any.

    void CreateContent(const unsigned char *p, int s, MHEngine *engine);
};

// Actions.
class MHSetTransparency: public MHActionInt {
  public:
    MHSetTransparency(): MHActionInt(":SetTransparency") {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg) { pTarget->SetTransparency(nArg, engine); }
};

class MHScaleBitmap: public MHActionIntInt {
  public:
    MHScaleBitmap(): MHActionIntInt(":ScaleBitmap") {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg1, int nArg2) { pTarget->ScaleBitmap(nArg1, nArg2, engine); }
};

// Actions added in the UK MHEG profile.
class MHSetBitmapDecodeOffset: public MHActionIntInt
{
  public:
    MHSetBitmapDecodeOffset(): MHActionIntInt(":SetBitmapDecodeOffset") {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg1, int nArg2)
    { pTarget->SetBitmapDecodeOffset(nArg1, nArg2, engine); }
};

class MHGetBitmapDecodeOffset: public MHActionObjectRef2
{
  public:
    MHGetBitmapDecodeOffset(): MHActionObjectRef2(":GetBitmapDecodeOffset")  {}
    virtual void CallAction(MHEngine *, MHRoot *pTarget, MHRoot *pArg1, MHRoot *pArg2) { pTarget->GetBitmapDecodeOffset(pArg1, pArg2); }
};


#endif
