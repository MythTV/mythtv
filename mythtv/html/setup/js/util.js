
function loadSetupPage(pageName) {
    loadContent("/setup/" + pageName + ".html",
                "/setup/js/" + pageName + ".js");
}

function interceptSetupMenuClicks() {
    $('#menu ul li a').click(function(event) {
        if (this.href.match("/Config/") == "/Config/") {
            event.preventDefault();
            loadContent(this.href, "/setup/js/Config.js");
        }
    });
}

function interceptSetupTabsFormClicks() {
alert("trying to intercept form clicks");
    $('#setuptabs ul li a').click(function(event) {
alert("checking: '" + this.href + "'");
        if (this.href.match("/Config/") == "/Config/") {
            event.preventDefault();
alert("form clicked");
            loadContent(this.href, "/setup/js/Config.js");
        }
    });
}

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

function showEditWindow() {
    $("#edit-bg").show();
    $("#editborder").show();
}

function hideEditWindow() {
    $("#editborder").hide();
    $("#edit-bg").hide();
}

function loadEditWindow(contentURL, jsURL) {
    clearEditMessages();
    loadDiv("editcontent", contentURL, jsURL);
    showEditWindow();
}

function clearEditMessages() {
    setEditStatusMessage("");
    setEditErrorMessage("");
}

function setEditStatusMessage(message) {
    $("#editStatusMessage").html(message);
    setTimeout('$("#editStatusMessage").html("")', statusMessageTimeout);
}

function setEditErrorMessage(message) {
    $("#editErrorMessage").html(message);
    setTimeout('$("#editErrorMessage").html("")', errorMessageTimeout);
}

function appendEditStatusMessage(message) {
    if ($("#editStatusMessage").html() != "")
        $("#editStatusMessage").append("<br>");

    $("#editStatusMessage").append(message);
    setTimeout('$("#editStatusMessage").html("")', statusMessageTimeout);
}

function appendEditErrorMessage(message) {
    if ($("#editErrorMessage").html() != "")
        $("#editErrorMessage").append("<br>");

    $("#editErrorMessage").append(message);
    setTimeout('$("#editErrorMessage").html("")', errorMessageTimeout);
}

