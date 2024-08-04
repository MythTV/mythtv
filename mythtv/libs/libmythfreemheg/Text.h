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
    MHText() = default;
    MHText(const MHText &ref);
    ~MHText() override;
    const char *ClassName() override // MHRoot
        { return "Text"; }
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHVisible
    void PrintMe(FILE *fd, int nTabs) const override; // MHVisible

    void Preparation(MHEngine *engine) override; // MHVisible
    void ContentPreparation(MHEngine *engine) override; // MHIngredient
    void ContentArrived(const unsigned char *data, int length, MHEngine *engine) override; // MHIngredient

    // Actions.
    // Extract the text from an object.  This can be used to load content from a file.
    void GetTextData(MHRoot *pDestination, MHEngine */*engine*/) override // MHRoot
        { pDestination->SetVariableValue(m_content); }
    MHIngredient *Clone(MHEngine */*engine*/) override // MHRoot
        { return new MHText(*this); } // Create a clone of this ingredient.
    void SetBackgroundColour(const MHColour &colour, MHEngine *engine) override; // MHRoot
    void SetTextColour(const MHColour &colour, MHEngine *engine) override; // MHRoot
    void SetFontAttributes(const MHOctetString &fontAttrs, MHEngine *engine) override; // MHRoot

    // Enumerated type lookup functions for the text parser.
    static int GetJustification(const QString& str);
    static int GetLineOrientation(const QString& str);
    static int GetStartCorner(const QString& str);

    // Display function.
    void Display(MHEngine *engine) override; // MHVisible
    QRegion GetOpaqueArea() override; // MHVisible

  protected:
    void Redraw();

    MHFontBody      m_origFont;
    MHOctetString   m_originalFontAttrs;
    MHColour        m_originalTextColour, m_originalBgColour;
    int             m_nCharSet        {-1};

    enum Justification   : std::uint8_t { Start = 1, End, Centre, Justified };
    enum LineOrientation : std::uint8_t { Vertical = 1, Horizontal };
    enum StartCorner     : std::uint8_t { UpperLeft = 1, UpperRight, LowerLeft, LowerRight };
    Justification   m_horizJ          {Start};
    Justification   m_vertJ           {Start};
    LineOrientation m_lineOrientation {Horizontal};
    StartCorner     m_startCorner     {UpperLeft};
    bool            m_fTextWrap       {false};
    // Internal attributes.  The font colour, background colour and font attributes are
    // internal attributes in UK MHEG.
//  MHFontBody      m_font;
    MHColour        m_textColour, m_bgColour;
    MHOctetString   m_fontAttrs;
    MHOctetString   m_content; // The content as an octet string

    MHTextDisplay   *m_pDisplay       {nullptr}; // Pointer to the display object.
    bool            m_fNeedsRedraw    {false};

    // Create the Unicode content from the character input.
    void CreateContent(const unsigned char *p, int s, MHEngine *engine);
};

class MHHyperText : public MHText, public MHInteractible
{
  public:
    MHHyperText() : MHInteractible(this) {}
    const char *ClassName() override // MHText
        { return "HyperText"; }
    ~MHHyperText() override = default;
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHText
    void PrintMe(FILE *fd, int nTabs) const override; // MHText

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

// Get Text Data - get the data out of a text object.
class MHGetTextData: public MHActionObjectRef
{
  public:
    MHGetTextData(): MHActionObjectRef(":GetTextData")  {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, MHRoot *pArg) override // MHActionObjectRef
        { pTarget->GetTextData(pArg, engine); }
};

// Actions added in UK MHEG profile.
class MHSetBackgroundColour: public MHSetColour {
  public:
    MHSetBackgroundColour(): MHSetColour(":SetBackgroundColour") { }
  protected:
    void SetColour(const MHColour &colour, MHEngine *engine) override // MHSetColour
        { Target(engine)->SetBackgroundColour(colour, engine); }
};

class MHSetTextColour: public MHSetColour {
  public:
    MHSetTextColour(): MHSetColour(":SetTextColour") { }
  protected:
    void SetColour(const MHColour &colour, MHEngine *engine) override // MHSetColour
        { Target(engine)->SetTextColour(colour, engine); }
};

class MHSetFontAttributes: public MHElemAction
{
  public:
    MHSetFontAttributes(): MHElemAction(":SetFontAttributes") {}
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHElemAction
    void Perform(MHEngine *engine) override; // MHElemAction
  protected:
    void PrintArgs(FILE *fd, int /*nTabs*/) const override // MHElemAction
        { m_fontAttrs.PrintMe(fd, 0); }
    MHGenericOctetString m_fontAttrs; // New font attributes.
};



#endif
