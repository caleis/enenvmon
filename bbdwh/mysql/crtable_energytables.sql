
CREATE TABLE EnergyMain (
	TimeStamp	TIMESTAMP,
	Power		FLOAT,
	Reactivepower	FLOAT,
	Powerfactor	FLOAT,
	Voltage		FLOAT,
	Total		FLOAT,
	Totalreturned	FLOAT,
	Isvalid		BOOLEAN,
	PRIMARY KEY 	(TimeStamp),
	UNIQUE		(TimeStamp)
);
CREATE TABLE EnergySolar (
	TimeStamp	TIMESTAMP,
	Power		FLOAT,
	Reactivepower	FLOAT,
	Powerfactor	FLOAT,
	Voltage		FLOAT,
	Total		FLOAT,
	Totalreturned	FLOAT,
	Isvalid		BOOLEAN,
	PRIMARY KEY 	(TimeStamp),
	UNIQUE		(TimeStamp)
);

