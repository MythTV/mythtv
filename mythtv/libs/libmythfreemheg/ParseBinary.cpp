/* ParseBinary.cpp

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

/*
Parser for ASN1 binary notation.  Does minimal syntax checking, assuming that this will already have
been done before the binary was produced.  Creates a MHParseNode tree structure.
*/
#include <cstdlib> // malloc etc.

#include "Engine.h"
#include "ParseBinary.h"
#include "ASN1Codes.h"
#include "ParseNode.h"
#include "BaseClasses.h"
#include "Root.h"
#include "Groups.h"
#include "Logging.h"

static constexpr int INDEFINITE_LENGTH { -1 };

// Get the next byte.  In most all cases it's an error if we reach end-of-file
// and we throw an exception.
unsigned char MHParseBinary::GetNextChar()
{
    if (m_p >= m_data.size())
    {
        MHERROR("Unexpected end of file");
    }

    return m_data[m_p++];
}


// Parse a string argument.  ASN1 strings can include nulls as valid characters.
void MHParseBinary::ParseString(int endStr, MHOctetString &str)
{
    // TODO: Don't deal with indefinite length at the moment.
    if (endStr == INDEFINITE_LENGTH)
    {
        MHERROR("Indefinite length strings are not implemented");
    }

    int nLength = endStr - m_p;
    auto *stringValue = (unsigned char *)malloc(nLength + 1);
    if (stringValue == nullptr)
    {
        MHERROR("Out of memory");
    }

    unsigned char *p = stringValue;

    while (m_p < endStr)
    {
        *p++ = GetNextChar();
    }

    str.Copy(MHOctetString((const char *)stringValue, nLength));
    free(stringValue);
}

// Parse an integer argument.  Also used for bool and enum.
int MHParseBinary::ParseInt(int endInt)
{
    int intVal = 0;
    bool firstByte = true;

    if (endInt == INDEFINITE_LENGTH)
    {
        MHERROR("Indefinite length integers are not implemented");
    }

    while (m_p < endInt)
    {
        unsigned char ch = GetNextChar();

        // Integer values are signed so if the top bit is set in the first byte
        // we need to set the sign bit.
        if (firstByte && ch >= 128)
        {
            intVal = -1;
        }

        firstByte = false;
        // NOLINTNEXTLINE(clang-analyzer-core.UndefinedBinaryOperatorResult)
        intVal = (intVal << 8) | ch;
    }

    return intVal;
}


