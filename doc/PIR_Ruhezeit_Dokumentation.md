# Dokumentation: PIR Ruhezeit & Sensor-Steuerung

Diese Dokumentation beschreibt die Funktionalität und Konfiguration der PIR-Sensor-Steuerung während der Ruhezeit (Do Not Disturb) in der Zwitscherbox V6.

## Übersicht

Der PIR-Bewegungsmelder löst normalerweise bei jeder erkannten Bewegung die Wiedergabe eines zufälligen Audiostücks aus. Mit der Einführung der **PIR Ruhezeit-Option** kann dieses Verhalten nun zeitgesteuert eingeschränkt werden, um beispielsweise nächtliche Störungen zu vermeiden.

## Funktionen

### 1. PIR in Ruhezeit deaktivieren
Zusätzlich zur allgemeinen Ruhezeit (in der nur die LED-Signale unterdrückt werden) kann nun explizit festgelegt werden, ob auch der PIR-Sensor in diesem Zeitraum stumm bleiben soll.

*   **Option:** `PIR in Ruhezeit deaktivieren`
*   **Verhalten:** Wenn aktiv, ignoriert die Box jegliche Bewegungserkennung zwischen der eingestellten Start- und Endzeit.
*   **Voraussetzung:** Ein aktiver NTP-Server-Sync ist notwendig, damit die Box die aktuelle Uhrzeit kennt.

### 2. Konfiguration über das Webinterface
Die Einstellungen befinden sich im Web-Konfigurationsmenü unter dem Reiter **"Ruhezeit (Bitte nicht stören)"**:

*   **Ruhezeit aktivieren:** Schaltet die zeitgesteuerte Unterdrückung generell ein.
*   **PIR in Ruhezeit deaktivieren:** Setzt den PIR-Sensor während der Ruhezeit außer Kraft.
*   **Start/Ende:** Definiert das Zeitfenster (z.B. 22:00 bis 08:00).

## Technische Implementierung

### Konfigurations-Variablen (`AppConfig.h/cpp`)
Die Einstellung wird in der `config` gespeichert:
```cpp
bool quiet_time_pir_disabled; // Speichert den Status der Checkbox
```

### Logik-Prüfung (`AudioEngine.cpp`)
In der Methode `checkPirAndTimeout()` wird vor dem Auslösen der Wiedergabe geprüft, ob der Sensor ignoriert werden soll:

```cpp
if (pirStateHigh) {
    // Prüfung ob PIR während der Ruhezeit ignoriert werden soll
    if (config.quiet_time_pir_disabled && mqttHandler.isQuietTime()) {
        // Bewegung erkannt, aber ignoriert
        return;
    }
    // ... normaler Trigger-Code
}
```

### Zeitprüfung (`MqttHandler.cpp`)
Die Methode `isQuietTime()` berechnet basierend auf der aktuellen Systemzeit und der konfigurierten Zeitzone, ob das aktuelle Zeitfenster innerhalb der Ruhezeit liegt.

## Letzte Fehlerbehebungen (PIR)

*   **Pin-Korrektur:** Der PIR-Sensor wurde von Pin 16 auf den korrekten Hardware-Pin **18** verschoben.
*   **API-Anpassung:** Aufgrund von Breaking Changes in der `ESP32-audioI2S` Bibliothek (v3.0+) wurde der Trigger-Mechanismus auf das neue Callback-System (`Audio::audio_info_callback`) umgestellt, um zuverlässiges Auslösen nach dem Ende eines Tracks zu gewährleisten.
