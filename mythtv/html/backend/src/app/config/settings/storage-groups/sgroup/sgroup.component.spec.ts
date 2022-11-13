import { ComponentFixture, TestBed } from '@angular/core/testing';

import { SgroupComponent } from './sgroup.component';

describe('SgroupComponent', () => {
  let component: SgroupComponent;
  let fixture: ComponentFixture<SgroupComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ SgroupComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(SgroupComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
