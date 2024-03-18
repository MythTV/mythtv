#ifndef UPNP_HELPERS_H
#define UPNP_HELPERS_H

#include <QString>
#include <QDateTime>
#include <QDate>
#include <QTime>
#include <QSize>

#include <chrono>
#include <cstdint>

#include "libmythbase/compat.h"

#include "upnpexp.h"

using namespace std::chrono_literals;

// NOTE These are for formatting to the UPnP related specs or extracting data
//      from UPnP formatted strings ONLY.
//
//      * Do NOT clutter it up with stuff which is unrelated to those tasks.
//
//      * If adding or editing please cite the reference to the corresponding
//        part of the specification or RFC alongside the changes and in any
//        commit message.
//
//      * Any changes which fail to cite the specs may be reverted.

/**
 * \brief Helpers for formatting dates and times to UPnP, DLNA and Dublin Core specifications
 *
 * NOTE These are for formatting to the UPnP related specs or extracting data
 *      from UPnP formatted strings ONLY.
 *
 *      * Do NOT clutter it up with stuff which is unrelated to those tasks.
 *
 *      * If adding or editing please cite the reference to the corresponding
 *        part of the specification or RFC alongside the changes and in any
 *        commit message.
 *
 *      * Any changes which fail to cite the specs may be reverted.
 */
namespace UPnPDateTime
{
    //-----------------------------------------------------------------------
    // UPnP ContentDirectory Service 2008, 2013
    //-----------------------------------------------------------------------

    //-----------------------------------------------------------------------
    //    Appendix B. AV Working Committee Properties
    //             B.2 Resource Encoding Characteristics Properties
    //-----------------------------------------------------------------------

    /**
     * res\@duration Format
     *  B.2.1.4 res\@duration - UPnP ContentDirectory Service 2008, 2013
     */
    UPNP_PUBLIC QString resDurationFormat(std::chrono::milliseconds msec);

    //-----------------------------------------------------------------------
    //    Appendix D. EBNF Syntax Definitions
    //             D.1 Date&Time Syntax
    //-----------------------------------------------------------------------

    /**
     * Duration Format
     *
     * UPnP ContentDirectory Service 2008, 2013
     * Appendix D.1 Date&Time Syntax
     */
    UPNP_PUBLIC QString DurationFormat(std::chrono::milliseconds msec);

    /**
     * Time Format
     *
     * UPnP ContentDirectory Service 2008, 2013
     * Appendix D.1 Date&Time Syntax
     */
    UPNP_PUBLIC QString TimeFormat(QTime time);

    /**
     * Time Format
     *
     * UPnP ContentDirectory Service 2008, 2013
     * Appendix D.1 Date&Time Syntax
     */
    UPNP_PUBLIC QString TimeFormat(std::chrono::milliseconds msec);

    /**
     * Date-Time Format
     *
     * UPnP ContentDirectory Service 2008, 2013
     * Appendix D.1 Date&Time Syntax
     */
    UPNP_PUBLIC QString DateTimeFormat(const QDateTime &dateTime);

    /**
     * Named-Day Format
     *
     * UPnP ContentDirectory Service 2008, 2013
     * Appendix D.1 Date&Time Syntax
     */
    UPNP_PUBLIC QString NamedDayFormat(const QDateTime &dateTime);

    /**
     * Named-Day Format
     *
     * UPnP ContentDirectory Service 2008, 2013
     * Appendix D.1 Date&Time Syntax
     */
    UPNP_PUBLIC QString NamedDayFormat(QDate date);
};

/**
 * \brief Helpers for UPnP Protocol 'stuff'
 *
 * NOTE These are for formatting to the UPnP related specs or extracting data
 *      from UPnP formatted strings ONLY.
 *
 *      * Do NOT clutter it up with stuff which is unrelated to those tasks.
 *
 *      * If adding or editing please cite the reference to the corresponding
 *        part of the specification or RFC alongside the changes and in any
 *        commit message.
 *
 *      * Any changes which fail to cite the specs may be reverted.
 */
namespace UPNPProtocol
{
    enum TransferProtocol : std::uint8_t
    {
        kHTTP,
        kRTP
    };
}

/**
 * \brief Helpers for building DLNA flag, strings and profiles
 *
 * NOTE These are for formatting to the DLNA related specs or extracting data
 *      from DLNA formatted strings ONLY.
 *
 *      * Do NOT clutter it up with stuff which is unrelated to those tasks.
 *
 *      * If adding or editing please cite the reference to the corresponding
 *        part of the specification or RFC alongside the changes and in any
 *        commit message.
 *
 *      * Any changes which fail to cite the specs may be reverted.
 */
namespace DLNA
{
    /**
     * \brief Try to determine a valid DLNA profile name for the file based on
     *        the supplied metadata
     *
     * MM protocolInfo values: 4th field
     *
     * Section 7.4.1.3.17
     */
    UPNP_PUBLIC QString DLNAProfileName( const QString &mimeType,
                                         QSize resolution = QSize(),
                                         double videoFrameRate = 0.0,
                                         const QString &container = "",
                                         const QString &vidCodec = "",
                                         const QString &audioCodec = "");


    /**
     * \brief Create a properly formatted string for the 4th field of
     *        res\@protocolInfo
     *
     * MM protocolInfo values: 4th field
     *
     * Section 7.4.1.3.17
     *
     * The order of values in the string is mandatory, so using this helper
     * will ensure compliance
     */
    UPNP_PUBLIC QString DLNAFourthField( UPNPProtocol::TransferProtocol protocol,
                                         const QString &mimeType,
                                         QSize resolution,
                                         double videoFrameRate,
                                         const QString &container,
                                         const QString &vidCodec,
                                         const QString &audioCodec,
                                         bool isTranscoded);

