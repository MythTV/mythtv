
/*!
 * \namespace MythOverview
 * \brief Namespace for functions used in the 'Overview' page, overview.qsp
 */
var MythOverview = new function() {

    /*!
    * \fn Init
    * \public
    *
    * Initialise, called by the window load event once the page has finished
    * loading
    */
    this.Init = function()
    {
        console.log("MythOverview Init");
        LoadGalleria();
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
    * \fn GetFrontendStatus
    * \public
    *
    * Asynchronously fetch the frontend status directly from the frontend and
    * update the page accordingly
    */
    this.GetFrontendStatus = function (name, address)
    {
        var url = "http://" + address + "/Frontend/GetStatus";
        $.ajax({url: url,
                dataType: "xml",
                success: function(data, textStatus, jqXHR) {
                    console.log("Success");
                    $xml = $( data );
                    $stateStrings = $xml.find("String");
                    //console.log($stateStrings);
                    $stateStrings.each(function() {
                        if ($(this).find("Key").text() == "state")
                        {
                            $("#frontendStatus-" + name).html( $(this).find("Value").text() );
                            console.log($(this).find("Key").text() + " : " + $(this).find("Value").text());
                            return;
                        }
                    });
                },
                error: function(jqXHR, textStatus, errorThrown) {
                    console.log("GetFrontendStatus Error: " + errorThrown);
                },
                complete: function() {
                }});
    };

    /*!
    * \fn LoadGalleria
    * \private
    *
    * Load and initialise the galleria plugin
    */
    var LoadGalleria = function ()
    {
        Galleria.loadTheme('/3rdParty/jquery/galleria/themes/classic/galleria.classic.min.js');
        $("#lasttencontent").galleria({
            width: 680,
            height: 440,
            autoplay: 7000,
            imageCrop: true,
            showInfo: true
        });
    };

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
        wsClient.name = "Overview Frontend Status";
        wsClient.eventReceiver = function(event) { HandleMythEvent(event) };
        wsClient.filters = ["CLIENT_DISCONNECTED", "CLIENT_CONNECTED"];
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
        if (tokens[0] == "CLIENT_DISCONNECTED")
        {
            if (tokens.length < 3 || tokens[1] != "HOSTNAME")
                return;
            var hostname = tokens[2];
            SetFrontendOffline(hostname);
        }
        else if (tokens[0] == "CLIENT_CONNECTED")
        {
            if (tokens.length < 3 || tokens[1] != "HOSTNAME")
                return;
            var hostname = tokens[2];
            SetFrontendOnline(hostname);
        }
    };

    /*!
    * \fn SetFrontendOffline
    * \private
    *
    * Update the page when a frontend goes offline
    */
    var SetFrontendOffline = function(hostname)
    {
        console.log("removeFrontend: frontendRow-" + hostname);
        var frontendStatus = document.getElementById("frontendStatus-" + hostname);
        if (typeof frontendStatus != "undefined")
        {
            frontendStatus.classList.remove("frontendOnline");
            frontendStatus.classList.add("frontendOffline");
            frontendStatus.textContent = "Offline"; // TODO: Not translated, so this should be done another way - icon?
        }
    };

    /*!
    * \fn SetFrontendOnline
    * \private
    *
    * Update the page when a frontend comes online
    */
    var SetFrontendOnline = function(hostname)
    {
        console.log("addFrontend: " + hostname);
        var frontendStatus = document.getElementById("frontendStatus-" + hostname);
        if (typeof frontendStatus != "undefined")
        {
            frontendStatus.classList.remove("frontendOffline");
            frontendStatus.classList.add("frontendOnline");
            frontendStatus.textContent = "Online"; // TODO: Not translated, so this should be done another way - icon?
        }
    };

};

window.addEventListener("load", MythOverview.Init);
window.addEventListener("unload", MythOverview.Destructor);
