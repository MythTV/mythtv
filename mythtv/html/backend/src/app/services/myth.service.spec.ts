import { TestBed } from '@angular/core/testing';

import { MythService } from './myth.service';

describe('MythService', () => {
  let service: MythService;

  beforeEach(() => {
    TestBed.configureTestingModule({});
    service = TestBed.inject(MythService);
  });

  it('should be created', () => {
    expect(service).toBeTruthy();
  });
});
