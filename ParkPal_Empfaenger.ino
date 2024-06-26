#include <SPI.h>
#include "RF24.h"         // RF24 Library für NRF24L01 Funkmodul
#include <Wire.h>
#include <U8g2lib.h>      // U8g2 Library für OLED Display

/* Hardware Konfiguration: Setze nRF24L01 Funkmodul auf SPI Bus mit Pins 9 & 10 */
RF24 radio(9, 10);         // Erstelle ein RF24 Objekt mit den Pins 9 (CE) und 10 (CSN)

byte node_A_address[6] = "NodeA";   // Adresse des eigenen Knotens
byte node_B_address[6] = "NodeB";   // Adresse des anderen Knotens

unsigned long lastDataReceivedTime = 0; // Zeitpunkt des letzten Dateneingangs
unsigned long heartbeatInterval = 1000; // Intervall für Heartbeat (alle 3 Sekunden)
unsigned long connectionTimeout = 1000; // Timeout für die Verbindung (3 Sekunden)
bool radioConnected = true;            // Status der Funkverbindung
const int dist_step_01 = 80;      // Entfernung für den ersten Balken(von 4 Balken pro Sensor)
const int dist_step_02 = 50;      // Entfernung für den zweiten Balken
const int dist_step_03 = 25;      // Entfernung für den dritten Balken
const int dist_step_04 = 10;      // Entfernung für den vierten Balken
int sensor_distances[3];    // Array für die Sensorwerte
int NoFunkSignalCounter = 0;    // Zähler für fehlende Funkverbindung

U8G2_SH1106_128X64_NONAME_1_HW_I2C OLED(U8G2_R0); // OLED Display Objekt

// BuzzerController Klasse für die Steuerung des Buzzers
class BuzzerController {
private:
    int buzzerPin;

public:
    BuzzerController(int pin) {      // Konstruktor, initialisiert den Buzzer Pin
        buzzerPin = pin;
        pinMode(buzzerPin, OUTPUT);
    }

    void controlBuzzer(int distance1, int distance2, int distance3) {
        // Steuert den Buzzer basierend auf den Entfernungen zu Hindernissen
        int minDistance = min(min(distance1, distance2), distance3);

        if (minDistance > 80) {
            noTone(buzzerPin);   // Buzzer ausschalten, wenn Entfernung > 80 cm
        } else if (minDistance <= 10) {
            tone(buzzerPin, 440); // Kontinuierlicher Ton für Entfernung <= 10 cm
        } else {
            int duration;
            if (minDistance <= 30) {
                duration = map(minDistance, 10, 30, 50, 100); // Dauer für Entfernungen <= 30 cm
            } else {
                duration = map(minDistance, 31, 80, 100, 300); // Dauer für Entfernungen > 30 cm
            }
            int frequency = map(minDistance, 10, 80, 470, 430); // Frequenz abhängig von Entfernung

            tone(buzzerPin, frequency);
            delay(duration);
            noTone(buzzerPin);
            delay(duration);
        }
    }
    void turnOffBuzzer(){
       noTone(buzzerPin); 
    }
    
};

BuzzerController buzzer(8); // Buzzer auf Pin 8 initialisieren

// 'Ausrufezeichen', 1x6px
const unsigned char bitmap_Ausrufezeichen [] PROGMEM = {
	0x03, 0x01, 0x01, 0x00, 0x01
};

// 'Verbindung 0 bar', 12x8px
const unsigned char bitmap_Verbindung_0_bar [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x60, 0x00
};
// 'Verbindung 1 bar', 12x8px
const unsigned char bitmap_Verbindung_1_bar [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x08, 0x01, 0x60, 0x00, 0x60, 0x00
};
// 'Verbindung 2 bars', 12x8px
const unsigned char bitmap_Verbindung_2_bar [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0xf8, 0x01, 0x04, 0x02, 0xf2, 0x04, 0x08, 0x01, 0x60, 0x00, 0x60, 0x00
};
// 'Verbindung 3 bars', 12x8px
const unsigned char bitmap_Verbindung_3_bar [] PROGMEM = {
	0xfc, 0x03, 0x02, 0x04, 0xf9, 0x09, 0x04, 0x02, 0xf2, 0x04, 0x08, 0x01, 0x60, 0x00, 0x60, 0x00
};

