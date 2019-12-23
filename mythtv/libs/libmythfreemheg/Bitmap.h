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
    MHBitmap() = default;
    MHBitmap(const MHBitmap &ref)
        : MHVisible(ref), m_fTiling(ref.m_fTiling),
          m_nOrigTransparency(ref.m_nOrigTransparency) {}
    const char *ClassName() override // MHRoot
        { return "Bitmap"; }
    ~MHBitmap() override;
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHVisible
    void PrintMe(FILE *fd, int nTabs) const override; // MHVisible

    void Preparation(MHEngine *engine) override; // MHVisible
    void ContentPreparation(MHEngine *engine) override; // MHIngredient
    void ContentArrived(const unsigned char *data, int length, MHEngine *engine) override; // MHIngredient

    // Actions.
    void SetTransparency(int nTransPerCent, MHEngine *engine) override; // MHRoot
    void ScaleBitmap(int xScale, int yScale, MHEngine *engine) override; // MHRoot
    void SetBitmapDecodeOffset(int newXOffset, int newYOffset, MHEngine *engine) override; // MHRoot
    void GetBitmapDecodeOffset(MHRoot *pXOffset, MHRoot *pYOffset) override; // MHRoot
    MHIngredient *Clone(MHEngine */*engine*/) override // MHRoot
        { return new MHBitmap(*this); } // Create a clone of this ingredient.

    // Display function.
    void Display(MHEngine *d) override; // MHVisible
    QRegion GetVisibleArea() override; // MHVisible
    QRegion GetOpaqueArea() override; // MHVisible

  protected:
    bool    m_fTiling           {false};
    int     m_nOrigTransparency {0};

    // Internal attributes
    int     m_nTransparency     {0};
    // Added in UK MHEG
    int     m_nXDecodeOffset    {0};
    int     m_nYDecodeOffset    {0};

    MHBitmapDisplay  *m_pContent {nullptr}; // Pointer to current image if any.

    void CreateContent(const unsigned char *data, int length, MHEngine *engine);
};

// Actions.
class MHSetTransparency: public MHActionInt {
  public:
    MHSetTransparency(): MHActionInt(":SetTransparency") {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg) override // MHActionInt
        { pTarget->SetTransparency(nArg, engine); }
};

class MHScaleBitmap: public MHActionIntInt {
  public:
    MHScaleBitmap(): MHActionIntInt(":ScaleBitmap") {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg1, int nArg2) override // MHActionIntInt
        { pTarget->ScaleBitmap(nArg1, nArg2, engine); }
};

// Actions added in the UK MHEG profile.
class MHSetBitmapDecodeOffset: public MHActionIntInt
{
  public:
    MHSetBitmapDecodeOffset(): MHActionIntInt(":SetBitmapDecodeOffset") {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg1, int nArg2) override // MHActionIntInt
        { pTarget->SetBitmapDecodeOffset(nArg1, nArg2, engine); }
};

class MHGetBitmapDecodeOffset: public MHActionObjectRef2
{
  public:
    MHGetBitmapDecodeOffset(): MHActionObjectRef2(":GetBitmapDecodeOffset")  {}
    void CallAction(MHEngine */*engine*/, MHRoot *pTarget, MHRoot *pArg1, MHRoot *pArg2) override // MHActionObjectRef2
        { pTarget->GetBitmapDecodeOffset(pArg1, pArg2); }
};


#endif
