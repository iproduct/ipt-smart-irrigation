#include <Arduino.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <coap-simple.h>
#include <ArduinoJson.h>
#include "arduino_secrets.h"

#define MAX_JSON_SIZE 1024    // maximum size of a JSON command message
#define MAX_COMMANDS 10       // maximum number of commands sent to the robot
#define MAX_COMMAND_PARAMS 8  // maximum number of commands sent to the robot

#define NO_COMMAND 0    // no robot command received
#define OPEN_VALVE 1    // open selected single irrigation valve
#define OPEN_VALVES 2   // open selected irrigation valves
#define CLOSE_VALVE 3   // close selected irrigation valves, if no parameters closes all valves
#define CLOSE_VALVES 4  // close selected irrigation valves, if no parameters closes all valves

#define LITERS_PER_PULSE 0.303  // traveled distance per encoder pulse
// #define MIN_MOISTURE 980        // minimum moisture reading value
// #define MAX_MOISTURE 1940       // maximum moisture reading value
#define MIN_MOISTURE 900   // minimum moisture reading value
#define MAX_MOISTURE 2920  // maximum moisture reading value

#define NUM_VALVES 3             // number of magnetic valves
#define NUM_FLOWMETER_SENSORS 3  // number of flowmeter sensors
#define NUM_MOISTURE_SENSORS 6   // number of soil moisture sensors

// define motor driver pins and constants
#define VALVE1 19  // digital pin controlling Valve 1
#define VALVE2 18  // digital pin controlling Valve 2
#define VALVE3 5   // digital pin controlling Valve 3

#define FLOW1 13  // digital pin for reading the Flow Meter 1
#define FLOW2 4   // digital pin for reading the Flow Meter 2
#define FLOW3 23  // digital pin for reading the Flow Meter 3

#define DEBOUNCETIME 18  // in miliseconds

// define GPIO pins
int valvePins[] = { VALVE1, VALVE2, VALVE3 };
int valveStates[] = { 0, 0, 0 };        // 0 means closed, 1 open
int valveClosingTimes[] = { 0, 0, 0 };  // 0 means no automatic closing - valve sholud be closed explicitely using CLOSE_VALVES command
int flowPins[] = { FLOW1, FLOW2, FLOW3 };
int moistureSensorPins[] = { 36, 39, 34, 35, 32, 33 };

const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;

const char *device_id = "IR_CONTROLLER_01";


IPAddress remote_ip(192, 168, 0, 17);  // backend server IP
const int remote_port = 5683;          // default port for CoAP

typedef struct
{
  int valve[NUM_VALVES];
  int flow[NUM_FLOWMETER_SENSORS];
  int moist[NUM_MOISTURE_SENSORS];
} State;

typedef struct
{
  int id;
  int params[MAX_COMMAND_PARAMS];
} Command;

// UDP and CoAP class
// other initialize is "Coap coap(Udp, 512);"
// 2nd default parameter is COAP_BUF_MAX_SIZE(defaulit:128)
// For UDP fragmentation, it is good to set the maximum under
// 1280byte when using the internet connection.
WiFiUDP udp;
Coap coap(udp, MAX_JSON_SIZE);
StaticJsonDocument<MAX_JSON_SIZE> doc;

int commandStartTime = 0;

// MOVE COMMANDS
Command commands[MAX_COMMANDS];
int commandsNumber = 0;
int currentCommand = 0;

// Encoder state
State state = { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0, 0, 0, 0 } };

char eventString[MAX_JSON_SIZE];
char tempStr[256];
String start_time;

// ISR counters
int interruptCount1 = 0;
int interruptCount2 = 0;
int interruptCount3 = 0;
uint32_t debounceTimeout1 = 0;
uint32_t debounceTimeout2 = 0;
uint32_t debounceTimeout3 = 0;
uint32_t debounceTimeoutOld1 = 0;
uint32_t debounceTimeoutOld2 = 0;
uint32_t debounceTimeoutOld3 = 0;
long flowCount1 = 0;
long flowCount2 = 0;
long flowCount3 = 0;  // Interrupt Service Routines


void IRAM_ATTR isrFlow1() {
  debounceTimeout1 = xTaskGetTickCount();  // get millis() inside ISR
  if (debounceTimeout1 - debounceTimeoutOld1 > DEBOUNCETIME) {
    interruptCount1++;
    debounceTimeoutOld1 = debounceTimeout1;
  }
}

void IRAM_ATTR isrFlow2() {
  debounceTimeout2 = xTaskGetTickCount();  // get millis() inside ISR
  if (debounceTimeout2 - debounceTimeoutOld2 > DEBOUNCETIME) {
    interruptCount2++;
    debounceTimeoutOld2 = debounceTimeout2;
  }
}

