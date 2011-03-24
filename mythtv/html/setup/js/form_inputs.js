var hostOptions = "";

function getHostsOptionList() {
    if (hostOptions.length == 0) {
        hostOptions = "";
        $.ajaxSetup({ async: false });
        $.getJSON("/Myth/GetHosts", function(data) {
            $.each(data.StringList.Values, function(i, value) {
                hostOptions += "<option>" + value + "</option>";
            });
        });
        $.ajaxSetup({ async: true });
    }

    return hostOptions;
}

function hostsSelect(id) {
    var result = "<select id='" + id + "'>";

    result += getHostsOptionList();

    result += "</select>";

    return result;
}

