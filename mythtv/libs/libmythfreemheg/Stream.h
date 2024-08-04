/* Stream.h

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

#if !defined(STREAM_H)
#define STREAM_H

#include "Presentable.h"
// Dependencies
#include "Ingredients.h"
#include "Root.h"
#include "BaseClasses.h"
#include "Actions.h"

class MHStream : public MHPresentable  
{
  public:
    MHStream() = default;
    const char *ClassName() override // MHRoot
        { return "Stream"; }
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHIngredient
    void PrintMe(FILE *fd, int nTabs) const override; // MHIngredient

    void Preparation(MHEngine *engine) override; // MHIngredient
    void Activation(MHEngine *engine) override; // MHRoot
    void Deactivation(MHEngine *engine) override; // MHRoot
    void Destruction(MHEngine *engine) override; // MHIngredient
    void ContentPreparation(MHEngine *engine) override; // MHIngredient

    MHRoot *FindByObjectNo(int n) override; // MHRoot

    void BeginPlaying(MHEngine *engine) override; // MHPresentable
    void StopPlaying(MHEngine *engine) override; // MHPresentable

    // Actions
    void GetCounterPosition(MHRoot *pResult, MHEngine *engine) override; // MHRoot
    void GetCounterMaxPosition(MHRoot *pResult, MHEngine *engine) override; // MHRoot
    void SetCounterPosition(int /*pos*/, MHEngine *engine) override; // MHRoot
    void SetSpeed(int speed, MHEngine *engine) override; // MHRoot

  protected:
    MHOwnPtrSequence <MHPresentable> m_multiplex;
    enum Storage : std::uint8_t
                 { ST_Mem = 1, ST_Stream = 2 } m_nStorage {ST_Stream};
    int         m_nLooping {0}; // Infinity
};


class MHAudio : public MHPresentable  
{
  public:
    MHAudio() = default;
    const char *ClassName() override // MHRoot
        { return "Audio"; }
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHIngredient
    void PrintMe(FILE *fd, int nTabs) const override; // MHIngredient

    void Activation(MHEngine *engine) override; // MHRoot
    void Deactivation(MHEngine *engine) override; // MHRoot

    void BeginPlaying(MHEngine *engine) override; // MHPresentable
    void StopPlaying(MHEngine *engine) override; // MHPresentable

  protected:
    int  m_nComponentTag  {0};
    int  m_nOriginalVol   {0};

    bool m_fStreamPlaying {false};
};

class MHVideo : public MHVisible  
{
  public:
    MHVideo() = default;
    const char *ClassName() override // MHRoot
        { return "Video"; }
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHVisible
    void PrintMe(FILE *fd, int nTabs) const override; // MHVisible

    void Preparation(MHEngine *engine) override; // MHVisible
    void ContentPreparation(MHEngine *engine) override; // MHIngredient

    void Activation(MHEngine *engine) override; // MHVisible
    void Deactivation(MHEngine *engine) override; // MHVisible

    void Display(MHEngine *engine) override; // MHVisible
    QRegion GetVisibleArea() override; // MHVisible
    QRegion GetOpaqueArea() override // MHVisible
        { return GetVisibleArea(); } // Fully opaque.

    // Actions.
    void ScaleVideo(int xScale, int yScale, MHEngine *engine) override; // MHRoot
    void SetVideoDecodeOffset(int newXOffset, int newYOffset, MHEngine *engine) override; // MHRoot
    void GetVideoDecodeOffset(MHRoot *pXOffset, MHRoot *pYOffset, MHEngine *engine) override; // MHRoot

    void BeginPlaying(MHEngine *engine) override; // MHPresentable
    void StopPlaying(MHEngine *engine) override; // MHPresentable

  protected:
    int m_nComponentTag      {0};
    enum Termination : std::uint8_t
                     { VI_Freeze = 1, VI_Disappear } m_termination {VI_Disappear};
    // Added in UK MHEG
    int     m_nXDecodeOffset {0};
    int     m_nYDecodeOffset {0};
    int     m_nDecodeWidth   {0};
    int     m_nDecodeHeight  {0};

