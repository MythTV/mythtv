/* Text.cpp

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
#include "compat.h"

#include "Text.h"
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

MHText::MHText()
{
    m_nCharSet = -1;
    m_HorizJ = m_VertJ = Start;
    m_LineOrientation = Horizontal;
    m_StartCorner = UpperLeft;
    m_fTextWrap = false;
    m_pDisplay = NULL;
    m_fNeedsRedraw = false;
}

MHText::MHText(const MHText &ref): MHVisible(ref) // Copy constructor for cloning.
{
    m_OrigFont.Copy(ref.m_OrigFont);
    m_OriginalFontAttrs.Copy(ref.m_OriginalFontAttrs);
    m_OriginalTextColour.Copy(ref.m_OriginalTextColour);
    m_OriginalBgColour.Copy(ref.m_OriginalBgColour);
    m_nCharSet = ref.m_nCharSet;
    m_HorizJ = ref.m_HorizJ;
    m_VertJ = ref.m_VertJ;
    m_LineOrientation = ref.m_LineOrientation;
    m_StartCorner = ref.m_StartCorner;
    m_fTextWrap = ref.m_fTextWrap;
    m_pDisplay = NULL;
    m_fNeedsRedraw = ref.m_fNeedsRedraw;
}

MHText::~MHText()
{
    delete(m_pDisplay);
}


void MHText::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHVisible::Initialise(p, engine);
    // Font and attributes.
    MHParseNode *pFontBody = p->GetNamedArg(C_ORIGINAL_FONT);

    if (pFontBody)
    {
        m_OrigFont.Initialise(pFontBody->GetArgN(0), engine);
    }

    MHParseNode *pFontAttrs = p->GetNamedArg(C_FONT_ATTRIBUTES);

    if (pFontAttrs)
    {
        pFontAttrs->GetArgN(0)->GetStringValue(m_OriginalFontAttrs);
    }

    // Colours
    MHParseNode *pTextColour = p->GetNamedArg(C_TEXT_COLOUR);

    if (pTextColour)
    {
        m_OriginalTextColour.Initialise(pTextColour->GetArgN(0), engine);
    }

    MHParseNode *pBGColour = p->GetNamedArg(C_BACKGROUND_COLOUR);

    if (pBGColour)
    {
        m_OriginalBgColour.Initialise(pBGColour->GetArgN(0), engine);
    }

    // Character set
    MHParseNode *pChset = p->GetNamedArg(C_CHARACTER_SET);

    if (pChset)
    {
        m_nCharSet = pChset->GetArgN(0)->GetIntValue();
    }

    // Justification
    MHParseNode *pHJust = p->GetNamedArg(C_HORIZONTAL_JUSTIFICATION);

    if (pHJust)
    {
        m_HorizJ = (enum Justification)pHJust->GetArgN(0)->GetEnumValue();
    }

    MHParseNode *pVJust = p->GetNamedArg(C_VERTICAL_JUSTIFICATION);

    if (pVJust)
    {
        m_VertJ = (enum Justification)pVJust->GetArgN(0)->GetEnumValue();
    }

    // Line orientation
    MHParseNode *pLineO = p->GetNamedArg(C_LINE_ORIENTATION);

    if (pLineO)
    {
        m_LineOrientation = (enum LineOrientation)pLineO->GetArgN(0)->GetEnumValue();
    }

    // Start corner
    MHParseNode *pStartC = p->GetNamedArg(C_START_CORNER);

    if (pStartC)
    {
        m_StartCorner = (enum StartCorner)pStartC->GetArgN(0)->GetEnumValue();
    }

    // Text wrapping
    MHParseNode *pTextWrap = p->GetNamedArg(C_TEXT_WRAPPING);

    if (pTextWrap)
    {
        m_fTextWrap = pTextWrap->GetArgN(0)->GetBoolValue();
    }

    m_pDisplay = engine->GetContext()->CreateText();
    m_fNeedsRedraw = true;
}

static const char * const rchJustification[] =
{
    "start", // 1
    "end",
    "centre",
    "justified" // 4
};

// Look up the Justification. Returns zero if it doesn't match.  Used in the text parser only.
int MHText::GetJustification(const char *str)
{
    for (int i = 0; i < (int)(sizeof(rchJustification) / sizeof(rchJustification[0])); i++)
    {
        if (strcasecmp(str, rchJustification[i]) == 0)
        {
            return (i + 1);    // Numbered from 1
        }
    }

    return 0;
}

static const char * const rchlineOrientation[] =
{
    "vertical", // 1
    "horizontal"
};

int MHText::GetLineOrientation(const char *str)
{
    for (int i = 0; i < (int)(sizeof(rchlineOrientation) / sizeof(rchlineOrientation[0])); i++)
    {
        if (strcasecmp(str, rchlineOrientation[i]) == 0)
        {
            return (i + 1);
        }
    }

    return 0;
}

static const char * const rchStartCorner[] =
{
    "upper-left", // 1
    "upper-right",
    "lower-left",
    "lower-right" // 4
};

int MHText::GetStartCorner(const char *str)
{
    for (int i = 0; i < (int)(sizeof(rchStartCorner) / sizeof(rchStartCorner[0])); i++)
    {
        if (strcasecmp(str, rchStartCorner[i]) == 0)
        {
            return (i + 1);
        }
    }

    return 0;
}

void MHText::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:Text ");
    MHVisible::PrintMe(fd, nTabs + 1);

    if (m_OrigFont.IsSet())
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":OrigFont ");
        m_OrigFont.PrintMe(fd, nTabs + 1);
        fprintf(fd, "\n");
    }

    if (m_OriginalFontAttrs.Size() > 0)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":FontAttributes ");
        m_OriginalFontAttrs.PrintMe(fd, nTabs + 1);
        fprintf(fd, "\n");
    }

    if (m_OriginalTextColour.IsSet())
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":TextColour ");
        m_OriginalTextColour.PrintMe(fd, nTabs + 1);
        fprintf(fd, "\n");
    }

    if (m_OriginalBgColour.IsSet())
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":BackgroundColour ");
        m_OriginalBgColour.PrintMe(fd, nTabs + 1);
        fprintf(fd, "\n");
    }

    if (m_nCharSet >= 0)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":CharacterSet %d\n", m_nCharSet);
    }

    if (m_HorizJ != Start)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":HJustification %s\n", rchJustification[m_HorizJ-1]);
    }

    if (m_VertJ != Start)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":VJustification %s\n", rchJustification[m_VertJ-1]);
    }

    if (m_LineOrientation != Horizontal)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":LineOrientation %s\n", rchlineOrientation[m_LineOrientation-1]);
    }

    if (m_StartCorner != UpperLeft)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":StartCorner %s\n", rchStartCorner[m_StartCorner-1]);
    }

    if (m_fTextWrap)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":TextWrapping true\n");
    }

    PrintTabs(fd, nTabs);
    fprintf(fd, "}\n");
}

void MHText::Preparation(MHEngine *engine)
{
    if (m_fAvailable)
    {
        return;
    }

    // Set the colours and font up from the originals if specified otherwise use the application defaults.
    //  if (m_OrigFont.IsSet()) m_Font.Copy(m_OrigFont);
    //  else m_Font.Copy(engine->m_DefaultFont);
    if (m_OriginalTextColour.IsSet())
    {
        m_textColour.Copy(m_OriginalTextColour);
    }
    else
    {
        engine->GetDefaultTextColour(m_textColour);
    }

    if (m_OriginalBgColour.IsSet())
    {
        m_bgColour.Copy(m_OriginalBgColour);
    }
    else
    {
        engine->GetDefaultBGColour(m_bgColour);
    }

    if (m_OriginalFontAttrs.Size() > 0)
    {
        m_fontAttrs.Copy(m_OriginalFontAttrs);
    }
    else
    {
        engine->GetDefaultFontAttrs(m_fontAttrs);
    }

    MHVisible::Preparation(engine);

    if (m_pDisplay == NULL)
    {
	    m_pDisplay = engine->GetContext()->CreateText();
    }

    m_pDisplay->SetSize(m_nBoxWidth, m_nBoxHeight);
    m_fNeedsRedraw = true;
}

// Content preparation.  If it's included we can set up the content.
void MHText::ContentPreparation(MHEngine *engine)
{
    MHVisible::ContentPreparation(engine);

    if (m_ContentType == IN_NoContent)
    {
        MHERROR("Text object must have content");
    }

    if (m_ContentType == IN_IncludedContent)
    {
        CreateContent(m_IncludedContent.Bytes(), m_IncludedContent.Size(), engine);
    }
}

// Called when external content is available.
void MHText::ContentArrived(const unsigned char *data, int length, MHEngine *engine)
{
    CreateContent(data, length, engine);
    // Now signal that the content is available.
    engine->EventTriggered(this, EventContentAvailable);
    m_fNeedsRedraw = true;
}

//
void MHText::CreateContent(const unsigned char *p, int s, MHEngine *engine)
{
    m_Content.Copy(MHOctetString((const char *)p, s));
    engine->Redraw(GetVisibleArea()); // Have to redraw if the content has changed.
    m_fNeedsRedraw = true;
    //  fprintf(fd, "Text content is now "); m_Content.PrintMe(0); fprintf(fd, "\n");
}

void MHText::SetTextColour(const MHColour &colour, MHEngine *engine)
{
    m_textColour.Copy(colour);
    m_fNeedsRedraw = true;
    engine->Redraw(GetVisibleArea());
}

void MHText::SetBackgroundColour(const MHColour &colour, MHEngine *engine)
{
    m_bgColour.Copy(colour);
    // Setting the background colour doesn't affect the text image but we have to
    // redraw it onto the display.
    engine->Redraw(GetVisibleArea());
}

void MHText::SetFontAttributes(const MHOctetString &fontAttrs, MHEngine *engine)
{
    m_fontAttrs.Copy(fontAttrs);
    m_fNeedsRedraw = true;
    engine->Redraw(GetVisibleArea());
}

// UK MHEG. Interpret the font attributes.
static void InterpretAttributes(const MHOctetString &attrs, int &style, int &size, int &lineSpace, int &letterSpace)
{
    // Set the defaults.
    style = 0;
    size = 0x18;
    lineSpace = 0x18;
    letterSpace = 0;

    if (attrs.Size() == 5)   // Short form.
    {
        style = attrs.GetAt(0); // Only the bottom nibble is significant.
        size = attrs.GetAt(1);
        lineSpace = attrs.GetAt(2);
        // Is this big-endian or little-endian?  Assume big.
        letterSpace = attrs.GetAt(3) * 256 + attrs.GetAt(4);

        if (letterSpace > 32767)
        {
            letterSpace -= 65536;    // Signed.
        }
    }
    else   // Textual form.
    {
        const unsigned char *str = attrs.Bytes();
        char *p = (char *)str;
        char *q = strchr(p, '.'); // Find the terminating dot

        if (q != NULL)   // plain, italic etc.
        {
            if (q - p == 6 && strncmp(p, "italic", 6) == 0)
            {
                style = 1;
            }
            else if (q - p == 4 && strncmp(p, "bold", 4) == 0)
            {
                style = 2;
            }
            else if (q - p == 11 && strncmp(p, "bold-italic", 11) == 0)
            {
                style = 3;
            }

            // else it's plain.
            p = q + 1;
            q = strchr(p, '.'); // Find the next dot.
        }

        if (q != NULL)   // Size
        {
            size = atoi(p);

            if (size == 0)
            {
                size = 0x18;
            }

            p = q + 1;
            q = strchr(p, '.'); // Find the next dot.
        }

        if (q != NULL)   // lineSpacing
        {
            lineSpace = atoi(p);

            if (lineSpace == 0)
            {
                lineSpace = 0x18;
            }

            p = q + 1;
            q = strchr(p, '.'); // Find the next dot.
        }

        if (q != NULL)   // letter spacing.  May be zero or negative
        {
            letterSpace = atoi(p);
        }
    }
}

// We break the text up into pieces of line segment.
class MHTextItem
{
  public:
    MHTextItem();
    MHOctetString m_Text; // UTF-8 text
    QString m_Unicode; // Unicode text
    int m_nUnicode; // Number of characters in it
    int m_Width; // Size of this block
    MHRgba m_Colour; // Colour of the text
    int m_nTabCount; // Number of tabs immediately before this (usually zero)

    // Generate new items inheriting properties from the previous
    MHTextItem *NewItem();
};

MHTextItem::MHTextItem()
{
    m_nUnicode = 0;
    m_Width = 0; // Size of this block
    m_Colour = MHRgba(0, 0, 0, 255);
    m_nTabCount = 0;
}

MHTextItem *MHTextItem::NewItem()
{
    MHTextItem *pItem = new MHTextItem;
    pItem->m_Colour = m_Colour;
    return pItem;
}

// A line consists of one or more sequences of text items.
class MHTextLine
{
  public:
    MHTextLine(): m_nLineWidth(0), m_nLineHeight(0), m_nDescent(0) {}
    ~MHTextLine();
    MHSequence <MHTextItem *> m_Items;
    int m_nLineWidth;
    int m_nLineHeight;
    int m_nDescent;
};

MHTextLine::~MHTextLine()
{
    for (int i = 0; i < m_Items.Size(); i++)
    {
        delete(m_Items.GetAt(i));
    }
}

// Tabs are set every 56 pixels
// = (FONT_WIDTHRES * 56)/ 72 points - see libmythtv/mhi.cpp
// = (54 * 56)/72 = 42
#define TABSTOP 42 // pts
static inline int Tabs(int nXpos, int nTabCount)
{
    int nNextTab = nXpos;
    if (nTabCount-- > 0)
    {
        nNextTab += TABSTOP - nXpos % TABSTOP;
        nNextTab += nTabCount * TABSTOP;
    }
    return nNextTab; 
}

// I attempted to use QSimpleRichText but that does not give sufficient fine control over
// the layout.  UK MHEG specifies the use of the Tiresias font and broadcasters appear to
// assume that all MHEG applications will lay the text out in the same way.

// Recreate the image.
void MHText::Redraw()
{
    if (! m_fRunning || !m_pDisplay)
    {
        return;
    }

    if (m_nBoxWidth == 0 || m_nBoxHeight == 0)
    {
        return;    // Can't draw zero sized boxes.
    }

    m_pDisplay->SetSize(m_nBoxWidth, m_nBoxHeight);
    m_pDisplay->Clear();

    MHRgba textColour = GetColour(m_textColour);
    // Process any escapes in the text and construct the text arrays.
    MHSequence <MHTextLine *> theText;
    // Set up the first item on the first line.
    MHTextItem *pCurrItem = new MHTextItem;
    MHTextLine *pCurrLine = new MHTextLine;
    pCurrLine->m_Items.Append(pCurrItem);
    theText.Append(pCurrLine);
    MHStack <MHRgba> m_ColourStack; // Stack to handle nested colour codes.
    m_ColourStack.Push(textColour);
    pCurrItem->m_Colour = textColour;

//  FILE *fd=stdout; fprintf(fd, "Redraw Text "); m_Content.PrintMe(fd, 0); fprintf(fd, "\n");
    int i = 0;

    while (i < m_Content.Size())
    {
        unsigned char ch = m_Content.GetAt(i++);

        if (ch == 0x09) // Tab - start a new item if we have any text in the existing one.
        {
            if (pCurrItem->m_Text.Size() != 0)
            {
                pCurrItem = pCurrItem->NewItem();
                pCurrLine->m_Items.Append(pCurrItem);
            }
            if (m_HorizJ == Start)
                pCurrItem->m_nTabCount++;
        }

        else if (ch == 0x0d)    // CR - line break.
        {
            // TODO: Two CRs next to one another are treated as </P> rather than <BR><BR>
            // This should also include the sequence CRLFCRLF.
            pCurrLine = new MHTextLine;
            theText.Append(pCurrLine);
            pCurrItem = pCurrItem->NewItem();
            pCurrLine->m_Items.Append(pCurrItem);
        }

        else if (ch == 0x1b)   // Escape - special codes.
        {
            if (i == m_Content.Size())
            {
                break;
            }

            unsigned char code = m_Content.GetAt(i);
            // The only codes we are interested in are the start and end of colour.
            // TODO: We may also need "bold" and some hypertext colours.

            if (code >= 0x40 && code <= 0x5e)   // Start code
            {
                // Start codes are followed by a parameter count and a number of parameter bytes.
                if (++i == m_Content.Size())
                {
                    break;
                }

                unsigned char paramCount = m_Content.GetAt(i);
                i++;

                if (code == 0x43 && paramCount == 4 && i + paramCount <= m_Content.Size())
                {
                    // Start of colour.
                    if (pCurrItem->m_Text.Size() != 0)
                    {
                        pCurrItem = pCurrItem->NewItem();
                        pCurrLine->m_Items.Append(pCurrItem);
                    }

                    pCurrItem->m_Colour = MHRgba(m_Content.GetAt(i), m_Content.GetAt(i + 1),
                                                 m_Content.GetAt(i + 2), 255 - m_Content.GetAt(i + 3));
                    // Push this colour onto the colour stack.
                    m_ColourStack.Push(pCurrItem->m_Colour);
                }
                else
                {
                    MHLOG(MHLogWarning, QString("Unknown text escape code 0x%1").arg(code, 2, 16));
                }

                i += paramCount; // Skip the parameters
            }
            else if (code >= 0x60 && code <= 0x7e)   // End code.
            {
                i++;

                if (code == 0x63)
                {
                    if (m_ColourStack.Size() > 1)
                    {
                        m_ColourStack.Pop();

                        // Start a new item since we're using a new colour.
                        if (pCurrItem->m_Text.Size() != 0)
                        {
                            pCurrItem = pCurrItem->NewItem();
                            pCurrLine->m_Items.Append(pCurrItem);
                        }

                        // Set the subsequent text in the colour we're using now.
                        pCurrItem->m_Colour = m_ColourStack.Top();
                    }
                }
                else MHLOG(MHLogWarning, QString("Unknown text escape code 0x%1").arg(code,2,16));
            }
            else MHLOG(MHLogWarning, QString("Unknown text escape code 0x%1").arg(code,2,16));
        }

        else if (ch <= 0x1f)
        {
            // Certain characters including LF and the marker codes between 0x1c and 0x1f are
            // explicitly intended to be ignored.  Include all the other codes.
        }

        else   // Add to the current text.
        {
            int nStart = i - 1;

            while (i < m_Content.Size() && m_Content.GetAt(i) >= 0x20)
            {
                i++;
            }

            pCurrItem->m_Text.Append(MHOctetString(m_Content, nStart, i - nStart));
        }
    }

    // Set up the initial attributes.
    int style, size, lineSpace, letterSpace;
    InterpretAttributes(m_fontAttrs, style, size, lineSpace, letterSpace);
    // Create a font with this information.
    m_pDisplay->SetFont(size, (style & 2) != 0, (style & 1) != 0);

    // Calculate the layout of each section.
    for (i = 0; i < theText.Size(); i++)
    {
        MHTextLine *pLine = theText.GetAt(i);
        pLine->m_nLineWidth = 0;

        for (int j = 0; j < pLine->m_Items.Size(); j++)
        {
            MHTextItem *pItem = pLine->m_Items.GetAt(j);

            // Set any tabs.
            pLine->m_nLineWidth = Tabs(pLine->m_nLineWidth, pItem->m_nTabCount);

            if (pItem->m_Unicode.isEmpty())   // Convert UTF-8 to Unicode.
            {
                int s = pItem->m_Text.Size();
                pItem->m_Unicode = QString::fromUtf8((const char *)pItem->m_Text.Bytes(), s);
                pItem->m_nUnicode = pItem->m_Unicode.length();
            }

            // Fit the text onto the line.
            int nFullText = pItem->m_nUnicode;
            // Get the box size and update pItem->m_nUnicode to the number that will fit.
            QRect rect = m_pDisplay->GetBounds(pItem->m_Unicode, pItem->m_nUnicode, m_nBoxWidth - pLine->m_nLineWidth);

            if (nFullText != pItem->m_nUnicode && m_fTextWrap)   // Doesn't fit, we have to word-wrap.
            {
                int nTruncated = pItem->m_nUnicode; // Just in case.
                // Now remove characters until we find a word-break character.
                while (pItem->m_nUnicode > 0 && pItem->m_Unicode[pItem->m_nUnicode] != ' ')
                {
                    pItem->m_nUnicode--;
                }

                // If there are now word-break characters we truncate the text.
                if (pItem->m_nUnicode == 0)
                {
                    pItem->m_nUnicode = nTruncated;
                }

                // Special case to avoid infinite loop if the box is very narrow.
                if (pItem->m_nUnicode == 0)
                {
                    pItem->m_nUnicode = 1;
                }

                // We need to move the text we've cut off this line into a new line.
                int nNewWidth = nFullText - pItem->m_nUnicode;
                int nNewStart = pItem->m_nUnicode;

                // Remove any spaces at the start of the new section.
                while (nNewWidth != 0 && pItem->m_Unicode[nNewStart] == ' ')
                {
                    nNewStart++;
                    nNewWidth--;
                }

                if (nNewWidth != 0)
                {
                    // Create a new line from the extra text.
                    MHTextLine *pNewLine = new MHTextLine;
                    theText.InsertAt(pNewLine, i + 1);
                    // The first item on the new line is the rest of the text.
                    MHTextItem *pNewItem = pItem->NewItem();
                    pNewLine->m_Items.Append(pNewItem);
                    pNewItem->m_Unicode = pItem->m_Unicode.mid(nNewStart, nNewWidth);
                    pNewItem->m_nUnicode = nNewWidth;

                    // Move any remaining items, e.g. in a different colour, from this line onto the new line.
                    while (pLine->m_Items.Size() > j + 1)
                    {
                        pNewLine->m_Items.Append(pLine->m_Items.GetAt(j + 1));
                        pLine->m_Items.RemoveAt(j + 1);
                    }
                }

                // Remove any spaces at the end of the old section.  If we don't do that and
                // we are centering or right aligning the text we'll get it wrong.
                while (pItem->m_nUnicode > 1 && pItem->m_Unicode[pItem->m_nUnicode-1] == ' ')
                {
                    pItem->m_nUnicode--;
                }

                rect = m_pDisplay->GetBounds(pItem->m_Unicode, pItem->m_nUnicode);
            }

            pItem->m_Width = rect.width();
            pLine->m_nLineWidth += rect.width();

            if (rect.height() > pLine->m_nLineHeight)
            {
                pLine->m_nLineHeight = rect.height();
            }

            if (rect.bottom() > pLine->m_nDescent)
            {
                pLine->m_nDescent = rect.bottom();
            }
        }
    }

    // Now output the text.
    int yOffset = 0;
    // If there isn't space for all the lines we should drop extra lines.
    int nNumLines = theText.Size();

    do
    {
        if (m_VertJ == End)
        {
            yOffset = m_nBoxHeight - nNumLines * lineSpace;
        }
        else if (m_VertJ == Centre)
        {
            yOffset = (m_nBoxHeight - nNumLines * lineSpace) / 2;
        }

        if (yOffset < 0)
        {
            nNumLines--;
        }
    }
    while (yOffset < 0);

    for (i = 0; i < nNumLines; i++)
    {
        MHTextLine *pLine = theText.GetAt(i);
        int xOffset = 0;

        if (m_HorizJ == End)
        {
            xOffset = m_nBoxWidth - pLine->m_nLineWidth;
        }
        else if (m_HorizJ == Centre)
        {
            xOffset = (m_nBoxWidth - pLine->m_nLineWidth) / 2;
        }

        for (int j = 0; j < pLine->m_Items.Size(); j++)
        {
            MHTextItem *pItem = pLine->m_Items.GetAt(j);

            // Tab across if necessary.
            xOffset = Tabs(xOffset, pItem->m_nTabCount);

            if (! pItem->m_Unicode.isEmpty())   // We may have blank lines.
            {
                m_pDisplay->AddText(xOffset, yOffset + (pLine->m_nLineHeight + lineSpace) / 2 - pLine->m_nDescent,
                                    pItem->m_Unicode.left(pItem->m_nUnicode), pItem->m_Colour);
            }

            xOffset += pItem->m_Width;
        }

        yOffset += lineSpace;

        if (yOffset + lineSpace > m_nBoxHeight)
        {
            break;
        }
    }

    // Clean up.
    for (int k = 0; k < theText.Size(); k++)
    {
        delete(theText.GetAt(k));
    }
}

void MHText::Display(MHEngine *engine)
{
    if (! m_fRunning || ! m_pDisplay || m_nBoxWidth == 0 || m_nBoxHeight == 0)
    {
        return;    // Can't draw zero sized boxes.
    }

    // We only need to recreate the display if something has changed.
    if (m_fNeedsRedraw)
    {
        Redraw();
        m_fNeedsRedraw = false;
    }

    // Draw the background first, then the text.
    engine->GetContext()->DrawRect(m_nPosX, m_nPosY, m_nBoxWidth, m_nBoxHeight, GetColour(m_bgColour));
    m_pDisplay->Draw(m_nPosX, m_nPosY);
}

// Return the area actually obscured.  This is empty unless the background is opaque.
QRegion MHText::GetOpaqueArea()
{
    if (! m_fRunning || (GetColour(m_bgColour)).alpha() != 255)
    {
        return QRegion();
    }
    else
    {
        return QRegion(QRect(m_nPosX, m_nPosY, m_nBoxWidth, m_nBoxHeight));
    }
}


MHHyperText::MHHyperText(): MHInteractible(this)
{

}

MHHyperText::~MHHyperText()
{

}

void MHHyperText::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHText::Initialise(p, engine);
    MHInteractible::Initialise(p, engine);
    //
}

void MHHyperText::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:HyperText ");
    MHText::PrintMe(fd, nTabs + 1);
    MHInteractible::PrintMe(fd, nTabs + 1);
    fprintf(fd, "****TODO\n");
    PrintTabs(fd, nTabs);
    fprintf(fd, "}\n");
}


void MHSetFontAttributes::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHElemAction::Initialise(p, engine); // Target
    m_FontAttrs.Initialise(p->GetArgN(1), engine); // New font attrs
}

void MHSetFontAttributes::Perform(MHEngine *engine)
{
    // Get the new font attributes.
    MHOctetString newAttrs;
    m_FontAttrs.GetValue(newAttrs, engine);
    Target(engine)->SetFontAttributes(newAttrs, engine);
}
