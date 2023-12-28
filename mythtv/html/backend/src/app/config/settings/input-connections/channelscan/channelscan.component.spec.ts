import { ComponentFixture, TestBed } from '@angular/core/testing';

import { ChannelscanComponent } from './channelscan.component';

describe('ChannelscanComponent', () => {
  let component: ChannelscanComponent;
  let fixture: ComponentFixture<ChannelscanComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ ChannelscanComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(ChannelscanComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
