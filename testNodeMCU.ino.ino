#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include <SD.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <RTClib.h>

#define SS_LORA 15   //GPIO 15 - D8
#define RST_LORA 16  //GPIO 16 - D0
#define DIO0_LORA 4  //GPIO 4 - D2
#define SS_SD 2      //GPIO 2 - D4

const char* ssid = "HadjiSaidWiFi";       //HadjiSaidWiFi
const char* password = "@Ljavier262013";  //@Ljavier262013

const char* host = "script.google.com";
const int httpsPort = 443;
WiFiClientSecure client;

String GAS_ID = "AKfycbz_f1YJHcQWdpvxmc-qIp9BCepATLL6VcormfqQQj40YEupITX5KLEu-SN--lVP0Qo7";  // GSheet Script Deployment ID

byte MasterNode = 0xFF;
byte Node1 = 0xBB;
byte Node2 = 0xCC;
byte Node3 = 0xDD;
String SenderNode = "";
String outgoing;    // outgoing message
byte msgCount = 0;  // count of outgoing messages
String incoming = "";

unsigned long previousMillis = 0;
unsigned long int previoussecs = 0;
unsigned long int currentsecs = 0;
unsigned long currentMillis = 0;
int lastSend = 0;  // updated every 10minutes (600 seconds)
int Secs = 0;

float pv_volts;
float pv_power;
float batt_volts;
float batt_level;
float batt_power;
float led_status;
float temperature;
float light;

int rN1 = 0;
int rN2 = 0;
int rN3 = 0;
String NodeSheet = "";

File dataFile;
RTC_DS3231 rtc;

void setup() {
  Serial.begin(115200);
  SPI.begin();
  Wire.begin();  // Start the I2C
  delay(10);
  while (!Serial)
    ;

  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  initWIFI();
  initRTC();
  initLORA();
  initSD();
}

void loop() {
  unsigned long currentMillis = millis();
  currentsecs = currentMillis / 1000;

  if ((currentsecs - previoussecs) >= 3) {
    Secs = Secs + 1;
    if (Secs >= 10) {
      Secs = 0;
      if ((currentsecs - lastSend) >= 120) {
        rN1 = 0;
        rN2 = 0;
        rN3 = 0;
        lastSend = currentsecs;
        Serial.print("lastSend: ");
        Serial.println(lastSend);
      }
    }
    if ((Secs >= 1) && (Secs <= 3)) {
      if (rN1 == 0) {
        sendMessage("10", MasterNode, Node1);
        Serial.println("Send to Node 1");
      } else {
        Serial.println("NO SEND to Node 1");
      }
    }
    if ((Secs >= 4) && (Secs <= 6)) {
      if (rN2 == 0) {
        sendMessage("20", MasterNode, Node2);
        Serial.println("Send to Node 2");
      } else {
        Serial.println("NO SEND to Node 2");
      }
    }
    if ((Secs >= 7) && (Secs <= 9)) {
      if (rN2 == 0) {
        sendMessage("30", MasterNode, Node2);
        Serial.println("Send to Node 3");
      } else {
        Serial.println("NO SEND to Node 3");
      }
    }
    previoussecs = currentsecs;
  }
  onReceive(LoRa.parsePacket());

  for (int i = 1; i <= 3; i++) {
    String FileName = "SL" + String(i) + ".txt";
    if (SD.exists(FileName)) {
      if (WiFi.status() == WL_CONNECTED) {
        readSDCard(FileName, "SL" + String(i));
      }
    }
  }
  // initRTC();
  // delay(1000);
}

void initWIFI() {
  // Connect to WiFi network
  // Serial.println();
  // Serial.println();
  // Serial.print("Connecting to ");
  // Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    // Serial.println("WiFi not connected");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Print the IP address
  Serial.print("Use this URL to connect: ");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");

  client.setInsecure();
}

void initRTC() {
  if (!rtc.begin()) {
    Serial.println("Starting RTC Failed");
    return;
  }
  Serial.println("RTC working.");

  DateTime now = rtc.now();
  Serial.println(now.timestamp());
}

void initLORA() {
  LoRa.setPins(SS_LORA, RST_LORA, DIO0_LORA);
  if (!LoRa.begin(433E6)) {
    Serial.println("LORA init Failed!");
    return;
  }
  Serial.println("LORA working.");
}

