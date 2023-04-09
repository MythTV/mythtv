import { ComponentFixture, TestBed } from '@angular/core/testing';

import { ChannelEditorComponent } from './channel-editor.component';

describe('ChannelEditorComponent', () => {
  let component: ChannelEditorComponent;
  let fixture: ComponentFixture<ChannelEditorComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ ChannelEditorComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(ChannelEditorComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
