import { ComponentFixture, TestBed } from '@angular/core/testing';

import { BackendnetworkComponent } from './backendnetwork.component';

describe('BackendnetworkComponent', () => {
  let component: BackendnetworkComponent;
  let fixture: ComponentFixture<BackendnetworkComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ BackendnetworkComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(BackendnetworkComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