void initSD() {
  if (!SD.begin(SS_SD)) {
    Serial.println("Starting SD Card Failed!");
    return;
  }
  Serial.println("SD CARD working.");

  if (SD.exists("data_log.txt"))
    Serial.println("data_log.txt exists.");
  else {
    Serial.println("data_log.txt doesn't exist.");
    /* open a new file and immediately close it:
      this will create a new file */
    Serial.println("Creating data_log.txt...");
    dataFile = SD.open("data_log.txt", FILE_WRITE);
    dataFile.close();
    /* Now Check again if the file exists in
      the SD card or not */
    if (SD.exists("data_log.txt"))
      Serial.println("data_log.txt exists.");
    else
      Serial.println("data_log.txt doesn't exist.");
  }
}

void sendMessage(String outgoing, byte MasterNode, byte otherNode) {
  LoRa.beginPacket();             // start packet
  LoRa.write(otherNode);          // add destination address
  LoRa.write(MasterNode);         // add sender address
  LoRa.write(msgCount);           // add message ID
  LoRa.write(outgoing.length());  // add payload length
  LoRa.print(outgoing);           // add payload
  LoRa.endPacket();               // finish packet and send it
  msgCount++;                     // increment message ID
}

void onReceive(int packetSize) {
  if (packetSize == 0) return;  // if there's no packet, return
  // read packet header bytes:
  int recipient = LoRa.read();  // recipient address
  byte sender = LoRa.read();    // sender address

  byte incomingMsgId = LoRa.read();   // incoming msg ID
  byte incomingLength = LoRa.read();  // incoming msg length
  while (LoRa.available()) {
    incoming += (char)LoRa.read();
  }
  if (incomingLength != incoming.length()) {  // check length for error
    Serial.println("error: message length does not match length");
    return;  // skip rest of function
  }
  // if the recipient isn't this device or broadcast,
  if (recipient != Node1 && recipient != MasterNode) {
    Serial.println("This message is not for me.");
    return;  // skip rest of function
  }
  Serial.println("RSSI: " + String(LoRa.packetRssi()));
  Serial.println("Snr: " + String(LoRa.packetSnr()));

  if (sender == 0XBB) {
    rN1 = 1;
    NodeSheet = "SL1";
    String pvv = getValue(incoming, ',', 0);
    String pvp = getValue(incoming, ',', 1);
    String bv = getValue(incoming, ',', 2);
    String bl = getValue(incoming, ',', 3);
    String bp = getValue(incoming, ',', 4);
    String ls = getValue(incoming, ',', 5);
    String t = getValue(incoming, ',', 6);
    String l = getValue(incoming, ',', 7);

    pv_volts = pvv.toFloat();
    pv_power = pvp.toFloat();
    batt_volts = bv.toFloat();
    batt_level = bl.toFloat();
    batt_power = bp.toFloat();
    led_status = ls.toFloat();
    temperature = t.toFloat();
    light = l.toFloat();
    incoming = "";
  }
  if (sender == 0XCC) {
    rN2 = 1;
    NodeSheet = "SL2";
    String pvv = getValue(incoming, ',', 0);
    String pvp = getValue(incoming, ',', 1);
    String bv = getValue(incoming, ',', 2);
    String bl = getValue(incoming, ',', 3);
    String bp = getValue(incoming, ',', 4);
    String ls = getValue(incoming, ',', 5);
    String t = getValue(incoming, ',', 6);
    String l = getValue(incoming, ',', 7);

    pv_volts = pvv.toFloat();
    pv_power = pvp.toFloat();
    batt_volts = bv.toFloat();
    batt_level = bl.toFloat();
    batt_power = bp.toFloat();
    led_status = ls.toFloat();
    temperature = t.toFloat();
    light = l.toFloat();
    incoming = "";
  }
  if (sender == 0XDD) {
    rN3 = 1;
    NodeSheet = "SL3";
    String pvv = getValue(incoming, ',', 0);
    String pvp = getValue(incoming, ',', 1);
    String bv = getValue(incoming, ',', 2);
    String bl = getValue(incoming, ',', 3);
    String bp = getValue(incoming, ',', 4);
    String ls = getValue(incoming, ',', 5);
    String t = getValue(incoming, ',', 6);
    String l = getValue(incoming, ',', 7);

    pv_volts = pvv.toFloat();
    pv_power = pvp.toFloat();
    batt_volts = bv.toFloat();
    batt_level = bl.toFloat();
    batt_power = bp.toFloat();
    led_status = ls.toFloat();
    temperature = t.toFloat();
    light = l.toFloat();
    incoming = "";
  }

  Serial.print(NodeSheet);

  Serial.print(" | (pV) = ");
  Serial.print(pv_volts);

  Serial.print(" | (pW) : ");
  Serial.print(pv_power);

  Serial.print(" | (bV) = ");
  Serial.print(batt_volts);

  Serial.print(" | (bP) : ");
  Serial.print(batt_power);

  Serial.print(" | (Level) : ");
  Serial.print(batt_level);

  Serial.print(" | (Status) : ");
  Serial.print(led_status);

  Serial.print(" | (lux): ");
  Serial.print(light);

  Serial.print(" | (*C): ");
  Serial.println(temperature);

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected");
    saveToSD(NodeSheet, pv_volts, pv_power, batt_volts, batt_level, batt_power, led_status, light, temperature, LoRa.packetSnr(), LoRa.packetRssi());
  } else {
    saveToCloud(NodeSheet, pv_volts, pv_power, batt_volts, batt_level, batt_power, led_status, light, temperature, LoRa.packetSnr(), LoRa.packetRssi(), "0");
  }
}

