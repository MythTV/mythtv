#ifndef VIDEOMULTIPLEX_H_
#define VIDEOMULTIPLEX_H_

#include <QString>
#include <QDateTime>

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC VideoMultiplex : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.1" );

    Q_PROPERTY( int             MplexId          READ MplexId          WRITE setMplexId           )
    Q_PROPERTY( int             SourceId         READ SourceId         WRITE setSourceId          )
    Q_PROPERTY( int             TransportId      READ TransportId      WRITE setTransportId       )
    Q_PROPERTY( int             NetworkId        READ NetworkId        WRITE setNetworkId         )
    Q_PROPERTY( long long       Frequency        READ Frequency        WRITE setFrequency         )
    Q_PROPERTY( QString         Inversion        READ Inversion        WRITE setInversion         )
    Q_PROPERTY( long long       SymbolRate       READ SymbolRate       WRITE setSymbolRate        )
    Q_PROPERTY( QString         FEC              READ FEC              WRITE setFEC               )
    Q_PROPERTY( QString         Polarity         READ Polarity         WRITE setPolarity          )
    Q_PROPERTY( QString         Modulation       READ Modulation       WRITE setModulation        )
    Q_PROPERTY( QString         Bandwidth        READ Bandwidth        WRITE setBandwidth         )
    Q_PROPERTY( QString         LPCodeRate       READ LPCodeRate       WRITE setLPCodeRate        )
    Q_PROPERTY( QString         HPCodeRate       READ HPCodeRate       WRITE setHPCodeRate        )
    Q_PROPERTY( QString         TransmissionMode READ TransmissionMode WRITE setTransmissionMode  )
    Q_PROPERTY( QString         GuardInterval    READ GuardInterval    WRITE setGuardInterval     )
    Q_PROPERTY( bool            Visible          READ Visible          WRITE setVisible           )
    Q_PROPERTY( QString         Constellation    READ Constellation    WRITE setConstellation     )
    Q_PROPERTY( QString         Hierarchy        READ Hierarchy        WRITE setHierarchy         )
    Q_PROPERTY( QString         ModulationSystem READ ModulationSystem WRITE setModulationSystem  )
    Q_PROPERTY( QString         RollOff          READ RollOff          WRITE setRollOff           )
    Q_PROPERTY( QString         SIStandard       READ SIStandard       WRITE setSIStandard        )
    Q_PROPERTY( int             ServiceVersion   READ ServiceVersion   WRITE setServiceVersion    )
    Q_PROPERTY( QDateTime       UpdateTimeStamp  READ UpdateTimeStamp  WRITE setUpdateTimeStamp   )
    Q_PROPERTY( QString         DefaultAuthority READ DefaultAuthority WRITE setDefaultAuthority  )

    PROPERTYIMP    ( int        , MplexId          )
    PROPERTYIMP    ( int        , SourceId         )
    PROPERTYIMP    ( int        , TransportId      )
    PROPERTYIMP    ( int        , NetworkId        )
    PROPERTYIMP    ( long long  , Frequency        )
    PROPERTYIMP    ( QString    , Inversion        )
    PROPERTYIMP    ( long long  , SymbolRate       )
    PROPERTYIMP    ( QString    , FEC              )
    PROPERTYIMP    ( QString    , Polarity         )
    PROPERTYIMP    ( QString    , Modulation       )
    PROPERTYIMP    ( QString    , Bandwidth        )
    PROPERTYIMP    ( QString    , LPCodeRate       )
    PROPERTYIMP    ( QString    , HPCodeRate       )
    PROPERTYIMP    ( QString    , TransmissionMode )
    PROPERTYIMP    ( QString    , GuardInterval    )
    PROPERTYIMP    ( bool       , Visible          )
    PROPERTYIMP    ( QString    , Constellation    )
    PROPERTYIMP    ( QString    , Hierarchy        )
    PROPERTYIMP    ( QString    , ModulationSystem )
    PROPERTYIMP    ( QString    , RollOff          )
    PROPERTYIMP    ( QString    , SIStandard       )
    PROPERTYIMP    ( int        , ServiceVersion   )
    PROPERTYIMP    ( QDateTime  , UpdateTimeStamp  )
    PROPERTYIMP    ( QString    , DefaultAuthority )

    public:

        static inline void InitializeCustomTypes();

    public:

        VideoMultiplex(QObject *parent = 0)
            : QObject         ( parent ),
              m_MplexId       ( 0      )
        {
        }

        VideoMultiplex( const VideoMultiplex &src )
        {
            Copy( src );
        }

        void Copy( const VideoMultiplex &src )
        {
            m_MplexId          = src.m_MplexId          ;
            m_SourceId         = src.m_SourceId         ;
            m_TransportId      = src.m_TransportId      ;
            m_NetworkId        = src.m_NetworkId        ;
            m_Frequency        = src.m_Frequency        ;
            m_Inversion        = src.m_Inversion        ;
            m_SymbolRate       = src.m_SymbolRate       ;
            m_FEC              = src.m_FEC              ;
            m_Polarity         = src.m_Polarity         ;
            m_Modulation       = src.m_Modulation       ;
            m_Bandwidth        = src.m_Bandwidth        ;
            m_LPCodeRate       = src.m_LPCodeRate       ;
            m_HPCodeRate       = src.m_HPCodeRate       ;
            m_TransmissionMode = src.m_TransmissionMode ;
            m_GuardInterval    = src.m_GuardInterval    ;
            m_Visible          = src.m_Visible          ;
            m_Constellation    = src.m_Constellation    ;
            m_Hierarchy        = src.m_Hierarchy        ;
            m_ModulationSystem = src.m_ModulationSystem ;
            m_RollOff          = src.m_RollOff          ;
            m_SIStandard       = src.m_SIStandard       ;
            m_ServiceVersion   = src.m_ServiceVersion   ;
            m_UpdateTimeStamp  = src.m_UpdateTimeStamp  ;
            m_DefaultAuthority = src.m_DefaultAuthority ;
        }
};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::VideoMultiplex  )
Q_DECLARE_METATYPE( DTC::VideoMultiplex* )

namespace DTC
{
inline void VideoMultiplex::InitializeCustomTypes()
{
    qRegisterMetaType< VideoMultiplex   >();
    qRegisterMetaType< VideoMultiplex*  >();
}
}

#endif
