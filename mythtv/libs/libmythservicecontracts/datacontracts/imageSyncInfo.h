#ifndef IMAGESYNCINFO_H
#define IMAGESYNCINFO_H

#include <QString>

#include "libmythservicecontracts/serviceexp.h"
#include "libmythservicecontracts/datacontracthelper.h"



namespace DTC
{

class SERVICE_PUBLIC ImageSyncInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.00" )

    Q_PROPERTY( bool            Running     READ Running        WRITE setRunning    )
    Q_PROPERTY( int             Current     READ Current        WRITE setCurrent    )
    Q_PROPERTY( int             Total       READ Total          WRITE setTotal      )

    PROPERTYIMP    ( bool       , Running      )
    PROPERTYIMP    ( int        , Current      )
    PROPERTYIMP    ( int        , Total        )

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE ImageSyncInfo(QObject *parent = nullptr)
                        : QObject         ( parent ),
                          m_Running       ( false  ),
                          m_Current       ( 0 ),
                          m_Total         ( 0 )
        {
        }

        void Copy( const ImageSyncInfo *src )
        {
            m_Running       = src->m_Running;
            m_Current       = src->m_Current;
            m_Total         = src->m_Total;
        }

    private:
        Q_DISABLE_COPY(ImageSyncInfo);
};

inline void ImageSyncInfo::InitializeCustomTypes()
{
    qRegisterMetaType< ImageSyncInfo* >();
}

} // namespace DTC

#endif // IMAGESYNCINFO_H
