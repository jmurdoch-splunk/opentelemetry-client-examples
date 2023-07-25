#include <SPI.h>
#include <ArduinoJson.h>
#include <TimeLib.h>

#if defined(ARDUINO_CHALLENGER_2040_WIFI_RP2040)
#include <WiFiEspAT.h>
#else
#include <WiFi.h>
#endif

// Wireless
#define SSID "myap"
#define PASS "mypsk"

// Incoming voltage to calculate watts
#define VOLTAGE 230

// Sensor details
#define HA_0_TYPE SENSOR
#define HA_0_ID "efergy"
#define HA_0_UID "0"
#define HA_0_DEV "power"
#define HA_0_UNIT "W"
#define HA_0_NAME "Live Power Consumption"
#define HA_0_STATS "measurement"

// Arduino pins
#define RFM01S_DATA D12
#define RFM01S_NSEL D13

// RFM Definitions
#define RFM_CONFIG      0x897D // 433, batt/wake off, 12pF, 67Khz b/w, CLK off
#define RFM_FREQUENCY   0xA586 // 433.535MHz (efergy specific)
#define RFM_RECEIVER    0xC049 // Rx:Enable, RSSI:-79dB
#define RFM_LOW_DUTY    0xCC0E // DEFAULT (OFF)
#define RFM_BATT_CLOCK  0xC200 // DEFAULT (OFF)
#define RFM_AFC_MODE    0xC627 // +7/-8 range, hi-acc, output reg, on
#define RFM_DATA_FILTER 0xC46C // Fast lock, Digital filter (FSK, not OOK)
#define RFM_DATA_RATE   0xC822 // 10142bps 
#define RFM_OUTPUT_FIFO 0xCE84 // DEFAULT but with FIFO disabled
#define RFM_STATUS      0x0000
#define RFM_RESET       0xFF00

// Microsecond definitions
#define PREAMBLE_LOW_MAX 1950
#define PREAMBLE_LOW_MIN 1875
#define PREAMBLE_HIGH_MAX 525
#define PREAMBLE_HIGH_MIN 475
#define PULSE_MAX 175
#define PULSE_MID 100
#define PULSE_MIN 10
#define WAITTIME 10 // us between bit polls

// Millisecond definitions
#define WAKETIME 500 // ms between expected data packets

#define BITCOUNT 64

// Processing states
enum { WAITING, PREAMBLE, PAYLOAD };
uint8_t phase = WAITING;

// Variables that capture states
uint8_t bits, thisValue, lastValue = 0;
uint32_t gap, thisTime, lastTime = 0;

// Holds the data received
struct packet { 
  // Align a raw stream of bytes with a structured data type
  union {
    // Push bits into this - easy for checksums
    uint8_t raw[8];
    // Read data from this
    struct {
      // Static
      uint8_t hdr; // Byte 0
      uint16_t uid; // Byte 1+2

      // Byte 3 - Reverse order as RP2040 is little-endian
      unsigned padding:4; // 0x0F
      unsigned ival:2;    // 0x30
      unsigned batt:1;    // 0x40
      unsigned learn:1;   // 0x80

      // Ampere readings
      uint8_t amp_msb; // Byte 4 
      uint8_t amp_lsb; // Byte 5
      uint8_t amp_exp; // Byte 6

      // Checksum
      uint8_t crc;     // Byte 7
    }__attribute__((packed));
  };

};
struct packet pkt;

const char* ssid = SSID;
const char* pass = PASS;

// Otel details
const char* otelhost = "192.168.1.217";
const uint16_t otelport = 4318;

enum { BINARY, SENSOR };

// An SPI command is two bytes long
uint16_t spi_command(uint16_t input) {
  uint16_t status;

  digitalWrite(RFM01S_NSEL, LOW);
  status = SPI.transfer16(input);
  digitalWrite(RFM01S_NSEL, HIGH);

  return status;
}

void rfm_init(void) {
  // Setup pins
  pinMode(RFM01S_DATA, INPUT);
  pinMode(RFM01S_NSEL, OUTPUT);
  digitalWrite(RFM01S_NSEL, HIGH);
  
  // SPI Setup with RFM01S
#if defined(ARDUINO_CHALLENGER_2040_WIFI_RP2040)
  SPI.setRX(PIN_WIRE0_SDA); // Challenger 2040 WiFi fix
#endif
  SPI.begin();
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));

  // Bounce the radio
  spi_command(RFM_RESET);
  delay(1000);
  
  // Setup sequence
  spi_command(RFM_CONFIG);
  spi_command(RFM_FREQUENCY); 
  spi_command(RFM_DATA_RATE); 
  spi_command(RFM_AFC_MODE);
  spi_command(RFM_DATA_FILTER);
  spi_command(RFM_OUTPUT_FIFO);
  spi_command(RFM_RECEIVER);
  
  Serial.print(F("Radio status should be 0x400: 0x"));
  Serial.println(spi_command(RFM_STATUS) & 0x400, HEX);
}

