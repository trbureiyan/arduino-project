/*
 * ALCOHOLÍMETRO CON ARDUINO (modo test de LEDs por simulación cíclica)
 *
 * Cambia SIMULATE_SENSOR a true para falsear lecturas y probar la barra de LEDs.
 * Mantiene las mejoras previas: debounce, EEPROM.update, calentamiento, etc.
 */

#include <EEPROM.h>

// ========== CONFIGURACIÓN DE PINES ==========
const int ANALOG_SENSOR = A0;          // Pin del sensor analógico (MQ-3 AOUT)
const int LED_SAVE_INDICATOR = 11;     // LED que indica cuando se está guardando / actividad
const int BUTTON_SAVE = 12;            // Botón para guardar lectura actual (con pull-down externa)
const int BUTTON_READ = 13;            // Botón para mostrar lecturas guardadas (con pull-down externa)

// ========== CONFIGURACIÓN DE LEDS ==========
const int LED_COUNT = 8;               // Número de LEDs en la barra
int ledPins[] = {3, 4, 5, 6, 7, 8, 9, 10};  // Pines de los LEDs de nivel

// ========== MODO SIMULACIÓN (TEST) ==========
const bool SIMULATE_SENSOR = true;     // <-- true = simula valores, false = usa MQ-3 real
const unsigned long SIM_STEP_MS = 500; // tiempo entre pasos de simulación (ms)
int simLevel = 0;
int simDir = 1;                        // 1 = sube, -1 = baja
unsigned long lastSimMillis = 0;

// ========== VARIABLES GLOBALES ==========
int readingIndex = 0;                  // Índice para recorrer lecturas guardadas (0 = más reciente)
unsigned long lastDebounceSave = 0;
unsigned long lastDebounceRead = 0;
const unsigned long DEBOUNCE_MS = 50;
int prevSaveState = LOW;
int prevReadState = LOW;

// ========== CONSTANTES DE CALIBRACIÓN ==========
const int SENSOR_MIN = 700;            // Valor mínimo del sensor (calibrar según tu sensor)
const int SENSOR_MAX = 900;            // Valor máximo del sensor (calibrar según tu sensor)
const int MAX_STORED_READINGS = 3;     // Número máximo de lecturas almacenables (0..2)
const unsigned long HEATER_WARMUP_MS = 20000UL; // 20s de calentamiento inicial (mínimo recomendable)

// ========== EEPROM ADDRESSES ==========
const int ADDR_READING0 = 0; // más reciente
const int ADDR_READING1 = 1;
const int ADDR_READING2 = 2;

// ===================== FUNCIONES =====================
void setup() {
  // Serial.begin(9600); // Descomenta si quieres ver valores en Serial

  pinMode(LED_SAVE_INDICATOR, OUTPUT);

  // Botones: el usuario indicó resistencias pull-down externas -> usar INPUT
  pinMode(BUTTON_SAVE, INPUT);
  pinMode(BUTTON_READ, INPUT);

  // PINS de LEDs como salida
  for (int i = 0; i < LED_COUNT; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }

  // Indicar calentamiento del calefactor (heater) del MQ-3
  unsigned long start = millis();
  while (millis() - start < HEATER_WARMUP_MS) {
    // Parpadeo suave en indicador mientras calienta
    digitalWrite(LED_SAVE_INDICATOR, HIGH);
    delay(200);
    digitalWrite(LED_SAVE_INDICATOR, LOW);
    delay(300);
  }
  digitalWrite(LED_SAVE_INDICATOR, LOW);
}

// Guarda lectura en EEPROM (manteniendo las últimas 3 lecturas)
// value: 0..LED_COUNT
void guardarLectura(int value) {
  int lectura1 = EEPROM.read(ADDR_READING0);
  int lectura2 = EEPROM.read(ADDR_READING1);

  if (lectura1 == 255) lectura1 = 0;
  if (lectura2 == 255) lectura2 = 0;

  EEPROM.update(ADDR_READING2, lectura2);      // la más antigua pasa a posición 2
  EEPROM.update(ADDR_READING1, lectura1);      // antigua -> posición 1
  EEPROM.update(ADDR_READING0, value);         // nueva lectura en posición 0

  // Feedback visual corto
  digitalWrite(LED_SAVE_INDICATOR, HIGH);
  delay(400);
  digitalWrite(LED_SAVE_INDICATOR, LOW);
}

