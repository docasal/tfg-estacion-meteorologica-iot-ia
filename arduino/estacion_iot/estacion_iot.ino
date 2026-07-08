#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ArduinoJson.h>

#include "Arduino_H7_Video.h"
#include "lvgl.h"

#define LDR_PIN A0

// -----------------------------
// WIFI Y MQTT
// -----------------------------
const char* ssid = "********";
const char* password = "********";
const char* mqtt_server = "*********";

const char* TOPIC_DATOS = "estacionmeteorologica";
const char* TOPIC_PREDICCION = "estacionmeteorologica/prediccion";

WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiServer webServer(80);

// -----------------------------
// SENSOR BME280
// -----------------------------
Adafruit_BME280 bme;

// -----------------------------
// DISPLAY GIGA
// -----------------------------
Arduino_H7_Video Display(800, 480, GigaDisplayShield);

// -----------------------------
// TIEMPOS
// -----------------------------
unsigned long lastUpdate = 0;
const unsigned long updateInterval = 5UL * 60UL * 1000UL;  // 5 minutos

// -----------------------------
// DATOS ACTUALES
// -----------------------------
float tempActual = 0.0;
float presActual = 0.0;
float humActual  = 0.0;
int luzActual    = 0;

String estadoActual = "----";
String iconoActual = "";
String colorEstadoActual = "#7f8c8d";

// -----------------------------
// PREDICCION IA A 1 HORA
// -----------------------------
float tempPrev = 0.0;
float presPrev = 0.0;
float humPrev  = 0.0;

String estadoPrev = "ESPERANDO";
String iconoPrev = "?";
String colorEstadoPrev = "#7f8c8d";

String timestampPrediccion = "";
String horizontePrediccion = "1h";
bool prediccionRecibida = false;

// -----------------------------
// OBJETOS LVGL
// -----------------------------
lv_obj_t *labelTempActual;
lv_obj_t *labelPresActual;
lv_obj_t *labelHumActual;
lv_obj_t *labelEstadoActual;

lv_obj_t *labelTempPrev;
lv_obj_t *labelPresPrev;
lv_obj_t *labelHumPrev;
lv_obj_t *labelEstadoPrev;

// -----------------------------
// FUNCIONES AUXILIARES
// -----------------------------
lv_color_t getEstadoColorLVGL(String estado) {
  if (estado == "SOLEADO") return lv_palette_main(LV_PALETTE_ORANGE);
  if (estado == "NUBLADO") return lv_palette_main(LV_PALETTE_GREY);
  if (estado == "LLUVIA")  return lv_palette_main(LV_PALETTE_BLUE);
  if (estado == "NOCHE")   return lv_palette_main(LV_PALETTE_BLUE_GREY);
  return lv_palette_main(LV_PALETTE_GREY);
}

String getEstadoColorWeb(String estado) {
  if (estado == "SOLEADO") return "#f39c12";
  if (estado == "NUBLADO") return "#7f8c8d";
  if (estado == "LLUVIA")  return "#3498db";
  if (estado == "NOCHE")   return "#34495e";
  return "#7f8c8d";
}

String getIconoWeb(String estado) {
  if (estado == "SOLEADO") return "☀";
  if (estado == "NUBLADO") return "☁";
  if (estado == "LLUVIA")  return "🌧";
  if (estado == "NOCHE")   return "🌙";
  return "?";
}

// -----------------------------
// LOGICA DE ESTADO ACTUAL
// -----------------------------
void calcularEstadoActual() {

  if ((humActual >= 75.0 && luzActual < 100) ||
      (humActual >= 78.0 && presActual < 1010.0)) {
    estadoActual = "LLUVIA";
  }
  else if (luzActual > 700) {
    estadoActual = "NOCHE";
  }
  else if (luzActual <= 5) {
    estadoActual = "SOLEADO";
  }
  else {
    estadoActual = "NUBLADO";
  }

  iconoActual = getIconoWeb(estadoActual);
  colorEstadoActual = getEstadoColorWeb(estadoActual);
}

