/*!
 * \namespace MythRecordings
 * \brief Namespace for functions used in the 'Recordings' page, recordings.qsp
 */
var MythRecordings = new function() {

    /*!
    * \fn Init
    * \public
    *
    * Initialise, called by the window load event once the page has finished
    * loading
    */
    this.Init = function()
    {
        RegisterMythEventHandler();
    };

    /*!
    * \fn Destructor
    * \public
    */
    this.Destructor = function()
    {
        DeregisterMythEventHandler();
    };

   /*!
    * \fn DeleteRecording
    * \param int The ID of the recording
    * \param bool True if the programme should be re-recorded in the future
    * \param bool True if we should force deletion, bypassing the 'wastebin'
    * \public
    *
    * Delete the given recording by recording ID.
    *
    * This removes the file from filesystem!
    */
    this.DeleteRecording = function (recordedId, allowRerecord, forceDelete)
    {
        hideMenu("optMenu");
        $("#wastebin").show();

        var reRecord = allowRerecord ? 1 : 0;
        var force = forceDelete ? 1 : 0;
        var url = "/tv/ajax_backends/dvr_util.qsp?_action=deleteRecording&RecordedId=" + recordedId +
                "&allowRerecord=" + reRecord + "&forceDelete=" + force;
        var ajaxRequest = $.ajax( url )
                                .done(function()
                                {
                                    MythRecordings.RecordingDeleted( ajaxRequest.responseText, false );
                                });
    }

   /*!
    * \fn UnDeleteRecording
    * \param int The ID of the recording
    * \public
    *
    * Remove the recording from the 'wastebin'. Cannot be completed if the
    * recording was already expired.
    */
    this.UnDeleteRecording = function (recordedId)
    {
        hideMenu("optMenu");

        var url = "/tv/ajax_backends/dvr_util.qsp?_action=unDeleteRecording&RecordedId=" + recordedId;
        var ajaxRequest = $.ajax( url )
                                .done(function()
                                {
                                    MythRecordings.RecordingDeleted( ajaxRequest.responseText, true );
                                });
    }

   /*!
    * \fn RecordingDeleted
    * \param int The ID of the recording
    * \param bool True if the recording has been undeleted
    * \public
    *
    * Recording has been removed from current view, either because it was
    * deleted or in the case of the Deleted Recordings view, undeleted
    */
    this.RecordingDeleted = function (recordedId, unDeleted)
    {
        if (!unDeleted)
            console.log("Recording Deleted: " + recordedId);
        var id = recordedId + "_row";
        $(jq(id)).hide('slide',{direction:'left'},1000);
    }

   /*!
    * \fn RecordingAdded
    * \param int The ID of the recording
    * \public
    *
    * Recording has been added to the current view, either because it has
    * just started or the recording group was changed
    */
    this.RecordingAdded = function (recordedId)
    {
        console.log("Recording Added: " + recordedId);
        // Refresh the page, this isn't the ideal, but it's the easiest solution
        // for now until we refactor a little
        reloadTVContent();
    }

   /*!
    * \fn RecordingUpdated
    * \param int The ID of the recording
    * \public
    *
    * Recording information has been updated
    */
    this.RecordingUpdated = function (recordedId)
    {
        console.log("Recording Updated: " + recordedId);
        // STUB
    }

   /*!
    * \fn PlayInBrowser
    * \param int The ID of the recording
    * \public
    *
    * Play this recording in the browser (HTML 5 Video)
    */
    this.PlayInBrowser = function (recordedId)
    {
        loadFrontendContent("/tv/tvplayer.qsp?RecordedId=" + recordedId);
    }

   /*!
    * \fn PlayOnFrontend
    * \param int The ID of the recording
    * \param string Frontend IP address
    * \param int Frontend internal webserver port (default is 6547)
    * \public
    *
    * Start playback of this recording on the frontend at the IP:PORT
    */
    this.PlayOnFrontend = function (recordedId, ipaddress, port)
    {
        if (ipaddress.length > 15) // IPv6 Address
            ipaddress = "[" + ipaddress + "]"; // Yay for stupid design of IPv6

        var surl = "http://" + ipaddress + ":" + port + "/Frontend/PlayRecording?RecordedId=" + recordedId;
        $.ajax({ url: surl, type: 'POST' });
    }

   /*!
    * \fn GetStream
    * \param int The ID of the recording
    * \param string The stream (playlist) type
    * \public
    *
    * Download a playlist file describing a url to this recording for playback
    * in a third party client which understands such playlists
    */
    this.GetStream = function (recordedId, type)
    {
        window.location.href = "/tv/stream.qsp?MediaType=recording&MediaID=" + recordedId + "&PlaylistFormat=" + type;
    }

    /*!
     * \var wsClient
     * \private
     * WebSocketEventClient object
     */
    var wsClient = {};

    /*!
    * \fn RegisterMythEventHandler
    * \private
    *
    * Register a WebSocketEventClient with the global WebSocketEventHandler
    */
    var RegisterMythEventHandler = function()
    {
        wsClient = new parent.WebSocketEventClient();
        wsClient.name = "Recording List";
        wsClient.eventReceiver = function(event) { HandleMythEvent(event) };
        wsClient.filters = ["MASTER_UPDATE_REC_INFO", "RECORDING_LIST_CHANGE",
                            "UPDATE_FILE_SIZE"];
        parent.globalWSHandler.AddListener(wsClient);
    };

    /*!
    * \fn DeregisterMythEventHandler
    * \private
    *
    * Deregister a WebSocketEventClient with the global WebSocketEventHandler
    */
    var DeregisterMythEventHandler = function()
    {
        parent.globalWSHandler.RemoveListener(wsClient);
    };

    /*!
    * \fn HandleMythEvent
    * \private
    *
    * Handle an incoming MythEvent from the WebSocketEventHandler
    */
    var HandleMythEvent = function(event)
    {
        var tokens = event.split(" ");
        if (!tokens.length)
            return;
        // TODO: Add some information to the event so we can decide whether
        // the current page needs reloading.
        if (tokens[0] == "MASTER_UPDATE_REC_INFO")
        {
            if (tokens.length < 2)
                return;
            var recordedId = tokens[1];
        }
        else if (tokens[0] == "RECORDING_LIST_CHANGE")
        {
            if (tokens.length < 3)
                return;
            var type = tokens[1];
            var recordedId = tokens[2];
            if (type == "ADD")
            {
                MythRecordings.RecordingAdded(recordedId);
            }
            else if (type == "DELETE")
            {
                MythRecordings.RecordingDeleted(recordedId);
            }
            else if (type == "UPDATE")
            {
                MythRecordings.RecordingUpdated(recordedId);
            }
        }
    };
};

window.addEventListener("load", MythRecordings.Init);
window.addEventListener("unload", MythRecordings.Destructor);
