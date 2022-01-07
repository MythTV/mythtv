#ifndef V2RECRULEFILTER_H_
#define V2RECRULEFILTER_H_

#include <QString>
#include <QDateTime>

#include "libmythbase/http/mythhttpservice.h"


/////////////////////////////////////////////////////////////////////////////

class V2RecRuleFilter : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.0" );

    SERVICE_PROPERTY2( int        , Id             )
    SERVICE_PROPERTY2( QString    , Description    )

    public:

        Q_INVOKABLE V2RecRuleFilter(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2RecRuleFilter *src )
        {
            m_Id            = src->m_Id            ;
            m_Description   = src->m_Description   ;
        }

    private:
        Q_DISABLE_COPY(V2RecRuleFilter);
};

Q_DECLARE_METATYPE(V2RecRuleFilter*)

#endif
