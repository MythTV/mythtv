function testDBSettings() {
    var result = 0;
    var host = $("#dbHost").val();
    var user = $("#dbUserName").val();
    var pass = $("#dbPassword").val();
    var name = $("#dbName").val();
    var port = $("#dbPort").val();

    if (name == null)
        name = "mythconverg";

    if (port == null)
        port = 3306;

    $.ajaxSetup({ async: false });
    $.post("/Myth/TestDBSettings",
        { HostName: host, UserName: user, Password: pass, DBName: name, dbPort: port},
        function(data) {
            if (data.bool == "true") {
                result = 1;
                alert("Database connection succeeded!");
            }
            else
                alert("Database connection failed.");
        }, "json").error(function(data) {
            alert("Database connection failed.");
        });
    $.ajaxSetup({ async: true });

    return result;
}

$("#wizardtabs").tabs();
