#ifndef MYTHUIPROCEDURAL_H
#define MYTHUIPROCEDURAL_H

// MythTV
#include "mythuitype.h"

// Std
#include <memory>

using ShaderSource = std::shared_ptr<QByteArray>;

class MythUIProcedural : public MythUIType
{
  public:
    MythUIProcedural(MythUIType* Parent, const QString& Name);

  protected:
    void DrawSelf(MythPainter* Painter, int XOffset, int YOffset, int AlphaMod, QRect ClipRect) override;
    bool ParseElement(const QString& FileName, QDomElement& Element, bool ShowWarnings) override;
    void CopyFrom(MythUIType* Base) override;
    void CreateCopy(MythUIType* Parent) override;
    void Pulse() override;
    void Finalize(void) override; // MythUIType
    static ShaderSource LoadShaderSource(const QString &filename);

  protected:
    QString      m_hash;
    ShaderSource m_vertexSource { nullptr };
    ShaderSource m_fragmentSource { nullptr };
};

#endif
