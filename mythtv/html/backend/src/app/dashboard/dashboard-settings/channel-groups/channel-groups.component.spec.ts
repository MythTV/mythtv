import { ComponentFixture, TestBed } from '@angular/core/testing';

import { ChannelGroupsComponent } from './channel-groups.component';

describe('ChannelGroupsComponent', () => {
  let component: ChannelGroupsComponent;
  let fixture: ComponentFixture<ChannelGroupsComponent>;

  beforeEach(() => {
    TestBed.configureTestingModule({
    imports: [ChannelGroupsComponent]
});
    fixture = TestBed.createComponent(ChannelGroupsComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
