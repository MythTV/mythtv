USE mythtconverg;
ALTER TABLE conflictresolutionsingle DROP PRIMARY KEY;
ALTER TABLE conflictresolutionoverride DROP PRIMARY KEY;
ALTER TABLE conflictresolutionsingle ADD INDEX (preferchannum, preferstarttime);
ALTER TABLE conflictresolutionoverride ADD INDEX (channum, starttime);
