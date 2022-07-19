import { ComponentFixture, TestBed } from '@angular/core/testing';

import { UnicableComponent } from './unicable.component';

describe('UnicableComponent', () => {
  let component: UnicableComponent;
  let fixture: ComponentFixture<UnicableComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ UnicableComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(UnicableComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
