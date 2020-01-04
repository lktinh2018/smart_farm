# This repo use ESP32 (Gateway) & STM32 (Node) for smart farm project.

# How to connect:
	+ Gateway (ESP32):
		- Lora: 
			+ M0	: GND
			+ M1	: GND
			+ RX	: D4
			+ TX	: D5	
			+ VCC	: 3.3
			+ GND	: GND	
			
			
	+ Node (STM32)
		- Lora:
			+ M0	: GND
			+ M1	: GND
			+ RX	: A9
			+ TX	: A10	
			+ VCC	: 3.3
			+ GND	: GND	

		- AM2315:
			+ Red 	(VCC)	: 3.3
			+ Brown (GND) 	: GND
			+ Yellow(SDA)	: B7 (Pull Up Registor)
			+ White	(SCL)	: B6 (Pull Up Registor)

		- Solenoid:
			+ B10
			+ GND

		- Analog pin (read battery status):
			+ B1


# Supported channel:
	+ Channel /iFarm/control: Control solenoid.
		{"device_id":1,"type":2,"solenoid":1}
	+ Channel /iFarm/config: Config default value for node (Auto mod: 1/0, temp: min/max, hum: min/max).
		{"device_id":1,"command":1,"auto_mode":1,"temp":{"min_temp":1.1,"max_temp":2.2},"hum":{"min_hum":3.3,"max_hum":4.4}, "sending_cycle":10}
	+ Channel /iFarm/device: Receive data sensor, battery status of nodes.
		{"device_id":1,"temperature":100,"humidity":100,"battery":267}