void IRAM_ATTR isrFlow3() {
  debounceTimeout3 = xTaskGetTickCount();  // get millis() inside ISR
  if (debounceTimeout3 - debounceTimeoutOld3 > DEBOUNCETIME) {
    interruptCount3++;
    debounceTimeoutOld3 = debounceTimeout3;
  }
}

void setupSensorsAndValves() {
  // setup valve GPIO pins
  for (int i = 0; i < NUM_VALVES; i++) {
    pinMode(valvePins[i], OUTPUT);
  }

  // setup flowmeter sensors
  pinMode(FLOW1, INPUT);                                            // Sets the echoPin as an INPUT
  pinMode(FLOW2, INPUT);                                            // Sets the echoPin as an INPUT
  pinMode(FLOW3, INPUT);                                            // Sets the echoPin as an INPUT                                           // Sets the echoPin as an INP
  attachInterrupt(digitalPinToInterrupt(FLOW1), isrFlow1, CHANGE);  // attach iterrupt to speed sensor SpeedL pin, detects every change high->low and low->high
  attachInterrupt(digitalPinToInterrupt(FLOW2), isrFlow2, CHANGE);  // attach iterrupt to speed sensor SpeedL pin, detects every change high->low and low->high
  attachInterrupt(digitalPinToInterrupt(FLOW3), isrFlow3, CHANGE);  // attach iterrupt to speed sensor SpeedL pin, detects every change high->low and low->high
  Serial.println("Flow meters setup successful");

  // setup flow molisture sensors
  for (int i = 0; i < NUM_MOISTURE_SENSORS; i++) {
    pinMode(moistureSensorPins[i], INPUT_PULLDOWN);  // Sets the moisture sensor pins as Input
  }
  Serial.println("Soil moisture sensors setup successful");
}

void trunOffAllValves() {
  for (int i = 0; i < NUM_VALVES; i++) {
    valveStates[i] = LOW;
    digitalWrite(valvePins[i], LOW);
  }
}

// flow meters
void resetFlowCounts() {
  flowCount1 = 0;
  flowCount2 = 0;
  flowCount3 = 0;
  interruptCount1 = 0;
  interruptCount2 = 0;
  interruptCount3 = 0;
}

void updateFlowCounts() {
  flowCount1 += interruptCount1;
  flowCount2 += interruptCount2;
  flowCount3 += interruptCount3;
  interruptCount1 = 0;
  interruptCount2 = 0;
  interruptCount3 = 0;
  // Serial.printf("An flowmeter interrupt has occurred. Total: %d, %d, %d\n", flowCount1, flowCount2, flowCount3);
}

State getState() {
  //get valve states
  memcpy(state.valve, valveStates, sizeof(valveStates));
  // get flowmeter state
  updateFlowCounts();
  state.flow[0] = flowCount1;
  state.flow[1] = flowCount2;
  state.flow[2] = flowCount3;

  // get moisture state
  for (int i = 0; i < NUM_MOISTURE_SENSORS; i++) {
    int sensorVal = analogRead(moistureSensorPins[i]);
    if (sensorVal > MIN_MOISTURE) {
      state.moist[i] = map(sensorVal, MIN_MOISTURE, MAX_MOISTURE, 100, 0);
    } else {
      state.moist[i] = -1;
    }
    // Serial.printf("%d: %4.0f%%, ", sensorVal, state.moist[i]);
  }
  // Serial.println();
  return state;
}

char *encodeState(State state) {
  sprintf(tempStr, "%d", state.moist[0]);
  for (int i = 1; i < NUM_MOISTURE_SENSORS; i++) {
    sprintf(tempStr, "%s, %d", tempStr, state.moist[i]);
  }
  sprintf(eventString, "{\"type\":\"state\", \"time\":%d, \"start_time\":%s, \"deviceId\": \"%s\", \"valves\": [%d, %d, %d], \"flows\":[%d, %d, %d], \"moists\":[%s]}", 
    millis(), start_time.c_str(), device_id, state.valve[0], state.valve[1], state.valve[2], state.flow[0], state.flow[1], state.flow[2], tempStr);

  return eventString;
}

// CoAP client response callback
void callback_response(CoapPacket &packet, IPAddress ip, int port);

