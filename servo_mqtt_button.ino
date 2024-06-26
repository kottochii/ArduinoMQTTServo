#include <ArduinoMqttClient.h>
#if defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_AVR_UNO_WIFI_REV2)
  #include <WiFiNINA.h>
#elif defined(ARDUINO_SAMD_MKR1000)
  #include <WiFi101.h>
#elif defined(ARDUINO_ARCH_ESP8266)
  #include <ESP8266WiFi.h>
#elif defined(ARDUINO_PORTENTA_H7_M7) || defined(ARDUINO_NICLA_VISION) || defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_GIGA) || defined(ARDUINO_OPTA)
  #include <WiFi.h>
#elif defined(ARDUINO_PORTENTA_C33)
  #include <WiFiC3.h>
#elif defined(ARDUINO_UNOR4_WIFI)
  #include <WiFiS3.h>
#endif

#include "config.h"
#include <vector>
///////data is in config.h
char ssid[] = SECRET_SSID;    // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)

// 15ms required for the motor to get through the iteration
#define ITERATION_MOVEMENT_DELAY    15

#ifdef MQTT_CONNECTION_REQUIRES_SSL
WiFiSSLClient wifiClient;
#else
WiFiClient wifiClient;
#endif
MqttClient mqttClient(wifiClient);

const char broker[] = BROKER_ADDRESS;
int        port     = BROKER_PORT;
const char topic[]  = MQTT_TOPIC;
#include <Servo.h>


Servo main_servo;

bool button_pressed()
{
    return digitalRead(BUTTON_PIN);
}

struct
{
    bool last_value = false;
    bool just_changed = false;
} 
button_state;

struct 
{
    enum target_type
    {
        CLOSE,
        OPEN
    };
    bool targeted = false;
    target_type target = CLOSE;
    enum current_state
    {
        CLOSING,
        OPENING,
        CLOSED,
        OPENED,
        TOCLOSE,
        TOOPEN
    };
}
motor_state;

int lastServerReport = millis();



void setup() {
    //Initialize serial and wait for port to open:
    Serial.begin(9600);
    /*
    // should not expect connections to the Arduino
    while (!Serial) {
        ; // wait for serial port to connect. Needed for native USB port only
    }
    */

    // init the "PROBLEM" LED
    pinMode(LED_BUILTIN, OUTPUT);
    // set to "on" straight away to see that the connection was not established
    digitalWrite(LED_BUILTIN, HIGH);

    // attempt to connect to WiFi network:
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    uint cnxn_attempts = 0;
    int connection_result = 0;
    while (((connection_result = WiFi.begin(ssid, pass)) != WL_CONNECTED) && (cnxn_attempts < 10)) {
        // failed, retry
        Serial.print(".");
        delay(1000);
        cnxn_attempts++;
    }

    if(connection_result == WL_CONNECTED)
    {
        Serial.println("You're connected to the network");
        Serial.println();

        #ifdef MQTT_CLIENT_ID
        mqttClient.setId(MQTT_CLIENT_ID);
        #else
        mqttClient.setId("motor_" MOTOR_ID);
        #endif

        #if defined( MQTT_USERNAME) && defined (MQTT_PASSWORD)
        mqttClient.setUsernamePassword(MQTT_USERNAME, MQTT_PASSWORD);
        #endif

        Serial.print("Attempting to connect to the MQTT broker: ");
        Serial.println(broker);

        if (!mqttClient.connect(broker, port)) {
            Serial.print("MQTT connection failed! Error code = ");
            Serial.println(mqttClient.connectError());

        }
        else
        {
            // set the message receive callback
            mqttClient.onMessage(onMqttMessage);
            // subscribe to a topic
            mqttClient.subscribe(topic);

            Serial.print("Connected, waiting for messages");
        }
    }
    else
    {
        Serial.println("Wifi connection failed.");
        Serial.println("You can restart the controller to try again or continue without the connection.");
        Serial.println("Only physical button will be enabled then.");
    }
    // set up the servo and the button
    main_servo.attach(SERVO_PIN);           // attaches the servo on pin 9 to the servo object
    #ifdef STARTPOINT
    main_servo.write(STARTPOINT);           // start with given value
    #else
    main_servo.write(0);                    // start with 0 degrees
    #endif
    // init button
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
    static int previousTime = millis();
    int currentTime = millis();

    // if the server's disconnected, turn the light on to indicate the problem
    if (!wifiClient.connected()) {
        digitalWrite(LED_BUILTIN, HIGH);
    }
    else if(currentTime - lastServerReport > 10000)
    {
        digitalWrite(LED_BUILTIN, HIGH);
    }
    else digitalWrite(LED_BUILTIN, LOW);

    // every 10 seconds report state
    if(currentTime - previousTime > 500)
    {
        previousTime = currentTime;
        sendMqttMessage(get_current_state());
    }
    // poll messages only if wifi is active
    if (wifiClient.connected())
        mqttClient.poll();


    // first of all check the button state
    bool current_button = button_pressed();
    if(button_state.last_value != current_button)
    {
        button_state.last_value = current_button;
        button_state.just_changed = true;
    }

    // factual onButtonPressed event
    if(button_state.just_changed)
    {
        button_state.just_changed = false;
        // only get triggered when the button is pressed
        if(current_button)
        {
            Serial.println("Button was pressed");
            motor_state.targeted = !motor_state.targeted;

            if(motor_state.targeted)
            {
                switch(motor_state.target)
                {
                    case motor_state.CLOSE: motor_state.target = motor_state.OPEN; break;
                    case motor_state.OPEN: motor_state.target = motor_state.CLOSE; break;
                }
            }
        }
    }

    // move motor if req'd
    if(motor_state.targeted)
    {
        int local_target;
        int current_motor_value = main_servo.read();
        bool target_reached = false;
        switch(motor_state.target)
        {
            case motor_state.CLOSE:
                local_target = current_motor_value - 1;
                if(current_motor_value <= GARAGE_DOOR_CLOSED)
                    target_reached = true;
                break;
            case motor_state.OPEN:
                local_target = current_motor_value + 1;
                if(current_motor_value >= GARAGE_DOOR_OPEN)
                    target_reached = true;
                break;
            default: return;
        }

        if(target_reached)
        {
            Serial.println("Door is now closed");
            motor_state.targeted = false;
        }

        main_servo.write(local_target);
        delay(ITERATION_MOVEMENT_DELAY);


    }
}

