#include "dsmr.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "config.h"

//Define the data we're interested in.
//This list shows all supported fields, remove any fields you are not using from the below list to make the parsing and printing code smaller.
using MyData = ParsedData <
               /* String */ identification,
               /* String */ //p1_version,
               /* String */ timestamp,
               /* String */ equipment_id,
               /* FixedValue */ energy_delivered_tariff1,
               /* FixedValue */ energy_delivered_tariff2,
               /* FixedValue */ energy_returned_tariff1,
               /* FixedValue */ energy_returned_tariff2,
               /* String */ //electricity_tariff,
               /* FixedValue */ power_delivered,
               /* FixedValue */ power_returned,
               /* FixedValue */ //electricity_threshold,
               /* uint8_t */ //electricity_switch_position,
               /* uint32_t */ //electricity_failures,
               /* uint32_t */ //electricity_long_failures,
               /* String */ //electricity_failure_log,
               /* uint32_t */ //electricity_sags_l1,
               /* uint32_t */ //electricity_sags_l2,
               /* uint32_t */ //electricity_sags_l3,
               /* uint32_t */ //electricity_swells_l1,
               /* uint32_t */ //electricity_swells_l2,
               /* uint32_t */ //electricity_swells_l3,
               /* String */ //message_short,
               /* String */ //message_long,
               /* FixedValue */ //voltage_l1,
               /* FixedValue */ //voltage_l2,
               /* FixedValue */ //voltage_l3,
               /* FixedValue */ //current_l1,
               /* FixedValue */ //current_l2,
               /* FixedValue */ //current_l3,
               /* FixedValue */ //power_delivered_l1,
               /* FixedValue */ //power_delivered_l2,
               /* FixedValue */ //power_delivered_l3,
               /* FixedValue */ //power_returned_l1,
               /* FixedValue */ //power_returned_l2,
               /* FixedValue */ //power_returned_l3,
               /* uint16_t */ //gas_device_type,
               /* String */ gas_equipment_id,
               /* uint8_t */ //gas_valve_position,
               /* TimestampedFixedValue */ gas_delivered
               /* uint16_t */ //thermal_device_type,
               /* String */ //thermal_equipment_id,
               /* uint8_t */ //thermal_valve_position,
               /* TimestampedFixedValue */ //thermal_delivered,
               /* uint16_t */ //water_device_type,
               /* String */ //water_equipment_id,
               /* uint8_t */ //water_valve_position,
               /* TimestampedFixedValue */ //water_delivered,
               /* uint16_t */ //slave_device_type,
               /* String */ //slave_equipment_id,
               /* uint8_t */ //slave_valve_position,
               /* TimestampedFixedValue */ //slave_delivered
               >;


//print received DSMR fields
#ifdef ESP32_DSMR_DEBUG
struct Printer {
  template<typename Item>
  void apply(Item &i) {
    if (i.present()) {
      Serial.print(Item::name);
      Serial.print(F(": "));
      Serial.print(i.val());
      Serial.print(Item::unit());
      Serial.println();
    }
  }
};
#endif

//esp32 hardware serial2
//RX = A16
//TX = A17
//request pin D2 or connect to 5V

P1Reader reader(&Serial2, 2);

unsigned long last;

void connectWifi() {
  WiFi.mode(WIFI_STA);

  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int connectCounter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    if (connectCounter > 30) {
      ESP.restart(); //restart after few seconds and try again
    }
    connectCounter++;
  }

  Serial.println("connected");
  Serial.println(WiFi.localIP());
}


void setup() {
  Serial.begin(115200);
  Serial2.begin(115200);
  btStop();
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  connectWifi();

  // start a read right away
  reader.enable(true);
  last = millis();

  Serial.println("DSMR started");
  digitalWrite(LED_PIN, HIGH);
}

void loop () {
  // Allow the reader to check the serial buffer regularly
  reader.loop();

  // Every 10 sec, fire off a one-off reading
  unsigned long now = millis();
  if (now - last > 10000) {
    reader.enable(true);
    last = now;
  }

  if (reader.available()) {
    MyData data;
    String err;
    if (reader.parse(&data, &err)) {
#ifdef ESP32_DSMR_DEBUG
      // Parse succesful, print result
      data.applyEach(Printer());
#endif
      DynamicJsonDocument  message(300);
      message["identification"] = data.identification;
      message["timestamp"] = data.timestamp;
      message["equipment_id"] = data.equipment_id;
      message["energy_delivered_tariff1"] = (float)data.energy_delivered_tariff1;
      message["energy_delivered_tariff2"] = (float)data.energy_delivered_tariff2;
      message["energy_returned_tariff1"] = (float)data.energy_returned_tariff1;
      message["energy_returned_tariff2"] = (float)data.energy_returned_tariff2;

      message["power_delivered"] = (float)data.power_delivered;
      message["power_returned"] = (float)data.power_returned;

      //message["power_delivered_l1"] = (float)data.power_delivered_l1;
      //message["power_returned_l1"] = (float)data.power_returned_l1;

      message["gas_equipment_id"] = data.gas_equipment_id;
      message["gas_delivered"] = (float)data.gas_delivered;

      if (WiFi.status() == WL_CONNECTED) {
        digitalWrite(LED_PIN, LOW);
        HTTPClient http;
        http.begin(NODERED_SERVER);
        http.addHeader("Content-Type", "application/json");

        String output;
        serializeJson(message, output);
        http.POST(output);
        digitalWrite(LED_PIN, HIGH);
      } else {
        Serial.println("WiFi not connected!");
        connectWifi();
      }
    } else {
      // Parser error, print error
      Serial.println(err);
    }
  }
}