    /**
     * \brief Create a properly formatted string for the 4th field of
     *        res\@protocolInfo
     *
     * MM protocolInfo values: 4th field
     *
     * Section 7.4.1.3.17
     *
     * The order of values in the string is mandatory, so using this helper
     * will ensure compliance
     */
    UPNP_PUBLIC QString ProtocolInfoString( UPNPProtocol::TransferProtocol protocol,
                                            const QString &mimeType,
                                            QSize resolution = QSize(),
                                            double videoFrameRate = 0.0,
                                            const QString &container = "",
                                            const QString &vidCodec = "",
                                            const QString &audioCodec = "",
                                            bool isTranscoded = false);

    /**
     * DLNA FLAGS
     *
     * DLNA Guidelines March 2014
     * Part 1.1 Architectures and Protocols
     * Section 7.4.1.3.24 MM flags-param (flags parameter)
     * Subsection 2
     *
     * Binary bit flags
     *
     * NOTE There are some interdependencies between certain flags and some
     *      are mutally exclusive. FlagsString() should account for these
     *      but care should still be taken. Read the DLNA documentation.
     */
    enum DLNA_Flags
    {
        // NAME                    BIT             BIT #      REFERENCE

        // Sender Pacing (server controls the playback speed)
        //
        // Not currently applicable to MythTV. We don't control the rate of data
        // to the client.
        ksp_flag             = 0x80000000,   // #31 : 7.4.1.3.28 MM sp-flag (sender paced flag)

        // Limited Operations Model
        //
        // NOTE The following are for use under the Limited Operations Model ONLY
        // where the server only supports seeking within limited ranges
        //
        // They don't apply to MythTV which supports the Full Operations Model
        klop_npt             = 0x40000000,  // #30 : 7.4.1.3.29 MM lop-npt (limited operations flags)
        klop_bytes           = 0x20000000,  // #29 : 7.4.1.3.29 MM lop-bytes (limited operations flags)

        // DLNA Play Container support
        kplaycontainer_param = 0x10000000,  // #28 : 7.4.1.3.32 MM playcontainer-param (DLNA PlayContainer flag)

        // In-progress recording or sliding window (circular buffer)
        ks0_increasing       = 0x8000000,   // #27 : 7.4.1.3.33 MM s0-increasing (UCDAM s0 increasing flag)
        ksn_increasing       = 0x4000000,   // #26 : 7.4.1.3.34 MM sn-increasing (UCDAM sn increasing flag)

        // RTP Only
        krtsp_pause          = 0x2000000,   // #25 : (Pause media operation support for RTP Serving Endpoints)

        // Transfer Modes
        ktm_s                = 0x1000000,   // #24 : 7.4.1.3.35 MM tm-s (Streaming Mode Transfer flag)
        ktm_i                = 0x800000,    // #23 : 7.4.1.3.36 MM tm-i (Interactive Mode Transfer flag)
        ktm_b                = 0x400000,    // #22 : 7.4.1.3.37 MM tm-b (Background Mode Transfer flag)

        // TCP Flow & Keep-Alive support
        khttp_stalling       = 0x200000,    // #21 : 7.4.1.3.38 MM http-stalling (HTTP Connection Stalling flag)

        // DLNA 1.5+ Support - Should always be enabled
        kv1_5_flag           = 0x100000,    // #20 : 7.4.1.3.25 MM dlna-v1.5-flag (DLNAv1.5 version flag)

        //
        // Bits 19-18 are currently unused but reserved
        //

        // Link Protection related. DRM basically, not relevant to MythTV
        kLP_flag                = 0x10000,  // #16 : 7.5.3.5 in IEC 62481-3:2013 (Link Protection flag)
        kcleartextbyteseek_full = 0x8000,   // #15 : 7.5.3.6 (GUN TLQ89) in IEC 62481-3:2013 (Byte based full seek data availability with the Cleartext Byte Seek Request Header)
        klop_cleartextbytes     = 0x4000,   // #14 : 7.4.1.3.29 MM lop-cleartextbytes (limited operations flags)

        //
        // Bits 13-1 are currently unused but reserved
        //
    };

    /**
     * \brief Convert an integer composed of DNLA_Flags to a properly formatted
     *        string for use in XML
     *
     * DLNA.ORG_FLAGS - MM flags-param (flags parameter)
     *
     * Section 7.4.1.3.24.1
     */
    UPNP_PUBLIC QString FlagsString(uint32_t flags);

    /**
     * \brief Create a properly formatted Operations Parameter (op-param) string
     *        for the given transport protocol based on what is supported by
     *        MythTV
     *
     * DLNA.ORG_OP - op-param (Operations Parameter for HTTP)
     *
     * Section 7.4.1.3.20
     *
     * DLNA.ORG_OP - op-param (Operations Parameter for RTP)
     *
     * Section 7.4.1.3.21
     */
    UPNP_PUBLIC QString OpParamString(UPNPProtocol::TransferProtocol protocol);

    /**
     * \brief Create a properly formatted Conversion Indicator (ci-param) String
     *
     * DLNA.ORG_CI - ci-param (conversion indicator flag)
     *
     * Section 7.4.1.3.23
     *
     * Indicates whether the file was converted from the original (transcoded)
     * and is therefore of lower quality. It is used by clients to pick between
     * two different versions of the same video when they exist. Currently this
     * does not apply to MythTV but it may do in the future.
     */
    UPNP_PUBLIC QString ConversionIndicatorString(bool wasConverted);
};

#endif // UPNP_HELPERS_H
