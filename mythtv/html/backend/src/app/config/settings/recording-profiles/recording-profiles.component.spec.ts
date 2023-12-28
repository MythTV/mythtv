import { ComponentFixture, TestBed } from '@angular/core/testing';

import { RecordingProfilesComponent } from './recording-profiles.component';

describe('RecordingProfilesComponent', () => {
  let component: RecordingProfilesComponent;
  let fixture: ComponentFixture<RecordingProfilesComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ RecordingProfilesComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(RecordingProfilesComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
