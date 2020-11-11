#ifndef RECRULEFILTER_H_
#define RECRULEFILTER_H_

#include <QString>
#include <QDateTime>

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC RecRuleFilter : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    Q_PROPERTY( int             Id              READ Id               WRITE setId             )
    Q_PROPERTY( QString         Description     READ Description      WRITE setDescription    )

    PROPERTYIMP    ( int        , Id             )
    PROPERTYIMP_REF( QString    , Description    )

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE RecRuleFilter(QObject *parent = nullptr)
            : QObject         ( parent ),
              m_Id            ( 0      )
        {
        }

        void Copy( const RecRuleFilter *src )
        {
            m_Id            = src->m_Id            ;
            m_Description   = src->m_Description   ;
        }

    private:
        Q_DISABLE_COPY(RecRuleFilter);
};

inline void RecRuleFilter::InitializeCustomTypes()
{
    qRegisterMetaType< RecRuleFilter*  >();
}

} // namespace DTC

#endif

