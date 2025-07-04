
CREATE TABLE EnviroCond (
	TimeStamp	TIMESTAMP,
	TempLounge	FLOAT,
	TempHall	FLOAT,
	TempBedroom	FLOAT,
	TempDatactr	FLOAT,
	TempExt		FLOAT,
	Cloud		TINYINT(3),
	Rain		FLOAT,
	Windspeed	FLOAT,
	Winddeg		SMALLINT(3),
	Pressure	SMALLINT(4),
	Humidity	TINYINT(3),
	Visibility	SMALLINT(5),
	Summary		VARCHAR(20),
	PRIMARY KEY 	(TimeStamp),
	UNIQUE		(TimeStamp)
);
