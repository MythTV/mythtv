//////////////////////////////////////////////////////////////////////////////
// Program Name: labelValue.h
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef LABELVALUE_H_
#define LABELVALUE_H_

#include <QString>

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{

class SERVICE_PUBLIC LabelValue : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    Q_PROPERTY( QString    Label           READ Label
                                           WRITE setLabel       )
    Q_PROPERTY( QString    Value           READ Value
                                           WRITE setValue       )
    Q_PROPERTY( QString    Description     READ Description
                                           WRITE setDescription )
    Q_PROPERTY( bool       Active          READ Active
                                           WRITE setActive      )
    Q_PROPERTY( bool       Selected        READ Selected
                                           WRITE setSelected    )

    PROPERTYIMP( QString  , Label       )
    PROPERTYIMP( QString  , Value       )
    PROPERTYIMP( QString  , Description )
    PROPERTYIMP( bool     , Active      )
    PROPERTYIMP( bool     , Selected    )

    public:

        static inline void InitializeCustomTypes();

    public:

        LabelValue(QObject *parent = 0)
            : QObject       ( parent ),
              m_Label       (       ),
              m_Value       (       ),
              m_Description (       ),
              m_Active      ( false ),
              m_Selected    ( false )
        {
        }

        LabelValue( const LabelValue &src )
        {
            Copy( src );
        }

        void Copy( const LabelValue &src )
        {
            m_Label       = src.m_Label       ;
            m_Value       = src.m_Value       ;
            m_Description = src.m_Description ;
            m_Active      = src.m_Active      ;
            m_Selected    = src.m_Selected    ;
        }
};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::LabelValue )
Q_DECLARE_METATYPE( DTC::LabelValue* )

namespace DTC
{
inline void LabelValue::InitializeCustomTypes()
{
    qRegisterMetaType< LabelValue  >();
    qRegisterMetaType< LabelValue* >();
}
}

#endif