// 'Auto', 71x5px
const unsigned char bitmap_Auto [] PROGMEM = {
	0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x7f, 0xaa, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xbf, 0x2a, 0xaa, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0, 0x2a, 0xaa, 0x02, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0xa0, 0x2a, 0xfc, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x1f
};
// 'ParkPal Logo', 17x14px
const unsigned char bitmap_ParkPal_Logo [] PROGMEM = {
	0x00, 0x00, 0x00, 0x0e, 0x20, 0x00, 0xca, 0xae, 0x00, 0xae, 0x62, 0x00, 0xe2, 0xa2, 0x00, 0x00, 
	0x00, 0x00, 0x0e, 0xa2, 0x00, 0xca, 0xf2, 0x01, 0xae, 0xf2, 0x01, 0xe2, 0xe4, 0x00, 0x00, 0x40, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// '1 1 Leer', 25x7px
const unsigned char bitmap_1_1_leer [] PROGMEM = {
	0xfe, 0xff, 0xff, 0x01, 0x02, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00, 0x01, 
	0x03, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0x01
};
// '1 1 Voll', 25x7px
const unsigned char bitmap_1_1_voll [] PROGMEM = {
	0xfe, 0xff, 0xff, 0x01, 0xfe, 0xff, 0xff, 0x01, 0xfe, 0xff, 0xff, 0x01, 0xfe, 0xff, 0xff, 0x01, 
	0xff, 0xff, 0xff, 0x01, 0xff, 0xff, 0xff, 0x01, 0xff, 0xff, 0xff, 0x01
};

// '1 2 leer', 24x7px
const unsigned char bitmap_1_2_leer [] PROGMEM = {
	0xff, 0xff, 0xff, 0x01, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 
	0x00, 0x80, 0xff, 0xff, 0xff
};
// '1 2 voll', 24x7px
const unsigned char bitmap_1_2_voll [] PROGMEM = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff
};
// '1 3 leer', 25x7px
const unsigned char bitmap_1_3_leer [] PROGMEM = {
	0xff, 0xff, 0xff, 0x00, 0x01, 0x00, 0x80, 0x00, 0x01, 0x00, 0x80, 0x00, 0x01, 0x00, 0x80, 0x00, 
	0x01, 0x00, 0x80, 0x01, 0x01, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0x01
};
// '1 3 voll', 25x7px
const unsigned char bitmap_1_3_voll [] PROGMEM = {
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 
	0xff, 0xff, 0xff, 0x01, 0xff, 0xff, 0xff, 0x01, 0xff, 0xff, 0xff, 0x01
};
// '2 1 leer', 26x7px
const unsigned char bitmap_2_1_leer [] PROGMEM = {
	0xfe, 0xff, 0xff, 0x03, 0x02, 0x00, 0x00, 0x02, 0x03, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x02, 
	0x01, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x02, 0xff, 0xff, 0xff, 0x03
};
// '2 1 voll', 26x7px
const unsigned char bitmap_2_1_voll [] PROGMEM = {
	0xfe, 0xff, 0xff, 0x03, 0xfe, 0xff, 0xff, 0x03, 0xff, 0xff, 0xff, 0x03, 0xff, 0xff, 0xff, 0x03, 
	0xff, 0xff, 0xff, 0x03, 0xff, 0xff, 0xff, 0x03, 0xff, 0xff, 0xff, 0x03
};
// '2 2 leer', 24x8px
const unsigned char bitmap_2_2_leer [] PROGMEM = {
	0xff, 0xff, 0xff, 0x01, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 
	0x00, 0x80, 0x7f, 0x00, 0xfe, 0xc0, 0xff, 0x03
};
// '2 2 voll', 24x8px
const unsigned char bitmap_2_2_voll [] PROGMEM = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xff, 0x03
};
// '2 3 leer', 26x7px
const unsigned char bitmap_2_3_leer [] PROGMEM = {
	0xff, 0xff, 0xff, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x03, 0x01, 0x00, 0x00, 0x02, 
	0x01, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x02, 0xff, 0xff, 0xff, 0x03
};
// '2 3 voll', 26x7px
const unsigned char bitmap_2_3_voll [] PROGMEM = {
	0xff, 0xff, 0xff, 0x01, 0xff, 0xff, 0xff, 0x01, 0xff, 0xff, 0xff, 0x03, 0xff, 0xff, 0xff, 0x03, 
	0xff, 0xff, 0xff, 0x03, 0xff, 0xff, 0xff, 0x03, 0xff, 0xff, 0xff, 0x03
};
// '3 1 leer', 29x9px
const unsigned char bitmap_3_1_leer [] PROGMEM = {
	0xfc, 0xff, 0x00, 0x00, 0x04, 0x80, 0xff, 0x1f, 0x06, 0x00, 0x00, 0x18, 0x06, 0x00, 0x00, 0x08, 
	0x02, 0x00, 0x00, 0x08, 0x03, 0x00, 0x00, 0x08, 0x0f, 0x00, 0x00, 0x08, 0xf8, 0xff, 0x1f, 0x08, 
	0x00, 0x00, 0xf0, 0x0f
};
// '3 1 voll', 29x9px
const unsigned char bitmap_3_1_voll [] PROGMEM = {
	0xfc, 0xff, 0x00, 0x00, 0xfc, 0xff, 0xff, 0x1f, 0xfe, 0xff, 0xff, 0x1f, 0xfe, 0xff, 0xff, 0x0f, 
	0xfe, 0xff, 0xff, 0x0f, 0xff, 0xff, 0xff, 0x0f, 0xff, 0xff, 0xff, 0x0f, 0xf8, 0xff, 0xff, 0x0f, 
	0x00, 0x00, 0xf0, 0x0f
};
// '3 2 leer', 24x8px
const unsigned char bitmap_3_2_leer [] PROGMEM = {
	0xff, 0xff, 0xff, 0x01, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 
	0x00, 0x80, 0x01, 0x00, 0x80, 0xff, 0xff, 0xff
};
// '3 2 voll', 24x8px
const unsigned char bitmap_3_2_voll [] PROGMEM = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};
// '3 3 leer', 29x9px
const unsigned char bitmap_3_3_leer [] PROGMEM = {
	0x00, 0xe0, 0xff, 0x07, 0xff, 0x3f, 0x00, 0x04, 0x03, 0x00, 0x00, 0x0c, 0x02, 0x00, 0x00, 0x08, 
	0x02, 0x00, 0x00, 0x08, 0x02, 0x00, 0x00, 0x18, 0x02, 0x00, 0x00, 0x1e, 0x02, 0xff, 0xff, 0x03, 
	0xfe, 0x01, 0x00, 0x00
};
// '3 3 voll', 29x9px
const unsigned char bitmap_3_3_voll [] PROGMEM = {
	0x00, 0xe0, 0xff, 0x07, 0xff, 0xff, 0xff, 0x07, 0xff, 0xff, 0xff, 0x0f, 0xfe, 0xff, 0xff, 0x0f, 
	0xfe, 0xff, 0xff, 0x0f, 0xfe, 0xff, 0xff, 0x1f, 0xfe, 0xff, 0xff, 0x1f, 0xfe, 0xff, 0xff, 0x03, 
	0xfe, 0x01, 0x00, 0x00
};
// '4 1 leer', 35x10px
const unsigned char bitmap_4_1_leer [] PROGMEM = {
	0xe0, 0xff, 0x07, 0x00, 0x00, 0x30, 0x00, 0xfc, 0xff, 0x01, 0x18, 0x00, 0x00, 0x00, 0x07, 0x08, 
	0x00, 0x00, 0x00, 0x04, 0x0c, 0x00, 0x00, 0x00, 0x06, 0x06, 0x00, 0x00, 0x00, 0x02, 0x3f, 0x00, 
	0x00, 0x00, 0x02, 0xe0, 0x7f, 0x00, 0x00, 0x02, 0x00, 0xc0, 0xff, 0x00, 0x02, 0x00, 0x00, 0x80, 
	0xff, 0x03
};
// '4 1 voll', 35x10px
const unsigned char bitmap_4_1_voll [] PROGMEM = {
	0xe0, 0xff, 0x07, 0x00, 0x00, 0xf0, 0xff, 0xff, 0xff, 0x01, 0xf8, 0xff, 0xff, 0xff, 0x07, 0xf8, 
	0xff, 0xff, 0xff, 0x07, 0xfc, 0xff, 0xff, 0xff, 0x07, 0xfe, 0xff, 0xff, 0xff, 0x03, 0xff, 0xff, 
	0xff, 0xff, 0x03, 0xe0, 0xff, 0xff, 0xff, 0x03, 0x00, 0xc0, 0xff, 0xff, 0x03, 0x00, 0x00, 0x80, 
	0xff, 0x03
};
// '4 2 leer', 26x10px
const unsigned char bitmap_4_2_leer [] PROGMEM = {
	0xff, 0xff, 0xff, 0x03, 0x01, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x02, 
	0x01, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x02, 
	0xff, 0x03, 0xff, 0x03, 0x00, 0xfe, 0x01, 0x00
};
// '4 2 voll', 26x10px
const unsigned char bitmap_4_2_voll [] PROGMEM = {
	0xff, 0xff, 0xff, 0x03, 0xff, 0xff, 0xff, 0x03, 0xff, 0xff, 0xff, 0x03, 0xff, 0xff, 0xff, 0x03, 
	0xff, 0xff, 0xff, 0x03, 0xff, 0xff, 0xff, 0x03, 0xff, 0xff, 0xff, 0x03, 0xff, 0xff, 0xff, 0x03, 
	0xff, 0xff, 0xff, 0x03, 0x00, 0xfe, 0x01, 0x00
};
// '4 3 leer', 35x10px
const unsigned char bitmap_4_3_leer [] PROGMEM = {
	0x00, 0x00, 0xff, 0x3f, 0x00, 0xfc, 0xff, 0x01, 0x60, 0x00, 0x07, 0x00, 0x00, 0xc0, 0x00, 0x01, 
	0x00, 0x00, 0x80, 0x00, 0x03, 0x00, 0x00, 0x80, 0x01, 0x02, 0x00, 0x00, 0x00, 0x03, 0x02, 0x00, 
	0x00, 0xe0, 0x07, 0x02, 0x00, 0xf0, 0x3f, 0x00, 0x02, 0xf8, 0x1f, 0x00, 0x00, 0xfe, 0x0f, 0x00, 
	0x00, 0x00
};
// '4 3 voll', 35x10px
const unsigned char bitmap_4_3_voll [] PROGMEM = {
	0x00, 0x00, 0xff, 0x3f, 0x00, 0xfc, 0xff, 0xff, 0x7f, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0xff, 
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0x01, 0xfe, 0xff, 0xff, 0xff, 0x03, 0xfe, 0xff, 
	0xff, 0xff, 0x07, 0xfe, 0xff, 0xff, 0x3f, 0x00, 0xfe, 0xff, 0x1f, 0x00, 0x00, 0xfe, 0x0f, 0x00, 
	0x00, 0x00
};
// Struktur für die Datenpakete der Sensoren
struct DataPacket {
    int distance1;
    int distance2;
    int distance3;
};

