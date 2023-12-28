import { AfterViewInit, Component, ElementRef, Input, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';
import { PartialObserver } from 'rxjs';
import { CaptureCardService } from 'src/app/services/capture-card.service';
import { RecProfile, RecProfileGroup } from 'src/app/services/interfaces/recprofile.interface';
import { SetupService } from 'src/app/services/setup.service';
import { ProfileGroupComponent } from '../profile-group.component';

@Component({
  selector: 'app-recprofile',
  templateUrl: './recprofile.component.html',
  styleUrls: ['./recprofile.component.css']
})
export class RecprofileComponent implements OnInit, AfterViewInit {

  @Input() profile!: RecProfile;
  @Input() group!: RecProfileGroup;
  @Input() parentComponent!: ProfileGroupComponent;
  @ViewChild("recprofform") currentForm!: NgForm;
  @ViewChild("top") topElement!: ElementRef;


  successCount = 0;
  errorCount = 0;
  expectedCount = 0;

  paramList = [
    {
      CardType: 'V4L',
      Param: ['autotranscode', 'height', 'width'],
      Video: ['RTjpeg', 'MPEG-4'],
      Audio: ['MP3', 'Uncompressed']
    },
    {
      CardType: 'MPEG',
      Param: ['autotranscode', 'height', 'width'],
      Video: ['MPEG-2 Hardware Encoder'],
      Audio: ['MPEG-2 Hardware Encoder']
    },
    {
      CardType: 'MJPEG',
      Param: ['autotranscode', 'height', 'width'],
      Video: ['Hardware MJPEG'],
      Audio: ['MP3', 'Uncompressed']
    },
    {
      CardType: 'HDTV',
      Param: ['autotranscode', 'height', 'width'],
      Video: ['RTjpeg', 'MPEG-4'],
      Audio: ['MP3', 'Uncompressed']
    },
    {
      CardType: 'DVB',
      Param: ['autotranscode', 'recordingtype', 'recordmpts'],
      Video: [],
      Audio: []
    },
    {
      CardType: 'FIREWIRE',
      Param: ['autotranscode'],
      Video: [],
      Audio: []
    },
    {
      CardType: 'GO7007',
      Param: ['autotranscode', 'height', 'width'],
      Video: ['MPEG-4', 'MPEG-2'],
      Audio: ['MP3', 'Uncompressed']
    },
    {
      CardType: 'IMPORT',
      Param: ['autotranscode'],
      Video: [],
      Audio: []
    },
    {
      CardType: 'FREEBOX',
      Param: ['autotranscode', 'recordmpts'],
      Video: [],
      Audio: []
    },
    {
      CardType: 'HDHOMERUN',
      Param: ['autotranscode', 'recordingtype', 'recordmpts'],
      Video: [],
      Audio: []
    },
    {
      CardType: 'CRC_IP',
      Param: ['autotranscode', 'height', 'width'],
      Video: ['RTjpeg', 'MPEG-4'],
      Audio: ['MP3', 'Uncompressed']
    },
    {
      CardType: 'HDPVR',
      Param: ['autotranscode'],
      Video: ['MPEG-4 AVC Hardware Encoder'],
      Audio: ['AC3 Hardware Encoder', 'AAC Hardware Encoder']
    },
    {
      CardType: 'ASI',
      Param: ['autotranscode', 'recordmpts'],
      Video: [],
      Audio: []
    },
    {
      CardType: 'OCUR',
      Param: ['autotranscode', 'height', 'width'],
      Video: ['RTjpeg', 'MPEG-4'],
      Audio: ['MP3', 'Uncompressed']
    },
    {
      CardType: 'CETON',
      Param: ['autotranscode', 'recordmpts'],
      Video: [],
      Audio: []
    },
    {
      CardType: 'VBOX',
      Param: ['autotranscode', 'recordmpts'],
      Video: [],
      Audio: []
    },
    {
      CardType: 'SATIP',
      Param: ['autotranscode', 'recordingtype', 'recordmpts'],
      Video: [],
      Audio: []
    },
    {
      CardType: 'V4L2:uvcvideo',
      Param: ['autotranscode', 'height', 'width'],
      Video: ['V4L2:MPEG-2 Video'],
      Audio: ['V4L2:MPEG-1/2 Layer II encoding']
    },
    {
      CardType: 'TRANSCODE',
      Param: ['autotranscode', 'height', 'width'],
      Video: ['RTjpeg', 'MPEG-4'],
      Audio: ['MP3', 'Uncompressed']
    },
  ];

  //params is the entry of above list (paramList) currently in use
  params = {
    CardType: '',
    Param: [''],
    Video: [''],
    Audio: ['']
  };

  videoParamList = [
    {
      Codec: 'RTjpeg',
      Param: ['rtjpegquality', 'rtjpeglumafilter', 'rtjpegchromafilter']
    },
    {
      Codec: 'MPEG-4',
      Param: ['mpeg4bitrate', 'mpeg4maxquality', 'mpeg4minquality',
        'mpeg4qualdiff', 'scalebitrate', 'mpeg4optionvhq',
        'mpeg4option4mv', 'mpeg4optionidct', 'mpeg4optionime',
        'encodingthreadcount']
    },
    {
      Codec: 'MPEG-2',
      Param: ['mpeg2bitrate', 'scalebitrate', 'encodingthreadcount']
    },
    {
      Codec: 'Hardware MJPEG',
      Param: ['hardwaremjpegquality', 'hardwaremjpeghdecimation', 'hardwaremjpegvdecimation']
    },
    {
      Codec: 'MPEG-2 Hardware Encoder',
      Param: ['mpeg2streamtype', 'mpeg2aspectratio', 'mpeg2bitrate',
        'mpeg2maxbitrate']
    },
    {
      Codec: 'MPEG-4 AVC Hardware Encoder',
      Param: ['low_mpeg4avgbitrate', 'low_mpeg4peakbitrate',
        'medium_mpeg4avgbitrate', 'medium_mpeg4peakbitrate',
        'high_mpeg4avgbitrate', 'high_mpeg4peakbitrate'],
    },
    {
      Codec: 'V4L2:MPEG-2 Video',
      Param: [],
    },
  ];

  audioParamList = [
    {
      Codec: 'MP3',
      Param: ['samplerate', 'mp3quality', 'volume']
    },
    {
      Codec: 'MPEG-2 Hardware Encoder',
      Param: ['samplerate', 'mpeg2language', 'mpeg2audvolume']
    },
    {
      Codec: 'Uncompressed',
      Param: ['samplerate', 'volume']
    },
  ];


  recordingtypeOptions = [
    { Name: 'settings.rprofiles.rectype_normal', Value: 'all' },
    { Name: 'settings.rprofiles.rectype_tv', Value: 'tv' },
    { Name: 'settings.rprofiles.rectype_audio', Value: 'audio' }
  ];

  samplerateOptions = ['32000', '44100', '48000'];

  streamTypeOptions = [
    "MPEG-2 PS", "MPEG-2 TS",
    "MPEG-1 VCD", "PES AV",
    "PES V", "PES A",
    "DVD", "DVD-Special 1", "DVD-Special 2"
  ];

  mpeg2languageOptions = [
    { Name: 'settings.rprofiles.lang_main', Value: '0' },
    { Name: 'settings.rprofiles.lang_sap', Value: '1' },
    { Name: 'settings.rprofiles.lang_dual', Value: '2' },
  ];

  constructor(private translate: TranslateService, private captureCardService: CaptureCardService,
    public setupService: SetupService) {
    this.recordingtypeOptions.forEach(entry => {
      translate.get(entry.Name).subscribe(data => entry.Name = data);
    });
    this.mpeg2languageOptions.forEach(entry => {
      translate.get(entry.Name).subscribe(data => entry.Name = data);
    });
  }

  ngOnInit(): void {
    let params = this.paramList.find(entry => entry.CardType == this.group.CardType);
    if (params)
      this.params = params;
  }

  ngAfterViewInit(): void {
    this.setupService.setCurrentForm(this.currentForm);
    this.topElement.nativeElement.scrollIntoView({ behavior: "smooth", block: "start" });
  }


  findIx(name: string, defValue: any): number {
    let profIndex = this.profile.RecProfParams.findIndex(entry => entry.Name == name);
    if (profIndex == -1)
      profIndex = this.profile.RecProfParams.push({ Name: name, Value: defValue }) - 1;
    else {
      if (typeof defValue == 'boolean' && typeof this.profile.RecProfParams[profIndex].Value == 'string')
        this.profile.RecProfParams[profIndex].Value = (this.profile.RecProfParams[profIndex].Value == '1');
    }
    return profIndex;
  }

  saveObserver: PartialObserver<any> = {
    next: (x: any) => {
      if (this.profile.Id && x.bool)
        this.successCount++;
      else if (!this.profile.Id && x.int) {
        this.successCount++;
        this.profile.Id = x.int;
        this.saveForm(2);
      }
      else {
        console.log("saveObserver error", x);
        this.errorCount++;
        this.currentForm.form.markAsDirty();
      }
    },
    error: (err: any) => {
      console.log("saveObserver error", err);
      this.errorCount++;
      this.currentForm.form.markAsDirty();
    }
  };

  saveForm(part: number) {
    switch (part) {
      // @ts-ignore: Fallthrough case in switch
      case 1:
        // 1. Save profile
        this.successCount = 0;
        this.errorCount = 0;
        this.expectedCount = 0;
        if (this.profile.Id <= 0) {
          this.captureCardService.AddRecProfile(this.group.Id, this.profile.Name, this.profile.VideoCodec,
            this.profile.AudioCodec)
            .subscribe(this.saveObserver);
          this.expectedCount++;
          return;
        } else {
          this.captureCardService.UpdateRecProfile(this.profile.Id, this.profile.VideoCodec,
            this.profile.AudioCodec)
            .subscribe(this.saveObserver);
          this.expectedCount++;
          // Fall through to case 2
        }
      case 2:
        // 2. Save profile parameters
        this.saveParams(this.params.Param);
        let vParams = this.videoParamList.find(entry => entry.Codec == this.profile.VideoCodec);
        if (vParams)
          this.saveParams(vParams.Param);
        else {
          console.log("ERROR videoparams not found");
          this.errorCount++;
          this.currentForm.form.markAsDirty();
        }
        let aParams = this.audioParamList.find(entry => entry.Codec == this.profile.AudioCodec);
        if (aParams)
          this.saveParams(aParams.Param);
        else {
          console.log("ERROR audioparams not found");
          this.errorCount++;
          this.currentForm.form.markAsDirty();
        }
    }
  }

  saveParams(Params: string[]) {
    Params.forEach(entry => {
      let value = this.profile.RecProfParams[(this.findIx(entry, '0'))].Value;
      if (typeof value == 'boolean')
        value = value ? '1' : '0';
      else
        value = value.toString();
      this.captureCardService.UpdateRecProfileParam(this.profile.Id, entry, value)
        .subscribe(this.saveObserver);
      this.expectedCount++;
    });

  }

}
