import { ComponentFixture, TestBed } from '@angular/core/testing';

import { FrontendsComponent } from './frontends.component';

describe('FrontendsComponent', () => {
  let component: FrontendsComponent;
  let fixture: ComponentFixture<FrontendsComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ FrontendsComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(FrontendsComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
