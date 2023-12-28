import { AfterViewInit, Component, Input, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';
import { MythService } from 'src/app/services/myth.service';
import { SetupService } from 'src/app/services/setup.service';

interface GroupCard {
  GroupName: string,
  LocalizedName: string,
  DirNames: string[],
};

@Component({
  selector: 'app-sgroup',
  templateUrl: './sgroup.component.html',
  styleUrls: ['./sgroup.component.css']
})
export class SgroupComponent implements OnInit, AfterViewInit {

  @Input() sgroup!: GroupCard;
  @Input() hostName!: string;

  @ViewChild("sgroupform") currentForm!: NgForm;

  successCount = 0;
  errorCount = 0;
  expectCount = 0;
  showEditDlg = false;
  editDlgNum = -1;
  editDirName = "";
  editDirs: string[] = [];
  dirSelect: String[] = [];
  selectedDir = "";
  upString = "UP 1 LEVEL";

  constructor(private mythService: MythService, public setupService: SetupService,
    private translate: TranslateService) {

    translate.get("settings.sgroups.updir")
      .subscribe(data => this.upString = "// ** " + data + " **");

  }

  ngOnInit(): void {
    this.editDirs = this.sgroup.DirNames.slice();
  }

  ngAfterViewInit(): void {
    this.setupService.setCurrentForm(this.currentForm);
  }

  selectDir() {
    // Get rid of trailing slashes
    while (this.editDirName.charAt(this.editDirName.length - 1) == "/") {
      this.editDirName = this.editDirName.substring(0, this.editDirName.length - 1);
    }
    // Up 1 level selected
    if (this.selectedDir.startsWith("//")) {
      let slash = this.editDirName.lastIndexOf("/");
      this.editDirName = this.editDirName.substring(0, slash);
      if (this.editDirName.length == 0)
        this.editDirName = "/";
    }
    else
      this.editDirName = this.editDirName + "/" + this.selectedDir;
    this.fillDirList();
  }

  textChange() {
    this.fillDirList();
  }

  fillDirList() {
    if (this.editDirName.indexOf("/") != 0)
      this.editDirName = "/" + this.editDirName;
    this.mythService.GetDirListing(this.editDirName)
      .subscribe(data => {
        if (this.editDirName != "/")
          data.DirListing.unshift(this.upString);
        this.dirSelect = data.DirListing;
      });
  }

  addDirectory() {
    this.editDirs.push("/");
    this.editDirectory(this.editDirs.length - 1);
  }

  editDirectory(ix: number) {
    this.editDirName = this.editDirs[ix];
    this.editDlgNum = ix;
    this.showEditDlg = true;
    this.fillDirList();
  }

  deleteDirectory(ix: number) {
    this.editDirs[ix] = '';
    this.currentForm.form.markAsDirty();
  }

  saveObserver = {
    next: (x: any) => {
      if (x.bool) {
        this.successCount++;
        if (this.successCount == this.expectCount)
          // filter out "" and "/" strings
          this.sgroup.DirNames = this.editDirs.filter(entry => entry.length > 1);
      }
      else {
        this.errorCount++;
        this.currentForm.form.markAsDirty();
      }
    },
    error: (err: any) => {
      console.error(err);
      this.errorCount++;
      this.currentForm.form.markAsDirty();
    },
  };

  saveForm() {
    this.successCount = 0;
    this.errorCount = 0;
    this.expectCount = 0;

    // Delete duplicate entries from input
    for (let ix = 0; ix < this.editDirs.length; ix++) {
      if (this.editDirs[ix] == "/")
        this.editDirs[ix] = ""
      for (let ix2 = ix + 1; ix2 < this.editDirs.length; ix2++) {
        if (this.editDirs[ix] == this.editDirs[ix2])
          this.editDirs[ix2] = "";
      }
    }
    // entries to add
    this.editDirs.forEach(entry => {
      if (entry != "") {
        if (this.sgroup.DirNames.indexOf(entry) == -1) {
          // Add Entry
          this.mythService.AddStorageGroupDir({
            GroupName: this.sgroup.GroupName,
            DirName: entry,
            HostName: this.hostName
          }).subscribe(this.saveObserver);
          this.expectCount++;
        }
      }
    });

    // entries to delete
    this.sgroup.DirNames.forEach(entry => {
      if (this.editDirs.indexOf(entry) == -1) {
        // Delete Entry
        this.mythService.RemoveStorageGroupDir({
          GroupName: this.sgroup.GroupName,
          DirName: entry,
          HostName: this.hostName
        }).subscribe(this.saveObserver);
        this.expectCount++;
      }
    });
  }

}
