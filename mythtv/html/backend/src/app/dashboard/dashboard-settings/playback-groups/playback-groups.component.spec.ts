import { ComponentFixture, TestBed } from '@angular/core/testing';

import { PlaybackGroupsComponent } from './playback-groups.component';

describe('PlaybackGroupsComponent', () => {
  let component: PlaybackGroupsComponent;
  let fixture: ComponentFixture<PlaybackGroupsComponent>;

  beforeEach(() => {
    TestBed.configureTestingModule({
      declarations: [PlaybackGroupsComponent]
    });
    fixture = TestBed.createComponent(PlaybackGroupsComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
