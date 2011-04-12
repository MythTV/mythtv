function checkDatabase() {
    var repair = $("#repairTables").val();

    $.post("/Myth/CheckDatabase",
        { Repair: repair },
        function(data) {
            if (data.bool == "true")
                setStatusMessage("Database is healthy.");
            else
                setErrorMessage("Error while checking database, check backend logs.");
        }, "json").error(function(data) {
            setErrorMessage("Error while checking database, check backend logs.");
        });
    setStatusMessage("Checking database tables...");
}

function backupDatabase() {
    $.post("/Myth/BackupDatabase", {},
        function(data) {
            if (data.bool == "true")
                setStatusMessage("Database backup completed successfully.");
            else
                setErrorMessage("Database backup failed, check backend logs.");
        }, "json").error(function(data) {
            setErrorMessage("Database backup failed, check backend logs.");
        });
    setStatusMessage("Database backup started...");
}

