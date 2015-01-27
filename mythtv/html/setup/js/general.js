
console.log("Loaded general.js");

/*!
 * \namespace MythGeneralSetup
 * \brief Namespace for functions used in the 'General' page, guide.qsp
 */
var MythGeneralSetup = new function() {

    /*!
    * \fn Init
    * \public
    *
    * Initialise, called by the window load event once the page has finished
    * loading
    */
    this.Init = function()
    {
        console.log("MythGeneralSetup Init");
        setupTabs("generaltabs");
    };

    /*!
    * \fn Destructor
    * \public
    */
    this.Destructor = function()
    {
        // Stop timers, unregister event listeners etc here
    };

};

if (document.readyState == "complete")
    MythGeneralSetup.Init();
else
    window.addEventListener("load", MythGeneralSetup.Init);
window.addEventListener("unload", MythGeneralSetup.Destructor);