// Struktur für das Heartbeat Paket
struct HeartbeatPacket {
    char heartbeat;
};

// Array für verschiedene Bitmaps für die Signalstärke Anzeige
const unsigned char *bitmaps[] = {
    bitmap_Verbindung_0_bar,
    bitmap_Verbindung_1_bar,
    bitmap_Verbindung_2_bar,
    bitmap_Verbindung_3_bar
};

// Initialisierung der Funkverbindung
void setupRadio() {
    radio.begin();                  // Funkmodul initialisieren
    radio.setPALevel(RF24_PA_LOW);  // Sendeleistung auf niedrig setzen
    radio.openWritingPipe(node_A_address);  // Schreib-Pipe mit eigener Adresse öffnen
    radio.openReadingPipe(1, node_B_address); // Lese-Pipe mit anderer Adresse öffnen
    radio.startListening();         // Funkmodul in den Empfangsmodus versetzen
}

// Initialisierung des OLED Displays
void setupOLED() {
    OLED.begin();                   // OLED Display initialisieren
    OLED.setFont(u8g2_font_amstrad_cpc_extended_8n); // Schriftart setzen
    OLED.setColorIndex(1);          // Farbe auf Weiß setzen
}

// Berechnung der X-Positionen der Texte basierend auf den Sensorwerten
int* calculateNumberPositions() {
    static const int bmpX[] = {11, 51, 82};    // X-Positionen der Bitmaps
    static const int bmpWidths[] = {35, 25, 35};   // Breiten der Bitmaps
    static int numberX[3];   // X-Positionen der Zahlen

    for (int i = 0; i < 3; ++i) {
        int textWidth = 8 * String(sensor_distances[i]).length();  // Breite der Zahlen
        numberX[i] = bmpX[i] + (bmpWidths[i] - textWidth) / 2;   // Zahlenposition berechnen
    }
    return numberX;   // Array mit Zahlenpositionen zurückgeben
}

