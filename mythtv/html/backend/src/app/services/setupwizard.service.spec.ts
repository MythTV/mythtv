import { provideHttpClient, withInterceptorsFromDi } from '@angular/common/http';
import { TestBed } from '@angular/core/testing';

import { SetupWizardService } from './setupwizard.service';

describe('SetupwizardService', () => {
  let service: SetupWizardService;

  beforeEach(() => {
    TestBed.configureTestingModule({
    imports: [],
    providers: [provideHttpClient(withInterceptorsFromDi())]
});
    service = TestBed.inject(SetupWizardService);
  });

  it('should be created', () => {
    expect(service).toBeTruthy();
  });
});
