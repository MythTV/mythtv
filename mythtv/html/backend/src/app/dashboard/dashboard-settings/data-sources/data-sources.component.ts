import { AfterViewInit, Component, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';
import { MythService } from 'src/app/services/myth.service';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-data-sources',
  templateUrl: './data-sources.component.html',
  styleUrls: ['./data-sources.component.css']
})
export class DataSourcesComponent implements OnInit, AfterViewInit {

  @ViewChild("datasources")
  currentForm!: NgForm;


  movieList = [
    { Label: 'TheMovieDB.org V3 movie', Value: "metadata/Movie/tmdb3.py" },
  ];

  tvList = [
    { Label: 'TheMovieDB.org V3 television', Value: "metadata/Television/tmdb3tv.py" },
    { Label: 'TheTVDatabaseV4', Value: "metadata/Television/ttvdb4.py" },
    { Label: 'TVmaze.com', Value: "metadata/Television/tvmaze.py" },
  ];


  successCount = 0;
  errorCount = 0;

  MovieGrabber = "metadata/Movie/tmdb3.py";
  TelevisionGrabber = "metadata/Television/ttvdb4.py";
  DailyArtworkUpdates = false;

  constructor(private mythService: MythService, private translate: TranslateService,
    private setupService: SetupService) {
  }

  ngOnInit(): void {
    this.loadTranslations();
    this.loadValues();
    this.markPristine();
  }

  loadTranslations() {
    // this.movieList.forEach((entry) =>
    //   this.translate.get(entry.Label).subscribe(data => entry.Label = data));
  }

  ngAfterViewInit() {
    this.setupService.setCurrentForm(this.currentForm);
  }

  loadValues() {
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "MovieGrabber", Default: "metadata/Movie/tmdb3.py" })
      .subscribe({
        next: data => this.MovieGrabber = data.String,
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "TelevisionGrabber", Default: "metadata/Television/ttvdb4.py" })
      .subscribe({
        next: data => this.TelevisionGrabber = data.String,
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "DailyArtworkUpdates", Default: "0" })
      .subscribe({
        next: data => this.DailyArtworkUpdates = (data.String == "1"),
        error: () => this.errorCount++
      });
  }

  markPristine() {
    setTimeout(() => this.currentForm.form.markAsPristine(), 200);
  }

  swObserver = {
    next: (x: any) => {
      if (x.bool)
        this.successCount++;
      else {
        this.errorCount++;
        if (this.currentForm)
          this.currentForm.form.markAsDirty();
      }
    },
    error: (err: any) => {
      console.error(err);
      this.errorCount++;
      if (this.currentForm)
        this.currentForm.form.markAsDirty();
    },
  };

  saveForm() {
    this.successCount = 0;
    this.errorCount = 0;

    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "MovieGrabber",
      Value: String(this.MovieGrabber)
    }).subscribe(this.swObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "TelevisionGrabber",
      Value: String(this.TelevisionGrabber)
    }).subscribe(this.swObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "DailyArtworkUpdates",
      Value: this.DailyArtworkUpdates ? "1" : "0"
    }).subscribe(this.swObserver);
 }

}
