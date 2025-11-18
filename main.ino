/*
 * ALCOHOLÍMETRO CON ARDUINO — con debug Serial opcional
 *
 * - LED_SAVE_INDICATOR -> D3 (reservado)
 * - LEDs de nivel -> D4..D11 (8 LEDs)
 * - Botones -> D12 (SAVE), D13 (READ)
 * - Sensor -> A0
 *
 * Activa/desactiva debug cambiando DEBUG a true/false.
 */

#include <EEPROM.h>

// ================= CONFIG =================
const bool DEBUG = true;               // <-- true = mensajes por Serial
const unsigned long SERIAL_BAUD = 115200;

// Pines
const int ANALOG_SENSOR = A0;
const int LED_SAVE_INDICATOR = 3;
const int BUTTON_SAVE = 12;
const int BUTTON_READ = 13;

// LEDs de nivel (8 leds)
const int LED_COUNT = 8;
int ledPins[] = {4, 5, 6, 7, 8, 9, 10, 11};

// Variables debounce
unsigned long lastDebounceSave = 0;
unsigned long lastDebounceRead = 0;
const unsigned long DEBOUNCE_MS = 50;
int prevSaveState = LOW;
int prevReadState = LOW;

// Calibración y EEPROM
const int SENSOR_MIN = 700;
const int SENSOR_MAX = 900;
const int MAX_STORED_READINGS = 3;
const unsigned long HEATER_WARMUP_MS = 20000UL;

const int ADDR_READING0 = 0;
const int ADDR_READING1 = 1;
const int ADDR_READING2 = 2;

// Impresión periódica en Serial para no saturar
unsigned long lastPrintMillis = 0;
const unsigned long PRINT_INTERVAL_MS = 500; // cada 500 ms

// ================= SETUP =================
void setup() {
  if (DEBUG) {
    Serial.begin(SERIAL_BAUD);
    Serial.println(F("=== Alcoholimetro: inicio (debug ON) ==="));
  }

  pinMode(LED_SAVE_INDICATOR, OUTPUT);
  pinMode(BUTTON_SAVE, INPUT);
  pinMode(BUTTON_READ, INPUT);

  for (int i = 0; i < LED_COUNT; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }

  // Calentamiento del heater con feedback en Serial y LED indicador
  if (DEBUG) Serial.print(F("Calentando sensor ("));
  if (DEBUG) Serial.print(HEATER_WARMUP_MS / 1000);
  if (DEBUG) Serial.println(F(" s)..."));

  unsigned long start = millis();
  while (millis() - start < HEATER_WARMUP_MS) {
    digitalWrite(LED_SAVE_INDICATOR, HIGH);
    delay(200);
    digitalWrite(LED_SAVE_INDICATOR, LOW);
    delay(300);

    // Mensaje cada 5 s para no saturar
    if (DEBUG && (millis() - start) % 5000 < 600) {
      unsigned long elapsed = (millis() - start) / 1000;
      Serial.print(F("  calentamiento: "));
      Serial.print(elapsed);
      Serial.println(F("s"));
    }
  }
  digitalWrite(LED_SAVE_INDICATOR, LOW);
  if (DEBUG) Serial.println(F("Heater listo."));
}

// ================= FUNCIONES =================
void guardarLectura(int value) {
  int lectura1 = EEPROM.read(ADDR_READING0);
  int lectura2 = EEPROM.read(ADDR_READING1);

  if (lectura1 == 255) lectura1 = 0;
  if (lectura2 == 255) lectura2 = 0;

  if (DEBUG) {
    Serial.print(F("Guardando lectura: "));
    Serial.print(value);
    Serial.print(F("  (antes: [0]="));
    Serial.print(lectura1);
    Serial.print(F(", [1]="));
    Serial.print(lectura2);
    Serial.println(F(")"));
  }

  EEPROM.update(ADDR_READING2, lectura2);
  EEPROM.update(ADDR_READING1, lectura1);
  EEPROM.update(ADDR_READING0, value);

  digitalWrite(LED_SAVE_INDICATOR, HIGH);
  delay(400);
  digitalWrite(LED_SAVE_INDICATOR, LOW);

  if (DEBUG) Serial.println(F("Lectura almacenada en EEPROM."));
}

