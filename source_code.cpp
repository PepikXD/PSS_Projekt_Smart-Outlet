#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "Smart_Outlet_Monitor";
const char* password = "password123"; 

const int relayPin = 5;      
const int currentPin = 34;   
const int voltagePin = 35;   

const float mVperAmp = 100.0; 
const float currentNoiseThreshold = 0.25; 
float currentOffsetmV = 2500.0; 
bool currentCalibrated = false;

const float voltageNoiseThreshold = 150.0; 
const float voltageCalibrationFactor = 0.21; 

bool relayState = false;
float voltageRMS = 0.0;
float currentRMS = 0.0;
float powerWatts = 0.0;

WebServer server(80);

String getHTML() {
  String html = "<!DOCTYPE html><html lang='cs'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>IoT Energetický Monitor</title>";
  html += "<style>body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; text-align: center; background-color: #f0f2f5; color: #333; margin: 0; padding: 20px; }";
  html += ".container { max-width: 500px; margin: 0 auto; background-color: #fff; padding: 30px; border-radius: 15px; box-shadow: 0 10px 25px rgba(0,0,0,0.1); }";
  html += "h1 { color: #4A90E2; margin-bottom: 10px; font-size: 2.2em; }";
  html += "h2 { color: #666; font-weight: normal; margin-bottom: 30px; font-size: 1.1em; }";
  html += ".monitor-card { background: linear-gradient(135deg, #eef2f8 0%, #dce4f0 100%); padding: 20px; border-radius: 12px; margin-bottom: 25px; text-align: left; display: flex; flex-direction: column; gap: 15px; }";
  html += ".monitor-item { display: flex; justify-content: space-between; align-items: center; font-size: 1.2em; border-bottom: 1px solid #cddae6; padding-bottom: 10px; }";
  html += ".monitor-item:last-child { border-bottom: none; padding-bottom: 0; }";
  html += ".monitor-label { color: #555; font-weight: 500; }";
  html += ".monitor-value { font-weight: bold; color: #2C3E50; font-size: 1.4em; }";
  html += ".unit { font-size: 0.8em; color: #7f8c8d; margin-left: 5px; }";
  html += ".control-section { margin-top: 20px; border-top: 1px solid #eee; padding-top: 20px; }";
  html += ".status-text { font-size: 1.1em; margin-bottom: 15px; }";
  html += ".status-p { font-weight: bold; }";
  html += ".status-on { color: #2ECC71; } .status-off { color: #E74C3C; }";
  html += ".btn { display: inline-block; padding: 15px 40px; font-size: 1.2em; font-weight: bold; color: #fff; text-decoration: none; border: none; border-radius: 50px; cursor: pointer; transition: all 0.3s ease; box-shadow: 0 4px 6px rgba(0,0,0,0.1); text-transform: uppercase; letter-spacing: 1px; }";
  html += ".btn:hover { transform: translateY(-2px); box-shadow: 0 6px 12px rgba(0,0,0,0.15); }";
  html += ".on-btn { background-color: #2ECC71; } .on-btn:hover { background-color: #27AE60; }";
  html += ".off-btn { background-color: #E74C3C; } .off-btn:hover { background-color: #C0392B; }";
  html += "</style>";
  html += "<script>setInterval(function() { window.location.reload(); }, 3000);</script>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>Chytrá Zásuvka</h1>";
  html += "<h2>Monitorování a Ovládání ESP32</h2>";
  
  html += "<div class='monitor-card'>";
  html += "<div class='monitor-item'><span class='monitor-label'>Napětí:</span> <span class='monitor-value'>" + String(voltageRMS, 1) + "<span class='unit'>V AC</span></span></div>";
  html += "<div class='monitor-item'><span class='monitor-label'>Proud:</span> <span class='monitor-value'>" + String(currentRMS, 2) + "<span class='unit'>A AC</span></span></div>";
  html += "<div class='monitor-item'><span class='monitor-label'>Příkon:</span> <span class='monitor-value'>" + String(powerWatts, 1) + "<span class='unit'>W</span></span></div>";
  html += "</div>";

  html += "<div class='control-section'>";
  html += "<div class='status-text'>Zásuvka je aktuálně: ";
  if (relayState) {
    html += "<span class='status-p status-on'>ZAPNUTA</span></div>";
    html += "<a href='/toggle'><button class='btn off-btn'>VYPNOUT</button></a>";
  } else {
    html += "<span class='status-p status-off'>VYPNUTA</span></div>";
    html += "<a href='/toggle'><button class='btn on-btn'>ZAPNOUT</button></a>";
  }
  html += "</div>";
  
  html += "</div></body></html>";
  return html;
}