void otelPayload(bool type, String endpoint, String uid, String dev, String unit, String name, String stats, int state) {
  // We can't use HTTPClient as it might clash with other WiFi libraries
  WiFiClient client;

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
    }
  }

  String url = "/v1/metrics";
  DynamicJsonDocument payload(1024), metrics(1024);

  payload["resourceMetrics"][0]["resource"]["attributes"][0]["key"] = "service.name";
  payload["resourceMetrics"][0]["resource"]["attributes"][0]["value"]["stringValue"] = "myhome";
 
  metrics["name"] = dev + "" + uid;
  metrics["description"] = name + " " + uid;
  metrics["unit"] = unit;
  metrics["gauge"]["dataPoints"][0]["attributes"][0]["key"] = "uid";
  metrics["gauge"]["dataPoints"][0]["attributes"][0]["value"]["intValue"] = uid;
  metrics["gauge"]["dataPoints"][0]["startTimeUnixNano"] = now() * 1000000000;
  metrics["gauge"]["dataPoints"][0]["timeUnixNano"] = now() * 1000000000;  
  metrics["gauge"]["dataPoints"][0]["asInt"] = state;

  payload["resourceMetrics"][0]["scopeMetrics"][0]["metrics"][0] = metrics;
  String data;
  serializeJson(payload, data);
  Serial.println(data);
  
  // HTTP POST
  String httpreq = F("POST ");
  httpreq += url;
  httpreq += F(" HTTP/1.1\r\nHost: ");
  httpreq += otelhost;
  httpreq += ":";
  httpreq += String(otelport);
  httpreq += F("\r\nContent-Type: application/json");
  httpreq += F("\r\nContent-Length: ");
  httpreq += String(data.length());
  httpreq += F("\r\n\r\n");
  httpreq += data;
   
  // Try to connect
  if (!client.connect(otelhost, otelport)) {
    Serial.print(F("connection failed to "));
    Serial.println(String(otelhost));
    delay(5000);
    return;
  }
  
  // Try to send the data
  if (client.connected()) {
    client.println(httpreq);
    // Serial.println(F("POST Success"));
  } else {
    Serial.println(F("POST Failed"));
  }
  
  // Clean up and close out the connection
  client.flush();
  client.stop();
}

void processPacket(void) {   
  uint8_t crc = 0;

  // Calculate the checksum
  for (uint8_t i=0;i<7; i++)
    crc += pkt.raw[i];

  // If the header starts 0000 and the checksum is good
  if (((pkt.hdr & 0xf0) == 0x00) && (crc == pkt.crc)) {
    // Interval is either 0=10 secs, 1=15 secs or 2=20 secs
    uint8_t ival = (pkt.ival + 2 ) * 5;
    
    // amps / current = value-bytes * exp-byte^2 * 32768
    float watts = (float)(pkt.amp_msb << 8 | pkt.amp_lsb) * (1 << pkt.amp_exp) / (1 << 15) * VOLTAGE;    

    otelPayload(HA_0_TYPE, HA_0_ID, String(pkt.uid, HEX), HA_0_DEV, HA_0_UNIT, HA_0_NAME, HA_0_STATS, (int)(watts*10));
    
    // Go to sleep until the next packet is expected
    digitalWrite(RFM01S_NSEL, HIGH);
    delay((ival * 1000) - WAKETIME);
    digitalWrite(RFM01S_NSEL, LOW);
  } else {
    Serial.println(F("CRC error"));
  }
}

void configWireless() {
  //Special Startup for the Challenger 2040
#if defined(ARDUINO_CHALLENGER_2040_WIFI_RP2040)  
  Serial2.begin(115200);
  WiFi.init(Serial2, PIN_ESP_RST);
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("WiFiEspAT: No module");
    while (true);
  }
  WiFi.disconnect();
  WiFi.setPersistent();
  WiFi.endAP();
#endif

  // Standard WiFi Setup
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.println(F("WiFi Connected"));

  WiFi.sntp("0.uk.pool.ntp.org");
  while (WiFi.getTime() < 86400) {
    delay(500);
  }
  setTime(WiFi.getTime());
}

void setup() {
  // cu -s 115200 -l /dev/ttyACM0
  Serial.begin(115200);
  delay(5000);
  
  rfm_init();
  configWireless();
}

void loop() {
  thisValue = digitalRead(RFM01S_DATA);

  // If there's a change on DATA
  if (thisValue != lastValue) {
    thisTime = micros();
    gap = thisTime - lastTime;
    lastValue = thisValue;
    lastTime = thisTime;

    // Based on the phase, what do we do...
    switch (phase) {
      // Preamble pt1 - sustained low then rising edge
      case WAITING:
        if (thisValue && (gap > PREAMBLE_LOW_MIN) && (gap < PREAMBLE_LOW_MAX))
          phase = PREAMBLE;
        break;
      
      // Preamble pt2 - sustained high then falling edge    
      case PREAMBLE:
        if (!thisValue && (gap > PREAMBLE_HIGH_MIN) && (gap < PREAMBLE_HIGH_MAX)) {
          // We are good to process incoming data
          phase = PAYLOAD;
          bits = 0;
        } else {
          // False alarm, go back to listening
          phase = WAITING;
        }
        break;
        
      case PAYLOAD:
        // Data collection phase  
        if((gap > PULSE_MIN) && (gap < PULSE_MAX)) {
          // Only look for a rising edge only
          if(thisValue) {
            if ((gap > PULSE_MID)) 
              pkt.raw[bits/8] = (pkt.raw[bits/8] << 1);             
            else 
              pkt.raw[bits/8] = (pkt.raw[bits/8] << 1) + 1;
            bits++;
          }
        } else {
          // If we have enough to work with
          if (bits >= BITCOUNT)
            processPacket();
          phase = WAITING;
        }
        break;      
    }

    delayMicroseconds(WAITTIME);
  }
}
