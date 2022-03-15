import { HttpClientModule } from '@angular/common/http';
import { ComponentFixture, TestBed } from '@angular/core/testing';
import { FormsModule } from '@angular/forms';
import { RouterTestingModule } from '@angular/router/testing';

import { DbsetupComponent } from './dbsetup.component';

describe('DbsetupComponent', () => {
  let component: DbsetupComponent;
  let fixture: ComponentFixture<DbsetupComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ DbsetupComponent ],
      imports:      [ FormsModule,
                      HttpClientModule,
                      RouterTestingModule ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(DbsetupComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
