
function basename(path) {
    return path.replace( /\\/g, '/').replace( /.*\//, '' );
}

function dirname(path) {
    return path.replace( /\\/g, '/').replace( /\/[^\/]*$/, '' );;
}

function parentDirName(path) {
    return basename(dirname(path));
}

function setupPageName(path) {
    return basename(path).replace( /\\/g, '/').replace( /\.[^\.]$/, '' );
}

function showHelpWindow() {
    $("#helpWindow").show();
    $("#helpWindow").draggable();
}

function hideHelpWindow() {
    $("#helpWindow").hide();
}

function showHelp(title, content) {
    $("#helpTitle").html(title);
    $("#helpContent").html("FIXME: you can drag this window....  " + content);
    showHelpWindow();
}

function showEditWindow() {
    $("#edit-bg").show();
    $("#editborder").show();
}

function hideEditWindow() {
    $("#editborder").hide();
    $("#edit-bg").hide();
}

function clearEditMessages() {
    setEditStatusMessage("");
    setEditErrorMessage("");
}

function setEditStatusMessage(message) {
    $("#editErrorMessage").html("");
    $("#editStatusMessage").html(message);
    setTimeout('$("#editStatusMessage").html("")', statusMessageTimeout);
}

function setEditErrorMessage(message) {
    $("#editStatusMessage").html("");
    $("#editErrorMessage").html(message);
    setTimeout('$("#editErrorMessage").html("")', errorMessageTimeout);
}

function submitConfigForm(form) {
    var data = $("#config_form_" + form).serialize();
    var url = $("#__config_form_action__").val();

    /* FIXME, clear out _error divs */
    $.ajaxSetup({ async: false });
    $.post(url, data, function(data) {
        $.each(data, function(key, value) {
            $("#" + key + "_error").html(value);
        });
    }, "json");
    $.ajaxSetup({ async: true });
}

var hostOptions = "";
function getHostsOptionList() {
    if (hostOptions.length == 0) {
        hostOptions = "";
        $.ajaxSetup({ async: false });
        $.getJSON("/Myth/GetHosts", function(data) {
            $.each(data.QStringList, function(i, value) {
                hostOptions += "<option value='" + value + "'>" + value + "</option>";
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

