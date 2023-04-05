import { ComponentFixture, TestBed } from '@angular/core/testing';

import { VboxComponent } from './vbox.component';

describe('VboxComponent', () => {
  let component: VboxComponent;
  let fixture: ComponentFixture<VboxComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ VboxComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(VboxComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
