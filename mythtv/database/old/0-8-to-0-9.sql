USE mythconverg;

CREATE INDEX progid ON record (chanid, starttime);
CREATE INDEX title ON record (title(10)); 
CREATE INDEX title ON program (title(10));   
