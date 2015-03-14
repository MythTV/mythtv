
/*!
 * \namespace MythProgramSearch
 * \brief Namespace for functions used in the 'Program Search' page, programsearch.qsp
 */
var MythProgramSearch = new function() {

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
    * \fn SubmitFilterForm
    * \public
    *
    * Submits the filters for the search, including any search terms
    */
    this.SubmitFilterForm = function (form)
    {
        submitForm(form, 'programList', 'dissolve');
    }

    /*!
    * \fn RegisterMythEventHandler
    * \private
    *
    * Register a WebSocketEventClient with the global WebSocketEventHandler
    */
    var RegisterMythEventHandler = function()
    {
        wsClient = new parent.WebSocketEventClient();
        wsClient.name = "Programme Search";
        wsClient.eventReceiver = function(event) { HandleMythEvent(event) };
        wsClient.filters = ["SCHEDULE_CHANGE"];
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
        // the current page needs reloading. The schedule change may not
        // affect what is current displayed or we may only need to refresh part
        // of the content
        if (tokens[0] == "SCHEDULE_CHANGE")
        {
            reloadTVContent();
        }
    };
};

window.addEventListener("load", MythProgramSearch.Init);
window.addEventListener("unload", MythProgramSearch.Destructor);
