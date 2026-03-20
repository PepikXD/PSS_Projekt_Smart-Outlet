#include <WiFi.h>
#include <WebServer.h>

// ==========================================
// --- Konfigurace Wi-Fi (Access Point) ---
// ==========================================
const char* ssid = "Smart_Outlet_AP";
const char* password = "password123"; // Min. 8 znaků

// ==========================================
// --- Definice Pinů ---
// ==========================================
const int relayPin = 5;      // Ovládání relé
const int currentPin = 34;   // ACS712 OUT (Analogový vstup)
const int voltagePin = 35;   // ZMPT101B OUT (Analogový vstup)

// ==========================================
// --- Globální Kalibrační Proměnné ---
// ==========================================

// --- Proud (ACS712) ---
// Citlivost: 185 for 5A, 100 for 20A, 66 for 30A modul.
const float mVperAmp = 100.0; 
// Prahová hodnota pro odstranění šumu (Deadzone). 
// Ignoruje vše pod cca 50W-70W zátěže pro čistou nulu.
const float currentNoiseThreshold = 0.25; 
float currentOffsetmV = 2500.0; // Výchozí bod, bude překalibrován
bool currentCalibrated = false;

// --- Napětí (ZMPT101B) ---
// Prahová hodnota pro odstranění šumu. Pokud rawRMS < 150, vynutí 0V.
const float voltageNoiseThreshold = 150.0; 
// Kalibrační faktor: Upravte (např. 0.19 -> 0.22), aby ON napětí odpovídalo realitě (230V).
const float voltageCalibrationFactor = 0.21; 

// ==========================================
// --- Stavové proměnné ---
// ==========================================
bool relayState = false;
float voltageRMS = 0.0;
float currentRMS = 0.0;
float powerWatts = 0.0;

// Vytvoření instance webového serveru na portu 80
WebServer server(80);

// ==========================================
// --- Funkce pro generování HTML ---
// ==========================================
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
  // Automatické obnovení stránky každé 3 sekundy
  html += "<script>setInterval(function() { window.location.reload(); }, 3000);</script>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>Chytrá Zásuvka</h1>";
  html += "<h2>Monitorování a Ovládání ESP32</h2>";
  
  // Sekce Monitoringu
  html += "<div class='monitor-card'>";
  html += "<div class='monitor-item'><span class='monitor-label'>Napětí:</span> <span class='monitor-value'>" + String(voltageRMS, 1) + "<span class='unit'>V AC</span></span></div>";
  html += "<div class='monitor-item'><span class='monitor-label'>Proud:</span> <span class='monitor-value'>" + String(currentRMS, 2) + "<span class='unit'>A AC</span></span></div>";
  html += "<div class='monitor-item'><span class='monitor-label'>Příkon:</span> <span class='monitor-value'>" + String(powerWatts, 1) + "<span class='unit'>W</span></span></div>";
  html += "</div>";

  // Sekce Ovládání
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

// ==========================================
// --- Ovládací Funkce Relé ---
// ==========================================

// Zapnutí relé (Active Low - táhne pin k zemi)
void turnRelayOn() {
  pinMode(relayPin, OUTPUT);      // Nastavit jako výstup
  digitalWrite(relayPin, LOW);    // LOW sepne relé
  relayState = true;
  Serial.println("Relé: ZAPNUTO (GPIO 5 táhne k GND)");
}

// Vypnutí relé (Pin Mode Trick - "odpojí" pin)
void turnRelayOff() {
  // Přepnutím na INPUT pin přestane aktivně tlačit 3.3V,
  // což umožní internímu pull-up na relé modulu spolehlivě vypnout.
  pinMode(relayPin, INPUT);       // Nastavit jako vstup (float)
  relayState = false;
  // Při vypnutí relé resetujeme kalibraci proudu, aby se příště znova zkalibrovalo
  // i s magnetickým rušením od sepnutého relé.
  currentCalibrated = false; 
  Serial.println("Relé: VYPNUTO (GPIO 5 nastaven na INPUT)");
}

// ==========================================
// --- Funkce Měření ---
// ==========================================

// Měření RMS Proudu (ACS712)
float getACCurrentRMS() {
  // TRICK 1: Hard nula pokud je relé vypnuté.
  if (!relayState) return 0.00;

  // TRICK 2: Auto-Kalibrace Offsetu (Runs once when first turned ON)
  // Měří a nuluje offset včetně magnetického rušení od cívky relé.
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

  // --- Výpočet AC RMS (měříme 100ms / 5 cyklů pro stabilitu) ---
  unsigned long startTime = millis();
  float currentSum = 0;
  unsigned int sampleCount = 0;
  
  while (millis() - startTime < 100) {
    int rawValue = analogRead(currentPin);
    float sampleVoltage = (rawValue / 4095.0) * 3300.0; // Převod na mV
    
    // Výpočet okamžitého proudu
    float instantaneousCurrent = (sampleVoltage - currentOffsetmV) / mVperAmp;
    
    // Suma čtverců
    currentSum += (instantaneousCurrent * instantaneousCurrent);
    sampleCount++;
  }
  
  if (sampleCount == 0) return 0.00;
  float rms = sqrt(currentSum / sampleCount);

  // TRICK 3: Proudový Deadzone
  // Pokud je RMS pod 0.25A, považuje to za šum a vynutí 0.00A.
  if (rms < currentNoiseThreshold) return 0.00; 

  return rms;
}

