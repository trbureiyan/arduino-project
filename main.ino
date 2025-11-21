/*
 * ALCOHOLÍMETRO CON ARDUINO (actualizado para MQ-3 con 6 pines, usando 4 conexiones)
 *
 * Conexiones MQ-3 (según lo indicado):
 *  - H1 -> +5V
 *  - H2 -> GND
 *  - A1 -> A0 (señal analógica)
 *  - A2 -> 10k -> GND (crea el divisor/resistencia de referencia)
 *
 * Mejoras añadidas:
 *  - Tiempo de calentamiento al arrancar (HEATER_WARMUP_MS)
 *  - Anti-rebote para botones (detección de flancos)
 *  - Uso de EEPROM.update para reducir escrituras innecesarias
 *  - Manejo de bytes EEPROM no inicializados (valor 255)


#include <EEPROM.h>

// ========== CONFIGURACIÓN DE PINES ==========
const int ANALOG_SENSOR = A0;          // Pin del sensor analógico (MQ-3 AOUT)
const int LED_SAVE_INDICATOR = 11;     // LED que indica cuando se está guardando / actividad
const int BUTTON_SAVE = 12;            // Botón para guardar lectura actual (con pull-down externa)
const int BUTTON_READ = 13;            // Botón para mostrar lecturas guardadas (con pull-down externa)

// ========== CONFIGURACIÓN DE LEDS ==========
const int LED_COUNT = 8;               // Número de LEDs en la barra
int ledPins[] = {3, 4, 5, 6, 7, 8, 9, 10};  // Pines de los LEDs de nivel (3-4 rojo, 5-7 amarillo, 8-10 verde)

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
  // Serial.begin(9600); // Descomenta para debug por Serial

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
  // Leer valores existentes (tratar 255 como vacío -> mapear a 0)
  int lectura1 = EEPROM.read(ADDR_READING0);
  int lectura2 = EEPROM.read(ADDR_READING1);

  if (lectura1 == 255) lectura1 = 0;
  if (lectura2 == 255) lectura2 = 0;

  // Desplazar y escribir usando update para reducir desgaste
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
// Utiliza anti-rebote simple por flanco
void mostrarLecturasGuardadas() {
  int localIndex = 0;
  // Si ninguna lectura existe (255) se muestra 0
  while (true) {
    int raw = digitalRead(BUTTON_READ);

    // Anti-rebote: detectar flanco ascendente
    if (raw != prevReadState) {
      lastDebounceRead = millis();
      prevReadState = raw;
    }

    if ((millis() - lastDebounceRead) > DEBOUNCE_MS) {
      // flanco ascendente detectado
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
int leerNivelAlcohol() {
  int raw = analogRead(ANALOG_SENSOR);
  // Serial.println(raw);
  int nivel = map(raw, SENSOR_MIN, SENSOR_MAX, 0, LED_COUNT);
  nivel = constrain(nivel, 0, LED_COUNT);
  return nivel;
}

// ===================== LOOP PRINCIPAL =====================
void loop() {
  // 1) Leer sensor y mostrar en LEDs
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

  // 3) Manejo botón READ: al flanco ascendente entramos en modo mostrarLecturasGuardadas()
  int readRaw = digitalRead(BUTTON_READ);
  if (readRaw != prevReadState) {
    lastDebounceRead = millis();
    prevReadState = readRaw;
  }
  if ((millis() - lastDebounceRead) > DEBOUNCE_MS) {
    if (readRaw == HIGH) {
      mostrarLecturasGuardadas();
      // esperar a que suelte el botón para continuar
      while (digitalRead(BUTTON_READ) == HIGH) delay(10);
      delay(150);
    }
  }

  // Pequeño delay para evitar uso excesivo de CPU
  delay(60);
}
*/

