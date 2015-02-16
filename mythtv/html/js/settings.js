
/*!
 * \namespace MythSettings
 * \brief Namespace for functions used with settings widgets in various pages
 */
var MythSettings = new function() {

    this.UpdateRangeValue = function (rangeInput)
    {
        document.getElementById(rangeInput.id + '-Value').value = rangeInput.value;
        rangeInput.title = rangeInput.value;
    }

    this.EnableWidget = function (widgetId, enable)
    {
        document.getElementById(widgetId).disabled = !enable;
    }

};