void turnRelayOn() {
  pinMode(relayPin, OUTPUT);      
  digitalWrite(relayPin, LOW);    
  relayState = true;
  Serial.println("Relé: ZAPNUTO (GPIO 5 táhne k GND)");
}

void turnRelayOff() {
  pinMode(relayPin, INPUT);       
  relayState = false;
  currentCalibrated = false; 
  Serial.println("Relé: VYPNUTO (GPIO 5 nastaven na INPUT)");
}

float getACCurrentRMS() {
  if (!relayState) return 0.00;

  if (!currentCalibrated) {
    Serial.println("Kalibrace proudového offsetu (ACS712)...");
    long mV_sum = 0;
    for (int i = 0; i < 200; i++) {
        mV_sum += (analogRead(currentPin) / 4095.0) * 3300.0;
        delay(1);
    }
    currentOffsetmV = mV_sum / 200.0;
    currentCalibrated = true;
    Serial.print("Proudový offset zkalibrován na: ");
    Serial.println(currentOffsetmV);
  }

  unsigned long startTime = millis();
  float currentSum = 0;
  unsigned int sampleCount = 0;
  
  while (millis() - startTime < 100) {
    int rawValue = analogRead(currentPin);
    float sampleVoltage = (rawValue / 4095.0) * 3300.0; 
    
    float instantaneousCurrent = (sampleVoltage - currentOffsetmV) / mVperAmp;
    
    currentSum += (instantaneousCurrent * instantaneousCurrent);
    sampleCount++;
  }
  
  if (sampleCount == 0) return 0.00;
  float rms = sqrt(currentSum / sampleCount);

  if (rms < currentNoiseThreshold) return 0.00; 

  return rms;
}

float getACVoltageRMS() {
  if (!relayState) return 0.0; 

  float totalRMS = 0;
  int iterations = 10; 

  for (int j = 0; j < iterations; j++) {
    long voltageSum = 0;
    unsigned int sampleCount = 0;
    
    long subSum = 0;
    for(int i=0; i<100; i++) subSum += analogRead(voltagePin);
    int currentZero = subSum / 100;

    unsigned long startTime = micros(); 
    while (micros() - startTime < 20000) {
      int rawValue = analogRead(voltagePin);
      int shiftedValue = rawValue - currentZero;
      voltageSum += (long)shiftedValue * shiftedValue;
      sampleCount++;
    }
    
    if (sampleCount > 0) {
      totalRMS += sqrt(voltageSum / sampleCount);
    }
  }

  float avgRawRMS = totalRMS / iterations;

  if (avgRawRMS < voltageNoiseThreshold) return 0.0;

  return avgRawRMS * voltageCalibrationFactor; 
}

void handleRoot() {
  server.send(200, "text/html", getHTML());
}

void handleToggle() {
  if (relayState) {
    turnRelayOff();
  } else {
    turnRelayOn();
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n--- IoT Smart Energy Monitor Start ---");

  analogSetAttenuation(ADC_11db);
  
  turnRelayOff();

  Serial.print("Konfigurace Wi-Fi Access Pointu: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  Serial.print("AP Spuštěno. IP adresa: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/toggle", handleToggle);

  server.begin();
  Serial.println("Webový server spuštěn.");
}

void loop() {
  server.handleClient();

  voltageRMS = getACVoltageRMS();
  currentRMS = getACCurrentRMS();

  if (voltageRMS > 50.0 && currentRMS > 0.01) {
    powerWatts = voltageRMS * currentRMS;
  } else {
    powerWatts = 0.0;
  }

  delay(100);
}
