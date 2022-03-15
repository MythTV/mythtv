import { HttpClientModule } from '@angular/common/http';
import { TestBed } from '@angular/core/testing';

import { StatusService } from './status.service';

describe('StatusService', () => {
  let service: StatusService;

  beforeEach(() => {
    TestBed.configureTestingModule({
      imports: [ HttpClientModule ]
    });
    service = TestBed.inject(StatusService);
  });

  it('should be created', () => {
    expect(service).toBeTruthy();
  });
});
