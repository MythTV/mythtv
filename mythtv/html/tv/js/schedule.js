
function saveRecordingSchedule(formElement)
{
    var ruleID = saveFormData(formElement, scheduleSaved);
}

function scheduleSaved(recRuleId)
{
    document.getElementById("rule-Id").value = recRuleId.trim();
}
