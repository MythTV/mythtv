/* ParseText.cpp

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
   Or, point your browser to http://www.gnu.org/copyleft/gpl.html

*/

/*
Parser for the textual form of MHEG5.
This is very basic and is only there to enable some test programs to be run.
*/

#include "ParseText.h"
#include "ParseNode.h"
#include "BaseClasses.h"
#include "ASN1Codes.h"
#include "Root.h"
#include "Groups.h"
#include <ctype.h>
#include "Ingredients.h" // For GetEventType
#include "Text.h" // For GetJustification etc
#include "Engine.h"
#include "Logging.h"


#ifndef WIN32
#define stricmp strcasecmp
#endif


MHParseText::MHParseText(QByteArray &program)
{
    m_data = program;
    m_lineCount = 1;
    m_String = (unsigned char *)malloc(100); // Initial size - may grow.
    m_p = 0;
}

MHParseText::~MHParseText()
{
    free(m_String);
}

// Get the next character.
void MHParseText::GetNextChar()
{
    if ((int)m_p >= m_data.size())
    {
        m_ch = EOF;
    }
    else
    {
        m_ch = m_data[m_p++];
    }
}

// Maximum length of a tag (i.e. a symbol beginning with a colon). Actually  the longest is around 22 chars.
#define MAX_TAG_LENGTH  30