// Zeichnen der Sensordaten auf das OLED Display
void drawSensorData() {
    int* numberX = calculateNumberPositions(); // Zahlenposition berechnen
    OLED.firstPage();   // Erste Seite des Displays vorbereiten
    do {
        // Verschiedene Bitmaps und Sensordaten auf das Display zeichnen
        OLED.drawXBMP(0, 0, 17, 14, bitmap_ParkPal_Logo);
        OLED.drawXBMP(29, 0, 71, 5, bitmap_Auto);
        OLED.drawXBMP(110, 2, 12, 8, bitmap_Verbindung_0_bar);
        OLED.drawXBMP(110, 2, 12, 8, bitmap_Verbindung_1_bar);
        OLED.drawXBMP(110, 2, 12, 8, bitmap_Verbindung_2_bar);
        OLED.drawXBMP(110, 2, 12, 8, bitmap_Verbindung_3_bar);
        // Anzeige der Signalstärke je nach Sensorwerten
        OLED.drawXBMP(22, 9, 25, 7, sensor_distances[0] >= dist_step_04 ? bitmap_1_1_leer : bitmap_1_1_voll);
        OLED.drawXBMP(52, 9, 24, 7, sensor_distances[1] >= dist_step_04 ? bitmap_1_2_leer : bitmap_1_2_voll);
        OLED.drawXBMP(81, 9, 25, 7, sensor_distances[2] >= dist_step_04 ? bitmap_1_3_leer : bitmap_1_3_voll);
        OLED.drawXBMP(21, 19, 26, 7, sensor_distances[0] >= dist_step_03 ? bitmap_2_1_leer : bitmap_2_1_voll);
        OLED.drawXBMP(52, 19, 24, 8, sensor_distances[1] >= dist_step_03 ? bitmap_2_2_leer : bitmap_2_2_voll);
        OLED.drawXBMP(81, 19, 26, 7, sensor_distances[2] >= dist_step_03 ? bitmap_2_3_leer : bitmap_2_3_voll);
        OLED.drawXBMP(18, 29, 29, 9, sensor_distances[0] >= dist_step_02 ? bitmap_3_1_leer : bitmap_3_1_voll);
        OLED.drawXBMP(52, 30, 24, 8, sensor_distances[1] >= dist_step_02 ? bitmap_3_2_leer : bitmap_3_2_voll);
        OLED.drawXBMP(81, 29, 29, 9, sensor_distances[2] >= dist_step_02 ? bitmap_3_3_leer : bitmap_3_3_voll);
        OLED.drawXBMP(11, 40, 35, 10, sensor_distances[0] >= dist_step_01 ? bitmap_4_1_leer : bitmap_4_1_voll);
        OLED.drawXBMP(51, 42, 25, 10, sensor_distances[1] >= dist_step_01 ? bitmap_4_2_leer : bitmap_4_2_voll);
        OLED.drawXBMP(82, 40, 35, 10, sensor_distances[2] >= dist_step_01 ? bitmap_4_3_leer : bitmap_4_3_voll);
        // Sensordaten als Zahlen anzeigen
        OLED.drawStr(numberX[0], 62, String(sensor_distances[0]).c_str());
        OLED.drawStr(numberX[1], 64, String(sensor_distances[1]).c_str());
        OLED.drawStr(numberX[2], 62, String(sensor_distances[2]).c_str());
    } while (OLED.nextPage()); // Nächste Seite des Displays anzeigen
}

