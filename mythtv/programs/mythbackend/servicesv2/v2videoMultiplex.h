#ifndef V2VIDEOMULTIPLEX_H_
#define V2VIDEOMULTIPLEX_H_

#include <QString>
#include <QDateTime>

#include "libmythbase/http/mythhttpservice.h"


/////////////////////////////////////////////////////////////////////////////

class V2VideoMultiplex : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.1" );

    SERVICE_PROPERTY2( int        , MplexId          )
    SERVICE_PROPERTY2( int        , SourceId         )
    SERVICE_PROPERTY2( int        , TransportId      )
    SERVICE_PROPERTY2( int        , NetworkId        )
    SERVICE_PROPERTY2( long long  , Frequency        )
    SERVICE_PROPERTY2( QString    , Inversion        )
    SERVICE_PROPERTY2( long long  , SymbolRate       )
    SERVICE_PROPERTY2( QString    , FEC              )
    SERVICE_PROPERTY2( QString    , Polarity         )
    SERVICE_PROPERTY2( QString    , Modulation       )
    SERVICE_PROPERTY2( QString    , Bandwidth        )
    SERVICE_PROPERTY2( QString    , LPCodeRate       )
    SERVICE_PROPERTY2( QString    , HPCodeRate       )
    SERVICE_PROPERTY2( QString    , TransmissionMode )
    SERVICE_PROPERTY2( QString    , GuardInterval    )
    SERVICE_PROPERTY2( bool       , Visible          )
    SERVICE_PROPERTY2( QString    , Constellation    )
    SERVICE_PROPERTY2( QString    , Hierarchy        )
    SERVICE_PROPERTY2( QString    , ModulationSystem )
    SERVICE_PROPERTY2( QString    , RollOff          )
    SERVICE_PROPERTY2( QString    , SIStandard       )
    SERVICE_PROPERTY2( int        , ServiceVersion   )
    SERVICE_PROPERTY2( QDateTime  , UpdateTimeStamp  )
    SERVICE_PROPERTY2( QString    , DefaultAuthority )
    SERVICE_PROPERTY2( QString    , Description      )

    public:

        Q_INVOKABLE V2VideoMultiplex(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2VideoMultiplex *src )
        {
            m_MplexId          = src->m_MplexId          ;
            m_SourceId         = src->m_SourceId         ;
            m_TransportId      = src->m_TransportId      ;
            m_NetworkId        = src->m_NetworkId        ;
            m_Frequency        = src->m_Frequency        ;
            m_Inversion        = src->m_Inversion        ;
            m_SymbolRate       = src->m_SymbolRate       ;
            m_FEC              = src->m_FEC              ;
            m_Polarity         = src->m_Polarity         ;
            m_Modulation       = src->m_Modulation       ;
            m_Bandwidth        = src->m_Bandwidth        ;
            m_LPCodeRate       = src->m_LPCodeRate       ;
            m_HPCodeRate       = src->m_HPCodeRate       ;
            m_TransmissionMode = src->m_TransmissionMode ;
            m_GuardInterval    = src->m_GuardInterval    ;
            m_Visible          = src->m_Visible          ;
            m_Constellation    = src->m_Constellation    ;
            m_Hierarchy        = src->m_Hierarchy        ;
            m_ModulationSystem = src->m_ModulationSystem ;
            m_RollOff          = src->m_RollOff          ;
            m_SIStandard       = src->m_SIStandard       ;
            m_ServiceVersion   = src->m_ServiceVersion   ;
            m_UpdateTimeStamp  = src->m_UpdateTimeStamp  ;
            m_DefaultAuthority = src->m_DefaultAuthority ;
            m_Description      = src->m_Description      ;
        }

    private:
        Q_DISABLE_COPY(V2VideoMultiplex);
};

Q_DECLARE_METATYPE(V2VideoMultiplex*)


#endif
