# Anpassung der Build-Konfiguration

Dieses Dokument erklärt die notwendige Anpassung in der `platformio.ini`-Datei, um ein bekanntes Kompatibilitätsproblem mit dem ESP32 zu beheben.

## Das Problem

Bei der Kompilierung des Projekts trat ein Fehler auf, der sich auf den RMT-Treiber des ESP32 bezieht, welcher für die Ansteuerung von NeoPixel-LEDs (dem LED-Ring) verwendet wird. Die Fehlermeldung lautete:

```
E (3637) rmt: rmt_new_tx_channel(269): not able to power down in light sleep
```

Dieser Fehler tritt auf, weil eine ältere Version des ESP32-Frameworks (Arduino Core) ein Problem mit dem Energiemanagement des RMT-Treibers hat.

## Die Lösung

Um das Problem zu beheben, muss PlatformIO angewiesen werden, eine neuere, fehlerbereinigte Version des ESP32-Frameworks zu verwenden. Gleichzeitig muss sichergestellt werden, dass diese Version die spezifischen Definitionen für das YB-ESP32-S3-AMP-Board kennt.

Die Lösung besteht darin, die `platform`-Einstellung in der `platformio.ini` auf ein spezifisches Git-Repository zu setzen, das eine funktionierende Kombination aus Framework und Board-Definitionen bereitstellt.

Die korrekte Zeile in `platformio.ini` lautet:
```ini
[env]
platform = https://github.com/pioarduino/platform-espressif32.git
```

Diese Einstellung zwingt PlatformIO, die Plattform von diesem Repository zu klonen, was das Build-Problem behebt und eine erfolgreiche Kompilierung ermöglicht.
