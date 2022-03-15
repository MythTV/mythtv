import { ComponentFixture, TestBed } from '@angular/core/testing';
import { RouterTestingModule } from '@angular/router/testing';

import { SgsetupComponent } from './sgsetup.component';

describe('SgsetupComponent', () => {
  let component: SgsetupComponent;
  let fixture: ComponentFixture<SgsetupComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ SgsetupComponent ],
      imports:      [ RouterTestingModule ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(SgsetupComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
