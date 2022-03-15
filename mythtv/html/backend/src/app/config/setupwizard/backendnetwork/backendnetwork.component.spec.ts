import { HttpClientModule } from '@angular/common/http';
import { ComponentFixture, TestBed } from '@angular/core/testing';
import { RouterTestingModule } from '@angular/router/testing';

import { BackendnetworkComponent } from './backendnetwork.component';

describe('BackendnetworkComponent', () => {
  let component: BackendnetworkComponent;
  let fixture: ComponentFixture<BackendnetworkComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ BackendnetworkComponent ],
      imports:       [ HttpClientModule,
                       RouterTestingModule ]
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
