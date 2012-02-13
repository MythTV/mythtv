//////////////////////////////////////////////////////////////////////////////
// Program Name: labelValue.h
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
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

        static void InitializeCustomTypes()
        {
            qRegisterMetaType< LabelValue  >();
            qRegisterMetaType< LabelValue* >();
        }

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

#endif
