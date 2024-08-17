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

#include <cstdlib> // malloc etc.

#include <array>
#include <QString>
#include <vector>

using MHPointVec = std::vector<int>; // Duplicated in freemheg.h

#include "Logging.h" // For MHASSERT

class MHEngine;

// These templates should really be obtained from a library.  They are defined here to
// allow easy porting to a variety of libraries.

// Simple sequence class.
template <class BASE> class MHSequence {
    public:
        MHSequence() = default;
        // The destructor frees the vector but not the elements.
        ~MHSequence() { free(reinterpret_cast<void*>(m_values)); }
        // Get the current size.
        int Size() const { return m_vecSize; }
        // Get an element at a particular index.
        BASE GetAt(int i) const { MHASSERT(i >= 0 && i < m_vecSize); return m_values[i]; }
        BASE operator[](int i) const { return GetAt(i); }
        // Add a new element at position n and move the existing element at that position
        // and anything above it up one place.
        void InsertAt(BASE b, int n) {
            MHASSERT(n >= 0 && n <= m_vecSize);
            // NOLINTNEXTLINE(bugprone-sizeof-expression)
            BASE *ptr = (BASE*)realloc(reinterpret_cast<void*>(m_values), (m_vecSize+1) * sizeof(BASE));
            if (ptr == nullptr) throw "Out of Memory";
            m_values = ptr;
            for (int i = m_vecSize; i > n; i--) m_values[i] = m_values[i-1];
            m_values[n] = b;
            m_vecSize++;
        }
        // Add an element to the end of the sequence.
        void Append(BASE b) { InsertAt(b, m_vecSize); }
        // Remove an element and shift all the others down.
        void RemoveAt(int i) {
            MHASSERT(i >= 0 && i < m_vecSize);
            for (int j = i+1; j < m_vecSize; j++) m_values[j-1] = m_values[j];
            m_vecSize--;
        }
    protected:
        int   m_vecSize {0};
        BASE *m_values  {nullptr};
};

// As above, but it deletes the pointers when the sequence is destroyed.
template <class BASE> class MHOwnPtrSequence: public MHSequence<BASE*> {
    public:
        ~MHOwnPtrSequence() { for(int i = 0; i < MHSequence<BASE*>::m_vecSize; i++) delete(MHSequence<BASE*>::GetAt(i)); }
};


// Simple stack.
template <class BASE> class MHStack: protected MHSequence<BASE> {
    public:
        // Test for empty
        bool Empty() const { return Size() == 0; }
        // Pop an item from the stack.
        BASE Pop() {
            MHASSERT(MHSequence<BASE>::m_vecSize > 0);
            return MHSequence<BASE>::m_values[--MHSequence<BASE>::m_vecSize];
        }
        // Push an element on the stack.
        void Push(BASE b) { this->Append(b); }
        // Return the top of the stack.
        BASE Top() { 
            MHASSERT(MHSequence<BASE>::m_vecSize > 0);
            return MHSequence<BASE>::m_values[MHSequence<BASE>::m_vecSize-1];
        }
        int Size() const { return MHSequence<BASE>::Size(); }
};

class MHParseNode;

// An MHEG octet string is a sequence of bytes which can include nulls.  MHEG, or at least UK MHEG, indexes
// strings from 1.  We use 0 indexing in calls to these functions.
class MHOctetString  
{
  public:
    MHOctetString() = default;
    MHOctetString(const char *str, int nLen = -1); // From character string
    MHOctetString(const unsigned char *str, int nLen); // From byte vector
    MHOctetString(const MHOctetString &str, int nOffset=0, int nLen=-1); // Substring
    MHOctetString(const MHOctetString& o) { Copy(o); }
    virtual ~MHOctetString();

    void Copy(const MHOctetString &str);
    MHOctetString& operator=(const MHOctetString& o)
        { if (this==&o) return *this; Copy(o); return *this; }
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
    int            m_nLength {0};
    unsigned char *m_pChars {nullptr};
};

// A colour is encoded as a string or the index into a palette.
// Palettes aren't defined in UK MHEG so the palette index isn't really relevant.
class MHColour {
  public:
    MHColour() = default;
    void Initialise(MHParseNode *p, MHEngine *engine);
    void PrintMe(FILE *fd, int nTabs) const;
    bool IsSet() const { return m_nColIndex >= 0 || m_colStr.Size() != 0; }
    void SetFromString(const char *str, int nLen);
    void Copy(const MHColour &col);
    MHOctetString m_colStr;
    int           m_nColIndex {-1};
};

// An object reference is used to identify and refer to an object.
// Internal objects have the m_pGroupId field empty.
class MHObjectRef
{
  public:
    MHObjectRef() = default;
    void Initialise(MHParseNode *p, MHEngine *engine);
    void Copy(const MHObjectRef &objr);
    static MHObjectRef Null;

    MHObjectRef& operator=(const MHObjectRef&) = default;

    // Sometimes the object reference is optional.  This tests if it has been set
    bool IsSet() const { return (m_nObjectNo != 0 || m_groupId.Size() != 0); }
    void PrintMe(FILE *fd, int nTabs) const;
    bool Equal(const MHObjectRef &objr, MHEngine *engine) const;
    QString Printable() const;

    int           m_nObjectNo {0};
    MHOctetString m_groupId;
};

// A content reference gives the location (e.g. file name) to find the content.
class MHContentRef
{
  public:
    MHContentRef() = default;

    MHContentRef& operator=(const MHContentRef&) = default;

