import { ComponentFixture, TestBed } from '@angular/core/testing';

import { HdhomerunComponent } from './hdhomerun.component';

describe('HdhomerunComponent', () => {
  let component: HdhomerunComponent;
  let fixture: ComponentFixture<HdhomerunComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ HdhomerunComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(HdhomerunComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