// Zeichnen der Anzeige für keine Funkverbindung auf das OLED Display
void drawNoConnection() {
    OLED.firstPage();   // Erste Seite des Displays vorbereiten
    do {
        // Bitmaps für keine Funkverbindung anzeigen
        OLED.drawXBMP(0, 0, 17, 14, bitmap_ParkPal_Logo);
        OLED.drawXBMP(29, 0, 71, 5, bitmap_Auto);
        OLED.drawXBMP(124, 2, 1, 6, bitmap_Ausrufezeichen);
        OLED.drawXBMP(110, 2, 12, 8, bitmap_Verbindung_0_bar);
        OLED.drawXBMP(110, 2, 12, 8, bitmaps[NoFunkSignalCounter]);
        OLED.drawXBMP(22, 9, 25, 7, bitmap_1_1_leer);
        OLED.drawXBMP(52, 9, 24, 7, bitmap_1_2_leer);
        OLED.drawXBMP(81, 9, 25, 7, bitmap_1_3_leer);
        OLED.drawXBMP(21, 19, 26, 7, bitmap_2_1_leer);
        OLED.drawXBMP(52, 19, 24, 8, bitmap_2_2_leer);
        OLED.drawXBMP(81, 19, 26, 7, bitmap_2_3_leer);
        OLED.drawXBMP(18, 29, 29, 9, bitmap_3_1_leer);
        OLED.drawXBMP(52, 30, 24, 8, bitmap_3_2_leer);
        OLED.drawXBMP(81, 29, 29, 9, bitmap_3_3_leer);
        OLED.drawXBMP(11, 40, 35, 10, bitmap_4_1_leer);
        OLED.drawXBMP(51, 42, 25, 10, bitmap_4_2_leer);
        OLED.drawXBMP(82, 40, 35, 10, bitmap_4_3_leer);
    } while (OLED.nextPage()); // Nächste Seite des Displays anzeigen

    NoFunkSignalCounter++;  // Zähler für fehlende Funkverbindung erhöhen
    delay(300); // Kurze Verzögerung
    if (NoFunkSignalCounter > 3) {
        NoFunkSignalCounter = 0;    // Zähler zurücksetzen nach 4 Bitmaps
    }
}

