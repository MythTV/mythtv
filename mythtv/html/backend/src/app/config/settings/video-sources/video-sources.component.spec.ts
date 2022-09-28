import { ComponentFixture, TestBed } from '@angular/core/testing';

import { VideoSourcesComponent } from './video-sources.component';

describe('VideoSourcesComponent', () => {
  let component: VideoSourcesComponent;
  let fixture: ComponentFixture<VideoSourcesComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ VideoSourcesComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(VideoSourcesComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
