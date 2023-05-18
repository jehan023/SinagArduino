#include <SPI.h>
#include <LoRa.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <BH1750.h>
#include <Wire.h>
/*Module SX1278 // Arduino UNO/NANO    
    GND         ->   GND
    Vcc         ->   3.3V
    MISO        ->   D12
    MOSI        ->   D11     
    SLCK        ->   D13
    Nss         ->   D10
    Di00        ->   D2
    RST         ->   D9       
 */
// Define analog input
#define LED_LDRpin A0
#define PV_VoltagePin A7
#define PV_CurrentPin A2
#define BAT_VoltagePin A1
#define BAT_CurrentPin A6
#define ONE_WIRE_BUS 3

#define ss 10
#define rst 9
#define dio0 2
String outgoing;    // outgoing message
byte msgCount = 0;  // count of outgoing messages
byte MasterNode = 0xFF;
byte Node3 = 0xDD;  // SL3

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature temp(&oneWire);

// Define the sensitivity of the current sensor (mV/A)
float sensitivity = 0.100;  // 100mV/A for ACS712 20A

// Floats for ADC voltage & Input voltage
float pv_avg_volts = 0.0;
float pv_avg_amps = 0.0;
float pv_power = 0.0;
float bat_avg_volts = 0.0;
float bat_avg_amps = 0.0;
float bat_power = 0.0;
float avg_temp = 0.0;
float ldrValue = 0.0;
uint16_t avg_lux;
float batt_level = 0.0;
float led_status = 0.0;
float charging = 0.0;

// Floats for resistor values in divider (in ohms)
float R1 = 30000.0;
float R2 = 7500.0;

// Float for Reference Voltage
float ref_voltage = 5.0;

// Threshold for LDR to detect LAMP STATUS
float ldr_threshold = 200.0;

BH1750 lightMeter;
String Mymessage = "";

void setup() {
  Serial.begin(9600);
  Wire.begin();
  lightMeter.begin();
  lightMeter.configure(BH1750::CONTINUOUS_HIGH_RES_MODE);
  temp.begin();
  Serial.println("DC Sensors Test");

  while (!Serial)
    ;
  Serial.println("LoRa Node3 [SL3]");
  LoRa.setPins(ss, rst, dio0);
  if (!LoRa.begin(433E6)) {  // or 915E6, the MHz speed of yout module
    Serial.println("LORA init failed!");
    while (1)
      ;
  }
  Serial.println("LoRa working.");
}

void loop() {
  onReceive(LoRa.parsePacket());

  // ReadSensors();
  // delay(1000);
}

void LDR(float amps) {
  ldrValue = analogRead(LED_LDRpin);  //reads the value of the sensor [0-1023]

  if (ldrValue <= ldr_threshold || amps > 0.0)  //checks if ambient light is normal
  {
    Serial.print(ldrValue);
    Serial.println(" | LAMP ON");
    led_status = 1.0;
  } else {
    Serial.print(ldrValue);
    Serial.println(" | LAMP OFF");
    led_status = 0.0;
  }
}

void dSOC() {
  if (bat_avg_volts > 3.35) {
    batt_level = 100.0;
  } else if (bat_avg_volts > 3.30) {
    batt_level = 90.0;
  } else if (bat_avg_volts > 3.25) {
    batt_level = 80.0;
  } else if (bat_avg_volts > 3.20) {
    batt_level = 70.0;
  } else if (bat_avg_volts > 3.15) {
    batt_level = 60.0;
  } else if (bat_avg_volts > 3.10) {
    batt_level = 50.0;
  } else if (bat_avg_volts > 3.05) {
    batt_level = 40.0;
  } else if (bat_avg_volts > 3.00) {
    batt_level = 30.0;
  } else if (bat_avg_volts > 2.90) {
    batt_level = 20.0;
  } else if (bat_avg_volts > 2.50) {
    batt_level = 10.0;
  } else {
    batt_level = 0.0;
  }
}