void mostrarNivelEnLEDs(int nivel) {
  nivel = constrain(nivel, 0, LED_COUNT);
  for (int i = 0; i < LED_COUNT; i++) {
    digitalWrite(ledPins[i], (i < nivel) ? HIGH : LOW);
  }
}

void mostrarLecturasGuardadas() {
  int localIndex = 0;
  if (DEBUG) Serial.println(F("Entrando en modo mostrar lecturas guardadas (pulsa READ para avanzar)"));

  while (true) {
    int raw = digitalRead(BUTTON_READ);

    if (raw != prevReadState) {
      lastDebounceRead = millis();
      prevReadState = raw;
    }

    if ((millis() - lastDebounceRead) > DEBOUNCE_MS) {
      if (raw == HIGH) {
        if (localIndex < MAX_STORED_READINGS) {
          int addr = ADDR_READING0 + localIndex;
          int lecturaAlmacenada = EEPROM.read(addr);
          if (lecturaAlmacenada == 255) lecturaAlmacenada = 0;

          if (DEBUG) {
            Serial.print(F("Mostrando lectura #"));
            Serial.print(localIndex);
            Serial.print(F(" (EEPROM addr "));
            Serial.print(addr);
            Serial.print(F("): "));
            Serial.println(lecturaAlmacenada);
          }

          mostrarNivelEnLEDs(lecturaAlmacenada);
          localIndex++;

          while (digitalRead(BUTTON_READ) == HIGH) delay(10); // espera a soltar botón
          delay(150);
        } else {
          break;
        }
      }
    }
  }

  mostrarNivelEnLEDs(0);
  if (DEBUG) Serial.println(F("Salió modo mostrar lecturas."));
  delay(200);
}

int leerNivelAlcohol() {
  int raw = analogRead(ANALOG_SENSOR);
  int nivel = map(raw, SENSOR_MIN, SENSOR_MAX, 0, LED_COUNT);
  nivel = constrain(nivel, 0, LED_COUNT);

  // info por Serial periódica
  if (DEBUG && (millis() - lastPrintMillis >= PRINT_INTERVAL_MS)) {
    lastPrintMillis = millis();
    Serial.print(F("Analog A0="));
    Serial.print(raw);
    Serial.print(F(" -> nivel="));
    Serial.println(nivel);
  }

  return nivel;
}

// ================= LOOP =================
void loop() {
  int nivelAlcohol = leerNivelAlcohol();
  mostrarNivelEnLEDs(nivelAlcohol);

  // botón SAVE (con debounce)
  int saveRaw = digitalRead(BUTTON_SAVE);
  if (saveRaw != prevSaveState) {
    lastDebounceSave = millis();
    prevSaveState = saveRaw;
  }
  if ((millis() - lastDebounceSave) > DEBOUNCE_MS) {
    if (saveRaw == HIGH) {
      if (DEBUG) Serial.println(F("Botón SAVE pulsado -> guardar lectura"));
      guardarLectura(nivelAlcohol);
      while (digitalRead(BUTTON_SAVE) == HIGH) delay(10);
      delay(150);
    }
  }

  // botón READ
  int readRaw = digitalRead(BUTTON_READ);
  if (readRaw != prevReadState) {
    lastDebounceRead = millis();
    prevReadState = readRaw;
  }
  if ((millis() - lastDebounceRead) > DEBOUNCE_MS) {
    if (readRaw == HIGH) {
      if (DEBUG) Serial.println(F("Botón READ pulsado -> mostrar lecturas"));
      mostrarLecturasGuardadas();
      while (digitalRead(BUTTON_READ) == HIGH) delay(10);
      delay(150);
    }
  }

  delay(60);
}
