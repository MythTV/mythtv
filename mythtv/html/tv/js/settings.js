
function updateRangeValue(rangeInput)
{
    document.getElementById(rangeInput.id + '-Value').value = rangeInput.value;
    rangeInput.title = rangeInput.value;
}
