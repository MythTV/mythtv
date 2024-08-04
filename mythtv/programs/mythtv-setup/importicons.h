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

// Qt
#include <QDir>
#include <QUrl>

// MythTV
#include "libmythui/mythscreentype.h"

class MythUIButton;
class MythUIButtonList;
class MythUIButtonListItem;
class MythUITextEdit;
class MythUIText;
class MythUIProgressDialog;

class ImportIconsWizard : public MythScreenType
{
    Q_OBJECT

  public:
    ImportIconsWizard(MythScreenStack *parent, bool fRefresh,
                      QString channelname = "");
   ~ImportIconsWizard() override;

    bool Create(void) override; // MythScreenType
    void Load(void) override; // MythScreenType
//    bool keyPressEvent(QKeyEvent *) override; // MythScreenType
    void customEvent(QEvent *event) override; // MythUIType

    struct SearchEntry               //! search entry results
    {
        QString strID;               //!< the remote channel id
        QString strName;             //!< the remote name
        QString strLogo;             //!< the actual logo
    };

  private:

    enum dialogState : std::uint8_t
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
    using ListEntries = QList<CSVEntry>;
    //! iterator over list of CSV entries
    using ListEntriesIter = QList<CSVEntry>::Iterator;

    ListEntries m_listEntries;       //!< list of TV channels to search for
    ListEntries m_missingEntries;    //!< list of TV channels with no unique icon
    ListEntriesIter m_iter;          //!< the current iterator
    ListEntriesIter m_missingIter;

    //! List of SearchEntry entries
    using ListSearchEntries = QList<SearchEntry>;
    //! iterator over list of SearchEntry entries
    using ListSearchEntriesIter = QList<SearchEntry>::Iterator;

    /*! \brief changes a string into csv format
     * \param str the string to change
     * \return the actual string
     */
    static QString escape_csv(const QString& str);

    /*! \brief extracts the csv values out of a string
     * \param strLine the string to work on
     * \return the actual QStringList
     */
    static QStringList extract_csv(const QString& strLine);

    /*! \brief use the equivalent of wget to fetch the POST command
     * \param url the url to send this to
     * \param strParam the string to send
     * \return the actual string
     */
    static QString wget(QUrl& url,const QString& strParam);

    /*! \brief looks up the string to determine the caller/xmltvid
     * \param strParam the string to work on
     * \return true/false
     */
    bool lookup(const QString& strParam);

    /*! \brief search the remote db for icons etc
     * \param strParam the string to work on
     * \return true/false
     */
    bool search(const QString& strParam);

    /*! \brief submit the icon information back to the remote db
     * \return true/false
     */
    bool submit();

    /*! \brief retrieve the actual logo for the TV channel
     * \param strParam str the string to work on
     * \return true/false
     */
    bool findmissing(const QString& strParam);

    /*! \brief checks and attempts to download the logo file to the appropriate
     *   place
     * \param url the icon url
     * \param localChanId the local ID number of the channel
     * \return true/false
     */
    bool checkAndDownload(const QString& url, const QString& localChanId);

    /*! \brief attempt the inital load of the TV channel information
     * \return true if successful
     */
    bool initialLoad(const QString& name="");

    /*! \brief attempts to move the iteration on one/more than one
     * \return true if we can go again or false if we cannot
     */
    bool doLoad();

  protected slots:
    void enableControls(ImportIconsWizard::dialogState state=STATE_NORMAL);         //!< enable/disable the controls
    void manualSearch();           //!< process the manual search
    void menuSelection(MythUIButtonListItem *item);//!< process the icon selection
    void skip();                   //!< skip this icon
    void askSubmit(const QString& strParam);
    void Close() override; // MythScreenType

  private slots:
    void itemChanged(MythUIButtonListItem *item);

  protected:
    void Init(void) override; // MythScreenType

  private:
    ListSearchEntries m_listSearch;  //!< the list of SearchEntry
    QString m_strMatches;            //!< the string for the submit() call

    QString m_strChannelDir;         //!< the location of the channel icon dir
    QString m_strChannelname;        //!< the channel name if searching for a single channel icon
    QString m_strParam;

    bool m_fRefresh       {false};   //!< are we doing a refresh or not
    int m_nMaxCount       {0};       //!< the maximum number of TV channels
    int m_nCount          {0};       //!< the current search point (0..m_nMaxCount)
    int m_missingMaxCount {0};       //!< the total number of missing icons
    int m_missingCount    {0};       //!< the current search point (0..m_missingCount)

                        //!< the default url
    const QString m_url {"http://services.mythtv.org/channel-icon/"}; 
    QDir m_tmpDir;

    void startDialog();

    MythScreenStack      *m_popupStack     {nullptr};
    MythUIProgressDialog *m_progressDialog {nullptr};

    MythUIButtonList     *m_iconsList      {nullptr}; //!< list of potential icons
    MythUITextEdit       *m_manualEdit     {nullptr}; //!< manual edit field
    MythUIText           *m_nameText       {nullptr}; //!< name field for the icon
    MythUIButton         *m_manualButton   {nullptr}; //!< manual button field
    MythUIButton         *m_skipButton     {nullptr}; //!< button skip
    MythUIText           *m_statusText     {nullptr};

    MythUIImage          *m_preview        {nullptr};
    MythUIText           *m_previewtitle   {nullptr};

};

Q_DECLARE_METATYPE(ImportIconsWizard::SearchEntry)

#endif // IMPORTICONS_H