String get_current_state()
{
    // state and motor id to distinguish between multiple motors
    String returnable = "state ";
    returnable += MOTOR_ID;

    // degree to which the motor is currently open, [0.00, 1.00] where 0. is fully closed, 1. is fully open
    returnable += " ";
    auto opened_degree = ((double)main_servo.read() / GARAGE_DOOR_OPEN);
    if(opened_degree < 0)
        opened_degree = 0;
    else if (opened_degree > 1)
        opened_degree = 1;
    returnable += opened_degree;

    // whether the motor is moving or stopped
    returnable += " ";
    returnable += motor_state.targeted?"moving":"stopped";

    // if moving, then direction to which it is moving
    if(motor_state.targeted)
        switch(motor_state.target)
        {
            case motor_state.CLOSE:
                returnable += " close";
                break;
            case motor_state.OPEN:
                returnable += " open";
                break;
        }
    return returnable;
}

void sendMqttMessage(String message)
{
    mqttClient.beginMessage(topic);
    mqttClient.print(message);
    mqttClient.endMessage();
}

void onMqttMessage(int messageSize) {
    auto msg = mqttClient.readString();
    std::vector<String> tokens;
    // split string by \s
    while(msg.length() > 0)
    {
        int index = msg.indexOf(' ');
        // space is not found
        if(index == -1)
        {
            // put entire or rest of the message
            tokens.push_back(msg);
            Serial.println(tokens.size());
            break;
        }
        else
        {
            tokens.push_back(msg.substring(0, index)); // push back only part of it
            msg = msg.substring(index + 1); // cut the original string
        }
    }
    // echoes the entire message
    for(auto i = tokens.begin(); i != tokens.end(); i++)
    {
        Serial.print(*i);
        Serial.println();
    }

    // from this point, the handlers of commands will return if there is nothing for them to do

    // the server should let know that it is up at least once in 10 seconds
    if(tokens[0] == "server")
    {
        Serial.println("SERVER MSG");
        if(tokens.size() != 2)
            return;
        if(tokens[1] == "ok")
        {
            Serial.println("SERVER OK");
            lastServerReport = millis();
        }
    }

    // if the request is isthere
    if(tokens[0] == "isthere")
    {
        if(tokens.size() != 2)
            return;
        if(tokens[1] == MOTOR_ID)
            sendMqttMessage(((String)"thereis ") + MOTOR_ID);
    }

    // if the request is status
    if(tokens[0] == "status")
    {
        if(tokens.size() != 2)
            return;
        if(tokens[1] == MOTOR_ID)
        {
            // this is our request, give the current state of the motor
            sendMqttMessage(get_current_state());
        }
    }    

    // if the request is open
    if(tokens[0] == "open")
    {
        if(tokens.size() != 2)
            return;
        if(tokens[1] == MOTOR_ID)
        {
            // this is our request, target open
            motor_state.target = motor_state.OPEN;
            motor_state.targeted = true;
        }
    }
    
    // if the request is open
    if(tokens[0] == "close")
    {
        if(tokens.size() != 2)
            return;
        if(tokens[1] == MOTOR_ID)
        {
            // this is our request, target close
            motor_state.target = motor_state.CLOSE;
            motor_state.targeted = true;
        }
    }

    // if the request is open
    if(tokens[0] == "stop")
    {
        if(tokens.size() != 2)
            return;
        if(tokens[1] == MOTOR_ID)
        {
            // this is our request, stop immidiately
            motor_state.targeted = false;
        }
    }

}
