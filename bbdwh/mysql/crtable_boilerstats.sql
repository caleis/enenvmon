
CREATE TABLE BoilerStats (
	TimeStamp	TIMESTAMP,
	Periodctr	SMALLINT(4),
	DownOnperiod	SMALLINT(4),
	DownOnctr	TINYINT(3),
	DownOnpercentage	FLOAT,
	UpOnperiod	SMALLINT(4),
	UpOnctr		TINYINT(3),
	UpOnpercentage	FLOAT,
	PRIMARY KEY 	(TimeStamp),
	UNIQUE		(TimeStamp)
);
