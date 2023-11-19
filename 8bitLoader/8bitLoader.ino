
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "WiFiCreds.h" // Defines SSID and PASSWORD

#define API = "http://192.168.86.23:8102/run"

#define HALT 13
#define PROG 12
#define RESET 14

const int MEMORY_ADDRESS[4] {27, 26, 25, 33};

#define MEMORY_WRITE 32
#define MEMORY_WRITE_MODE HIGH

#define SHIFT_CLK 15    // PIN 11 SRCLK
#define SHIFT_LATCH 2  // PIN 12 RCLK
#define SHIFT_DATA 0   // PIN 14

#define NOP 0b00000000
#define LDA 0b00010000
#define ADD 0b00100000
#define SUB 0b00110000
#define STA 0b01000000
#define LDI 0b01010000
#define JMP 0b01100000
#define JC  0b01110000
#define JZ  0b10000000
#define OUT 0b11100000
#define HLT 0b11110000

DynamicJsonDocument doc(1024);

void setup() {
  Serial.begin(115200);

  // SN74HC595N shift register pins
  pinMode(SHIFT_DATA, OUTPUT);
  pinMode(SHIFT_CLK, OUTPUT);
  pinMode(SHIFT_LATCH, OUTPUT);
 
  // Memory address select
  for (int i = 0; i < 4; i++) {
     digitalWrite(MEMORY_ADDRESS[i], LOW);
     pinMode(MEMORY_ADDRESS[i], OUTPUT);
  }
 
  digitalWrite(HALT, LOW);
  pinMode(HALT, OUTPUT);

  digitalWrite(PROG, LOW);
  pinMode(PROG, OUTPUT);

  digitalWrite(RESET, LOW);
  pinMode(RESET, OUTPUT);

  digitalWrite(MEMORY_WRITE, !MEMORY_WRITE_MODE);
  pinMode(MEMORY_WRITE, OUTPUT);
  
  WiFi.begin(SSID, PASSWORD);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Add by 3 program
  halt();
  setMemoryValue(0, LDI + 0); 
  setMemoryValue(1, ADD + 14); 
  setMemoryValue(2, OUT);
  setMemoryValue(3, JMP + 1);
  setMemoryValue(14, 3);
  run();

  delay(10000);

}

void loop() {
  
  String json;
 
  json = httpGETRequest(API);
  Serial.println(json);
  DeserializationError err = deserializeJson(doc, json);
  JsonObject obj = doc.as<JsonObject>();

  if (json != "" && !err) {
    Serial.println(json);

    // Loading a new program, first halt and put in program mode
    halt();
    
    // Set clock speed
    int clockSpeed = obj["ClockSpeed"];
    int clockTime = obj["Time"];
    const char* programName = obj["Name"];
    if (clockTime == 0) {
      clockTime = 30;
    }

    JsonArray arr1 = doc["Instructions"].as<JsonArray>();
    for (int i = 0; i < arr1.size(); i++) {
      JsonObject instruction = arr1[i];
      const char* instructionName = instruction["Name"];
      String in = String(instructionName); 
      int argument = instruction["Argument"];
      setMemoryValue(i, getInstruction(in) + argument);
    }

    JsonArray arr2 = doc["Memory"].as<JsonArray>();
    for (int i = 0; i < arr2.size(); i++) {
      JsonObject memory = arr2[i];
      int address  = memory["Address"];
      int value = memory["Value"];
      setMemoryValue(address, value);
    }
    
    run();

    Serial.print("Running: ");
    Serial.println(programName);
    for(int i = 1; i <= clockTime; i++) {
      delay(1000);
      if (i % 60 == 0) {
        Serial.println(".");
      } else {
        Serial.print(".");
      }
    }
  } else {
    // No program in the queue
    delay(10000);
    Serial.println("delay 10 seconds");
  }
}

void halt() {
  // Halt the computer and put into programming mode
  digitalWrite(HALT, HIGH);
  delay(50);
  digitalWrite(PROG, HIGH);
  delay(200);
  digitalWrite(RESET, HIGH);
  delay(50);
  digitalWrite(RESET, LOW);
}

void run() {
    // Run
    digitalWrite(RESET, HIGH);
    delay(50);
    digitalWrite(RESET, LOW);
    delay(50);    
    digitalWrite(PROG, LOW);
    delay(50);
    digitalWrite(HALT, LOW);
}

// Return instruction from short name
int getInstruction(String name) {
  if (name == "NOP") {
    return NOP;
  }
  if (name == "LDA") {
    return LDA;
  }
  if (name == "ADD") {
    return ADD;
  }
  if (name == "SUB") {
    return SUB;
  }
  if (name == "STA") {
    return STA;
  }
  if (name == "LDI") {
    return LDI;
  }
  if (name == "JMP") {
    return JMP;
  }
    if (name == "JC") {
    return JC;
  }
    if (name == "JZ") {
    return JZ;
  }
  if (name == "OUT") {
    return OUT;
  }  
  if (name == "HLT") {
    return HLT;
  }        

  return 0;
}

/*
 * Output the memory value to the shift register
 */
void setMemoryValue(int address, int value) {
  Serial.println("Writing value:");
  Serial.println(value);

  shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, value);

  digitalWrite(SHIFT_LATCH, LOW);
  digitalWrite(SHIFT_LATCH, HIGH);
  digitalWrite(SHIFT_LATCH, LOW);

  for (int i = 0; i < 4; i++) {
     digitalWrite(MEMORY_ADDRESS[i], address & 1);
     address = address >> 1;
  }
  
  delay(200);
  digitalWrite(MEMORY_WRITE, MEMORY_WRITE_MODE);
  delay(50);
  digitalWrite(MEMORY_WRITE, !MEMORY_WRITE_MODE);
  delay(200);

  // Set all address lines low
  for (int i = 0; i < 4; i++) {
     digitalWrite(MEMORY_ADDRESS[i], LOW);
  }

  shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, 0);
}

String httpGETRequest(const char* serverName) {
  HTTPClient http;

  // Your IP address with path or Domain name with URL path 
  http.begin(serverName);

  // If you need Node-RED/server authentication, insert user and password below
  //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");

  // Send HTTP POST request
  int httpResponseCode = http.GET();

  String payload = ""; 

  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}





