import { ComponentFixture, TestBed } from '@angular/core/testing';

import { MachineinfoComponent } from './machineinfo.component';

describe('MachineinfoComponent', () => {
  let component: MachineinfoComponent;
  let fixture: ComponentFixture<MachineinfoComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ MachineinfoComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(MachineinfoComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