void ReadSensors() {
  float pv_v_analog = 0.0;
  float pv_c_analog = 0.0;
  float pv_v_digital = 0.0;
  float pv_c_digital = 0.0;
  float pv_volts = 0.0;
  float pv_amps = 0.0;
  float PVsample_V = 0.0;
  float PVsample_C = 0.0;
  float bat_v_analog = 0.0;
  float bat_c_analog = 0.0;
  float bat_v_digital = 0.0;
  float bat_c_digital = 0.0;
  float bat_volts = 0.0;
  float bat_amps = 0.0;
  float BATsample_V = 0.0;
  float BATsample_C = 0.0;
  float sample_T = 0.0;
  float sample_L = 0.0;

  //uint16_t lux = lightMeter.readLightLevel();
  temp.requestTemperatures();
  //float tempC = temp.getTempCByIndex(0);

  // Read the Analog Input of Voltage and Current Sensor
  for (int x = 0; x < 150; x++) {  // run through loop 10x
    // SOLAR PANEL ANALOG READ
    pv_v_analog = analogRead(PV_VoltagePin);
    pv_c_analog = analogRead(PV_CurrentPin);

    // BATTERY ANALOG READ
    bat_v_analog = analogRead(BAT_VoltagePin);
    bat_c_analog = analogRead(BAT_CurrentPin);

    // TEMP AND AMBIENT LIGHT READ
    float tempC = temp.getTempCByIndex(0);
    uint16_t lux = lightMeter.readLightLevel();

    // SOLAR PANEL ADC
    pv_v_digital = (pv_v_analog * ref_voltage) / 1024.0;
    pv_c_digital = (pv_c_analog * ref_voltage) / 1024.0;
    pv_volts = (pv_v_digital / (R2 / (R1 + R2))) - 0.05;  //offset 0.23V
    pv_amps = (2.33 - pv_c_digital) / sensitivity;        //offset when 0 current: 2.33V

    if (pv_volts <= 0.10) {
      pv_volts = 0.0;
    }
    if (pv_amps <= 0.0) {
      pv_amps = 0.0;
    }

    // BATTERY ADC
    bat_v_digital = (bat_v_analog * ref_voltage) / 1024.0;
    bat_c_digital = (bat_c_analog * ref_voltage) / 1024.0;
    bat_volts = (bat_v_digital / (R2 / (R1 + R2))) - 0.20;  //offset 0.19V
    bat_amps = (2.33 - bat_c_digital) / sensitivity;        //offset when 0 current: 2.33V

    // Serial.println(pv_volts);

    if (bat_volts <= 0.10) {
      bat_volts = 0.0;
    }
    if (bat_amps <= 0.0) {
      bat_amps = 0.0;
    }

    PVsample_V = PVsample_V + pv_volts;
    PVsample_C = PVsample_C + pv_amps;
    BATsample_V = BATsample_V + bat_volts;
    BATsample_C = BATsample_C + bat_amps;
    sample_T = sample_T + tempC;
    sample_L = sample_L + lux;
    delay(3);
  }
  pv_avg_volts = (PVsample_V) / 150;
  if (pv_avg_volts <= 0.10) {
    pv_avg_volts = 0.0;
  }
  pv_avg_amps = (PVsample_C) / 150;
  pv_power = pv_avg_volts * pv_avg_amps;

  bat_avg_volts = (BATsample_V) / 150;
  bat_avg_amps = (BATsample_C) / 150;
  bat_power = bat_avg_volts * bat_avg_amps;

  avg_temp = (sample_T) / 150;
  avg_lux = (sample_L) / 150;

  if (pv_power > 0.0 || pv_avg_volts > 3.0) {
    charging = 1.0;
    batt_level = ceil((bat_avg_volts - 2.50) / (3.65 - 2.50) * 100);
    if (batt_level > 100.0) {
      batt_level = 100.0;
    }
  } else {
    charging = 0.0;
    // dSOC();
    batt_level = ceil((bat_avg_volts - 2.50) / (3.40 - 2.50) * 100);
    if (batt_level > 100.0) {
      batt_level = 100.0;
    }
  }

  LDR(bat_avg_amps);

  // Print results to Serial Monitor to 2 decimal places
  Serial.print("(pV) = ");
  Serial.print(pv_avg_volts);

  Serial.print(" | (pA) : ");
  Serial.print(pv_avg_amps);

  Serial.print(" | (pW) : ");
  Serial.print(pv_power);

  Serial.print(" | (bV) = ");
  Serial.print(bat_avg_volts);

  Serial.print(" | (bA) : ");
  Serial.print(bat_avg_amps);

  Serial.print(" | (bW) : ");
  Serial.print(bat_power);

  Serial.print(" | (Level) : ");
  Serial.print(batt_level);

  Serial.print(" | (Status) : ");
  Serial.print(led_status);

  Serial.print(" | (lux): ");
  Serial.print(avg_lux);

  Serial.print(" | (*C): ");
  Serial.print(avg_temp);

  Serial.print(" | (Charging): ");
  Serial.println(charging);
}

void onReceive(int packetSize) {
  if (packetSize == 0) return;  // if there's no packet, return
  // read packet header bytes:
  int recipient = LoRa.read();        // recipient address
  byte sender = LoRa.read();          // sender address
  byte incomingMsgId = LoRa.read();   // incoming msg ID
  byte incomingLength = LoRa.read();  // incoming msg length
  String incoming = "";
  while (LoRa.available()) {
    incoming += (char)LoRa.read();
  }
  Serial.println(incoming);
  if (incomingLength != incoming.length()) {  // check length for error
    Serial.println("error: message length does not match length");
    ;
    return;  // skip rest of function
  }
  // if the recipient isn't this device or broadcast,
  if (recipient != Node3 && recipient != MasterNode) {
    Serial.println("This message is not for me.");
    ;
    return;  // skip rest of function
  }
  Serial.println(incoming);
  Mymessage = "";
  if (incoming == "SL3") {
    ReadSensors();
    Mymessage = Mymessage + pv_avg_volts + "," + pv_power + "," + bat_avg_volts + "," + batt_level + "," + bat_power + "," + led_status + "," + avg_temp + "," + avg_lux + "," + charging;
    for (int i = 0; i < 2; i++) {
      sendMessage(Mymessage, MasterNode, Node3);
      delay(5000);
    }
    delay(100);
    Mymessage = "";
  }
  Mymessage = "";
}

void sendMessage(String outgoing, byte MasterNode, byte Node3) {
  LoRa.beginPacket();             // start packet
  LoRa.write(MasterNode);         // add destination address
  LoRa.write(Node3);              // add sender address
  LoRa.write(msgCount);           // add message ID
  LoRa.write(outgoing.length());  // add payload length
  LoRa.print(outgoing);           // add payload
  LoRa.endPacket();               // finish packet and send it
  msgCount++;                     // increment message ID
}