// -----------------------------
// LECTURA DE SENSORES
// -----------------------------
void leerSensores() {
  tempActual = bme.readTemperature();
  humActual  = bme.readHumidity();
  presActual = bme.readPressure() / 100.0F;

  int luz1 = analogRead(LDR_PIN);
  delay(10);
  int luz2 = analogRead(LDR_PIN);
  luzActual = (luz1 + luz2) / 2;

  calcularEstadoActual();

  Serial.println("Datos actualizados:");
  Serial.print("Temperatura: "); Serial.println(tempActual);
  Serial.print("Humedad: "); Serial.println(humActual);
  Serial.print("Presion: "); Serial.println(presActual);
  Serial.print("Luz: "); Serial.println(luzActual);
  Serial.print("Estado: "); Serial.println(estadoActual);
}

// -----------------------------
// CALLBACK MQTT PREDICCION
// -----------------------------
void recibirMQTT(char* topic, byte* payload, unsigned int length) {
  String mensaje = "";

  for (unsigned int i = 0; i < length; i++) {
    mensaje += (char)payload[i];
  }

  Serial.println("Mensaje MQTT recibido:");
  Serial.println(topic);
  Serial.println(mensaje);

  if (String(topic) == TOPIC_PREDICCION) {

    StaticJsonDocument<768> doc;
    DeserializationError error = deserializeJson(doc, mensaje);

    if (error) {
      Serial.print("Error parseando JSON de prediccion: ");
      Serial.println(error.c_str());
      return;
    }

    if (doc.containsKey("temperatura_prevista")) {
      tempPrev = doc["temperatura_prevista"];
    }

    if (doc.containsKey("humedad_prevista")) {
      humPrev = doc["humedad_prevista"];
    }

    if (doc.containsKey("presion_prevista")) {
      presPrev = doc["presion_prevista"];
    }

    if (doc.containsKey("estado_previsto")) {
      estadoPrev = doc["estado_previsto"].as<String>();
    }

    if (doc.containsKey("timestamp_prediccion")) {
      timestampPrediccion = doc["timestamp_prediccion"].as<String>();
    }

    if (doc.containsKey("horizonte")) {
      horizontePrediccion = doc["horizonte"].as<String>();
    }

    iconoPrev = getIconoWeb(estadoPrev);
    colorEstadoPrev = getEstadoColorWeb(estadoPrev);
    prediccionRecibida = true;

    Serial.println("Prediccion actualizada:");
    Serial.print("Temperatura prevista: "); Serial.println(tempPrev);
    Serial.print("Humedad prevista: "); Serial.println(humPrev);
    Serial.print("Presion prevista: "); Serial.println(presPrev);
    Serial.print("Estado previsto: "); Serial.println(estadoPrev);

    actualizarShield();
  }
}

// -----------------------------
// MQTT
// -----------------------------
void publicarMQTT() {
  if (!mqttClient.connected()) return;

  char mensaje[220];
  snprintf(
    mensaje,
    sizeof(mensaje),
    "{ \"temperatura\": %.2f, \"humedad\": %.2f, \"presion\": %.2f, \"luz\": %d }",
    tempActual,
    humActual,
    presActual,
    luzActual
  );

  mqttClient.publish(TOPIC_DATOS, mensaje);
  Serial.println("Datos enviados por MQTT:");
  Serial.println(mensaje);
}

void conectarWiFi() {
  Serial.print("Conectando al WiFi");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi conectado");
  Serial.print("IP Arduino: ");
  Serial.println(WiFi.localIP());
}

void conectarMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Conectando a MQTT...");

    if (mqttClient.connect("ArduinoGiga")) {
      Serial.println("conectado");

      mqttClient.subscribe(TOPIC_PREDICCION);
      Serial.print("Suscrito a: ");
      Serial.println(TOPIC_PREDICCION);

    } else {
      Serial.print("error, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" reintentando en 2 segundos");
      delay(2000);
    }
  }
}

// -----------------------------
// UI LVGL
// -----------------------------
lv_obj_t* crearTarjeta(int x, int y, int w, int h, const char* titulo, const char* subtitulo) {
  lv_obj_t *card = lv_obj_create(lv_scr_act());
  lv_obj_set_size(card, w, h);
  lv_obj_set_pos(card, x, y);

  lv_obj_set_style_radius(card, 18, 0);
  lv_obj_set_style_bg_color(card, lv_color_white(), 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_shadow_width(card, 20, 0);
  lv_obj_set_style_shadow_color(card, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
  lv_obj_set_style_pad_all(card, 16, 0);

  lv_obj_t *title = lv_label_create(card);
  lv_label_set_text(title, titulo);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(title, lv_palette_darken(LV_PALETTE_BLUE_GREY, 3), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

  lv_obj_t *sub = lv_label_create(card);
  lv_label_set_text(sub, subtitulo);
  lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(sub, lv_palette_darken(LV_PALETTE_GREY, 1), 0);
  lv_obj_align_to(sub, title, LV_ALIGN_OUT_BOTTOM_MID, 0, 6);

  return card;
}

lv_obj_t* crearDato(lv_obj_t *parent, const char* texto, int y) {
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_set_size(box, 320, 42);
  lv_obj_set_pos(box, 12, y);

  lv_obj_set_style_radius(box, 12, 0);
  lv_obj_set_style_bg_color(box, lv_palette_lighten(LV_PALETTE_BLUE_GREY, 5), 0);
  lv_obj_set_style_border_width(box, 0, 0);
  lv_obj_set_style_shadow_width(box, 0, 0);
  lv_obj_set_style_pad_left(box, 12, 0);
  lv_obj_set_style_pad_top(box, 9, 0);

  lv_obj_t *label = lv_label_create(box);
  lv_label_set_text(label, texto);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(label, lv_palette_darken(LV_PALETTE_BLUE_GREY, 4), 0);
  lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);

  return label;
}

lv_obj_t* crearEstado(lv_obj_t *parent, const char* texto, lv_color_t color, int y) {
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_set_size(box, 320, 78);
  lv_obj_set_pos(box, 12, y);

  lv_obj_set_style_radius(box, 16, 0);
  lv_obj_set_style_bg_color(box, color, 0);
  lv_obj_set_style_border_width(box, 0, 0);
  lv_obj_set_style_shadow_width(box, 0, 0);

  lv_obj_t *label = lv_label_create(box);
  lv_label_set_text(label, texto);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

  return label;
}

void actualizarShield() {
  static char buffer[100];

  snprintf(buffer, sizeof(buffer), "Temperatura: %.1f \xC2\xB0""C", tempActual);
  lv_label_set_text(labelTempActual, buffer);

  snprintf(buffer, sizeof(buffer), "Presion: %.1f hPa", presActual);
  lv_label_set_text(labelPresActual, buffer);

  snprintf(buffer, sizeof(buffer), "Humedad: %.1f %%", humActual);
  lv_label_set_text(labelHumActual, buffer);

  lv_label_set_text(labelEstadoActual, estadoActual.c_str());
  lv_obj_set_style_bg_color(lv_obj_get_parent(labelEstadoActual), getEstadoColorLVGL(estadoActual), 0);

  if (prediccionRecibida) {
    snprintf(buffer, sizeof(buffer), "Temperatura prevista: %.1f \xC2\xB0""C", tempPrev);
    lv_label_set_text(labelTempPrev, buffer);

    snprintf(buffer, sizeof(buffer), "Presion prevista: %.1f hPa", presPrev);
    lv_label_set_text(labelPresPrev, buffer);

    snprintf(buffer, sizeof(buffer), "Humedad prevista: %.1f %%", humPrev);
    lv_label_set_text(labelHumPrev, buffer);

    lv_label_set_text(labelEstadoPrev, estadoPrev.c_str());
    lv_obj_set_style_bg_color(lv_obj_get_parent(labelEstadoPrev), getEstadoColorLVGL(estadoPrev), 0);
  } else {
    lv_label_set_text(labelTempPrev, "Temperatura prevista: esperando");
    lv_label_set_text(labelPresPrev, "Presion prevista: esperando");
    lv_label_set_text(labelHumPrev, "Humedad prevista: esperando");

    lv_label_set_text(labelEstadoPrev, "ESPERANDO IA");
    lv_obj_set_style_bg_color(lv_obj_get_parent(labelEstadoPrev), lv_palette_main(LV_PALETTE_GREY), 0);
  }
}

void construirShield() {
  Display.begin();
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xEAF6FF), 0);

  lv_obj_t *cardActual = crearTarjeta(
    25, 40, 360, 390,
    "Medidas en tiempo real",
    "Datos actuales del sistema embebido"
  );

  labelTempActual   = crearDato(cardActual, "Temperatura: --.- C", 70);
  labelPresActual   = crearDato(cardActual, "Presion: ----.- hPa", 122);
  labelHumActual    = crearDato(cardActual, "Humedad: --.- %", 174);
  labelEstadoActual = crearEstado(cardActual, "----", lv_palette_main(LV_PALETTE_GREY), 245);

  lv_obj_t *cardPrev = crearTarjeta(
    415, 40, 360, 390,
    "Prevision a 1 hora",
    "Estimacion mediante IA y MQTT"
  );

  labelTempPrev   = crearDato(cardPrev, "Temperatura prevista: esperando", 70);
  labelPresPrev   = crearDato(cardPrev, "Presion prevista: esperando", 122);
  labelHumPrev    = crearDato(cardPrev, "Humedad prevista: esperando", 174);
  labelEstadoPrev = crearEstado(cardPrev, "ESPERANDO IA", lv_palette_main(LV_PALETTE_GREY), 245);

  actualizarShield();
}

