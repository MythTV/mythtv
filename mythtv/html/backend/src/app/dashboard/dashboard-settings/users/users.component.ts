import { Component, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';
import { DataService } from 'src/app/services/data.service';
import { MythService } from 'src/app/services/myth.service';

interface MyUser {
  Name: string;
  canUpdate?: boolean;
  canDelete?: boolean;
  newUser?: boolean;
  oldPassword?: string;
  newPassword?: string;
  newPassword2?: string;
}



@Component({
  selector: 'app-users',
  templateUrl: './users.component.html',
  styleUrls: ['./users.component.css']
})
export class UsersComponent implements OnInit {

  @ViewChild("userform") currentForm!: NgForm;

  users: MyUser[] = [];

  dialogHeader = "";
  loaded = false;
  apiAuthReqd = 'NONE';
  saveApiAuthReqd = 'NONE';
  displayUserDlg = false;
  displayDelete = false;
  dupName = false;
  successCount = 0;
  errorCount = 0;
  errorMsg = '';
  authReqFail = 0;
  validIP = true;
  authIPFail = 0;
  ipSaved = false;
  hostName = '';
  AllowConnFromAll = false;


  msg = {
    Success: 'common.success',
    Failed: 'common.failed',
    NetFail: 'common.networkfail',
    warningText: 'settings.common.warning',
    headingNew: "dashboard.users.new",
    headingEdit: "dashboard.users.edit_label",
    pwDiffer: "dashboard.users.pwdiffer",
    oldPwWrong: "dashboard.users.oldpwwrong"
  }

  user: MyUser = { Name: '' };

  constructor(private translate: TranslateService, private mythService: MythService,
    public dataService: DataService) {
  }

  ngOnInit(): void {
    this.loadTranslations();
    this.loadUsers();
    this.loadHostName();
    this.loadOther();
  }

  loadTranslations(): void {
    for (const [key, value] of Object.entries(this.msg)) {
      this.translate.get(value).subscribe(data => {
        Object.defineProperty(this.msg, key, { value: data });
      });
    }
  }

  loadUsers() {
    this.users = [];
    this.mythService.GetUsers().subscribe(data => {
      data.StringList.forEach((value) => {
        let canUpdate = (this.dataService.loggedInUser == null
          || this.dataService.loggedInUser == 'admin'
          || this.dataService.loggedInUser == value);
        let canDelete = (value != "admin"
          && (!this.dataService.loggedInUser
            || this.dataService.loggedInUser == 'admin'));
        this.users.push({
          Name: value,
          canUpdate: canUpdate,
          canDelete: canDelete,
        })
      })
      this.loaded = true;
      this.users = [...this.users];
    });
  }

  loadapiAuthReqd() {
    this.authIPFail = 0;
    if (!this.hostName) {
      this.authReqFail++;
      return;
    }
    this.mythService.GetSetting({ HostName: this.hostName, Key: "APIAuthReqd", Default: "NONE" })
      .subscribe({
        next: data => {
          this.apiAuthReqd = data.String
          this.saveApiAuthReqd = this.apiAuthReqd;
        },
        error: () => {
          // this.apiAuthReqd = !this.apiAuthReqd;
          this.apiAuthReqd = this.saveApiAuthReqd;
          this.authReqFail++;
        },
      });
  }

  loadHostName() {
    this.mythService.GetHostName().subscribe({
      next: data => {
        this.hostName = data.String;
        this.loadapiAuthReqd();
        // this.loadIP();
      },
    })
  }

  loadOther() {
    this.mythService.GetSetting({ HostName: this.hostName, Key: "AllowConnFromAll", Default: "0" })
      .subscribe({
        next:
          data => {
            this.AllowConnFromAll = (data.String == "1");
          }
      });
  }

  openNew() {
    this.dialogHeader = this.msg.headingNew;
    this.user = { Name: '', canUpdate: true, newUser: true, canDelete: true };
    this.user.oldPassword = '';
    this.user.newPassword = '';
    this.user.newPassword2 = '';
    this.displayUserDlg = true;
  }

  editUser(user: MyUser) {
    this.dialogHeader = this.msg.headingEdit;
    this.user = user;
    this.user.oldPassword = '';
    this.user.newPassword = '';
    this.user.newPassword2 = '';
    this.displayUserDlg = true;
  }

  deleteRequest(user: MyUser) {
    this.displayDelete = true;
    this.user = user;
  }

  deleteUser() {
    this.errorCount = 0;
    this.mythService.ManageDigestUser({
      Action: 'Remove',
      UserName: this.user.Name,
    }).subscribe({
      next: (data) => {
        if (data.bool) {
          this.currentForm.form.markAsPristine();
          this.displayDelete = false;
          this.loadUsers();
        }
        else
          this.errorCount++;
      },
      error: () => {
        this.errorCount++;
      }
    });
  }

  checkName() {
    this.dupName = false;
    this.user.Name.trim();
    let match = this.users.find((ent) =>
      ent.Name == this.user.Name);
    if (match)
      this.dupName = true;
  }

  closeDialog() {
    this.displayUserDlg = false;
  }

  saveReq() {
    this.errorMsg = '';
    if (this.user.newPassword != this.user.newPassword2) {
      this.errorMsg = this.msg.pwDiffer;
      return;
    }
    if (!this.dataService.loggedInUser
      || this.dataService.loggedInUser == 'admin') {
      this.saveUser();
    }
    else if (this.dataService.loggedInUser == this.user.Name) {
      this.mythService.LoginUser(this.user.Name, this.user.oldPassword!)
        .subscribe({
          next: (data) => {
            if (data.String && data.String.length > 1)
              this.saveUser();
            else
              this.errorMsg = this.msg.oldPwWrong;
          },
          error: (data) => {
            this.errorMsg = this.msg.oldPwWrong;
          }
        });
    }
  }

  saveUser() {
    let action = 'ChangePassword';
    let pw = this.user.oldPassword;
    if (this.user.newUser) {
      action = 'Add';
      pw = this.user.newPassword;
    }
    this.mythService.ManageDigestUser({
      Action: action,
      UserName: this.user.Name,
      Password: pw,
      NewPassword: this.user.newPassword
    }).subscribe({
      next: (data) => {
        if (data.bool) {
          this.currentForm.form.markAsPristine();
          this.successCount++;
          if (this.user.newUser) {
            this.user.newUser = false;
            this.users.push(this.user);
            this.users = [...this.users];
          }
        }
        else
          this.errorCount++;
      },
      error: (data) => {
        this.errorCount++;
      }
    });
  }

  saveapiAuthReqd() {
    this.authReqFail = 0;
    if (!this.hostName) {
      this.authReqFail++;
      return;
    }
    this.mythService.PutSetting({
      HostName: this.hostName, Key: "APIAuthReqd",
      Value: this.apiAuthReqd
    }).subscribe({
      next: (x) => {
        this.loadapiAuthReqd()
      },
      error: (x) => {
        this.loadapiAuthReqd();
        this.authReqFail++;
      }
    });
  }

}