/*
 * ALCOHOLÍMETRO ARDUINO + BLUETOOTH (App Inventor)
 * * COMANDOS RECIBIDOS DESDE APP:
 * 'I' -> Iniciar: Activa lectura y transmisión de datos.
 * 'X' -> Detener: Detiene transmisión y apaga LEDs.
 * 'S' -> Guardar: Guarda la lectura actual en EEPROM.
 * * COMANDOS ENVIADOS A APP:
 * Número (0-8) seguido de salto de línea.
 */

#include <EEPROM.h>

// ========== CONFIGURACIÓN DE PINES ==========
const int ANALOG_SENSOR = A0;          
const int LED_SAVE_INDICATOR = 13;     

// Botones Físicos (Backup manual)
const int BUTTON_SAVE = 2;             
const int BUTTON_READ = 3;             

// ========== CONFIGURACIÓN DE LEDS ==========
const int LED_COUNT = 8;               
int ledPins[] = {5, 6, 7, 8, 9, 10, 11, 12}; 

// ========== VARIABLES GLOBALES ==========
// Control de Bluetooth
bool sistemaActivo = false;    // Controla si enviamos datos o no
int nivelActual = 0;           // Almacena el último nivel leído
char comandoBT = ' ';          // Variable para guardar comando recibido

// Variables originales
unsigned long lastDebounceSave = 0;
unsigned long lastDebounceRead = 0;
const unsigned long DEBOUNCE_MS = 50;
int prevSaveState = LOW;
int prevReadState = LOW;

// Configuración Sensor
const bool SIMULATE_SENSOR = true;    // ¡PONER EN FALSE PARA USO REAL!
const unsigned long SIM_STEP_MS = 500; 
int simLevel = 0;
int simDir = 1;                       
unsigned long lastSimMillis = 0;
const int SENSOR_MIN = 700;            
const int SENSOR_MAX = 900;            
const int MAX_STORED_READINGS = 3;     
const unsigned long HEATER_WARMUP_MS = 20000UL; 

// EEPROM
const int ADDR_READING0 = 0; 
const int ADDR_READING1 = 1;
const int ADDR_READING2 = 2;

void setup() {
  // INICIAMOS PUERTO SERIE (Para Bluetooth HC-05)
  // El HC-05 suele venir por defecto a 9600 baudios.
  Serial.begin(9600); 

  pinMode(LED_SAVE_INDICATOR, OUTPUT);
  pinMode(BUTTON_SAVE, INPUT);
  pinMode(BUTTON_READ, INPUT);

  for (int i = 0; i < LED_COUNT; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }

  // Calentamiento (Solo visualización en LED 13, no bloquea Serial pero espera)
  unsigned long start = millis();
  while (millis() - start < HEATER_WARMUP_MS) {
    digitalWrite(LED_SAVE_INDICATOR, HIGH);
    delay(200);
    digitalWrite(LED_SAVE_INDICATOR, LOW);
    delay(300);
  }
  digitalWrite(LED_SAVE_INDICATOR, LOW);
}

// --- Función para guardar en EEPROM ---
void guardarLectura(int value) {
  int lectura1 = EEPROM.read(ADDR_READING0);
  int lectura2 = EEPROM.read(ADDR_READING1);
  if (lectura1 == 255) lectura1 = 0;
  if (lectura2 == 255) lectura2 = 0;

  EEPROM.update(ADDR_READING2, lectura2);      
  EEPROM.update(ADDR_READING1, lectura1);      
  EEPROM.update(ADDR_READING0, value);         

  // Feedback visual
  for(int i=0; i<3; i++){
    digitalWrite(LED_SAVE_INDICATOR, HIGH);
    delay(100);
    digitalWrite(LED_SAVE_INDICATOR, LOW);
    delay(100);
  }
}

// --- Función para encender barra LEDs ---
void mostrarNivelEnLEDs(int nivel) {
  nivel = constrain(nivel, 0, LED_COUNT);
  for (int i = 0; i < LED_COUNT; i++) {
    if (i < nivel) digitalWrite(ledPins[i], HIGH);
    else digitalWrite(ledPins[i], LOW);
  }
}

