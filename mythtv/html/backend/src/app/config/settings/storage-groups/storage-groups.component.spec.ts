import { ComponentFixture, TestBed } from '@angular/core/testing';

import { StorageGroupsComponent } from './storage-groups.component';

describe('StorageGroupsComponent', () => {
  let component: StorageGroupsComponent;
  let fixture: ComponentFixture<StorageGroupsComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ StorageGroupsComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(StorageGroupsComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