void processCommand(JsonObject cmdObj, int i) {
  String cmd = cmdObj["command"];
  Serial.printf("Command[%d] -> %s\n", i, cmd);

  for (int j = 0; j < MAX_COMMAND_PARAMS; j++) {  // zero all params by default
    commands[i].params[j] = 0;
  }

  if (cmd.equals("OPEN_VALVE")) {
    commands[i].id = OPEN_VALVE;
    commands[i].params[0] = cmdObj["valve"];
    commands[i].params[1] = cmdObj["duration"];
  } else if (cmd.equals("OPEN_VALVES")) {
    commands[i].id = OPEN_VALVES;
    if (cmdObj["valves"]) {
      for (int v = 0; v < NUM_VALVES; v++) {
        commands[i].params[v] = cmdObj["valves"][v];
      }
    }
    if (cmdObj["durations"]) {  // different valves opening durations
      for (int d = 0; d < NUM_VALVES; d++) {
        commands[i].params[NUM_VALVES + d] = cmdObj["durations"][d];  // duration 0 means unlimited duration - until explicitely closed (default value)
      }
    } else {  // same opening duration for all valves
      for (int d = 0; d < NUM_VALVES; d++) {
        commands[i].params[NUM_VALVES + d] = cmdObj["duration"];  // duration 0 means unlimited duration - until explicitely closed (default value)
      }
    }
  }

  else if (cmd.equals("CLOSE_VALVE")) {
    commands[i].id = CLOSE_VALVE;
    commands[i].params[0] = cmdObj["valve"];
  }

  else if (cmd.equals("CLOSE_VALVES")) {
    commands[i].id = CLOSE_VALVES;
  }
}

// CoAP server endpoint URL
void callback_commands(CoapPacket &packet, IPAddress ip, int port) {
  // send response
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;

  char t[packet.tokenlen + 1];
  memcpy(t, packet.token, packet.tokenlen);
  t[packet.tokenlen] = NULL;

  String message(p);
  String token(t);

  Serial.println(message);
  Serial.println(token);
  Serial.println(ip);
  Serial.println(port);

  // Parse the JSON input
  DeserializationError err = deserializeJson(doc, message);
  // Parse succeeded?
  if (err) {
    Serial.print(F("deserializeJson() returned "));
    Serial.println(err.f_str());
    const char *responseStr = "Can not parse JSON content";
    coap.sendResponse(ip, port, packet.messageid, responseStr, strlen(responseStr), COAP_BAD_REQUEST, COAP_APPLICATION_JSON, reinterpret_cast<const uint8_t *>(&token[0]), packet.tokenlen);
    return;
  }

  if (doc.is<JsonArray>()) {
    // Walk the JsonArray efficiently
    int i = 0;
    for (JsonObject cmd : doc.as<JsonArray>()) {
      processCommand(cmd, i);
      i++;
      if (i >= MAX_COMMANDS) break;
    }
    commandsNumber = i;
    currentCommand = 0;
  } else {
    commandsNumber = 1;
    currentCommand = 0;
    processCommand(doc.as<JsonObject>(), 0);
  }

  const char *responseStr = reinterpret_cast<const char *>(&message[0]);
  coap.sendResponse(ip, port, packet.messageid, responseStr, strlen(responseStr), COAP_CONTENT, COAP_APPLICATION_JSON, reinterpret_cast<const uint8_t *>(&token[0]), packet.tokenlen);
}

