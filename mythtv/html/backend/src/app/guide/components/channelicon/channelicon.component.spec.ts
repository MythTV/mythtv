import { ComponentFixture, TestBed } from '@angular/core/testing';

import { ChannelIconComponent } from './channelicon.component';

describe('ChannelIconComponent', () => {
  let component: ChannelIconComponent;
  let fixture: ComponentFixture<ChannelIconComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ ChannelIconComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(ChannelIconComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
