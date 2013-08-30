/* Text.h

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


#if !defined(TEXT_H)
#define TEXT_H

#include "Visible.h"
#include "BaseActions.h"
// Dependencies
#include "Presentable.h"
#include "Ingredients.h"
#include "Root.h"
#include "BaseClasses.h"

class MHEngine;
class MHTextDisplay;

class MHText : public MHVisible  
{
  public:
    MHText();
    MHText(const MHText &ref);
    ~MHText();
    virtual const char *ClassName() { return "Text"; }
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *, int nTabs) const;

    virtual void Preparation(MHEngine *engine);
    virtual void ContentPreparation(MHEngine *engine);
    virtual void ContentArrived(const unsigned char *data, int length, MHEngine *engine);

    // Actions.
    // Extract the text from an object.  This can be used to load content from a file.
    virtual void GetTextData(MHRoot *pDestination, MHEngine *) { pDestination->SetVariableValue(m_Content); }
    virtual MHIngredient *Clone(MHEngine *) { return new MHText(*this); } // Create a clone of this ingredient.
    virtual void SetBackgroundColour(const MHColour &colour, MHEngine *engine);
    virtual void SetTextColour(const MHColour &colour, MHEngine *engine);
    virtual void SetFontAttributes(const MHOctetString &fontAttrs, MHEngine *engine);

    // Enumerated type lookup functions for the text parser.
    static int GetJustification(const char *str);
    static int GetLineOrientation(const char *str);
    static int GetStartCorner(const char *str);

    // Display function.
    virtual void Display(MHEngine *d);
    virtual QRegion GetOpaqueArea();

  protected:
    void Redraw();

    MHFontBody      m_OrigFont;
    MHOctetString   m_OriginalFontAttrs;
    MHColour        m_OriginalTextColour, m_OriginalBgColour;
    int             m_nCharSet;
    enum Justification { Start = 1, End, Centre, Justified } m_HorizJ, m_VertJ;
    enum LineOrientation { Vertical = 1, Horizontal } m_LineOrientation;
    enum StartCorner { UpperLeft = 1, UpperRight, LowerLeft, LowerRight } m_StartCorner;
    bool            m_fTextWrap;
    // Internal attributes.  The font colour, background colour and font attributes are
    // internal attributes in UK MHEG.
//  MHFontBody      m_Font;
    MHColour        m_textColour, m_bgColour;
    MHOctetString   m_fontAttrs;
    MHOctetString   m_Content; // The content as an octet string

    MHTextDisplay   *m_pDisplay; // Pointer to the display object.
    bool            m_fNeedsRedraw;

    // Create the Unicode content from the character input.
    void CreateContent(const unsigned char *p, int s, MHEngine *engine);
};

class MHHyperText : public MHText, public MHInteractible
{
  public:
    MHHyperText();
    virtual const char *ClassName() { return "HyperText"; }
    virtual ~MHHyperText();
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;

    // Implement the actions in the main inheritance line.
    virtual void SetInteractionStatus(bool newStatus, MHEngine *engine)
    { InteractSetInteractionStatus(newStatus, engine); }
    virtual bool GetInteractionStatus(void) { return InteractGetInteractionStatus(); }
    virtual void SetHighlightStatus(bool newStatus, MHEngine *engine)
    { InteractSetHighlightStatus(newStatus, engine); }
    virtual bool GetHighlightStatus(void) { return InteractGetHighlightStatus(); }
    virtual void Deactivation(MHEngine *engine) { InteractDeactivation(); }
};

// Get Text Data - get the data out of a text object.
class MHGetTextData: public MHActionObjectRef
{
  public:
    MHGetTextData(): MHActionObjectRef(":GetTextData")  {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, MHRoot *pArg) { pTarget->GetTextData(pArg, engine); }
};

// Actions added in UK MHEG profile.
class MHSetBackgroundColour: public MHSetColour {
  public:
    MHSetBackgroundColour(): MHSetColour(":SetBackgroundColour") { }
  protected:
    virtual void SetColour(const MHColour &colour, MHEngine *engine) { Target(engine)->SetBackgroundColour(colour, engine); }
};

class MHSetTextColour: public MHSetColour {
  public:
    MHSetTextColour(): MHSetColour(":SetTextColour") { }
  protected:
    virtual void SetColour(const MHColour &colour, MHEngine *engine) { Target(engine)->SetTextColour(colour, engine); }
};

class MHSetFontAttributes: public MHElemAction
{
  public:
    MHSetFontAttributes(): MHElemAction(":SetFontAttributes") {}
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void Perform(MHEngine *engine);
  protected:
    virtual void PrintArgs(FILE *fd, int /*nTabs*/) const { m_FontAttrs.PrintMe(fd, 0); }
    MHGenericOctetString m_FontAttrs; // New font attributes.
};



#endif
