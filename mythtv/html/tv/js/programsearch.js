
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
    };

    /*!
    * \fn Destructor
    * \public
    */
    this.Destructor = function()
    {
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
};

window.addEventListener("load", MythProgramSearch.Init);
window.addEventListener("unload", MythProgramSearch.Destructor);