// CoAP client response callback
void callback_response(CoapPacket &packet, IPAddress ip, int port) {
  // Serial.println("[Coap Response got]");

  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;
  String message(p);
  Serial.println(p);

  // Parse the JSON input
  DeserializationError err = deserializeJson(doc, message);
  // Parse succeeded?
  if (err) {
    Serial.print(F("deserializeJson() returned "));
    Serial.println(err.f_str());
    return;
  }
  String type_str = doc.as<JsonObject>()["type"];
  if (type_str == "time") {
    String time_str = doc.as<JsonObject>()["time"];
    start_time = time_str;
    Serial.println(start_time);
  }
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void setup() {
  Serial.begin(115200);
  setupSensorsAndValves();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  printWifiStatus();

  // LED State
  // pinMode(LED, OUTPUT);
  // digitalWrite(LED, HIGH);
  // LEDSTATE = true;

  // add server url endpoints.
  // can add multiple endpoint urls.
  // exp) coap.server(callback_switch, "switch");
  //      coap.server(callback_env, "env/temp");
  //      coap.server(callback_env, "env/humidity");
  Serial.println("Setup Callback Commands");
  coap.server(callback_commands, "commands");

  // client response callback.
  // this endpoint is single callback.
  Serial.println("Setup Response Callback");
  coap.response(callback_response);

  // start coap server/client
  coap.start();

  coap.get(remote_ip, remote_port, "time");
  coap.put(remote_ip, remote_port, "register", device_id, strlen(device_id));
}

void loop() {
  coap.loop();
  // if (commandStartTime == 0) {
  //   commandStartTime = millis();
  //   digitalWrite(valvePins[2], HIGH);
  // }

  if (commandsNumber > 0 && currentCommand < commandsNumber) {
    commandStartTime = millis();

    if (commands[currentCommand].id == OPEN_VALVE) {
      int valve = commands[currentCommand].params[0];
      int openingDuration = commands[currentCommand].params[1];
      Serial.printf("### OPENING VALVE: %d for %d ms\n", valve, openingDuration);
      valveStates[valve] = 1;
      if (openingDuration > 0) {  // set the valve autoclose time if specified
        valveClosingTimes[valve] = commandStartTime + openingDuration;
      } else {
        valveClosingTimes[valve] = 0;
      }
      digitalWrite(valvePins[valve], valveStates[valve]);
    }

    else if (commands[currentCommand].id == OPEN_VALVES) {
      Serial.print("### OPENING VALVES: [");
      for (int i = 0; i < NUM_VALVES; i++) {
        Serial.printf("%d ", commands[currentCommand].params[i]);
      }
      Serial.printf("] for durations (ms): [");
      for (int i = 0; i < NUM_VALVES; i++) {
        Serial.printf("%d ", commands[currentCommand].params[NUM_VALVES + i]);
      }
      Serial.println("]");
      for (int i = 0; i < NUM_VALVES; i++) {
        valveStates[i] = commands[currentCommand].params[i];
        if (commands[currentCommand].params[NUM_VALVES + i] > 0) {  // set the valve autoclose time if specified
          valveClosingTimes[i] = commandStartTime + commands[currentCommand].params[NUM_VALVES + i];
        } else {
          valveClosingTimes[i] = 0;
        }
        digitalWrite(valvePins[i], valveStates[i]);
      }

    } else if (commands[currentCommand].id == CLOSE_VALVE) {
      int valve = commands[currentCommand].params[0];
      Serial.printf("### CLOSING VALVE: %d\n", valve);
      valveStates[valve] = LOW;
      digitalWrite(valvePins[valve], LOW);
    }

    else if (commands[currentCommand].id == CLOSE_VALVES) {
      Serial.println("### CLOSING ALL VALVES");
      trunOffAllValves();
    }
  }

  currentCommand++;
  if (currentCommand >= commandsNumber) {
    currentCommand = 0;
    commandsNumber = 0;
  }

  int currentTime = millis();
  for (int i = 0; i < NUM_VALVES; i++) {
    if (valveStates[i] && valveClosingTimes[i] > 0 && currentTime >= valveClosingTimes[i]) {  // close valve automatically if valveClosingTime is reached
      valveStates[i] = 0;
      digitalWrite(valvePins[i], valveStates[i]);
    }
  }
  state = getState();
  encodeState(state);
  // Serial.println(eventString);
  coap.put(remote_ip, remote_port, "sensors", eventString, strlen(eventString));

  delay(1000);

  // if ((sweepDelta > 0 || sweepMeasureTime > 0) && millis() - sweepMeasureTime > sweepPause) {
  //   if (sweepCurrentAngle <= sweepEndAngle) {
  //     sweepPause = max(sweepDelta * 10, 50);
  //     float sweepDistance = readUSDistance(US_SERVO_INDEX);
  //     sweepMeasureTime = millis();
  //     if (sweepDistance > 0) {  // ignore zero distance measurements
  //       // int dist = min(max((int)(sweepDistance * 1000), 0), 100000);
  //       // int angle = map(dist, 0, 100000, 0, 180);
  //       moveServo(HORIZONTAL_SERVO_INDEX, min(sweepCurrentAngle + sweepDelta, 180));
  //       sprintf(eventString, "{\"type\":\"distance\", \"time\":%d, \"angle\":%d, \"distance\":%f}", sweepMeasureTime, sweepCurrentAngle, sweepDistance);
  //       Serial.println(eventString);
  //       coap.put(remote_ip, 5683, "sensors", eventString, strlen(eventString));
  //       sweepCurrentAngle += sweepDelta;
  //     }
  //   } else {
  //     if (sweepDelta != 0) {  // move servo to 90 deg and finish
  //       sweepDelta = 0;
  //       moveServo(HORIZONTAL_SERVO_INDEX, 90);
  //       sweepMeasureTime = millis();
  //       sweepPause = max(abs(sweepEndAngle - 90) * 20, 50);  // enough time to position servo to start angle
  //     } else {                                               // finish sweeping
  //       sweepMeasureTime = 0;                                // deactivating sweeping
  //       detachServo(HORIZONTAL_SERVO_INDEX);
  //     }
  //   }
  // }
  // state state = readUSDistance();
  // sprintf(eventString, "{\"type\":\"distance\", \"time\":%d, \"distanceL\":%f, \"distanceR\":%f}", millis(), state.reading[0], state.reading[1]);
  // Serial.println(eventString);
  // coap.put(remote_ip, 5683, "sensors/distance", eventString, strlen(eventString));
}
/*
if you change LED, req/res test with coap-client(libcoap), run following.
coap-client -m get coap://(arduino ip addr)/light
coap-client -e "1" -m put coap://(arduino ip addr)/light
coap-client -e "0" -m put coap://(arduino ip addr)/light
*/
