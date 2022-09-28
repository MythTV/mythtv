import { ComponentFixture, TestBed } from '@angular/core/testing';

import { VsourceComponent } from './vsource.component';

describe('VsourceComponent', () => {
  let component: VsourceComponent;
  let fixture: ComponentFixture<VsourceComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ VsourceComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(VsourceComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