const char *rchTagNames[] =
{
    ":Application",
    ":Scene",
    ":StdID",
    ":StdVersion",
    ":ObjectInfo",
    ":OnStartUp",
    ":OnCloseDown",
    ":OrigGCPriority",
    ":Items",
    ":ResidentPrg",
    ":RemotePrg",
    ":InterchgPrg",
    ":Palette",
    ":Font",  // Occurs twice.
    ":CursorShape",
    ":BooleanVar",
    ":IntegerVar",
    ":OStringVar",
    ":ObjectRefVar",
    ":ContentRefVar",
    ":Link",
    ":Stream",
    ":Bitmap",
    ":LineArt",
    ":DynamicLineArt",
    ":Rectangle",
    ":Hotspot",
    ":SwitchButton",
    ":PushButton",
    ":Text",
    ":EntryField",
    ":HyperText",
    ":Slider",
    ":TokenGroup",
    ":ListGroup",
    ":OnSpawnCloseDown",
    ":OnRestart",
    "", // Default attributes - encoded as a group in binary
    ":CharacterSet",
    ":BackgroundColour",
    ":TextCHook",
    ":TextColour",
    ":Font",
    ":FontAttributes",
    ":InterchgPrgCHook",
    ":StreamCHook",
    ":BitmapCHook",
    ":LineArtCHook",
    ":ButtonRefColour",
    ":HighlightRefColour",
    ":SliderRefColour",
    ":InputEventReg",
    ":SceneCS",
    ":AspectRatio",
    ":MovingCursor",
    ":NextScenes",
    ":InitiallyActive",
    ":CHook",
    ":OrigContent",
    ":Shared",
    ":ContentSize",
    ":CCPriority",
    "" , // Link condition - always replaced by EventSource, EventType and EventData
    ":LinkEffect",
    ":Name",
    ":InitiallyAvailable",
    ":ProgramConnectionTag",
    ":OrigValue",
    ":ObjectRef",
    ":ContentRef",
    ":MovementTable",
    ":TokenGroupItems",
    ":NoTokenActionSlots",
    ":Positions",
    ":WrapAround",
    ":MultipleSelection",
    ":OrigBoxSize",
    ":OrigPosition",
    ":OrigPaletteRef",
    ":Tiling",
    ":OrigTransparency",
    ":BBBox",
    ":OrigLineWidth",
    ":OrigLineStyle",
    ":OrigRefLineColour",
    ":OrigRefFillColour",
    ":OrigFont",
    ":HJustification",
    ":VJustification",
    ":LineOrientation",
    ":StartCorner",
    ":TextWrapping",
    ":Multiplex",
    ":Storage",
    ":Looping",
    ":Audio",
    ":Video",
    ":RTGraphics",
    ":ComponentTag",
    ":OrigVolume",
    ":Termination",
    ":EngineResp",
    ":Orientation",
    ":MaxValue",
    ":MinValue",
    ":InitialValue",
    ":InitialPortion",
    ":StepSize",
    ":SliderStyle",
    ":InputType",
    ":CharList",
    ":ObscuredInput",
    ":MaxLength",
    ":OrigLabel",
    ":ButtonStyle",
    ":Activate",
    ":Add",
    ":AddItem",
    ":Append",
    ":BringToFront",
    ":Call",
    ":CallActionSlot",
    ":Clear",
    ":Clone",
    ":CloseConnection",
    ":Deactivate",
    ":DelItem",
    ":Deselect",
    ":DeselectItem",
    ":Divide",
    ":DrawArc",
    ":DrawLine",
    ":DrawOval",
    ":DrawPolygon",
    ":DrawPolyline",
    ":DrawRectangle",
    ":DrawSector",
    ":Fork",
    ":GetAvailabilityStatus",
    ":GetBoxSize",
    ":GetCellItem",
    ":GetCursorPosition",
    ":GetEngineSupport",
    ":GetEntryPoint",
    ":GetFillColour",
    ":GetFirstItem",
    ":GetHighlightStatus",
    ":GetInteractionStatus",
    ":GetItemStatus",
    ":GetLabel",
    ":GetLastAnchorFired",
    ":GetLineColour",
    ":GetLineStyle",
    ":GetLineWidth",
    ":GetListItem",
    ":GetListSize",
    ":GetOverwriteMode",
    ":GetPortion",
    ":GetPosition",
    ":GetRunningStatus",
    ":GetSelectionStatus",
    ":GetSliderValue",
    ":GetTextContent",
    ":GetTextData",
    ":GetTokenPosition",
    ":GetVolume",
    ":Launch",
    ":LockScreen",
    ":Modulo",
    ":Move",
    ":MoveTo",
    ":Multiply",
    ":OpenConnection",
    ":Preload",
    ":PutBefore",
    ":PutBehind",
    ":Quit",
    ":ReadPersistent",
    ":Run",
    ":ScaleBitmap",
    ":ScaleVideo",
    ":ScrollItems",
    ":Select",
    ":SelectItem",
    ":SendEvent",
    ":SendToBack",
    ":SetBoxSize",
    ":SetCachePriority",
    ":SetCounterEndPosition",
    ":SetCounterPosition",
    ":SetCounterTrigger",
    ":SetCursorPosition",
    ":SetCursorShape",
    ":SetData",
    ":SetEntryPoint",
    ":SetFillColour",
    ":SetFirstItem",
    ":SetFontRef",
    ":SetHighlightStatus",
    ":SetInteractionStatus",
    ":SetLabel",
    ":SetLineColour",
    ":SetLineStyle",
    ":SetLineWidth",
    ":SetOverwriteMode",
    ":SetPaletteRef",
    ":SetPortion",
    ":SetPosition",
    ":SetSliderValue",
    ":SetSpeed",
    ":SetTimer",
    ":SetTransparency",
    ":SetVariable",
    ":SetVolume",
    ":Spawn",
    ":Step",
    ":Stop",
    ":StorePersistent",
    ":Subtract",
    ":TestVariable",
    ":Toggle",
    ":ToggleItem",
    ":TransitionTo",
    ":Unload",
    ":UnlockScreen",
    ":GBoolean",
    ":GInteger",
    ":GOctetString",
    ":GObjectRef",
    ":GContentRef",
    ":NewColourIndex",
    ":NewAbsoluteColour",
    ":NewFontName",
    ":NewFontRef",
    ":NewContentSize",
    ":NewCCPriority",
    ":IndirectRef",
    /* UK MHEG */
    ":SetBackgroundColour",
    ":SetCellPosition",
    ":SetInputRegister",
    ":SetTextColour",
    ":SetFontAttributes",
    ":SetVideoDecodeOffset",
    ":GetVideoDecodeOffset",
    ":GetFocusPosition",
    ":SetFocusPosition",
    ":SetBitmapDecodeOffset",
    ":GetBitmapDecodeOffset",
    ":SetSliderParameters",
    /* Pseudo-operations.  These are encoded as LinkCondition in binary. */
    ":EventSource",
    ":EventType",
    ":EventData",
    ":ActionSlots"
};

