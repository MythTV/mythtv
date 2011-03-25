
function interceptMenuClicks() {
    $('#menu ul li a').click(function(event) {
        if (this.href.match("/Config/") == "/Config/") {
            event.preventDefault();
            loadContent(this.href, "/setup/js/Config.js");
        }
    });
}

function loadScreen(menuURL, contentURL, jsURL) {
    loadMenu(menuURL);
    if (contentURL && contentURL != "")
        loadContent(contentURL);
    if (jsURL && jsURL != "")
        $.getScript(jsURL);
}

function loadMenu(menuURL) {
    var html = $.ajax({
      url: menuURL,
        async: false
     }).responseText;

    $("#content").html("");
    $("#menu-bg").html(html);
}

function clearMenu() {
    $("#menu-bg").html("");
}

function loadContent(contentURL, jsURL) {
    $("#content").html("<br><br><b>Loading...</b>");

    var html = $.ajax({
      url: contentURL,
        async: false
     }).responseText;

    $("#content").html(html);

    if (jsURL) {
        $.getScript(jsURL);
    }
}

function getCurrentTab(id) {
    var selected = $(id).tabs('option', 'selected');
    return selected;
}

function addTab(id, url, label) {
    $(id).tabs("add", url, label);
}

function removeTab(id, index) {
    $(id).tabs("remove", index);
}

