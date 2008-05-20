/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Author(s):
 *      John Pullan, Matthew Wire
 *
 * Description:
 */

#ifndef IMPORTICONS_H
#define IMPORTICONS_H

#include <qsqldatabase.h>
#include <qurl.h>

#include "settings.h"

class ImportIconsWizard : public QObject, public ConfigurationWizard
{
    Q_OBJECT
public:
    ImportIconsWizard(bool fRefresh, QString channelname=""); //!< constructs an ImportIconWizard
    MythDialog *dialogWidget(MythMainWindow *parent, const char *widgetName);

    int exec();

private:

    enum dialogState
    {
        STATE_NORMAL,
        STATE_SEARCHING,
        STATE_DISABLED
    };

    struct CSVEntry                  //! describes the TV channel name
    {
        QString strChanId;           //!< local channel id
        QString strName;             //!< channel name
        QString strXmlTvId;          //!< the xmltvid
        QString strCallsign;         //!< callsign
        QString strTransportId;      //!< transport id
        QString strAtscMajorChan;    //!< ATSC major number
        QString strAtscMinorChan;    //!< ATSC minor number
        QString strNetworkId;        //!< network id
        QString strServiceId;        //!< service id
        QString strIconCSV;          //!< icon name (csv form)
        QString strNameCSV;          //!< name (csv form)
    };
    //! List of CSV entries
    typedef QValueList<CSVEntry> ListEntries;
    //! iterator over list of CSV entries
    typedef QValueListIterator<CSVEntry> ListEntriesIter;

    ListEntries m_listEntries;       //!< list of TV channels to search for
    ListEntries m_missingEntries;    //!< list of TV channels with no unique icon
    ListEntriesIter m_iter;          //!< the current iterator
    ListEntriesIter m_missingIter;

    struct SearchEntry               //! search entry results
    {
        QString strID;               //!< the remote channel id
        QString strName;             //!< the remote name
        QString strLogo;             //!< the actual logo
    };
    //! List of SearchEntry entries
    typedef QValueList<SearchEntry> ListSearchEntries;
    //! iterator over list of SearchEntry entries
    typedef QValueListIterator<SearchEntry> ListSearchEntriesIter;

    ListSearchEntries m_listSearch;  //!< the list of SearchEntry
    QString m_strMatches;            //!< the string for the submit() call

    static const QString url;        //!< the default url
    QString m_strChannelDir;         //!< the location of the channel icon dir
    QString m_strChannelname;        //!< the channel name if searching for a single channel icon

    bool m_fRefresh;                 //!< are we doing a refresh or not
    int m_nMaxCount;                 //!< the maximum number of TV channels
    int m_nCount;                    //!< the current search point (0..m_nMaxCount)
    int m_missingMaxCount;           //!< the total number of missing icons
    int m_missingCount;              //!< the current search point (0..m_missingCount)

    void startDialog();

    /*! \brief changes a string into csv format
     * \param str the string to change
     * \return the actual string
     */
    QString escape_csv(const QString& str);

    /*! \brief extracts the csv values out of a string
     * \param str the string to work on
     * \return the actual QStringList
     */
    QStringList extract_csv(const QString& strLine);

    /*! \brief use the equivalent of wget to fetch the POST command
     * \param url the url to send this to
     * \param strParam the string to send
     * \return the actual string
     */
    QString wget(QUrl& url,const QString& strParam);

    TransLineEditSetting *m_editName;    //!< name field for the icon
    TransListBoxSetting *m_listIcons;    //!< list of potential icons
    TransLineEditSetting *m_editManual;  //!< manual edit field
    TransButtonSetting *m_buttonManual;  //!< manual button field
    TransButtonSetting *m_buttonSkip;    //!< button skip
    TransButtonSetting *m_buttonSelect;    //!< button skip

    /*! \brief determines if a particular icon is blocked
     * \param str the string to work on
     * \return true/false
     */
    bool isBlocked(const QString& strParam);

    /*! \brief looks up the string to determine the caller/xmltvid
     * \param str the string to work on
     * \return true/false
     */
    bool lookup(const QString& strParam);

    /*! \brief search the remote db for icons etc
     * \param str the string to work on
     * \return true/false
     */
    bool search(const QString& strParam);

    /*! \brief submit the icon information back to the remote db
     * \param str the string to work on
     * \return true/false
     */
    bool submit(const QString& strParam);

    /*! \brief retrieve the actual logo for the TV channel
     * \param str the string to work on
     * \return true/false
     */
    bool findmissing(const QString& strParam);

    /*! \brief checks and attempts to download the logo file to the appropriate
     *   place
     * \param str the string of the downloaded url
     * \param localChanId the local ID number of the channel
     * \return true/false
     */
    bool checkAndDownload(const QString& str, const QString& localChanId);

    /*! \brief attempt the inital load of the TV channel information
     * \return the number of TV channels
     */
    uint initialLoad(QString name="");

    /*! \brief attempts to move the itaration on one/more than one
     * \return true if we can go again or false if we can not
     */
    bool doLoad();

    bool m_closeDialog;

    ~ImportIconsWizard() { };

protected slots:
    void enableControls(dialogState state=STATE_NORMAL, bool selectEnabled=true);         //!< enable/disable the controls
    void manualSearch();           //!< process the manual search
    void menuSelect();
    void menuSelection(int nIndex);//!< process the icon selection
    void skip();                   //!< skip this icon
    void cancelPressed();
    void finishButtonPressed();

};

#endif // IMPORTICONS_H
