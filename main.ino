/*
 * PROYECTO: ALCOHOLÍMETRO IOT - VERSIÓN PRODUCCIÓN FINAL
 * AUTORES: Brayan Toro Bustos & Pablo Trujillo Artunduaga
 * UNIVERSIDAD SURCOLOMBIANA - Curso de programacion fisica
 *
 * CARACTERÍSTICAS:
 * - Autocalibración de aire limpio al inicio.
 * - Filtro de promedio móvil para lecturas estables.
 * - Comunicación SoftwareSerial robusta en pines 2 y 4.
 * - Protocolo optimizado para App Inventor (Delimiter 10).
 *
 * MAPEO DE HARDWARE:
 * - Arduino UNO
 * - Sensor MQ-3: A0
 * - Bluetooth HC-05/IS-05: TX -> Pin 2, RX -> Pin 4
 * - Botones: Pin 11 (Guardar), Pin 12 (Leer/Historial) [Ajustar según tu diagrama final]
 * - LEDs: Pines 3, 5, 6, 7, 8, 9, 10, 13 (Ajustar según tu orden físico real)
 */

#include <SoftwareSerial.h>
#include <EEPROM.h>

// ================= CONFIGURACIÓN DE PINES =================
const int PIN_RX_BT = 2;    // Conectar TX del Bluetooth aquí
const int PIN_TX_BT = 4;    // Conectar RX del Bluetooth aquí
const int PIN_SENSOR = A0;
const int PIN_LED_TESTIGO = 13; // Usado para feedback y como último LED rojo

// ARRAY DE LEDS: ORDENAR DESDE EL VERDE (BAJO) HASTA EL ROJO (ALTO)
// IMPORTANTE: Pon aquí tus pines en orden físico exacto.
// Ejemplo basado en tu diagrama: Verdes -> Amarillos -> Rojos
int ledPins[] = {5, 6, 7, 8, 9, 10, 11, 12}; 
const int LED_COUNT = 8;

// ================= OBJETOS Y VARIABLES =================
SoftwareSerial BTSerial(PIN_RX_BT, PIN_TX_BT);

// Variables de Control
bool pruebaActiva = false;    
unsigned long lastUpdate = 0;
const int INTERVALO_ENVIO = 500; // 500ms para estabilidad de App

// Variables de Calibración
int baseAireLimpio = 0;       // Valor de referencia inicial
int umbralAlcohol = 0;        // Valor actual leído
const int RANGO_DETECCION = 300; // Cuánto debe subir el valor para ser "máximo"

void setup() {
  Serial.begin(9600);     // Para Monitor Serie (Debug PC)
  BTSerial.begin(9600);   // Para App Inventor

  // Configurar Pines
  pinMode(PIN_LED_TESTIGO, OUTPUT);
  for (int i = 0; i < LED_COUNT; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW); // Iniciar apagados
  }

  Serial.println("=== INICIANDO SISTEMA ALCOHOLIMETRO IOT ===");
  
  // --- FASE 1: PRECALENTAMIENTO Y CALIBRACIÓN ---
  // Muestra una animación en los LEDs mientras calibra
  Serial.print("Calibrando sensor (No soplar)...");
  long acumulado = 0;
  for(int i=0; i<20; i++) {
    acumulado += analogRead(PIN_SENSOR);
    
    // Animación de carga en LEDs
    digitalWrite(ledPins[i % LED_COUNT], HIGH);
    delay(100);
    digitalWrite(ledPins[i % LED_COUNT], LOW);
  }
  
  // Fijamos la base: El promedio de las 20 lecturas iniciales
  baseAireLimpio = acumulado / 20;
  Serial.print(" BASE FIJADA EN: ");
  Serial.println(baseAireLimpio);
  Serial.println("SISTEMA LISTO. Esperando comando de App...");
  
  // Parpadeo final de listo
  digitalWrite(PIN_LED_TESTIGO, HIGH); delay(200); digitalWrite(PIN_LED_TESTIGO, LOW);
}