// Senden des Heartbeat Signals an den anderen Knoten
void sendHeartbeat(unsigned long currentMillis) {
    if (currentMillis - lastDataReceivedTime > heartbeatInterval) {
        HeartbeatPacket heartbeat;
        heartbeat.heartbeat = 'H';  // Heartbeat Zeichen setzen

        radio.stopListening();      // Funkmodul in den Sendemodus versetzen
        bool ackReceived = radio.write(&heartbeat, sizeof(HeartbeatPacket)); // Heartbeat senden
        radio.startListening();     // Funkmodul wieder in den Empfangsmodus versetzen

        if(!ackReceived & currentMillis - lastDataReceivedTime >= connectionTimeout) {
            radioConnected = false; // Keine Bestätigung erhalten
        }
    }
}

// Verarbeiten der Kommunikation über Funk
void handleRadioCommunication() {
    DataPacket data;    // Datenstruktur für Sensorwerte

    if (radio.available()) {
        while (radio.available()) {
            radio.read(&data, sizeof(DataPacket)); // Daten lesen
        }

        sensor_distances[0] = data.distance1; // Sensorwerte aktualisieren
        sensor_distances[1] = data.distance2;
        sensor_distances[2] = data.distance3;

        drawSensorData();   // Sensordaten auf das Display zeichnen
        buzzer.controlBuzzer(sensor_distances[0], sensor_distances[1], sensor_distances[2]); // Buzzer steuern
        lastDataReceivedTime = millis(); // Zeitpunkt des letzten Dateneingangs aktualisieren
        radioConnected = true;  // Funkverbindung erfolgreich
    } else {
        if (!radioConnected) {
            drawNoConnection(); // Keine Funkverbindung anzeigen
            buzzer.turnOffBuzzer(); // schaltet den Buzzer ab wenn keine Funkverbindung mehr da ist
        }
    }
}

void setup() {
    setupRadio();           // Funkmodul initialisieren
    setupOLED();            // OLED Display initialisieren
}

void loop() {
    unsigned long currentMillis = millis(); // Aktuelle Zeit in Millisekunden
    sendHeartbeat(currentMillis); // Heartbeat senden
    handleRadioCommunication(); // Funkkommunikation verarbeiten
    
}
