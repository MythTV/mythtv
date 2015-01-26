
/*!
 * \namespace OverviewNS
 * \brief Namespace for functions used in the 'Overview' page, overview.qsp
 */
var overviewNS = new function() {

    /*!
    * \fn Init
    * \public
    *
    * Initialise, called by the window load event once the page has finished
    * loading
    */
    this.Init = function()
    {
        LoadGalleria();
        RegisterMythEventHandler();
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
                    console.log("Error: " + errorThrown);
                },
                complete: function() {
                    console.log("Complete");
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
    * \fn RegisterMythEventHandler
    * \private
    *
    * Register a WebSocketEventClient with the global WebSocketEventHandler
    */
    var RegisterMythEventHandler = function()
    {
        var wsClient = new WebSocketEventClient();
        wsClient.eventReceiver = function(event) { HandleMythEvent(event) };
        wsClient.filters = ["SYSTEM_EVENT"];
        globalWSHandler.AddListener(wsClient);
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
        if (tokens.length <= 2)
            return;
        if (tokens[1] == "CLIENT_DISCONNECTED")
        {
            if (tokens.length < 4 || tokens[2] != "HOSTNAME")
                return;
            var hostname = tokens[3];
            SetFrontendOffline(hostname);
        }
        else if (tokens[1] == "CLIENT_CONNECTED")
        {
            if (tokens.length < 4 || tokens[2] != "HOSTNAME")
                return;
            var hostname = tokens[3];
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

window.addEventListener("load", overviewNS.Init, false);
