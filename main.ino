/*
 * ALCOHOLÍMETRO: SISTEMA DE DIAGNÓSTICO Y PRODUCCIÓN
 * * CARACTERÍSTICAS:
 * 1. Debug en tiempo real por Monitor Serie (PC).
 * 2. Feedback visual en LED 13 al recibir CUALQUIER dato Bluetooth.
 * 3. Comandos por PC: 
 * - 'd': Activa modo DEBUG (muestra valores crudos del sensor constantemente).
 * - 'q': Quita modo DEBUG (silencio).
 * 4. Comandos por App:
 * - 'I': Iniciar prueba.
 * - 'X': Detener.
 * - 'S': Guardar.
 */

#include <SoftwareSerial.h>
#include <EEPROM.h>

// --- PINES ---
const int PIN_RX_BT = 0;  // Conectar al TX del HC-05
const int PIN_TX_BT = 1;  // Conectar al RX del HC-05
const int PIN_SENSOR = A0;
const int PIN_LED_TESTIGO = 13; // Parpadeara al recibir datos
// Pines de tus LEDs del semáforo
int ledPins[] = {5, 6, 7, 8, 9, 10, 11, 12}; 
const int LED_COUNT = 8;

// --- OBJETOS ---
SoftwareSerial BTSerial(PIN_RX_BT, PIN_TX_BT);

// --- VARIABLES DE ESTADO ---
bool modoDebug = false;       // Si es true, imprime valores del sensor al PC sin parar
bool pruebaActiva = false;    // Estado lógico de la app (midiendo o no)
unsigned long lastSensorRead = 0;

// --- CALIBRACIÓN ---
const int SENSOR_MIN = 120;   // Ajustar viendo el modo Debug
const int SENSOR_MAX = 600;

void setup() {
  // 1. Iniciar Comunicación PC
  Serial.begin(9600);
  Serial.println("\n=== INICIANDO SISTEMA DE DIAGNOSTICO ===");
  Serial.println("Escribe 'd' y presiona ENTER para probar el sensor.");
  Serial.println("Esperando conexion Bluetooth...");

  // 2. Iniciar Comunicación Bluetooth
  BTSerial.begin(9600); 

  // 3. Configurar Pines
  pinMode(PIN_LED_TESTIGO, OUTPUT);
  for (int i = 0; i < LED_COUNT; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }

  // 4. Calentamiento Rápido (Feedback visual)
  Serial.print("Calentando sensor (Espera 5s)...");
  for(int i=0; i<5; i++){
    digitalWrite(PIN_LED_TESTIGO, HIGH); delay(100);
    digitalWrite(PIN_LED_TESTIGO, LOW); delay(900);
    Serial.print(".");
  }
  Serial.println(" LISTO.");
}

void loop() {
  // ==========================================
  // A. ESCUCHAR AL PC (MONITOR SERIE)
  // ==========================================
  if (Serial.available()) {
    char cmdPC = Serial.read();
    if (cmdPC == 'd') {
      modoDebug = !modoDebug; // Alternar
      Serial.print("\n[PC] Modo Debug: ");
      Serial.println(modoDebug ? "ACTIVADO" : "DESACTIVADO");
    }
  }

  // ==========================================
  // B. ESCUCHAR AL BLUETOOTH (APP)
  // ==========================================
  if (BTSerial.available()) {
    char cmdBT = BTSerial.read();
    
    // 1. Feedback Visual Inmediato (Si parpadea, la conexión es buena)
    digitalWrite(PIN_LED_TESTIGO, HIGH);
    delay(50);
    digitalWrite(PIN_LED_TESTIGO, LOW);

    // 2. Feedback al PC (¿Qué diablos llegó?)
    Serial.print("[BT RECIBIDO]: '");
    Serial.print(cmdBT);
    Serial.print("' (ASCII: ");
    Serial.print((int)cmdBT);
    Serial.println(")");

    // 3. Lógica de Control
    if (cmdBT == 'I' || cmdBT == 'i') {
      pruebaActiva = true;
      Serial.println("-> COMANDO INICIAR ACEPTADO");
    } 
    else if (cmdBT == 'X' || cmdBT == 'x') {
      pruebaActiva = false;
      apagarLeds();
      Serial.println("-> COMANDO DETENER");
    }
    else if (cmdBT == 'S' || cmdBT == 's') {
       // Aquí iría tu lógica de guardar
       Serial.println("-> COMANDO GUARDAR");
    }
  }

  // ==========================================
  // C. RUTINA DE LECTURA (NO BLOQUEANTE)
  // ==========================================
  // Leemos cada 500ms para no saturar
  if (millis() - lastSensorRead > 500) {
    lastSensorRead = millis();
    
    int valorCrudo = analogRead(PIN_SENSOR);
    int nivelLed = map(valorCrudo, SENSOR_MIN, SENSOR_MAX, 0, LED_COUNT);
    nivelLed = constrain(nivelLed, 0, LED_COUNT);

    // 1. Salida Debug (Solo al PC)
    if (modoDebug) {
      Serial.print("[SENSOR DEBUG] RAW: ");
      Serial.print(valorCrudo);
      Serial.print(" | Nivel Mapeado: ");
      Serial.println(nivelLed);
    }

    // 2. Salida a la App (Solo si la prueba está activa)
    if (pruebaActiva) {
      mostrarNivelEnLEDs(nivelLed);
      BTSerial.println(nivelLed); // Enviar a App Inventor
      
      // Monitorizar en PC también que se está enviando
      Serial.print(">> Enviando a App: ");
      Serial.println(nivelLed);
    }
  }
}

// --- AUXILIAR ---
void mostrarNivelEnLEDs(int nivel) {
  for (int i = 0; i < LED_COUNT; i++) {
    if (i < nivel) digitalWrite(ledPins[i], HIGH);
    else digitalWrite(ledPins[i], LOW);
  }
}

void apagarLeds() {
  mostrarNivelEnLEDs(0);
}
