import { Component, ElementRef, OnInit, QueryList, ViewChild, ViewChildren } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';
import { MenuItem, MessageService, SortMeta } from 'primeng/api';
import { Menu } from 'primeng/menu';
import { Table, TableLazyLoadEvent } from 'primeng/table';
import { PartialObserver } from 'rxjs';
import { GetVideoListRequest, UpdateVideoMetadataRequest, VideoCategories, VideoCategoryList, VideoMetadataInfo } from 'src/app/services/interfaces/video.interface';
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
  @ViewChild("table") table!: Table;
  @ViewChildren('row') rows!: QueryList<ElementRef>;

  videos: VideoMetadataInfo[] = [];
  refreshing = false;
  successCount = 0;
  errorCount = 0;
  directory: string[] = [];
  catGroups: VideoCategories[] = [];
  video: VideoMetadataInfo = <VideoMetadataInfo>{ Title: '' };
  editingVideo?: VideoMetadataInfo;
  displayMetadataDlg = false;
  displayUnsaved = false;
  showAllVideos = false;
  lazyLoadEvent: TableLazyLoadEvent = {};
  totalRecords = 0;
  showTable = false;
  searchValue = '';
  selectedCategory: number | null = null;
  priorRequest: GetVideoListRequest = {};
  virtualScrollItemSize = 0;
  authorization = '';
  sortField = 'Title';
  sortOrder = 1;

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

    this.videoService.GetCategoryList()
      .subscribe((data: VideoCategoryList) => {
        this.catGroups = data.VideoCategoryList.VideoCategories;
        this.catGroups.sort((a, b) => {
          return a.Name?.localeCompare(b.Name || '') || 0
        });
        this.translate.get('dashboard.videos.allCategories').subscribe(translated =>{
          this.catGroups.unshift({ Id: -1, Name: translated});
        });
      });

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

    let sortField = this.utility.sortStorage.getItem('videos.sortField');
    if (sortField)
      this.sortField = sortField;

    let sortOrder = this.utility.sortStorage.getItem('videos.sortOrder');
    if (sortOrder)
      this.sortOrder = Number(sortOrder);
  }

  ngOnInit(): void {
    // Initial Load
    this.loadLazy({ first: 0, rows: 1 });
  }

  onSort(sortMeta: SortMeta) {
    this.sortField = sortMeta.field;
    this.sortOrder = sortMeta.order;
    this.utility.sortStorage.setItem("videos.sortField", sortMeta.field);
    this.utility.sortStorage.setItem('videos.sortOrder', sortMeta.order.toString());
  }

  loadLazy(event: TableLazyLoadEvent) {
    let accessToken = sessionStorage.getItem('accessToken');
    if (accessToken == null)
      this.authorization = ''
    else
      this.authorization = '&authorization=' + accessToken;
    this.lazyLoadEvent = event;
    let request: GetVideoListRequest = {
      Sort: "title",
      Folder: this.directory.join('/'),
      CollapseSubDirs: !this.showAllVideos,
      StartIndex: 0,
      Count: 1
    };

    if (event.first != undefined) {
      request.StartIndex = event.first;
      if (event.last)
        request.Count = event.last - event.first;
      else if (event.rows)
        request.Count = event.rows;
    }

    let sortField = this.sortField;
    if (Array.isArray(event.sortField))
      sortField = event.sortField[0];
    else if (event.sortField)
      sortField = event.sortField;
    request.Sort = sortField;
    if (event.sortOrder)
      request.Descending = (event.sortOrder < 0);
    if (request.Sort == 'SeasEp')
      request.Sort = `season,episode,title,releasedate`;
    else
      request.Sort += ',title,releasedate,season,episode';
    this.searchValue = this.searchValue.trim();
    if (this.searchValue)
      request.TitleRegEx = this.searchValue;
    if (this.selectedCategory != null)
      request.Category = this.selectedCategory;

    if (request.TitleRegEx != this.priorRequest.TitleRegEx
      || request.Category != this.priorRequest.Category) {
      this.menu.hide();
      this.priorRequest = request;
      this.showTable = false;
    }
    this.videoService.GetVideoList(request).subscribe(data => {
      let newList = data.VideoMetadataInfoList;
      this.totalRecords = data.VideoMetadataInfoList.TotalAvailable;
      this.videos.length = this.totalRecords;
      // populate page of virtual programs
      this.videos.splice(newList.StartIndex, newList.VideoMetadataInfos.length,
        ...newList.VideoMetadataInfos);
      // notify of change
      this.videos = [...this.videos]
      this.refreshing = false;
      this.showTable = true;
      let row = this.rows.get(0);
      if (row && row.nativeElement.offsetHeight)
        this.virtualScrollItemSize = row.nativeElement.offsetHeight;
      if (this.table) {
        this.table.totalRecords = this.totalRecords;
        this.table.virtualScrollItemSize = this.virtualScrollItemSize;
      }
    });

  }


  reLoadVideos() {
    this.showTable = false;
    this.videos.length = 0;
    this.refreshing = true;
    this.loadLazy(({ first: 0, rows: 1 }));
  }

  onFilter() {
    this.reLoadVideos();
  }

  showAllChange() {
    setTimeout(() => this.reLoadVideos(), 100);
  }

  resetSearch() {
    this.searchValue = '';
    this.reLoadVideos();
  }

  keydown(event: KeyboardEvent) {
    if (event.key == "Enter")
      this.onFilter();
  }

  URLencode(x: string): string {
    return encodeURI(x);
  }

  onDirectory(subdir: string) {
    this.directory.push(subdir);
    this.reLoadVideos();
  }

  breadCrumb(ix: number) {
    this.directory.length = ix;
    this.reLoadVideos();
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
