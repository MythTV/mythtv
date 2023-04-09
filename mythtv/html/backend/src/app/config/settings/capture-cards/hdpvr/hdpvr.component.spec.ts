import { ComponentFixture, TestBed } from '@angular/core/testing';

import { HdpvrComponent } from './hdpvr.component';

describe('HdpvrComponent', () => {
  let component: HdpvrComponent;
  let fixture: ComponentFixture<HdpvrComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ HdpvrComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(HdpvrComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
