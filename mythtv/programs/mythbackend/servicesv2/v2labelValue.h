//////////////////////////////////////////////////////////////////////////////
// Program Name: labelValue.h
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2LABELVALUE_H_
#define V2LABELVALUE_H_

#include "libmythbase/http/mythhttpservice.h"

class V2LabelValue : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    SERVICE_PROPERTY2( QString  , Label       )
    SERVICE_PROPERTY2( QString  , Value       )
    SERVICE_PROPERTY2( QString  , Description )
    SERVICE_PROPERTY2( bool     , Active      )
    SERVICE_PROPERTY2( bool     , Selected    );

    public:

        Q_INVOKABLE V2LabelValue(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2LabelValue *src )
        {
            m_Label       = src->m_Label       ;
            m_Value       = src->m_Value       ;
            m_Description = src->m_Description ;
            m_Active      = src->m_Active      ;
            m_Selected    = src->m_Selected    ;
        }

    private:
        Q_DISABLE_COPY(V2LabelValue);
};

Q_DECLARE_METATYPE(V2LabelValue*)

#endif