// Some example programs use these colour names
static struct
{
    const char *name;
    unsigned char r, g, b, t;
} colourTable[] =
{
    { "black",          0,  0,  0,  0   },
    { "transparent",    0,  0,  0,  255 },
    { "gray"/*sic*/,    128, 128, 128, 0 },
    { "darkgray"/*sic*/, 192, 192, 192, 0 },
    { "red",            255, 0,  0,  0 },
    { "darkred",        128, 0,  0,  0 },
    { "blue",           0,  0,  255, 0 },
    { "darkblue",       0,  0,  128, 0 },
    { "green",          0,  255, 0,  0 },
    { "darkgreen",      0,  128, 0,  0 },
    { "yellow",         255, 255, 0,  0 },
    { "cyan",           0,  255, 255, 0 },
    { "magenta",        255, 0,  255, 0 }
};


// Search for a tag and return it if it exists.  Returns -1 if it isn't found.
static int FindTag(const char *p)
{
    for (int i = 0; i < (int)(sizeof(rchTagNames) / sizeof(rchTagNames[0])); i++)
    {
        if (stricmp(p, rchTagNames[i]) == 0)
        {
            return i;
        }
    }

    return -1;
}


// Ditto for the enumerated types
#define MAX_ENUM        30

void MHParseText::Error(const char *str)
{
    MHERROR(QString("%1- at line %2\n").arg(str).arg(m_lineCount));
}

