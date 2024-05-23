// SSID and password for the ID
#define SECRET_SSID ""
#define SECRET_PASS ""

// ID to be distinguished on the motor repo
#define MOTOR_ID    ""

// this can be an IP or HTTP address
#define BROKER_ADDRESS  ""
#define BROKER_PORT     8884
// topic in which the communication is happening 
#define MQTT_TOPIC      "system/motor_devices"

// button pin should be defined but may be disconnected
// if you don't require a controlling button
#define BUTTON_PIN          3
#define SERVO_PIN           9

// degree to which servo is considered fully open or closed
// definitions on when door is closed and when door is open
// CLOSED must be LOWER than OPEN
#define GARAGE_DOOR_CLOSED  0
#define GARAGE_DOOR_OPEN    90

// definitions of MQTT username and password
// if undefined, anonymous connection will be attempted
/**
#define MQTT_USERNAME       ""
#define MQTT_PASSWORD       ""
*/