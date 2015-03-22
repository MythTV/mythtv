
function changePassword() {
    var result = false;
    var oldPassword = $("#oldPassword").val();
    var newPassword = $("#newPassword").val();
    var newPasswordConfirm = $("#newPasswordConfirm").val();

    if (newPassword.length == 0) {
        setErrorMessage("New password is empty.");
    } else if (newPassword != newPasswordConfirm) {
        setErrorMessage("New passwords do not match.");
    } else {
        $.post("/Myth/ChangePassword",
            { UserName: "admin",
              OldPassword: oldPassword,
              NewPassword: newPassword },
            function(data) {
                if (data.bool == "true")
                    setStatusMessage("Password successfully changed.");
                else
                    setErrorMessage("Error changing password, check backend logs for detailed information.");
            }, "json").error(function(data) {
                setErrorMessage("Error changing password, check backend logs for detailed information.");
            });
    }
}

$("#edit").dialog({
    modal: true,
    width: 850,
    height: 500,
    'title': 'Change Password',
    closeOnEscape: false,
    buttons: {
       'Change Password': function() {changePassword()},
       'Cancel': function() { $(this).dialog('close'); }
    }
});
showEditWindow();
