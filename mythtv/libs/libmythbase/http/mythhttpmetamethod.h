#ifndef MYTHHTTPMETAMETHOD_H
#define MYTHHTTPMETAMETHOD_H

// Qt
#include <QMetaMethod>

// MythTV
#include "http/mythhttptypes.h"

// Std
#include <memory>

class MythHTTPMetaMethod;
using HTTPMethodPtr  = std::shared_ptr<MythHTTPMetaMethod>;
using HTTPMethods    = std::map<QString,HTTPMethodPtr>;
using HTTPProperties = std::map<int,int>;

class MBASE_PUBLIC MythHTTPMetaMethod
{
  public:
    static   HTTPMethodPtr Create (int Index, QMetaMethod& Method, int RequestTypes,
                                   const QString& ReturnName = {}, bool Slot = true);
    void*    CreateParameter      (void* Parameter, int Type, const QString& Value);
    QVariant CreateReturnValue    (int Type, void* Value);

    bool                    m_valid         { false };
    bool                    m_protected     { false };
    int                     m_index         { 0 };
    int                     m_requestTypes  { HTTPUnknown };
    QMetaMethod             m_method;
    std::vector<QString>    m_names;
    std::vector<int>        m_types;
    QString                 m_returnTypeName;

  protected:
    MythHTTPMetaMethod(int Index, QMetaMethod& Method, int RequestTypes,
                       const QString& ReturnName, bool Slot);

  private:
    Q_DISABLE_COPY(MythHTTPMetaMethod)

    static inline bool ValidReturnType(int Type)
    {
        return (Type != QMetaType::UnknownType && Type != QMetaType::Void);
    }

    static inline bool ToBool(const QString& Value)
    {
        if (Value.compare("1", Qt::CaseInsensitive) == 0)
            return true;
        if (Value.compare("y", Qt::CaseInsensitive) == 0)
            return true;
        if (Value.compare("true", Qt::CaseInsensitive) == 0)
            return true;
        return false;
    }
};

#endif
