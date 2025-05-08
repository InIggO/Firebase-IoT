#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "DHT.h"

// Firebase helpers
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

// Sensor DHT
#define DHTPIN 21
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// LEDs
#define LED_GREEN 14
#define LED_RED 13
#define RGB_RED 4
#define RGB_GREEN 16
#define RGB_BLUE 17

// PWM para RGB
#define PWM_FREQ 5000
#define PWM_RESOLUTION 8
#define CH_RED 0
#define CH_GREEN 1
#define CH_BLUE 2

// NTP
#define NTP_SERVER "pool.ntp.org"
#define NTP_GMT_OFFSET_SEC 3600
#define NTP_DAYLIGHT_OFFSET_SEC 3600


// Firebase objetos
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

bool signupOK = false;

// Intervalos
unsigned long prevSensorMillis = 0;
unsigned long prevActuatorMillis = 0;
const unsigned long SENSOR_INTERVAL = 60000;  // 60 segundos
const unsigned long ACTUATOR_INTERVAL = 3000; // 3 segundos

// Prototipos de tiempo
String getLocalTimeISO();
String getLocalTimeUNIX();

void setup()
{
  Serial.begin(115200);
  dht.begin();

  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  // PWM RGB
  ledcSetup(CH_RED, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(CH_GREEN, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(CH_BLUE, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(RGB_RED, CH_RED);
  ledcAttachPin(RGB_GREEN, CH_GREEN);
  ledcAttachPin(RGB_BLUE, CH_BLUE);

  // WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(333);
  }
  Serial.println();
  Serial.print("Conectado con IP: ");
  Serial.println(WiFi.localIP());

  // NTP
  configTime(NTP_GMT_OFFSET_SEC, NTP_DAYLIGHT_OFFSET_SEC, NTP_SERVER);

  // Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase signup ok!");
    signupOK = true;
  } else {
    Serial.printf("Firebase signup failed: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop()
{
  unsigned long currentMillis = millis();

  // LECTURA DE ACTUADORES (cada 3 segundos)
  if (Firebase.ready() && signupOK && (currentMillis - prevActuatorMillis >= ACTUATOR_INTERVAL)) {
    prevActuatorMillis = currentMillis;

    // LED VERDE
    bool ledStateG = false;
    if (Firebase.RTDB.getBool(&fbdo, "actuador/led_g", &ledStateG)) {
      digitalWrite(LED_GREEN, ledStateG ? HIGH : LOW);
      Serial.print("LED VERDE: ");
      Serial.println(ledStateG ? "ON" : "OFF");
    }

    // LED ROJO
    bool ledStateR = false;
    if (Firebase.RTDB.getBool(&fbdo, "actuador/led_r", &ledStateR)) {
      digitalWrite(LED_RED, ledStateR ? HIGH : LOW);
      Serial.print("LED ROJO: ");
      Serial.println(ledStateR ? "ON" : "OFF");
    }

    // RGB LEDs
    int r = 0, g = 0, b = 0;
    if (Firebase.RTDB.getInt(&fbdo, "actuador/rgb/red", &r) &&
        Firebase.RTDB.getInt(&fbdo, "actuador/rgb/green", &g) &&
        Firebase.RTDB.getInt(&fbdo, "actuador/rgb/blue", &b)) {
      ledcWrite(CH_RED, r);
      ledcWrite(CH_GREEN, g);
      ledcWrite(CH_BLUE, b);
      Serial.printf("RGB -> R:%d G:%d B:%d\n", r, g, b);
    }
  }

  // LECTURA DE SENSOR (cada 60 segundos)
  if (Firebase.ready() && signupOK && (currentMillis - prevSensorMillis >= SENSOR_INTERVAL)) {
    prevSensorMillis = currentMillis;

    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();

    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Error leyendo del sensor DHT!");
      return;
    }

    String timestamp_unix = getLocalTimeUNIX();
    String timestamp_iso = getLocalTimeISO();
    String path = "sensor/" + timestamp_unix;

    Firebase.RTDB.setString(&fbdo, path + "/timestamp", timestamp_iso);
    Firebase.RTDB.setFloat(&fbdo, path + "/temperatura", temperature);
    Firebase.RTDB.setFloat(&fbdo, path + "/humedad", humidity);

    String comfort = "confortable";
    if (humidity > 60 && temperature > 30) {
      comfort = "incómodo";
    }
    Firebase.RTDB.setString(&fbdo, path + "/confort", comfort);

    Serial.println("Datos enviados:");
    Serial.println("  Temperatura: " + String(temperature));
    Serial.println("  Humedad: " + String(humidity));
    Serial.println("  Confort: " + comfort);
    Serial.println("  Timestamp (UNIX): " + timestamp_unix);
    Serial.println("  Timestamp (ISO): " + timestamp_iso);
  }
}

String getLocalTimeISO()
{
  struct tm timeinfo;
  char buffer[25];
  if (!getLocalTime(&timeinfo)) {
    return "NTP Error!";
  }
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &timeinfo);
  return String(buffer);
}

String getLocalTimeUNIX()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "NTP Error!";
  }
  return String(mktime(&timeinfo));
}

