/* BaseClasses.h

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

#if !defined(BASECLASSES_H)
#define BASECLASSES_H

#include "config.h"
#if HAVE_MALLOC_H
#include <malloc.h>
#endif

#include <QString>

#include "Logging.h" // For MHASSERT

class MHEngine;

// These templates should really be obtained from a library.  They are defined here to
// allow easy porting to a variety of libraries.

// Simple sequence class.
template <class BASE> class MHSequence {
    public:
        MHSequence(){ m_VecSize = 0; m_Values = 0; }
        // The destructor frees the vector but not the elements.
        ~MHSequence() { free(m_Values); }
        // Get the current size.
        int Size() const { return m_VecSize; }
        // Get an element at a particular index.
        BASE GetAt(int i) const { MHASSERT(i >= 0 && i < m_VecSize); return m_Values[i]; }
        BASE operator[](int i) const { return GetAt(i); }
        // Add a new element at position n and move the existing element at that position
        // and anything above it up one place.
        void InsertAt(BASE b, int n) {
            MHASSERT(n >= 0 && n <= m_VecSize);
            BASE *ptr = (BASE*)realloc(m_Values, (m_VecSize+1) * sizeof(BASE));
            if (ptr == NULL) throw "Out of Memory";
            m_Values = ptr;
            for (int i = m_VecSize; i > n; i--) m_Values[i] = m_Values[i-1];
            m_Values[n] = b;
            m_VecSize++;
        }
        // Add an element to the end of the sequence.
        void Append(BASE b) { InsertAt(b, m_VecSize); }
        // Remove an element and shift all the others down.
        void RemoveAt(int i) {
            MHASSERT(i >= 0 && i < m_VecSize);
            for (int j = i+1; j < m_VecSize; j++) m_Values[j-1] = m_Values[j];
            m_VecSize--;
        }
    protected:
        int m_VecSize;
        BASE *m_Values;
};

// As above, but it deletes the pointers when the sequence is destroyed.
template <class BASE> class MHOwnPtrSequence: public MHSequence<BASE*> {
    public:
        ~MHOwnPtrSequence() { for(int i = 0; i < MHSequence<BASE*>::m_VecSize; i++) delete(MHSequence<BASE*>::GetAt(i)); }
};


// Simple stack.
template <class BASE> class MHStack: protected MHSequence<BASE> {
    public:
        // Test for empty
        bool Empty() const { return Size() == 0; }
        // Pop an item from the stack.
        BASE Pop() {
            MHASSERT(MHSequence<BASE>::m_VecSize > 0);
            return MHSequence<BASE>::m_Values[--MHSequence<BASE>::m_VecSize];
        }
        // Push an element on the stack.
        void Push(BASE b) { this->Append(b); }
        // Return the top of the stack.
        BASE Top() { 
            MHASSERT(MHSequence<BASE>::m_VecSize > 0);
            return MHSequence<BASE>::m_Values[MHSequence<BASE>::m_VecSize-1];
        }
        int Size() const { return MHSequence<BASE>::Size(); }
};

class MHParseNode;

// An MHEG octet string is a sequence of bytes which can include nulls.  MHEG, or at least UK MHEG, indexes
// strings from 1.  We use 0 indexing in calls to these functions.
class MHOctetString  
{
  public:
    MHOctetString();
    MHOctetString(const char *str, int nLen = -1); // From character string
    MHOctetString(const unsigned char *str, int nLen); // From byte vector
    MHOctetString(const MHOctetString &str, int nOffset=0, int nLen=-1); // Substring
    virtual ~MHOctetString();

    void Copy(const MHOctetString &str);
    int Size() const { return m_nLength; }
    // Comparison - returns <0, =0, >0 depending on the ordering.
    int Compare(const MHOctetString &str) const;
    bool Equal(const MHOctetString &str) const { return Compare(str) == 0; }
    unsigned char GetAt(int i) const { MHASSERT(i >= 0 && i < Size()); return m_pChars[i]; }
    const unsigned char *Bytes() const { return m_pChars; } // Read-only pointer to the buffer.
    void Append(const MHOctetString &str); // Add text to the end of the string.

    QString Printable() const { return QString::fromLatin1((const char*)m_pChars, m_nLength); }

    void PrintMe(FILE *fd, int nTabs) const;

protected:
    int m_nLength;
    unsigned char *m_pChars;
};

// A colour is encoded as a string or the index into a palette.
// Palettes aren't defined in UK MHEG so the palette index isn't really relevant.
class MHColour {
  public:
    MHColour(): m_nColIndex(-1) {}
    void Initialise(MHParseNode *p, MHEngine *engine);
    void PrintMe(FILE *fd, int nTabs) const;
    bool IsSet() const { return m_nColIndex >= 0 || m_ColStr.Size() != 0; }
    void SetFromString(const char *str, int nLen);
    void Copy(const MHColour &col);
    MHOctetString m_ColStr;
    int m_nColIndex;
};

// An object reference is used to identify and refer to an object.
// Internal objects have the m_pGroupId field empty.
class MHObjectRef
{
  public:
    MHObjectRef() { m_nObjectNo = 0; }
    void Initialise(MHParseNode *p, MHEngine *engine);
    void Copy(const MHObjectRef &objr);
    static MHObjectRef Null;

    // Sometimes the object reference is optional.  This tests if it has been set
    bool IsSet() const { return (m_nObjectNo != 0 || m_GroupId.Size() != 0); }
    void PrintMe(FILE *fd, int nTabs) const;
    bool Equal(const MHObjectRef &objr, MHEngine *engine) const;
    QString Printable() const;

    int m_nObjectNo;
    MHOctetString m_GroupId;
};

// A content reference gives the location (e.g. file name) to find the content.
class MHContentRef
{
  public:
    MHContentRef() {}
    void Initialise(MHParseNode *p, MHEngine *engine);
    void PrintMe(FILE *fd, int nTabs) const { m_ContentRef.PrintMe(fd, nTabs); }
    void Copy(const MHContentRef &cr) { m_ContentRef.Copy(cr.m_ContentRef); }
    bool IsSet() const { return m_ContentRef.Size() != 0; }
    bool Equal(const MHContentRef &cr, MHEngine *engine) const;
    static MHContentRef Null;
    QString Printable() const { return m_ContentRef.Printable(); }

    MHOctetString m_ContentRef;
};

// "Generic" versions of int, bool etc can be either the value or an indirect reference.
class MHGenericBase
{
  public:
    MHObjectRef *GetReference(); // Return the indirect reference or fail if it's direct
    bool    m_fIsDirect;
protected:
    MHObjectRef m_Indirect;
};

class MHGenericBoolean: public MHGenericBase
{
  public:
    MHGenericBoolean() : m_fDirect(false) {}
    void Initialise(MHParseNode *p, MHEngine *engine);
    void PrintMe(FILE *fd, int nTabs) const;
    bool GetValue(MHEngine *engine) const; // Return the value, looking up any indirect ref.
protected:
    bool    m_fDirect;
};

class MHGenericInteger: public MHGenericBase
{
  public:
    MHGenericInteger() : m_nDirect(-1) {}
    void Initialise(MHParseNode *p, MHEngine *engine);
    void PrintMe(FILE *fd, int nTabs) const;
    int GetValue(MHEngine *engine) const; // Return the value, looking up any indirect ref.
protected:
    int     m_nDirect;
};

class MHGenericOctetString: public MHGenericBase
{
  public:
    MHGenericOctetString() {}
    void Initialise(MHParseNode *p, MHEngine *engine);
    void PrintMe(FILE *fd, int nTabs) const;
    void GetValue(MHOctetString &str, MHEngine *engine) const; // Return the value, looking up any indirect ref.
protected:
    MHOctetString   m_Direct;
};

class MHGenericObjectRef: public MHGenericBase
{
  public:
    MHGenericObjectRef() {}
    void Initialise(MHParseNode *p, MHEngine *engine);
    void PrintMe(FILE *fd, int nTabs) const;
    void GetValue(MHObjectRef &ref, MHEngine *engine) const; // Return the value, looking up any indirect ref.
protected:
    MHObjectRef m_ObjRef;
};

class MHGenericContentRef: public MHGenericBase
{
  public:
    MHGenericContentRef() {}
    void Initialise(MHParseNode *p, MHEngine *engine);
    void PrintMe(FILE *fd, int nTabs) const;
    void GetValue(MHContentRef &ref, MHEngine *engine) const; // Return the value, looking up any indirect ref.
protected:
    MHContentRef    m_Direct;
};

// In certain cases (e.g. parameters to Call) we have values which are the union of the base types.
class MHParameter
{
  public:
    MHParameter(): m_Type(P_Null) {}
    void Initialise(MHParseNode *p, MHEngine *engine);
    void PrintMe(FILE *fd, int nTabs) const;
    MHObjectRef *GetReference(); // Get an indirect reference.

    enum ParamTypes { P_Int, P_Bool, P_String, P_ObjRef, P_ContentRef, P_Null } m_Type; // Null is used when this is optional

    MHGenericInteger        m_IntVal;
    MHGenericBoolean        m_BoolVal;
    MHGenericOctetString    m_StrVal;
    MHGenericObjectRef      m_ObjRefVal;
    MHGenericContentRef     m_ContentRefVal;
};

// A union type.  Returned when a parameter is evaluated.
class MHUnion
{
  public:
    MHUnion() : m_Type(U_None), m_nIntVal(0), m_fBoolVal(false) {}
    MHUnion(int nVal) : m_Type(U_Int), m_nIntVal(nVal), m_fBoolVal(false) {}
    MHUnion(bool fVal) : m_Type(U_Bool), m_nIntVal(0), m_fBoolVal(fVal)  {}
    MHUnion(const MHOctetString &strVal) : m_Type(U_String), m_nIntVal(0), m_fBoolVal(false) { m_StrVal.Copy(strVal); }
    MHUnion(const MHObjectRef &objVal) : m_Type(U_ObjRef), m_nIntVal(0), m_fBoolVal(false) { m_ObjRefVal.Copy(objVal); };
    MHUnion(const MHContentRef &cnVal) : m_Type(U_ContentRef), m_nIntVal(0), m_fBoolVal(false) { m_ContentRefVal.Copy(cnVal); }

    void GetValueFrom(const MHParameter &value, MHEngine *engine); // Copies the argument, getting the value of an indirect args.
    QString Printable() const;

    enum UnionTypes { U_Int, U_Bool, U_String, U_ObjRef, U_ContentRef, U_None } m_Type;
    void CheckType (enum UnionTypes) const; // Check a type and fail if it doesn't match. 
    static const char *GetAsString(enum UnionTypes t);

    int             m_nIntVal;
    bool            m_fBoolVal;
    MHOctetString   m_StrVal;
    MHObjectRef     m_ObjRefVal;
    MHContentRef    m_ContentRefVal;
};

class MHFontBody {
    // A font body can either be a string or an object reference
  public:
    MHFontBody() {}
    void Initialise(MHParseNode *p, MHEngine *engine);
    void PrintMe(FILE *fd, int nTabs) const;
    bool IsSet() const { return m_DirFont.Size() != 0 || m_IndirFont.IsSet(); }
    void Copy(const MHFontBody &fb);
protected:
    MHOctetString m_DirFont;
    MHObjectRef m_IndirFont;
};

// This is used only in DynamicLineArt
class MHPointArg {
  public:
    MHPointArg() {}
    void Initialise(MHParseNode *p, MHEngine *engine);
    void PrintMe(FILE *fd, int nTabs) const;
    MHGenericInteger x, y;
};

#endif