    bool m_fStreamPlaying    {false};
};

// Real-time graphics - not needed for UK MHEG.
class MHRTGraphics : public MHVisible  
{
  public:
    MHRTGraphics() = default;
    const char *ClassName() override // MHRoot
        { return "RTGraphics"; }
    ~MHRTGraphics() override = default;
    void Initialise(MHParseNode *p, MHEngine *engine) override; // MHVisible
    void PrintMe(FILE *fd, int nTabs) const override; // MHVisible
    void Display(MHEngine */*engine*/) override {} // MHVisible - Not supported
};


class MHScaleVideo: public MHActionIntInt {
  public:
    MHScaleVideo(): MHActionIntInt(":ScaleVideo") {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg1, int nArg2) override // MHActionIntInt
        { pTarget->ScaleVideo(nArg1, nArg2, engine); }
};

// Actions added in the UK MHEG profile.
class MHSetVideoDecodeOffset: public MHActionIntInt
{
  public:
    MHSetVideoDecodeOffset(): MHActionIntInt(":SetVideoDecodeOffset") {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg1, int nArg2) override // MHActionIntInt
        { pTarget->SetVideoDecodeOffset(nArg1, nArg2, engine); }
};

class MHGetVideoDecodeOffset: public MHActionObjectRef2
{
  public:
    MHGetVideoDecodeOffset(): MHActionObjectRef2(":GetVideoDecodeOffset")  {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, MHRoot *pArg1, MHRoot *pArg2) override // MHActionObjectRef2
        { pTarget->GetVideoDecodeOffset(pArg1, pArg2, engine); }
};

class MHActionGenericObjectRefFix: public MHActionGenericObjectRef
{
public:
    explicit MHActionGenericObjectRefFix(const char *name) : MHActionGenericObjectRef(name) {}
    void Perform(MHEngine *engine) override; // MHActionGenericObjectRef
};

class MHGetCounterPosition: public MHActionGenericObjectRefFix
{
public:
    MHGetCounterPosition(): MHActionGenericObjectRefFix(":GetCounterPosition")  {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, MHRoot *pArg) override // MHActionGenericObjectRef
        { pTarget->GetCounterPosition(pArg, engine); }
};

class MHGetCounterMaxPosition: public MHActionGenericObjectRefFix
{
public:
    MHGetCounterMaxPosition(): MHActionGenericObjectRefFix(":GetCounterMaxPosition")  {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, MHRoot *pArg) override // MHActionGenericObjectRef
        { pTarget->GetCounterMaxPosition(pArg, engine); }
};

class MHSetCounterPosition: public MHActionInt
{
public:
    MHSetCounterPosition(): MHActionInt(":SetCounterPosition")  {}
    void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg) override // MHActionInt
        { pTarget->SetCounterPosition(nArg, engine); }
};


class MHSetSpeed: public MHElemAction
{
    using base = MHElemAction;
public:
    MHSetSpeed(): base(":SetSpeed") {}
    void Initialise(MHParseNode *p, MHEngine *engine) override { // MHElemAction
        //printf("SetSpeed Initialise args: "); p->PrintMe(stdout);
        base::Initialise(p, engine);
        MHParseNode *pn = p->GetArgN(1);
        if (pn->m_nNodeType == MHParseNode::PNSeq) pn = pn->GetArgN(0);
        m_argument.Initialise(pn, engine);
    }
    void Perform(MHEngine *engine) override { // MHElemAction
        Target(engine)->SetSpeed(m_argument.GetValue(engine), engine);
    }
protected:
    void PrintArgs(FILE *fd, int /*nTabs*/) const override // MHElemAction
        { m_argument.PrintMe(fd, 0); }
    MHGenericInteger m_argument;
};


#endif
