
function saveRecordingSchedule(formElement)
{
    if (formElement.reportValidity && formElement.reportValidity())
        saveFormData(formElement, scheduleSaved);
    else
    {
        if (formElement.checkValidity())
            saveFormData(formElement, scheduleSaved);
        else
        {
            // We really wany to use the HTML5 warnings, not show our own
            // but to do that requires a bit of a hack
            setErrorMessage("Required form fields missing.");
        }
    }
}

function scheduleSaved(recRuleId)
{
    document.getElementById("rule-Id").value = recRuleId.trim();
}