// --- Función Lectura Sensor ---
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

void loop() {
  // ==========================================
  // 1. ESCUCHAR COMANDOS BLUETOOTH (App Inventor)
  // ==========================================
  if (Serial.available() > 0) {
    comandoBT = Serial.read();

    // Interpretar comandos según tus bloques
    if (comandoBT == 'I') {         // Bloque btnIniciarPrueba
      sistemaActivo = true;
    } 
    else if (comandoBT == 'X') {    // Bloque btnDetener
      sistemaActivo = false;
      mostrarNivelEnLEDs(0);        // Apagar LEDs
    } 
    else if (comandoBT == 'S') {    // Bloque btnGuardarMedicion
      guardarLectura(nivelActual);
      // Opcional: Enviar confirmación de vuelta a la App si se desea
      // Serial.println("OK_GUARDADO"); 
    }
  }

  // ==========================================
  // 2. LÓGICA PRINCIPAL
  // ==========================================
  
  // Solo leemos y actualizamos si el sistema está activo (comando 'I')
  // O si queremos que funcione siempre en modo local, quitamos el "if".
  // Aquí asumimos que 'I' activa la transmisión.
  
  if (sistemaActivo) {
    nivelActual = leerNivelAlcohol();
    mostrarNivelEnLEDs(nivelActual);
    
    // ENVIAR DATO A LA APP
    // Usamos println para enviar el número y un salto de línea.
    // El Clock de la App lo recibirá.
    Serial.println(nivelActual);
    
    // Delay para no saturar el Bluetooth (La App suele leer cada 500ms-1s)
    delay(200); 
  } else {
    // Si no está activo, mantenemos LEDs apagados o en reposo
    // (Ya se apagaron en el comando 'X')
  }

  // ==========================================
  // 3. BOTONES FÍSICOS (Respaldo)
  // ==========================================
  
  // Botón Guardar Físico
  int saveRaw = digitalRead(BUTTON_SAVE);
  if (saveRaw != prevSaveState) {
    lastDebounceSave = millis();
    prevSaveState = saveRaw;
  }
  if ((millis() - lastDebounceSave) > DEBOUNCE_MS && saveRaw == HIGH) {
      guardarLectura(nivelActual); // Guarda lo que esté midiendo en ese momento
      while (digitalRead(BUTTON_SAVE) == HIGH) delay(10);
  }

  // Botón Leer (Historial) Físico
  // Nota: Mostrar historial interrumpe el Bluetooth momentáneamente
  int readRaw = digitalRead(BUTTON_READ);
  if (readRaw != prevReadState) {
    lastDebounceRead = millis();
    prevReadState = readRaw;
  }
  if ((millis() - lastDebounceRead) > DEBOUNCE_MS && readRaw == HIGH) {
      // Modo visualización local
      // (Podrías implementar enviar historial por BT si quisieras en el futuro)
      // Por ahora solo muestra en LEDs físicos
      mostrarLecturasGuardadas(); 
      while (digitalRead(BUTTON_READ) == HIGH) delay(10);
  }
}

// Función auxiliar para mostrar historial (la misma del código anterior corregido)
void mostrarLecturasGuardadas() {
  int localIndex = 0;
  unsigned long lastInteraction = millis();
  mostrarNivelEnLEDs(0);
  delay(300);

  while (true) {
    if (millis() - lastInteraction > 5000) break; // Timeout 5s

    if (digitalRead(BUTTON_READ) == HIGH) {
        lastInteraction = millis();
        if (localIndex < MAX_STORED_READINGS) {
            int val = EEPROM.read(ADDR_READING0 + localIndex);
            if (val == 255) val = 0;
            mostrarNivelEnLEDs(val);
            localIndex++;
            while(digitalRead(BUTTON_READ) == HIGH) delay(10);
            delay(200);
        } else {
            break;
        }
    }
  }
  mostrarNivelEnLEDs(0);
}
