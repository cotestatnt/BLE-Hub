#include <Arduino.h>

static const char createHubs[] PROGMEM = R"string_literal(
CREATE TABLE hubs (
	hub_id INT auto_increment not NULL,
	hub_name varchar(32) DEFAULT 'HUB Name' NOT NULL,
	hub_address varchar(18) NULL,
	location varchar(16) NULL,
	latitude FLOAT NULL,
	longitude FLOAT NULL,
	CONSTRAINT hubs_pk PRIMARY KEY (hub_id),
	CONSTRAINT name_unique UNIQUE KEY (hub_name),
	CONSTRAINT address_unique UNIQUE KEY (hub_address)
)
ENGINE=InnoDB
DEFAULT CHARSET=utf8mb4;
)string_literal";

static const char createBleDevices[] PROGMEM = R"string_literal(
CREATE TABLE ble_devices (
	ble_address varchar(18) NULL,
	ble_id varchar(32) NOT NULL,
	ble_name varchar(32) DEFAULT "BLE Device Name" NULL,
	connected_to varchar(32) DEFAULT "HUB Name" NULL,
	msg_epoch INT UNSIGNED NULL,
	msg_buffer TEXT NULL,
	CONSTRAINT ble_devices_pk PRIMARY KEY (ble_id),
	CONSTRAINT ble_id_unique UNIQUE KEY (ble_id),
	CONSTRAINT address_unique UNIQUE KEY (ble_address)
)
ENGINE=InnoDB
DEFAULT CHARSET=utf8mb4;
)string_literal";


static const char createConnections[] PROGMEM = R"string_literal(
CREATE TABLE connections (
	conn_id INT auto_increment NOT NULL,
	ble_id varchar(32) DEFAULT "BLE Device Name" NOT NULL,
	targets varchar(128) NOT NULL,
	msg_out TEXT NULL,
	CONSTRAINT connections_pk PRIMARY KEY (conn_id),
	CONSTRAINT connect_unique UNIQUE KEY (ble_id, targets)
)
ENGINE=InnoDB
DEFAULT CHARSET=utf8mb4;
)string_literal";


// This trigger will update field 'msg_buffer' of each device listed in edited 'connections' row
static const char newMessageTrigger[] PROGMEM = R"string_literal(
CREATE TRIGGER `new_message`
AFTER UPDATE ON `connections`
FOR EACH ROW
	update ble_devices
	set msg_buffer = new.msg_out,
	msg_epoch =  UNIX_TIMESTAMP(NOW())
	WHERE FIND_IN_SET(ble_id, REPLACE(new.targets, ', ', ',')) > 0;
)string_literal";



// Query used for getting list of devices connected to hub (with list of targets)
static const char selectDevices[] PROGMEM = R"string_literal(
SELECT ble_devices.ble_id, ble_devices.ble_address, ble_devices.ble_name, connections.targets
FROM ble_devices
LEFT JOIN connections ON ble_devices.ble_id = connections.ble_id
WHERE ble_devices.connected_to = '%s';
)string_literal";


// Check if is present a message for one of connected BLE device
static const char checkMessages[] PROGMEM = R"string_literal(
SELECT ble_address, ble_id, msg_epoch, msg_buffer FROM %s
where connected_to = '%s' and Length(`msg_buffer`)>0;
)string_literal";


static const char newMessage[] PROGMEM = R"string_literal(
UPDATE %s SET msg_out = '%s' WHERE ble_id = '%s' AND targets = '%s';
)string_literal";



// Get a list of devices and HUB names to fill HTML select options
static const char selectDevicesHubs[] PROGMEM = R"string_literal(
SELECT ble_id as name, 'device' AS type
FROM %s
UNION ALL
select hub_name as name, 'hub' AS type
FROM %s;
)string_literal";


// Insert or update a device (if ble_id already defined keep it's actual value)
static const char newBleDevice[] PROGMEM = R"string_literal(
INSERT INTO ble_devices (ble_address, ble_id, connected_to, ble_name)
VALUES ('%s', '%s', '%s', '%s')
ON DUPLICATE KEY UPDATE
    connected_to = VALUES(connected_to);
)string_literal";

// Insert or update a device
static const char newBleDeviceRewrite[] PROGMEM = R"string_literal(
INSERT INTO ble_devices (ble_address, ble_id, connected_to, ble_name)
VALUES ('%s', '%s', '%s', '%s')
ON DUPLICATE KEY UPDATE
    connected_to = VALUES(connected_to),
	ble_id = VALUES(ble_id),
	ble_name = VALUES(ble_name);
)string_literal";