// -----------------------------
// WEB
// -----------------------------
void servirWeb(WiFiClient client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html; charset=UTF-8");
  client.println("Connection: close");
  client.println();

  client.println("<!DOCTYPE html>");
  client.println("<html lang='es'>");
  client.println("<head>");
  client.println("<meta charset='UTF-8'>");
  client.println("<meta name='viewport' content='width=device-width, initial-scale=1.0'>");
  client.println("<meta http-equiv='refresh' content='300'>");
  client.println("<title>Estacion ambiental GIGA</title>");
  client.println("<style>");
  client.println("body { margin:0; font-family:Arial,sans-serif; background:linear-gradient(135deg,#dff6ff,#f7fbff); min-height:100vh; display:flex; justify-content:center; align-items:center; padding:20px; box-sizing:border-box; }");
  client.println(".container { display:grid; grid-template-columns:repeat(auto-fit,minmax(320px,1fr)); gap:20px; width:100%; max-width:950px; }");
  client.println(".card { background:white; border-radius:20px; box-shadow:0 10px 25px rgba(0,0,0,0.12); padding:25px; }");
  client.println("h1 { margin-top:0; text-align:center; color:#2c3e50; font-size:1.6em; }");
  client.println(".subtexto { text-align:center; color:#5f6b7a; font-size:0.95em; margin-top:-5px; margin-bottom:15px; }");
  client.println(".dato { font-size:1.1em; margin:12px 0; padding:10px 14px; border-radius:12px; background:#f4f8fb; }");
  client.println(".estado { margin-top:18px; text-align:center; font-size:1.3em; font-weight:bold; padding:16px; border-radius:16px; color:white; }");
  client.println(".emoji { font-size:2em; display:block; margin-bottom:8px; }");
  client.println(".nota { text-align:center; color:#7f8c8d; font-size:0.85em; margin-top:12px; }");
  client.println("</style>");
  client.println("</head>");
  client.println("<body>");
  client.println("<div class='container'>");

  // Tarjeta tiempo real
  client.println("<div class='card'>");
  client.println("<h1>Medidas en tiempo real</h1>");
  client.println("<div class='subtexto'>Datos actuales del sistema embebido</div>");

  client.print("<div class='dato'>🌡 <strong>Temperatura:</strong> ");
  client.print(tempActual, 1);
  client.println(" &deg;C</div>");

  client.print("<div class='dato'>🧭 <strong>Presion:</strong> ");
  client.print(presActual, 1);
  client.println(" hPa</div>");

  client.print("<div class='dato'>💧 <strong>Humedad:</strong> ");
  client.print(humActual, 1);
  client.println(" %</div>");

  client.print("<div class='estado' style='background:");
  client.print(colorEstadoActual);
  client.println(";'>");
  client.print("<span class='emoji'>");
  client.print(iconoActual);
  client.println("</span>");
  client.print(estadoActual);
  client.println("</div>");
  client.println("</div>");

  // Tarjeta prediccion
  client.println("<div class='card'>");
  client.println("<h1>Prevision a 1 hora</h1>");
  client.println("<div class='subtexto'>Estimacion mediante IA y MQTT</div>");

  if (prediccionRecibida) {
    client.print("<div class='dato'>🌡 <strong>Temperatura prevista:</strong> ");
    client.print(tempPrev, 1);
    client.println(" &deg;C</div>");

    client.print("<div class='dato'>🧭 <strong>Presion prevista:</strong> ");
    client.print(presPrev, 1);
    client.println(" hPa</div>");

    client.print("<div class='dato'>💧 <strong>Humedad prevista:</strong> ");
    client.print(humPrev, 1);
    client.println(" %</div>");

    client.print("<div class='estado' style='background:");
    client.print(colorEstadoPrev);
    client.println(";'>");
    client.print("<span class='emoji'>");
    client.print(iconoPrev);
    client.println("</span>");
    client.print(estadoPrev);
    client.println("</div>");

    client.print("<div class='nota'>Prediccion para: ");
    client.print(timestampPrediccion);
    client.println("</div>");

  } else {
    client.println("<div class='dato'>Esperando prediccion desde Python...</div>");
    client.println("<div class='estado' style='background:#7f8c8d;'>");
    client.println("<span class='emoji'>?</span>");
    client.println("ESPERANDO IA");
    client.println("</div>");
  }

  client.println("</div>");

  client.println("</div>");
  client.println("</body>");
  client.println("</html>");
}

// -----------------------------
// SETUP
// -----------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin();

  if (!bme.begin(0x76)) {
    Serial.println("No se encuentra el BME280 en 0x76, probando 0x77...");
    if (!bme.begin(0x77)) {
      Serial.println("No se encuentra el sensor BME280");
      while (1);
    }
  }

  Serial.println("Sensor BME280 detectado correctamente");

  conectarWiFi();

  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(recibirMQTT);

  webServer.begin();

  construirShield();

  leerSensores();
  actualizarShield();

  conectarMQTT();
  publicarMQTT();

  lastUpdate = millis();
}

// -----------------------------
// LOOP
// -----------------------------
void loop() {
  if (!mqttClient.connected()) {
    conectarMQTT();
  }

  mqttClient.loop();

  WiFiClient client = webServer.available();
  if (client) {
    servirWeb(client);
    delay(1);
    client.stop();
  }

  lv_timer_handler();
  delay(5);

  unsigned long now = millis();
  if (now - lastUpdate >= updateInterval) {
    lastUpdate = now;

    leerSensores();
    actualizarShield();
    publicarMQTT();
  }
}