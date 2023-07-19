import { Component, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';
import { LazyLoadEvent, MenuItem, MessageService } from 'primeng/api';
import { Menu } from 'primeng/menu';
import { PartialObserver } from 'rxjs';
import { UpdateVideoMetadataRequest, VideoMetadataInfo } from 'src/app/services/interfaces/video.interface';
import { UtilityService } from 'src/app/services/utility.service';
import { VideoService } from 'src/app/services/video.service';

@Component({
  selector: 'app-videos',
  templateUrl: './videos.component.html',
  styleUrls: ['./videos.component.css'],
  providers: [MessageService]
})
export class VideosComponent implements OnInit {

  @ViewChild("vidsform") currentForm!: NgForm;
  @ViewChild("menu") menu!: Menu;

  allVideos: VideoMetadataInfo[] = [];
  videos: VideoMetadataInfo[] = [];
  refreshing = false;
  loaded = false;
  successCount = 0;
  errorCount = 0;
  directory: string[] = [];
  video: VideoMetadataInfo = <VideoMetadataInfo>{ Title: '' };
  editingVideo?: VideoMetadataInfo;
  displayMetadataDlg = false;
  displayUnsaved = false;
  showAllVideos = false;

  mnu_markwatched: MenuItem = { label: 'dashboard.recordings.mnu_markwatched', command: (event) => this.markwatched(event, true) };
  mnu_markunwatched: MenuItem = { label: 'dashboard.recordings.mnu_markunwatched', command: (event) => this.markwatched(event, false) };
  mnu_updatemeta: MenuItem = { label: 'dashboard.recordings.mnu_updatemeta', command: (event) => this.updatemeta(event) };

  menuToShow: MenuItem[] = [];

  msg = {
    Success: 'common.success',
    Failed: 'common.failed',
    NetFail: 'common.networkfail',
  }


  constructor(private videoService: VideoService, private translate: TranslateService,
    private messageService: MessageService, public utility: UtilityService) {
    this.loadVideos();

    // translations
    for (const [key, value] of Object.entries(this.msg)) {
      this.translate.get(value).subscribe(data => {
        Object.defineProperty(this.msg, key, { value: data });
      });
    }

    const mnu_entries = [this.mnu_markwatched, this.mnu_markunwatched, this.mnu_updatemeta];
    mnu_entries.forEach(entry => {
      if (entry.label)
        this.translate.get(entry.label).subscribe(data =>
          entry.label = data
        );
    });

  }

  ngOnInit(): void {
  }


  loadVideos() {
    this.videoService.GetVideoList({ Sort: "FileName" }).subscribe(data => {
      this.allVideos = data.VideoMetadataInfoList.VideoMetadataInfos;
      this.filterVideos();
      this.loaded = true;
      this.refreshing = false;
    });
  }

  showAllChange() {
    this.refreshing = true;
    setTimeout(() => this.filterVideos(), 100);
  }

  filterVideos() {
    if (this.showAllVideos) {
      this.directory.length = 0;
      this.videos = [...this.allVideos];
    }
    else {
      this.videos = [];
      let prior = '';
      let search = this.directory.join('/');
      if (search.length > 0)
        search += '/';
      this.allVideos.forEach(
        video => {
          const parts = video.FileName.split('/');
          if (video.FileName.startsWith(search)) {
            if (parts.length == this.directory.length + 1) {
              this.videos.push(video);
              prior = '';
            }
            else {
              let dir = parts.slice(0, this.directory.length + 1)
                .join('/') + '/';
              if (dir != prior) {
                // Dummy directory entry
                this.videos.push(<VideoMetadataInfo>{
                  FileName: dir,
                  Title: parts[this.directory.length],
                  ContentType: 'D',  // indicates directory
                  Season: 0,
                  Episode: 0,
                  Length: 0
                });
                prior = dir;
              }
            }
          }
        }
      );
    }
    this.refreshing = false;
  }

  URLencode(x: string): string {
    return encodeURI(x);
  }

  onDirectory(subdir: string) {
    this.directory.push(subdir);
    this.filterVideos();
  }

  breadCrumb(ix: number) {
    this.directory.length = ix;
    this.filterVideos();
  }

  showMenu(video: VideoMetadataInfo, event: any) {
    this.video = video;
    this.menuToShow.length = 0;
    if (video.Watched)
      this.menuToShow.push(this.mnu_markunwatched);
    else
      this.menuToShow.push(this.mnu_markwatched);
    this.menuToShow.push(this.mnu_updatemeta);
    this.menu.toggle(event);
  }


  markwatched(event: any, Watched: boolean) {
    this.videoService.UpdateVideoWatchedStatus(this.video.Id, Watched).subscribe({
      next: (x) => {
        if (x.bool) {
          this.sendMessage('success', event.item.label, this.msg.Success);
          this.video.Watched = Watched;
        }
        else
          this.sendMessage('error', event.item.label, this.msg.Failed);
      },
      error: (err: any) => this.networkError(err)
    });
  }

  updatemeta(event: any) {
    this.editingVideo = this.video;
    this.video = Object.assign({}, this.video);
    if (this.video.ReleaseDate)
      this.video.ReleaseDate = new Date(this.video.ReleaseDate);
    else
      this.video.ReleaseDate = <Date><unknown>null
    this.displayMetadataDlg = true;
  }


  sendMessage(severity: string, action: string, text: string, extraText?: string) {
    if (extraText)
      extraText = '\n' + extraText;
    else
      extraText = '';
    this.messageService.add({
      severity: severity, summary: text,
      detail: action + ' ' + this.video.Title + ' ' + this.video.SubTitle + extraText,
      life: 3000,
    });
  }


  saveObserver: PartialObserver<any> = {
    next: (x: any) => {
      if (x.bool) {
        console.log("saveObserver success", x);
        this.successCount++;
        this.successCount++;
        this.currentForm.form.markAsPristine();
        if (this.editingVideo)
          Object.assign(this.editingVideo, this.video);
      }
      else {
        console.log("saveObserver error", x);
        this.errorCount++;
      }
    },
    error: (err: any) => {
      console.log("saveObserver error", err);
      this.errorCount++;
    }
  };

  saveVideo() {
    this.successCount = 0;
    this.errorCount = 0;
    this.displayUnsaved = false;
    let request: UpdateVideoMetadataRequest = {
      Id: this.video.Id,
      Episode: this.video.Episode,
      Inetref: this.video.Inetref,
      Plot: this.video.Description,
      ReleaseDate: this.video.ReleaseDate,
      Season: this.video.Season,
      SubTitle: this.video.SubTitle,
      Title: this.video.Title
    };

    this.videoService.UpdateVideoMetadata(request).subscribe(this.saveObserver);
  }

  networkError(err: any) {
    console.log("network error", err);
    this.sendMessage('error', '', this.msg.NetFail);
  }

  closeDialog() {
    if (this.currentForm.dirty) {
      if (this.displayUnsaved) {
        // Close on the unsaved warning
        this.displayUnsaved = false;
        this.displayMetadataDlg = false;
        this.editingVideo = undefined;
        this.currentForm.form.markAsPristine();
      }
      else
        // Close on the channel edit dialog
        // Open the unsaved warning
        this.displayUnsaved = true;
    }
    else {
      // Close on the channel edit dialog
      this.displayMetadataDlg = false;
      this.displayUnsaved = false;
      this.editingVideo = undefined;
    };
  }

}