void loop() {
  // 1. ESCUCHA COMANDOS DE APP O PC
  verificarComandosEntrantes();

  // 2. LÓGICA PRINCIPAL DE MEDICIÓN
  if (pruebaActiva && (millis() - lastUpdate > INTERVALO_ENVIO)) {
    lastUpdate = millis();

    // A) LEER CON FILTRO PROMEDIO (Suaviza la señal)
    int lecturaRaw = leerPromedioSensor(10); 
    
    // B) CALCULAR NIVEL RELATIVO
    // Restamos la base. Si da negativo, es 0.
    int delta = lecturaRaw - baseAireLimpio;
    if (delta < 0) delta = 0;

    // C) MAPEO INTELIGENTE A 8 LEDS
    // Mapeamos el 'delta' (incremento) de 0 a RANGO_DETECCION hacia 0 a 8 LEDs
    // Si delta es 0, nivel es 0. Si delta es 300, nivel es 8.
    int nivelLed = map(delta, 20, RANGO_DETECCION, 0, LED_COUNT);
    nivelLed = constrain(nivelLed, 0, LED_COUNT);

    // D) VISUALIZACIÓN
    mostrarBarraLeds(nivelLed);

    // E) ENVÍO DE DATOS
    enviarDatosApp(nivelLed);
    
    // F) DEBUG EN PC (Para que tú veas qué pasa)
    Serial.print("Base: "); Serial.print(baseAireLimpio);
    Serial.print(" | Actual: "); Serial.print(lecturaRaw);
    Serial.print(" | Delta: "); Serial.print(delta);
    Serial.print(" | Nivel Env: "); Serial.println(nivelLed);
  }
}

// ================= FUNCIONES AUXILIARES =================

// Lee el sensor N veces y devuelve el promedio
int leerPromedioSensor(int muestras) {
  long suma = 0;
  for(int i=0; i<muestras; i++) {
    suma += analogRead(PIN_SENSOR);
    delay(5); // Pequeña pausa entre lecturas
  }
  return (int)(suma / muestras);
}

// Controla el encendido progresivo de la barra
void mostrarBarraLeds(int nivel) {
  // Apagar todos primero (limpieza)
  for (int i = 0; i < LED_COUNT; i++) {
    digitalWrite(ledPins[i], LOW);
  }

  // Encender hasta el nivel actual
  // Si nivel es 0, no enciende ninguno. 
  // Si quieres que el primero siempre prenda, cambia "i < nivel" a "i <= nivel"
  for (int i = 0; i < nivel; i++) {
    digitalWrite(ledPins[i], HIGH);
  }
}

void enviarDatosApp(int valor) {
  // Enviar número limpio + Salto de línea (CRUCIAL para App Inventor)
  BTSerial.println(valor);
}

void verificarComandosEntrantes() {
  // --- REVISAR BLUETOOTH ---
  if (BTSerial.available()) {
    char cmd = BTSerial.read();
    procesarComando(cmd, "BT");
  }
  
  // --- REVISAR PC (Respaldo) ---
  if (Serial.available()) {
    char cmd = Serial.read();
    // Ignorar saltos de línea del monitor serie
    if(cmd != '\n' && cmd != '\r') procesarComando(cmd, "PC");
  }
}

void procesarComando(char cmd, String fuente) {
  Serial.print("Comando recibido de " + fuente + ": ");
  Serial.println(cmd);

  if (cmd == 'I' || cmd == 'i' || cmd == '1') {
    pruebaActiva = true;
    Serial.println("-> INICIANDO PRUEBA");
    // Resetear base por si acaso el ambiente cambió
    baseAireLimpio = leerPromedioSensor(10);
  } 
  else if (cmd == 'X' || cmd == 'x' || cmd == '2') {
    pruebaActiva = false;
    mostrarBarraLeds(0); // Apagar todo
    Serial.println("-> PRUEBA DETENIDA");
  }
  else if (cmd == 'S' || cmd == 's') {
    Serial.println("-> GUARDANDO DATOS (Simulado)");
    // Aquí iría tu lógica EEPROM si la necesitas
    for(int k=0; k<3; k++) { // Parpadeo de confirmación
       digitalWrite(PIN_LED_TESTIGO, HIGH); delay(50);
       digitalWrite(PIN_LED_TESTIGO, LOW); delay(50);
    }
  }
}