// Muestra el nivel dado (0..LED_COUNT) encendiendo los LEDs
void mostrarNivelEnLEDs(int nivel) {
  nivel = constrain(nivel, 0, LED_COUNT);
  for (int i = 0; i < LED_COUNT; i++) {
    if (i < nivel) digitalWrite(ledPins[i], HIGH);
    else digitalWrite(ledPins[i], LOW);
  }
}

// Mostrar lecturas guardadas: cada pulsación del botón READ muestra la siguiente lectura (0..2)
void mostrarLecturasGuardadas() {
  int localIndex = 0;
  while (true) {
    int raw = digitalRead(BUTTON_READ);

    // Anti-rebote: detectar flanco ascendente
    if (raw != prevReadState) {
      lastDebounceRead = millis();
      prevReadState = raw;
    }

    if ((millis() - lastDebounceRead) > DEBOUNCE_MS) {
      if (raw == HIGH) {
        if (localIndex < MAX_STORED_READINGS) {
          int addr = ADDR_READING0 + localIndex;
          int lecturaAlmacenada = EEPROM.read(addr);
          if (lecturaAlmacenada == 255) lecturaAlmacenada = 0; // tratar vacíos como 0

          mostrarNivelEnLEDs(lecturaAlmacenada);
          localIndex++;
          // Esperar a que suelte el botón para evitar lecturas repetidas
          while (digitalRead(BUTTON_READ) == HIGH) {
            delay(10);
          }
          delay(150); // pequeño retardo post-release para estabilidad
        } else {
          break; // ya mostramos las 3 lecturas
        }
      }
    }
  }

  // limpiar LEDs y resetear índice
  mostrarNivelEnLEDs(0);
  delay(200);
}

// Lee el sensor MQ-3 y mapea a 0..LED_COUNT
// Si SIMULATE_SENSOR == true, devuelve valores cíclicos para test
int leerNivelAlcohol() {
  if (SIMULATE_SENSOR) {
    unsigned long now = millis();
    if (now - lastSimMillis >= SIM_STEP_MS) {
      lastSimMillis = now;
      simLevel += simDir;
      if (simLevel >= LED_COUNT) {
        simLevel = LED_COUNT;
        simDir = -1;
      } else if (simLevel <= 0) {
        simLevel = 0;
        simDir = 1;
      }
    }
    return simLevel;
  } else {
    int raw = analogRead(ANALOG_SENSOR);
    int nivel = map(raw, SENSOR_MIN, SENSOR_MAX, 0, LED_COUNT);
    nivel = constrain(nivel, 0, LED_COUNT);
    return nivel;
  }
}

// ===================== LOOP PRINCIPAL =====================
void loop() {
  // 1) Leer sensor (o simulador) y mostrar en LEDs
  int nivelAlcohol = leerNivelAlcohol();
  mostrarNivelEnLEDs(nivelAlcohol);

  // 2) Manejo botón SAVE (detección de flanco ascendente con debounce)
  int saveRaw = digitalRead(BUTTON_SAVE);
  if (saveRaw != prevSaveState) {
    lastDebounceSave = millis();
    prevSaveState = saveRaw;
  }
  if ((millis() - lastDebounceSave) > DEBOUNCE_MS) {
    if (saveRaw == HIGH) {
      // Guardar la lectura actual
      guardarLectura(nivelAlcohol);
      // evitar múltiples guardados por rebote o pulsación larga
      while (digitalRead(BUTTON_SAVE) == HIGH) delay(10);
      delay(150);
    }
  }

  // 3) Manejo botón READ
  int readRaw = digitalRead(BUTTON_READ);
  if (readRaw != prevReadState) {
    lastDebounceRead = millis();
    prevReadState = readRaw;
  }
  if ((millis() - lastDebounceRead) > DEBOUNCE_MS) {
    if (readRaw == HIGH) {
      mostrarLecturasGuardadas();
      while (digitalRead(BUTTON_READ) == HIGH) delay(10);
      delay(150);
    }
  }

  // Pequeño delay para evitar uso excesivo de CPU
  delay(60);
}
