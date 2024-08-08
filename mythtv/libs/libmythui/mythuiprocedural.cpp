// Qt
#include <QDomDocument>
#include <QCryptographicHash>
#include <QTextStream>

// MythTV
#include "libmythbase/mythlogging.h"
#include "mythpainter.h"
#include "mythuiprocedural.h"
#include "mythuihelper.h"

/*! \brief A procedural texture class.
 *
 * Data to provide to any shader; assuming projection already handled:-
 *
 *                  Type             Bytes          Notes
 *  Transform       16*float(4x4)    64
 *  Vertices        4*float          16
 *  Texcoords       4*float          16             Optional
 *  Alpha           float            4
 *  Seconds         uint             4              Seconds since epoch to prog start
 *  Milliseconds    uint             4              Centiseconds since prog start
 *  Visible         uint             4
 *  Activated       uint             4
 *  Deactivated     uint             4
 *  Hidden          uint             4
 *  Index           ushort           2
 *  Elements        ushort           2
 *  Total                            128
 *  Max Vulkan                       128
 *
 * Full millisecond accuracy requires 64bit - or 8bytes - but 64bit integer
 * support is limited.
 * Max 32bit int:        2,147,483,647
 * Centiseconds in day:      8,640,000 - wraps after 248.5 days
 * Milliseconds in day:     86,400,000 - wraps after 24.8 days
*/
MythUIProcedural::MythUIProcedural(MythUIType* Parent, const QString& Name)
  : MythUIType(Parent, Name)
{
}

void MythUIProcedural::DrawSelf(MythPainter* Painter, int XOffset, int YOffset,
                                int AlphaMod, QRect ClipRect)
{
    QRect area = GetArea();
    area.translate(XOffset, YOffset);
    Painter->SetClipRect(ClipRect);
    Painter->DrawProcedural(area, AlphaMod, m_vertexSource, m_fragmentSource, m_hash);
}

bool MythUIProcedural::ParseElement(const QString& FileName, QDomElement& Element, bool ShowWarnings)
{
    if (Element.tagName() == "vertexsource")
    {
        m_vertexSource = LoadShaderSource(parseText(Element));
    }
    else if (Element.tagName() == "fragmentsource")
    {
        m_fragmentSource = LoadShaderSource(parseText(Element));
    }
    else
    {
        return MythUIType::ParseElement(FileName, Element, ShowWarnings);
    }

    return true;
}

void MythUIProcedural::CopyFrom(MythUIType* Base)
{
    auto * proc = dynamic_cast<MythUIProcedural*>(Base);
    if (proc)
    {
        m_vertexSource = proc->m_vertexSource;
        m_fragmentSource = proc->m_fragmentSource;
        m_hash = proc->m_hash;
    }
    MythUIType::CopyFrom(Base);
}

void MythUIProcedural::CreateCopy(MythUIType* Parent)
{
    auto * proc = new MythUIProcedural(Parent, objectName());
    proc->CopyFrom(this);
}

void MythUIProcedural::Pulse()
{
    MythUIType::Pulse();
    SetRedraw();
}

void MythUIProcedural::Finalize(void)
{
    if (!m_vertexSource)
        LOG(VB_GENERAL, LOG_WARNING, "Failed to retrieve vertex source code for procedural texture");

    if (!m_fragmentSource)
        LOG(VB_GENERAL, LOG_WARNING, "Failed to retrieve fragment source code for procedural texture");

    if (m_vertexSource && m_fragmentSource)
        m_hash = QCryptographicHash::hash(*m_vertexSource + *m_fragmentSource,
                                          QCryptographicHash::Md5);
}

ShaderSource MythUIProcedural::LoadShaderSource(const QString &filename)
{
    QFile f(GetMythUI()->GetThemeDir() + '/' + filename);

    if (!f.open(QFile::ReadOnly | QFile::Text))
    {
        LOG(VB_GENERAL, LOG_WARNING, "Failed to load source code for procedural texture from " + filename);
        return nullptr;
    }

    QTextStream in(&f);
    QString program = in.readAll();
    return std::make_shared<QByteArray>(program.toLatin1().constData());
}
