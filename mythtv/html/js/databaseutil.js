function checkDatabase() {
    var repair = $("#repairTables").val();

    $.post("/Myth/CheckDatabase",
        { Repair: repair },
        function(data) {
            if (data.bool == "true")
                setHeaderStatusMessage("Database is healthy.");
            else
                setHeaderErrorMessage("Error while checking database, check backend logs.");
        }, "json").error(function(data) {
            setHeaderErrorMessage("Error while checking database, check backend logs.");
        });
    setHeaderStatusMessage("Checking database tables...");
}

function backupDatabase() {
    $.post("/Myth/BackupDatabase", {},
        function(data) {
            if (data.bool == "true")
                setHeaderStatusMessage("Database backup completed successfully.");
            else
                setHeaderErrorMessage("Database backup failed, check backend logs.");
        }, "json").error(function(data) {
            setHeaderErrorMessage("Database backup failed, check backend logs.");
        });
    setHeaderStatusMessage("Database backup started...");
}

