import { provideHttpClient, withInterceptorsFromDi } from '@angular/common/http';
import { TestBed } from '@angular/core/testing';

import { MythService } from './myth.service';

describe('MythService', () => {
  let service: MythService;

  beforeEach(() => {
    TestBed.configureTestingModule({
    imports: [],
    providers: [provideHttpClient(withInterceptorsFromDi())]
});
    service = TestBed.inject(MythService);
  });

  it('should be created', () => {
    expect(service).toBeTruthy();
  });
});
