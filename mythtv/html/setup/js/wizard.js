function testDBSettings() {
    var result = 0;
    var host = $("#dbHost").val();
    var user = $("#dbUserName").val();
    var pass = $("#dbPassword").val();
    var name = $("#dbName").val();
    var port = $("#dbPort").val();

    clearEditMessages();

    if (name == null)
        name = "mythconverg";

    if (port == null)
        port = 3306;

    $.post("/Myth/TestDBSettings",
        { HostName: host, UserName: user, Password: pass, DBName: name, dbPort: port},
        function(data) {
            if (data.bool == "true") {
                result = 1;
                setEditStatusMessage("Database connection succeeded!");
            }
            else
                setEditErrorMessage("Database connection failed!");
        }, "json").error(function(data) {
            setEditErrorMessage("Database connection failed!");
        });

    return result;
}

$("#wizardtabs").tabs({ cache: true });
showEditWindow();