//  Simple recursive parser for ASN1 BER.
MHParseNode *MHParseBinary::DoParse()
{
    // Tag class
    enum : std::uint8_t { Universal, Context/*, Pseudo*/ } tagClass = Universal;
    // Byte count of end of this item.  Set to INDEFINITE_LENGTH if the length is Indefinite.
    int endOfItem = 0;
    unsigned int tagNumber = 0;

    // Read the first character.
    unsigned char ch = GetNextChar();

    // ASN1 Coding rules: Top two bits (0 and 1) indicate the tag class.
    // 0x00 - Universal,  0x40 - Application, 0x80 - Context-specific, 0xC0 - Private
    // We only use Universal and Context.
    switch (ch & 0xC0)
    {
        case 0x00: // Universal
            tagClass = Universal;
            break;
        case 0x80:
            tagClass = Context;
            break;
        default:
            MHERROR(QString("Invalid tag class = %1").arg(ch, 0, 16));
    }

    // Bit 2 indicates whether it is a simple or compound type.  Not used.
    // Lower bits are the tag number.
    tagNumber = ch & 0x1f;

    if (tagNumber == 0x1f)   // Except that if it is 0x1F then the tag is encoded in the following bytes.
    {
        ch = GetNextChar();
        tagNumber = ch & 0x7f;
        while ((ch & 0x80) != 0)   // Top bit set means there's more to come.
        {
            ch = GetNextChar();
            tagNumber = (tagNumber << 7) | (ch & 0x7f);
        }
    }

    // Next byte is the length.  If it is less than 128 it is the actual length, otherwise it
    // gives the number of bytes containing the length, except that if this is zero the item
    // has an "indefinite" length and is terminated by two zero bytes.
    ch = GetNextChar();

    if (ch & 0x80)
    {
        int lengthOfLength = ch & 0x7f;

        if (lengthOfLength == 0)
        {
            endOfItem = INDEFINITE_LENGTH;
        }
        else
        {
            endOfItem = 0;

            while (lengthOfLength--)
            {
                ch = GetNextChar();
                endOfItem = (endOfItem << 8) | ch;
            }

            endOfItem += m_p;
        }
    }
    else
    {
        endOfItem = ch + m_p;
    }

    if (tagClass == Context)
    {
        auto *pNode = new MHPTagged(tagNumber);

        try
        {
            // The argument here depends on the particular tag we're processing.
            switch (tagNumber)
            {
                case C_MULTIPLE_SELECTION:
                case C_OBSCURED_INPUT:
                case C_INITIALLY_AVAILABLE:
                case C_WRAP_AROUND:
                case C_TEXT_WRAPPING:
                case C_INITIALLY_ACTIVE:
                case C_MOVING_CURSOR:
                case C_SHARED:
                case C_ENGINE_RESP:
                case C_TILING:
                case C_BORDERED_BOUNDING_BOX:
                {
                    // BOOL
                    // If there is no argument we need to indicate that so that it gets
                    // the correct default value.
                    if (m_p != endOfItem)
                    {
                        int intVal = ParseInt(endOfItem); // May raise an exception
                        pNode->AddArg(new MHPBool(intVal != 0));
                    }

                    break;
                }

                case C_INPUT_TYPE:
                case C_SLIDER_STYLE:
                case C_TERMINATION:
                case C_ORIENTATION:
                case C_HORIZONTAL_JUSTIFICATION:
                case C_BUTTON_STYLE:
                case C_START_CORNER:
                case C_LINE_ORIENTATION:
                case C_VERTICAL_JUSTIFICATION:
                case C_STORAGE:
                {
                    // ENUM
                    if (m_p != endOfItem)
                    {
                        int intVal = ParseInt(endOfItem); // May raise an exception
                        pNode->AddArg(new MHPEnum(intVal));
                    }

                    break;
                }

                case C_INITIAL_PORTION:
                case C_STEP_SIZE:
                case C_INPUT_EVENT_REGISTER:
                case C_INITIAL_VALUE:
                case C_IP_CONTENT_HOOK:
                case C_MAX_VALUE:
                case C_MIN_VALUE:
                case C_LINE_ART_CONTENT_HOOK:
                case C_BITMAP_CONTENT_HOOK:
                case C_TEXT_CONTENT_HOOK:
                case C_STREAM_CONTENT_HOOK:
                case C_MAX_LENGTH:
                case C_CHARACTER_SET:
                case C_ORIGINAL_TRANSPARENCY:
                case C_ORIGINAL_GC_PRIORITY:
                case C_LOOPING:
                case C_ORIGINAL_LINE_STYLE:
                case C_STANDARD_VERSION:
                case C_ORIGINAL_LINE_WIDTH:
                case C_CONTENT_HOOK:
                case C_CONTENT_CACHE_PRIORITY:
                case C_COMPONENT_TAG:
                case C_ORIGINAL_VOLUME:
                case C_PROGRAM_CONNECTION_TAG:
                case C_CONTENT_SIZE:
                {
                    // INT
                    if (m_p != endOfItem)
                    {
                        int intVal = ParseInt(endOfItem); // May raise an exception
                        pNode->AddArg(new MHPInt(intVal));
                    }

                    break;
                }

                case C_OBJECT_INFORMATION:
                case C_CONTENT_REFERENCE:
                case C_FONT_ATTRIBUTES:
                case C_CHAR_LIST:
                case C_NAME:
                case C_ORIGINAL_LABEL:
                {
                    // STRING
                    // Unlike INT, BOOL and ENUM we can't distinguish an empty string
                    // from a missing string.
                    MHOctetString str;
                    ParseString(endOfItem, str);
                    pNode->AddArg(new MHPString(str));
                    break;
                }

                default:
                {
                    // Everything else has either no argument or is self-describing
                    // TODO: Handle indefinite length.
                    if (endOfItem == INDEFINITE_LENGTH)
                    {
                        MHERROR("Indefinite length arguments are not implemented");
                    }

                    while (m_p < endOfItem)
                    {
                        pNode->AddArg(DoParse());
                    }
                }
            }
        }
        catch (...)
        {
            // Memory clean-up
            delete pNode;
            throw;
        }

        return pNode;
    }

    // Universal - i.e. a primitive type.
    // Tag values

    switch (tagNumber)
    {
    case U_BOOL: // Boolean
    {
        int intVal = ParseInt(endOfItem);
        return new MHPBool(intVal != 0);
    }
    case U_INT: // Integer
    {
        int intVal = ParseInt(endOfItem);
        return new MHPInt(intVal);
    }
    case U_ENUM: // ENUM
    {
        int intVal = ParseInt(endOfItem);
        return new MHPEnum(intVal);
    }
    case U_STRING: // String
    {
        MHOctetString str;
        ParseString(endOfItem, str);
        return new MHPString(str);
    }
    case U_NULL: // ASN1 NULL
    {
        return new MHPNull;
    }
    case U_SEQUENCE: // Sequence
    {
        auto *pNode = new MHParseSequence();

        if (endOfItem == INDEFINITE_LENGTH)
        {
            MHERROR("Indefinite length sequences are not implemented");
        }

        try
        {
            while (m_p < endOfItem)
            {
                pNode->Append(DoParse());
            }
        }
        catch (...)
        {
            // Memory clean-up if error.
            delete pNode;
            throw;
        }

        return pNode;
    }
    default:
        MHERROR(QString("Unknown universal %1").arg(tagNumber));
    }
}
