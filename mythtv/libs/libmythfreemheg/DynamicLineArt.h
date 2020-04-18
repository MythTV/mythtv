/* DynamicLineArt.h

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


#if !defined(DYNAMICLINEART_H)
#define DYNAMICLINEART_H

#include "Visible.h"
#include "BaseActions.h"

class MHDynamicLineArt;
class MHDLADisplay;

class MHDynamicLineArt : public MHLineArt  
{
  public:
    MHDynamicLineArt() = default;
    ~MHDynamicLineArt() override;
    const char *ClassName() override // MHLineArt
        { return "DynamicLineArt"; }
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHLineArt
    void PrintMe(FILE *fd, int nTabs) const override; // MHLineArt
    void Preparation(MHEngine *engine) override; // MHLineArt

    // Display function.
    void Display(MHEngine *d) override; // MHLineArt
    // Get the opaque area.  This is only opaque if the background is.
    QRegion GetOpaqueArea() override; // MHVisible

    // This action also has the effect of clearing the drawing.
    void SetBoxSize(int nWidth, int nHeight, MHEngine *engine) override; // MHVisible

    // Actions
    void Clear() override; // MHRoot
    // These actions set the properties for subsequent drawing but don't affect anything drawn so far.
    void SetFillColour(const MHColour &colour, MHEngine *engine) override; // MHLineArt
    void SetLineColour(const MHColour &colour, MHEngine *engine) override; // MHLineArt
    void SetLineWidth(int nWidth, MHEngine *engine) override; // MHLineArt
    void SetLineStyle(int nStyle, MHEngine *engine) override; // MHLineArt

    void GetLineWidth(MHRoot *pResult) override // MHRoot
        { pResult->SetVariableValue(m_nLineWidth); }
    void GetLineStyle(MHRoot *pResult) override // MHRoot
        { pResult->SetVariableValue(m_lineStyle); }
    void GetLineColour(MHRoot *pResult) override; // MHRoot
    void GetFillColour(MHRoot *pResult) override; // MHRoot
    void DrawArcSector(bool fIsSector, int x, int y, int width, int height, int start, int arc, MHEngine *engine) override; // MHRoot
    void DrawLine(int x1, int y1, int x2, int y2, MHEngine *engine) override; // MHRoot
    void DrawOval(int x1, int y1, int width, int height, MHEngine *engine) override; // MHRoot
    void DrawRectangle(int x1, int y1, int x2, int y2, MHEngine *engine) override; // MHRoot
    void DrawPoly(bool fIsPolygon, const MHPointVec& xArray, const MHPointVec& yArray, MHEngine *engine) override; // MHRoot

  protected:
    MHDLADisplay *m_picture {nullptr}; // The sequence of drawing actions.
};

// Actions
// Get Line Width - return the current line width.
class MHGetLineWidth: public MHActionObjectRef
{
  public:
    MHGetLineWidth(): MHActionObjectRef(":GetLineWidth")  {}
    void CallAction(MHEngine */*engine*/, MHRoot *pTarget, MHRoot *pResult) override // MHActionObjectRef
        { pTarget->GetLineWidth(pResult); }
};

// Get Line Style - return the current line style.
class MHGetLineStyle: public MHActionObjectRef
{
  public:
    MHGetLineStyle(): MHActionObjectRef(":GetLineStyle")  {}
    void CallAction(MHEngine */*engine*/, MHRoot *pTarget, MHRoot *pResult) override // MHActionObjectRef
        { pTarget->GetLineStyle(pResult); }
};
// Get Line Colour - return the current line colour.
class MHGetLineColour: public MHActionObjectRef
{
  public:
    MHGetLineColour(): MHActionObjectRef(":GetLineColour")  {}
    void CallAction(MHEngine */*engine*/, MHRoot *pTarget, MHRoot *pResult) override // MHActionObjectRef
        { pTarget->GetLineColour(pResult); }
};
// Get Fill Colour - return the current fill colour.
class MHGetFillColour: public MHActionObjectRef
{
  public:
    MHGetFillColour(): MHActionObjectRef(":GetFillColour")  {}
    void CallAction(MHEngine */*engine*/, MHRoot *pTarget, MHRoot *pResult) override // MHActionObjectRef
        { pTarget->GetLineWidth(pResult); }
};
// Clear - reset the drawing
class MHClear: public MHElemAction {
  public:
    MHClear(): MHElemAction(":Clear")  {}
    void Perform(MHEngine *engine) override // MHElemAction
        { Target(engine)->Clear(); }
};
// Draw an arc or a sector (basically a filled arc).
class MHDrawArcSector: public MHActionInt6 {
  public:
    MHDrawArcSector(const char *name, bool fIsSector): MHActionInt6(name), m_fIsSector(fIsSector)  {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg1, int nArg2, int nArg3, int nArg4, int nArg5, int nArg6) override // MHActionInt6
    { pTarget->DrawArcSector(m_fIsSector, nArg1, nArg2, nArg3, nArg4, nArg5, nArg6, engine); }
  protected:
    bool m_fIsSector;
};
// Draw a line.
class MHDrawLine: public MHActionInt4 {
  public:
    MHDrawLine(): MHActionInt4(":DrawLine")  {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg1, int nArg2, int nArg3, int nArg4) override // MHActionInt4
    { pTarget->DrawLine(nArg1, nArg2, nArg3, nArg4, engine); }
};
// Draw an oval.
class MHDrawOval: public MHActionInt4 {
  public:
    MHDrawOval(): MHActionInt4(":DrawOval")  {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg1, int nArg2, int nArg3, int nArg4) override // MHActionInt4
    { pTarget->DrawOval(nArg1, nArg2, nArg3, nArg4, engine); }
};
// Draw a rectangle.
class MHDrawRectangle: public MHActionInt4 {
  public:
    MHDrawRectangle(): MHActionInt4(":DrawRectangle")  {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg1, int nArg2, int nArg3, int nArg4) override // MHActionInt4
    { pTarget->DrawRectangle(nArg1, nArg2, nArg3, nArg4, engine); }
};
// Polygon and PolyLine
class MHDrawPoly: public MHElemAction {
  public:
    MHDrawPoly(const char *name, bool fIsPolygon): MHElemAction(name), m_fIsPolygon(fIsPolygon) {}
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHElemAction
    void Perform(MHEngine *engine) override; // MHElemAction
  protected:
    void PrintArgs(FILE *fd, int nTabs) const override; // MHElemAction
    bool m_fIsPolygon;
    MHOwnPtrSequence<MHPointArg> m_points; // List of points
};

#endif