String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;
  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void saveToSD(String node, float pvv, float pvp, float bv, float bl, float bp, float ls, float l, float t, float snr, float rssi) {
  String FileName = node + ".txt";

  if (SD.exists(FileName)) {
    Serial.print(FileName);
    Serial.println(" exists.");
    dataFile = SD.open(FileName, FILE_WRITE);  // open the file for writing
    Serial.print(FileName);
    Serial.println(" opened successfully.");
  } else {
    Serial.print(FileName);
    Serial.println(" doesn't exists.");
    dataFile = SD.open(FileName, FILE_WRITE);  // create a new file for writing
    Serial.print(FileName);
    Serial.println(" created successfully.");
  }

  if (dataFile) {
    DateTime now = rtc.now();

    dataFile.println(now.timestamp() + " " + pvv + " " + pvp + " " + bv + " " + bl + " " + bp + " " + ls + " " + l + " " + t + " " + snr + " " + rssi);  // write some data to the file
    dataFile.close();                                                                                              // close the file
    Serial.println("Successfully insert data.");
  } else {
    Serial.println("File opening/creation failed!");
  }
}

void readSDCard(String FileName, String node) {
  // String FileName = "SL" + node + ".txt";
  // dataFile = SD.open(FileName, FILE_READ);

  if (SD.exists(FileName)) {
    Serial.print(FileName);
    Serial.println(" exist.");
    dataFile = SD.open(FileName, FILE_READ);

    if (dataFile) {
      while (dataFile.available()) {
        String line = dataFile.readStringUntil('\n');
        // Serial.println(line);
        String words[11];  // create an array to store up to 11 words
        int numWords = 0;
        String word = "";
        for (int i = 0; i < line.length(); i++) {
          char c = line.charAt(i);
          if (c == ' ' || i == line.length() - 1) {
            if (i == line.length() - 1) {
              word += c;
            }
            words[numWords] = word;
            numWords++;
            word = "";
          } else {
            word += c;
          }
        }
        saveToCloud(node, words[1].toFloat(), words[2].toFloat(), words[3].toFloat(), words[4].toFloat(), words[5].toFloat(), words[6].toFloat(), words[7].toFloat(), words[8].toFloat(), words[9].toFloat(), words[10].toFloat(), words[0]);
        delay(1000);
      }
      dataFile.close();     // close the file
      SD.remove(FileName);  // delete the file if existed
      Serial.println(FileName);
      Serial.println(" deleted.");
    } else {
      Serial.println("error opening File");
    }
  } else {
    return;
  }
}

void saveToCloud(String node, float pvv, float pvp, float bv, float bl, float bp, float ls, float l, float t, float snr, float rssi, String dt) {
  Serial.println("==========");
  Serial.print("connecting to ");
  Serial.println(host);

  //----------------------------------------Connect to Google host
  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed");
    return;
  }

  String url = "/macros/s/" + GAS_ID + "/exec?node=" + node + "&pvV=" + pvv + "&pvP=" + pvp + "&battV=" + bv + "&battLevel=" + bl + "&battP=" + bp + "&ledStatus=" + ls+ "&snr=" + snr + "&rssi=" + rssi + "&temp=" + t + "&lux=" + l + "&datetime=" + dt;
  Serial.print("requesting URL: ");
  Serial.println(url);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "User-Agent: BuildFailureDetectorESP8266\r\n" + "Connection: close\r\n\r\n");

  Serial.println("request sent");
  //----------------------------------------

  //----------------------------------------Checking whether the data was sent successfully or not
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');
  if (line.startsWith("{\"state\":\"success\"")) {
    Serial.println("esp8266/Arduino CI successfull!");
  } else {
    Serial.println("esp8266/Arduino CI has failed");
  }
  Serial.print("reply was : ");
  Serial.println(line);
  Serial.println("closing connection");
  Serial.println("==========");
  Serial.println();
  //----------------------------------------
}