    void Initialise(MHParseNode *p, MHEngine *engine);
    void PrintMe(FILE *fd, int nTabs) const { m_contentRef.PrintMe(fd, nTabs); }
    void Copy(const MHContentRef &cr) { m_contentRef.Copy(cr.m_contentRef); }
    bool IsSet() const { return m_contentRef.Size() != 0; }
    bool Equal(const MHContentRef &cr, MHEngine *engine) const;
    static MHContentRef Null;
    QString Printable() const { return m_contentRef.Printable(); }

    MHOctetString m_contentRef;
};

// "Generic" versions of int, bool etc can be either the value or an indirect reference.
class MHGenericBase
{
  public:
    MHObjectRef *GetReference(); // Return the indirect reference or fail if it's direct
    bool    m_fIsDirect {false};
protected:
    MHObjectRef m_indirect;
};

class MHGenericBoolean: public MHGenericBase
{
  public:
    MHGenericBoolean() = default;
    void Initialise(MHParseNode *p, MHEngine *engine);
    void PrintMe(FILE *fd, int nTabs) const;
    bool GetValue(MHEngine *engine) const; // Return the value, looking up any indirect ref.
protected:
    bool    m_fDirect {false};
};

class MHGenericInteger: public MHGenericBase
{
  public:
    MHGenericInteger() = default;
    void Initialise(MHParseNode *p, MHEngine *engine);
    void PrintMe(FILE *fd, int nTabs) const;
    int GetValue(MHEngine *engine) const; // Return the value, looking up any indirect ref.
protected:
    int     m_nDirect {-1};
};

class MHGenericOctetString: public MHGenericBase
{
  public:
    MHGenericOctetString() = default;
    void Initialise(MHParseNode *p, MHEngine *engine);
    void PrintMe(FILE *fd, int nTabs) const;
    void GetValue(MHOctetString &str, MHEngine *engine) const; // Return the value, looking up any indirect ref.
protected:
    MHOctetString   m_direct;
};

class MHGenericObjectRef: public MHGenericBase
{
  public:
    MHGenericObjectRef() = default;
    void Initialise(MHParseNode *p, MHEngine *engine);
    void PrintMe(FILE *fd, int nTabs) const;
    void GetValue(MHObjectRef &ref, MHEngine *engine) const; // Return the value, looking up any indirect ref.
protected:
    MHObjectRef m_objRef;
};

class MHGenericContentRef: public MHGenericBase
{
  public:
    MHGenericContentRef() = default;
    void Initialise(MHParseNode *p, MHEngine *engine);
    void PrintMe(FILE *fd, int nTabs) const;
    void GetValue(MHContentRef &ref, MHEngine *engine) const; // Return the value, looking up any indirect ref.
protected:
    MHContentRef    m_direct;
};

// In certain cases (e.g. parameters to Call) we have values which are the union of the base types.
class MHParameter
{
  public:
    MHParameter() = default;
    void Initialise(MHParseNode *p, MHEngine *engine);
    void PrintMe(FILE *fd, int nTabs) const;
    MHObjectRef *GetReference(); // Get an indirect reference.

    enum ParamTypes : std::uint8_t {
        P_Int,
        P_Bool,
        P_String,
        P_ObjRef,
        P_ContentRef,
        P_Null
    } m_Type { P_Null }; // Null is used when this is optional

    MHGenericInteger        m_intVal;
    MHGenericBoolean        m_boolVal;
    MHGenericOctetString    m_strVal;
    MHGenericObjectRef      m_objRefVal;
    MHGenericContentRef     m_contentRefVal;
};

// A union type.  Returned when a parameter is evaluated.
class MHUnion
{
  public:
    MHUnion() = default;
    MHUnion(int nVal) : m_Type(U_Int), m_nIntVal(nVal) {}
    MHUnion(bool fVal) : m_Type(U_Bool),  m_fBoolVal(fVal)  {}
    MHUnion(const MHOctetString &strVal) : m_Type(U_String) { m_strVal.Copy(strVal); }
    MHUnion(const MHObjectRef &objVal) : m_Type(U_ObjRef) { m_objRefVal.Copy(objVal); };
    MHUnion(const MHContentRef &cnVal) : m_Type(U_ContentRef) { m_contentRefVal.Copy(cnVal); }

    MHUnion& operator=(const MHUnion&) = default;

    void GetValueFrom(const MHParameter &value, MHEngine *engine); // Copies the argument, getting the value of an indirect args.
    QString Printable() const;

    enum UnionTypes : std::uint8_t { U_Int, U_Bool, U_String, U_ObjRef, U_ContentRef, U_None } m_Type {U_None};
    void CheckType (enum UnionTypes t) const; // Check a type and fail if it doesn't match.
    static const char *GetAsString(enum UnionTypes t);

    int             m_nIntVal  {0};
    bool            m_fBoolVal {false};
    MHOctetString   m_strVal;
    MHObjectRef     m_objRefVal;
    MHContentRef    m_contentRefVal;
};

class MHFontBody {
    // A font body can either be a string or an object reference
  public:
    MHFontBody() = default;
    void Initialise(MHParseNode *p, MHEngine *engine);
    void PrintMe(FILE *fd, int nTabs) const;
    bool IsSet() const { return m_dirFont.Size() != 0 || m_indirFont.IsSet(); }
    void Copy(const MHFontBody &fb);
protected:
    MHOctetString m_dirFont;
    MHObjectRef m_indirFont;
};

// This is used only in DynamicLineArt
class MHPointArg {
  public:
    MHPointArg() = default;
    void Initialise(MHParseNode *p, MHEngine *engine);
    void PrintMe(FILE *fd, int nTabs) const;
    MHGenericInteger m_x, m_y;
};

#endif
