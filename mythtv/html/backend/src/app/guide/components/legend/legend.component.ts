import { Component, Input, OnInit } from '@angular/core';
import { ScheduleOrProgram } from 'src/app/services/interfaces/program.interface';
import { ProgramGuide } from 'src/app/services/interfaces/programguide.interface';
import { MythService } from 'src/app/services/myth.service';

@Component({
  selector: 'app-legend',
  templateUrl: './legend.component.html',
  styleUrls: ['./legend.component.css']
})
export class LegendComponent implements OnInit {
  @Input() programGuide!: ProgramGuide | null;
  @Input() listPrograms!: ScheduleOrProgram[] | null;

  catTypes: string[] = [];
  categories: string[] = [];
  regex = /[^a-z0-9]/g;
  cssFile = '';

  constructor(private mythService: MythService) { }

  ngOnInit(): void {
    this.loadfInfo();
    let setTypes = new Set();
    let setCats = new Set();
    if (this.programGuide != null) {
      this.programGuide.ProgramGuide.Channels.forEach(
        (channel) => {
          channel.Programs.forEach(
            (program) => {
              if (program.CatType)
                setTypes.add(program.CatType);
              if (program.Category)
                setCats.add(program.Category);
            }
          );
        }
      );
    }
    else if (this.listPrograms != null) {
      this.listPrograms.forEach(
        (program) => {
          if (program.CatType)
            setTypes.add(program.CatType);
          if (program.Category)
            setCats.add(program.Category);
        }
      )
    }
    this.catTypes = [];
    let iter = setTypes.values();
    while (true) {
      let x = iter.next();
      if (x.done)
        break;
      else
        this.catTypes.push(<string>x.value);
    }
    this.categories = [];
    iter = setCats.values();
    while (true) {
      let x = iter.next();
      if (x.done)
        break;
      else
        this.categories.push(<string>x.value);
    }
    this.catTypes.sort();
    this.catTypes.push("default")
    this.categories.sort();
  }

  loadfInfo() {
    this.mythService.GetBackendInfo().subscribe(data => {
      this.cssFile = data.BackendInfo.Env.HttpRootDir + './assets/guidecolors.css';
    });
  }

}