// Měření RMS Napětí (ZMPT101B) s Continuous Zero-Tracking
float getACVoltageRMS() {
  // TRICK 1: Hard nula pokud je relé vypnuté.
  if (!relayState) return 0.0; 

  float totalRMS = 0;
  int iterations = 10; // Průměrování přes 10 AC cyklů (0.2 sekundy)

  for (int j = 0; j < iterations; j++) {
    long voltageSum = 0;
    unsigned int sampleCount = 0;
    
    // TRICK 2: Continuous Zero-Point Tracking
    // Najde nulový bod (DC offset) pro toto konkrétní okno měření.
    // Tím se eliminuje pomalý tepelný drift (např. těch 100V po restartu).
    long subSum = 0;
    for(int i=0; i<100; i++) subSum += analogRead(voltagePin);
    int currentZero = subSum / 100;

    unsigned long startTime = micros(); 
    // Vzorkování přesně 20000 mikrosekund (jeden 50Hz cyklus)
    while (micros() - startTime < 20000) {
      int rawValue = analogRead(voltagePin);
      int shiftedValue = rawValue - currentZero;
      // Používáme (long) pro sumu čtverců, aby nedošlo k přetečení
      voltageSum += (long)shiftedValue * shiftedValue;
      sampleCount++;
    }
    
    if (sampleCount > 0) {
      totalRMS += sqrt(voltageSum / sampleCount);
    }
  }

  // Průměrné rawRMS přes všech 10 cyklů
  float avgRawRMS = totalRMS / iterations;

  // TRICK 3: Agresivní Napěťový Noise Filter
  // Pokud rawRMS < 150 (zhruba pod 35V), vynutí 0.0V.
  if (avgRawRMS < voltageNoiseThreshold) return 0.0;

  // TRICK 4: Kalibrační Faktor
  // Používá globální faktor. Upravte 0.21, aby ON napětí sedělo na 230V.
  return avgRawRMS * voltageCalibrationFactor; 
}

// ==========================================
// --- Handlery Webového Serveru ---
// ==========================================

// Obsluha hlavní stránky ("/")
void handleRoot() {
  server.send(200, "text/html", getHTML());
}

// Obsluha přepnutí relé ("/toggle")
void handleToggle() {
  if (relayState) {
    turnRelayOff();
  } else {
    turnRelayOn();
  }
  // Přesměrování zpět na hlavní stránku
  server.sendHeader("Location", "/");
  server.send(303);
}

// ==========================================
// --- Základní Arduino Funkce ---
// ==========================================

void setup() {
  // Inicializace sériové linky pro ladění
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n--- IoT Smart Energy Monitor Start ---");

  // Nastavení ADC na plný rozsah 0-3.3V
  analogSetAttenuation(ADC_11db);
  
  // Start se zásuvkou ve VYPNUTÉM stavu
  turnRelayOff();

  // Konfigurace ESP32 jako Access Point
  Serial.print("Konfigurace Wi-Fi Access Pointu: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  // Tisk IP adresy (obvykle 192.168.4.1)
  Serial.print("AP Spuštěno. IP adresa: ");
  Serial.println(WiFi.softAPIP());

  // Definice cest (routes) pro server
  server.on("/", handleRoot);
  server.on("/toggle", handleToggle);

  // Start webového serveru
  server.begin();
  Serial.println("Webový server spuštěn.");
}

void loop() {
  // Vyřízení příchozích požadavků webového serveru
  server.handleClient();

  // --- Aktualizace měření ---
  // Měření RMS hodnot (zabere cca 300ms celkem)
  voltageRMS = getACVoltageRMS();
  currentRMS = getACCurrentRMS();

  // Výpočet příkonu (P = V * I, předpokládáme Power Factor = 1.0)
  // Počítáme jen pokud jsou hodnoty nenulové a smysluplné.
  if (voltageRMS > 50.0 && currentRMS > 0.01) {
    powerWatts = voltageRMS * currentRMS;
  } else {
    powerWatts = 0.0;
  }

  // Volitelný tisk do sériového monitoru pro ladění
  /*
  if (relayState) {
    Serial.print("V: "); Serial.print(voltageRMS, 1);
    Serial.print("V, I: "); Serial.print(currentRMS, 2);
    Serial.print("A, P: "); Serial.print(powerWatts, 1);
    Serial.println("W");
  }
  */
  
  // Krátká pauza pro stabilitu hlavní smyčky
  delay(100);
}