// Lexical analysis.  Get the next symbol.
void MHParseText::NextSym()
{
    while (1)
    {

        switch (m_ch)
        {
            case '\n':
                m_lineCount++; // And drop to next
            case ' ':
            case '\r':
            case '\t':
            case '\f':
                // Skip white space.
                GetNextChar();
                continue;

            case '/':
            {
                // Comment.
                GetNextChar();

                if (m_ch != '/')
                {
                    Error("Malformed comment");
                }

                do
                {
                    GetNextChar();
                }
                while (m_ch != '\n' && m_ch != '\f' && m_ch != '\r');

                continue; // Next symbol
            }

            case ':': // Start of a tag
            {
                m_nType = PTTag;
                char buff[MAX_TAG_LENGTH+1];
                char *p = buff;

                do
                {
                    *p++ = m_ch;
                    GetNextChar();

                    if (p == buff + MAX_TAG_LENGTH)
                    {
                        break;
                    }
                }
                while ((m_ch >= 'a' && m_ch <= 'z') || (m_ch >= 'A' && m_ch <= 'Z'));

                *p = 0;

                // Look it up and return it if it's found.
                m_nTag = FindTag(buff);

                if (m_nTag >= 0)
                {
                    return;
                }

                // Unrecognised tag.
                Error("Unrecognised tag");
            }

            case '"': // Start of a string
            {
                m_nType = PTString;
                // MHEG strings can include NULLs.  For the moment we pass back the length and also
                // null-terminate the strings.
                m_nStringLength = 0;

                while (1)
                {
                    GetNextChar();

                    if (m_ch == '"')
                    {
                        break;    // Finished the string.
                    }

                    if (m_ch == '\\')
                    {
                        GetNextChar();    // Escape character. Include the next char in the string.
                    }

                    if (m_ch == '\n' || m_ch == '\r')
                    {
                        Error("Unterminated string");
                    }

                    // We grow the buffer to the largest string in the input.
                    unsigned char *str = (unsigned char *)realloc(m_String, m_nStringLength + 2);

                    if (str == NULL)
                    {
                        Error("Insufficient memory");
                    }

                    m_String = str;
                    m_String[m_nStringLength++] = m_ch;
                }

                GetNextChar(); // Skip the closing quote
                m_String[m_nStringLength] = 0;
                return;
            }

            case '\'': // Start of a string using quoted printable
            {
                m_nType = PTString;
                m_nStringLength = 0;

                // Quotable printable strings contain escape sequences beginning with the
                // escape character '='.  The strings can span lines but each line must
                // end with an equal sign.
                while (1)
                {
                    GetNextChar();

                    if (m_ch == '\'')
                    {
                        break;
                    }

                    if (m_ch == '\n')
                    {
                        Error("Unterminated string");
                    }

                    if (m_ch == '=')   // Special code in quoted-printable.
                    {
                        // Should be followed by two hex digits or by white space and a newline.
                        GetNextChar();

                        if (m_ch == ' ' || m_ch == '\t' || m_ch == '\r' || m_ch == '\n')
                        {
                            // White space.  Remove everything up to the newline.
                            while (m_ch != '\n')
                            {
                                if (!(m_ch == ' ' || m_ch == '\t' || m_ch == '\r'))
                                {
                                    Error("Malformed quoted printable string");
                                }

                                GetNextChar();
                            }

                            continue; // continue with the first character on the next line
                        }
                        else
                        {
                            int byte = 0;

                            if (m_ch >= '0' && m_ch <= '9')
                            {
                                byte = m_ch - '0';
                            }
                            else if (m_ch >= 'A' && m_ch <= 'F')
                            {
                                byte = m_ch - 'A' + 10;
                            }
                            else if (m_ch >= 'a' && m_ch <= 'f')
                            {
                                byte = m_ch - 'a' + 10;
                            }
                            else
                            {
                                Error("Malformed quoted printable string");
                            }

                            byte *= 16;
                            GetNextChar();

                            if (m_ch >= '0' && m_ch <= '9')
                            {
                                byte += m_ch - '0';
                            }
                            else if (m_ch >= 'A' && m_ch <= 'F')
                            {
                                byte += m_ch - 'A' + 10;
                            }
                            else if (m_ch >= 'a' && m_ch <= 'f')
                            {
                                byte += m_ch - 'a' + 10;
                            }
                            else
                            {
                                Error("Malformed quoted printable string");
                            }

                            m_ch = byte; // Put this into the string.
                        }
                    }

                    // We grow the buffer to the largest string in the input.
                    unsigned char *str = (unsigned char *)realloc(m_String, m_nStringLength + 2);

                    if (str == NULL)
                    {
                        Error("Insufficient memory");
                    }

                    m_String = str;
                    m_String[m_nStringLength++] = m_ch;
                }

                GetNextChar(); // Skip the closing quote
                m_String[m_nStringLength] = 0;
                return;
            }

            case '`': // Start of a string using base 64
                // These can, presumably span lines.
                MHERROR("Base 64 string is not implemented");
                break;

            case '#': // Start of 3-byte hex constant.
                MHERROR("3-byte hex constant is not implemented");
                break;

            case '-':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            {
                m_nType = PTInt;
                bool negative = m_ch == '-';

                if (negative)
                {
                    GetNextChar();

                    if (m_ch < '0' || m_ch > '9')
                    {
                        Error("Expected digit after '-'");
                    }
                }

                // Start of a number.  Hex can be represented as 0xn.
                // Strictly speaking hex values cannot be preceded by a minus sign.
                m_nInt = m_ch - '0';
                GetNextChar();

                if (m_nInt == 0 && (m_ch == 'x' || m_ch == 'X'))
                {
                    MHERROR("Hex constant is not implemented");
                }

                while (m_ch >= '0' && m_ch <= '9')
                {
                    m_nInt = m_nInt * 10 + m_ch - '0';
                    // TODO: What about overflow?
                    GetNextChar();
                }

                if (negative)
                {
                    m_nInt = -m_nInt;
                }

                return;
            }

            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
            case 'g':
            case 'h':
            case 'i':
            case 'j':
            case 'k':
            case 'l':
            case 'm':
            case 'n':
            case 'o':
            case 'p':
            case 'q':
            case 'r':
            case 's':
            case 't':
            case 'u':
            case 'v':
            case 'w':
            case 'x':
            case 'y':
            case 'z':
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
            case 'G':
            case 'H':
            case 'I':
            case 'J':
            case 'K':
            case 'L':
            case 'M':
            case 'N':
            case 'O':
            case 'P':
            case 'Q':
            case 'R':
            case 'S':
            case 'T':
            case 'U':
            case 'V':
            case 'W':
            case 'X':
            case 'Y':
            case 'Z':
            {
                // Start of an enumerated type.
                m_nType = PTEnum;
                char buff[MAX_ENUM+1];
                char *p = buff;

                do
                {
                    *p++ = m_ch;
                    GetNextChar();

                    if (p == buff + MAX_ENUM)
                    {
                        break;
                    }
                }
                while ((m_ch >= 'a' && m_ch <= 'z') || (m_ch >= 'A' && m_ch <= 'Z') || m_ch == '-');

                *p = '\0';

                if (stricmp(buff, "NULL") == 0)
                {
                    m_nType = PTNull;
                    return;
                }

                if (stricmp(buff, "true") == 0)
                {
                    m_nType = PTBool;
                    m_fBool = true;
                    return;
                }

                if (stricmp(buff, "false") == 0)
                {
                    m_nType = PTBool;
                    m_fBool = false;
                    return;
                }

                // Look up the tag in all the tables.  Fortunately all the enumerations
                // are distinct so we don't need to know the context.
                m_nInt = MHLink::GetEventType(buff);

                if (m_nInt > 0)
                {
                    return;
                }

                m_nInt = MHText::GetJustification(buff);

                if (m_nInt > 0)
                {
                    return;
                }

                m_nInt = MHText::GetLineOrientation(buff);

                if (m_nInt > 0)
                {
                    return;
                }

                m_nInt = MHText::GetStartCorner(buff);

                if (m_nInt > 0)
                {
                    return;
                }

                m_nInt = MHSlider::GetOrientation(buff);

                if (m_nInt > 0)
                {
                    return;
                }

                m_nInt = MHSlider::GetStyle(buff);

                if (m_nInt > 0)
                {
                    return;
                }

                // Check the colour table.  If it's there generate a string containing the colour info.
                for (int i = 0; i < (int)(sizeof(colourTable) / sizeof(colourTable[0])); i++)
                {
                    if (stricmp(buff, colourTable[i].name) == 0)
                    {
                        m_nType = PTString;
                        unsigned char *str = (unsigned char *)realloc(m_String, 4 + 1);

                        if (str == NULL)
                        {
                            Error("Insufficient memory");
                        }

                        m_String = str;
                        m_String[0] = colourTable[i].r;
                        m_String[1] = colourTable[i].g;
                        m_String[2] = colourTable[i].b;
                        m_String[3] = colourTable[i].t;
                        m_nStringLength = 4;
                        m_String[m_nStringLength] = 0;
                        return;
                    }
                }

                Error("Unrecognised enumeration");
                break;
            }

            case '{': // Start of a "section".
                // The standard indicates that open brace followed by a tag should be written
                // as a single word.  We'll be more lenient and allow spaces or comments between them.
                m_nType = PTStartSection;
                GetNextChar();
                return;

            case '}': // End of a "section".
                m_nType = PTEndSection;
                GetNextChar();
                return;

            case '(': // Start of a sequence.
                m_nType = PTStartSeq;
                GetNextChar();
                return;

            case ')': // End of a sequence.
                m_nType = PTEndSeq;
                GetNextChar();
                return;

            case EOF:
                m_nType = PTEOF;
                return;

            default:
                Error("Unknown character");
                GetNextChar();
        }
    }
}

