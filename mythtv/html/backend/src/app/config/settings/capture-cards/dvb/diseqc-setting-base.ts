import { NgForm } from "@angular/forms";
import { Observable } from "rxjs";

export interface DiseqcSettingBase {
    currentForm: NgForm;
    saveForm() : Observable<any>;
}
