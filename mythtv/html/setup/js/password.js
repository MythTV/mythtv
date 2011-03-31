
function changePassword() {
    var result = false;
    var oldPassword = $("#oldPassword").val();
    var newPassword = $("#newPassword").val();
    var newPasswordConfirm = $("#newPasswordConfirm").val();

    if (newPassword.length == 0) {
        setHeaderErrorMessage("ERROR: New password is empty.");
    } else if (newPassword != newPasswordConfirm) {
        setHeaderErrorMessage("ERROR: New passwords do not match.");
    } else {
        $.post("/Myth/ChangePassword",
            { UserName: "admin",
              OldPassword: oldPassword,
              NewPassword: newPassword },
            function(data) {
                if (data.bool == "true")
                    setHeaderStatusMessage("Password successfully changed.");
                else
                    setHeaderErrorMessage("Error changing password, check backend logs for detailed information.");
            }, "json").error(function(data) {
                setHeaderErrorMessage("Error changing password, check backend logs for detailed information.");
            });
    }
}