MHParseNode *MHParseText::DoParse()
{
    MHParseNode *pRes = NULL;

    try
    {
        switch (m_nType)
        {
            case PTStartSection: // Open curly bracket
            {
                NextSym();

                // Should be followed by a tag.
                if (m_nType != PTTag)
                {
                    Error("Expected ':' after '{'");
                }

                MHPTagged *pTag = new MHPTagged(m_nTag);
                pRes = pTag;
                NextSym();

                while (m_nType != PTEndSection)
                {
                    pTag->AddArg(DoParse());
                }

                NextSym(); // Remove the close curly bracket.
                break;
            }

            case PTTag: // Tag on its own.
            {
                int nTag = m_nTag;
                MHPTagged *pTag = new MHPTagged(nTag);
                pRes = pTag;
                NextSym();

                switch (nTag)
                {
                    case C_ITEMS:
                    case C_LINK_EFFECT:
                    case C_ACTIVATE:
                    case C_ADD:
                    case C_ADD_ITEM:
                    case C_APPEND:
                    case C_BRING_TO_FRONT:
                    case C_CALL:
                    case C_CALL_ACTION_SLOT:
                    case C_CLEAR:
                    case C_CLONE:
                    case C_CLOSE_CONNECTION:
                    case C_DEACTIVATE:
                    case C_DEL_ITEM:
                    case C_DESELECT:
                    case C_DESELECT_ITEM:
                    case C_DIVIDE:
                    case C_DRAW_ARC:
                    case C_DRAW_LINE:
                    case C_DRAW_OVAL:
                    case C_DRAW_POLYGON:
                    case C_DRAW_POLYLINE:
                    case C_DRAW_RECTANGLE:
                    case C_DRAW_SECTOR:
                    case C_FORK:
                    case C_GET_AVAILABILITY_STATUS:
                    case C_GET_BOX_SIZE:
                    case C_GET_CELL_ITEM:
                    case C_GET_CURSOR_POSITION:
                    case C_GET_ENGINE_SUPPORT:
                    case C_GET_ENTRY_POINT:
                    case C_GET_FILL_COLOUR:
                    case C_GET_FIRST_ITEM:
                    case C_GET_HIGHLIGHT_STATUS:
                    case C_GET_INTERACTION_STATUS:
                    case C_GET_ITEM_STATUS:
                    case C_GET_LABEL:
                    case C_GET_LAST_ANCHOR_FIRED:
                    case C_GET_LINE_COLOUR:
                    case C_GET_LINE_STYLE:
                    case C_GET_LINE_WIDTH:
                    case C_GET_LIST_ITEM:
                    case C_GET_LIST_SIZE:
                    case C_GET_OVERWRITE_MODE:
                    case C_GET_PORTION:
                    case C_GET_POSITION:
                    case C_GET_RUNNING_STATUS:
                    case C_GET_SELECTION_STATUS:
                    case C_GET_SLIDER_VALUE:
                    case C_GET_TEXT_CONTENT:
                    case C_GET_TEXT_DATA:
                    case C_GET_TOKEN_POSITION:
                    case C_GET_VOLUME:
                    case C_LAUNCH:
                    case C_LOCK_SCREEN:
                    case C_MODULO:
                    case C_MOVE:
                    case C_MOVE_TO:
                    case C_MULTIPLY:
                    case C_OPEN_CONNECTION:
                    case C_PRELOAD:
                    case C_PUT_BEFORE:
                    case C_PUT_BEHIND:
                    case C_QUIT:
                    case C_READ_PERSISTENT:
                    case C_RUN:
                    case C_SCALE_BITMAP:
                    case C_SCALE_VIDEO:
                    case C_SCROLL_ITEMS:
                    case C_SELECT:
                    case C_SELECT_ITEM:
                    case C_SEND_EVENT:
                    case C_SEND_TO_BACK:
                    case C_SET_BOX_SIZE:
                    case C_SET_CACHE_PRIORITY:
                    case C_SET_COUNTER_END_POSITION:
                    case C_SET_COUNTER_POSITION:
                    case C_SET_COUNTER_TRIGGER:
                    case C_SET_CURSOR_POSITION:
                    case C_SET_CURSOR_SHAPE:
                    case C_SET_DATA:
                    case C_SET_ENTRY_POINT:
                    case C_SET_FILL_COLOUR:
                    case C_SET_FIRST_ITEM:
                    case C_SET_FONT_REF:
                    case C_SET_HIGHLIGHT_STATUS:
                    case C_SET_INTERACTION_STATUS:
                    case C_SET_LABEL:
                    case C_SET_LINE_COLOUR:
                    case C_SET_LINE_STYLE:
                    case C_SET_LINE_WIDTH:
                    case C_SET_OVERWRITE_MODE:
                    case C_SET_PALETTE_REF:
                    case C_SET_PORTION:
                    case C_SET_POSITION:
                    case C_SET_SLIDER_VALUE:
                    case C_SET_SPEED:
                    case C_SET_TIMER:
                    case C_SET_TRANSPARENCY:
                    case C_SET_VARIABLE:
                    case C_SET_VOLUME:
                    case C_SPAWN:
                    case C_STEP:
                    case C_STOP:
                    case C_STORE_PERSISTENT:
                    case C_SUBTRACT:
                    case C_TEST_VARIABLE:
                    case C_TOGGLE:
                    case C_TOGGLE_ITEM:
                    case C_TRANSITION_TO:
                    case C_UNLOAD:
                    case C_UNLOCK_SCREEN:
                    case C_CONTENT_REFERENCE:
                    case C_TOKEN_GROUP_ITEMS:
                    case C_POSITIONS:
                    case C_MULTIPLEX:
                    {
                        // These are parenthesised in the text form.  We have to remove the
                        // parentheses otherwise we will return a sequence which will not be
                        // be compatible with the binary form.
                        if (m_nType != PTStartSeq)
                        {
                            Error("Expected '('");
                        }

                        NextSym();

                        while (m_nType != PTEndSeq)
                        {
                            pTag->AddArg(DoParse());
                        }

                        NextSym(); // Remove the close parenthesis.
                        break;
                    }
                    case C_ORIGINAL_CONTENT:
                    case C_NEW_GENERIC_BOOLEAN:
                    case C_NEW_GENERIC_INTEGER:
                    case C_NEW_GENERIC_OCTETSTRING:
                    case C_NEW_GENERIC_OBJECT_REF:
                    case C_NEW_GENERIC_CONTENT_REF:
                    case C_ORIGINAL_VALUE:
                        // These always have an argument which may be a tagged item.
                    {
                        // Is it always the case that there is at least one argument so if we haven't
                        // had any arguments yet we should always process a tag as an argument?
                        pTag->AddArg(DoParse());
                        break;
                    }
                    default:

                        // This can be followed by an int, etc but a new tag is dealt with by the caller.
                        while (m_nType == PTBool || m_nType == PTInt || m_nType == PTString || m_nType == PTEnum || m_nType == PTStartSeq)
                        {
                            pTag->AddArg(DoParse());
                        }

                }

                break;
            }

            case PTInt:
            {
                pRes = new MHPInt(m_nInt);
                NextSym();
                break;
            }

            case PTBool:
            {
                pRes = new MHPBool(m_fBool);
                NextSym();
                break;
            }

            case PTString:
            {
                MHOctetString str;
                str.Copy(MHOctetString((const char *)m_String, m_nStringLength));
                pRes = new MHPString(str);
                NextSym();
                break;
            }

            case PTEnum:
            {
                pRes = new MHPEnum(m_nInt);
                NextSym();
                break;
            }

            case PTNull:
            {
                pRes = new MHPNull;
                NextSym();
                break;
            }

            case PTStartSeq: // Open parenthesis.
            {
                MHParseSequence *pSeq = new MHParseSequence;
                pRes = pSeq;
                NextSym();

                while (m_nType != PTEndSeq)
                {
                    pSeq->Append(DoParse());
                }

                NextSym(); // Remove the close parenthesis.
                break;
            }

            default:
                Error("Unexpected symbol");
        }

        return pRes;
    }
    catch (...)
    {
        delete(pRes);
        throw;
    }
}


// Run the parser
MHParseNode *MHParseText::Parse()
{
    GetNextChar(); // Initialise m_ch
    NextSym(); // Initialise the symbol values.
    return DoParse();
}

