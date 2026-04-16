#include "WebManager.h"
#include "GlobalConfig.h"
#include <WiFi.h>
#include <SD.h>
#include <ESPAsyncWebServer.h>
#include "LedController.h"

WebManager webManager(80);

WebManager::WebManager(uint16_t port) : server(port), dns(), apMode(false), pendingRestart(false), restartTime(0) {}

void WebManager::processDns() {
    dns.processNextRequest();
}


const char* DEFAULT_ROOT_CA = \
"-----BEGIN CERTIFICATE-----\n"
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
"A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
"T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
"B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
"B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
"KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
"OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
"jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
"qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
"rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
"hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
"ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
"3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
"NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
"ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
"TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
"jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
"oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
"4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
"mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
"emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
"-----END CERTIFICATE-----\n";

String WebManager::getHtmlPage() {
    String page;
    page.reserve(12000);
    page += "<html><head><title>Zwitscherbox Konfiguration</title>";
    page += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    page += "<meta charset='UTF-8'><style>";
    
    // Modernes CSS
    page += "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background-color:#f0f2f5;color:#1c1e21;margin:0;padding:20px;}";
    page += "h1{text-align:center;color:#2e7d32;margin-bottom:30px;font-weight:300;}";
    page += "form{max-width:600px;margin:0 auto;}";
    page += ".card{background:#fff;padding:20px;border-radius:12px;box-shadow:0 2px 4px rgba(0,0,0,0.1);margin-bottom:20px;}";
    page += "h2{font-size:1.2rem;color:#2e7d32;margin-top:0;border-bottom:2px solid #e8f5e9;padding-bottom:10px;margin-bottom:20px;}";
    page += ".field{margin-bottom:15px;}";
    page += "label{display:block;margin-bottom:6px;font-weight:600;font-size:0.9rem;}";
    page += "input[type=text],input[type=password],input[type=number],textarea{width:100%;padding:12px;border:1px solid #ddd;border-radius:8px;font-size:16px;transition:border-color 0.3s;}";
    page += "input:focus{outline:none;border-color:#4CAF50;background:#fafafa;}";
    page += "input[type=color]{width:100%;height:45px;border:1px solid #ddd;border-radius:8px;cursor:pointer;background:white;padding:4px;}";
    page += "textarea{height:150px;font-family:monospace;font-size:12px;resize:vertical;}";
    page += ".help-text{font-size:0.8rem;color:#666;margin-top:-8px;margin-bottom:15px;line-height:1.3;}";
    
    // Checkbox Styling
    page += ".row{display:flex;align-items:center;gap:10px;margin:15px 0 5px 0;}";
    page += "input[type=checkbox]{width:20px;height:20px;accent-color:#4CAF50;}";
    
    // Button Styling
    page += "input[type=submit]{background-color:#2e7d32;color:white;padding:15px;border:none;border-radius:8px;cursor:pointer;width:100%;font-size:18px;font-weight:bold;margin-top:10px;box-shadow:0 4px 6px rgba(46,125,50,0.2);transition:0.2s;}";
    page += "input[type=submit]:hover{background-color:#1b5e20;transform:translateY(-1px);}";
    page += "input[type=submit]:active{transform:translateY(1px);}";
    
    page += "</style></head><body>";
    page += "<h1>Zwitscherbox</h1>";
    page += "<div style='text-align:center;margin-bottom:20px;'><a href='/files' style='display:inline-block;background:#1976D2;color:white;padding:10px 20px;text-decoration:none;border-radius:8px;font-weight:bold;'>🎵 Zum SD-Karten Dateimanager</a></div>";
    page += "<form action='/save' method='POST'>";

    // Hilfsfunktionen (angepasst an das neue Layout mit Erklärungen)
    auto addTextField = [&](const String& id, const String& label, const String& value, const String& desc = "") {
        page += "<div class='field'><label for='" + id + "'>" + label + "</label>";
        page += "<input type='text' id='" + id + "' name='" + id + "' value='" + value + "'></div>";
        if (desc.length() > 0) page += "<div class='help-text'>" + desc + "</div>";
    };
    auto addPasswordField = [&](const String& id, const String& label, const String& value, const String& desc = "") {
        page += "<div class='field'><label for='" + id + "'>" + label + "</label>";
        page += "<input type='password' id='" + id + "' name='" + id + "' value='" + value + "'></div>";
        if (desc.length() > 0) page += "<div class='help-text'>" + desc + "</div>";
    };
    auto addNumberField = [&](const String& id, const String& label, int value, const String& desc = "") {
        page += "<div class='field'><label for='" + id + "'>" + label + "</label>";
        page += "<input type='number' id='" + id + "' name='" + id + "' value='" + String(value) + "'></div>";
        if (desc.length() > 0) page += "<div class='help-text'>" + desc + "</div>";
    };
    auto addCheckbox = [&](const String& id, const String& label, bool checked, const String& desc = "") {
        page += "<div class='row'><input type='checkbox' id='" + id + "' name='" + id + "' value='1' " + (checked ? "checked" : "") + ">";
        page += "<label for='" + id + "'> " + label + "</label></div>";
        if (desc.length() > 0) page += "<div class='help-text'>" + desc + "</div>";
    };
    auto addTextArea = [&](const String& id, const String& label, const String& value, const String& desc = "") {
        page += "<div class='field'><label for='" + id + "'>" + label + "</label>";
        page += "<textarea id='" + id + "' name='" + id + "'>" + value + "</textarea></div>";
        if (desc.length() > 0) page += "<div class='help-text'>" + desc + "</div>";
    };
    auto addColorPicker = [&](const String& id, const String& label, const String& value, const String& desc = "") {
        String hexColor = value;
        if (hexColor.equalsIgnoreCase("RAINBOW")) {
            hexColor = "#000000";
        } else if (!hexColor.startsWith("#") && hexColor.length() > 0) {
            while (hexColor.length() < 6) hexColor = "0" + hexColor;
            hexColor = "#" + hexColor;
        }
        page += "<div class='field'><label for='" + id + "'>" + label + "</label>";
        page += "<div style='display:flex; gap:10px;'>";
        page += "<input type='color' style='width:60px; height:45px; padding:0; border:1px solid #ddd; border-radius:8px; cursor:pointer;' id='" + id + "_color' value='" + hexColor + "'";
        page += " oninput=\"document.getElementById('" + id + "').value = this.value\">";
        page += "<input type='text' id='" + id + "' name='" + id + "' value='" + value + "'";
        page += " oninput=\"if(this.value.match(/^#[0-9a-fA-F]{6}$/)) document.getElementById('" + id + "_color').value = this.value\">";
        page += "</div>";
        if (desc.length() > 0) page += "<div class='help-text' style='margin-top:5px;'>" + desc + "</div>";
        page += "</div>";
    };
    
    auto addTimeField = [&](const String& id, const String& label, const String& value, const String& desc = "") {
        page += "<div class='field'><label for='" + id + "'>" + label + "</label>";
        page += "<input type='time' id='" + id + "' name='" + id + "' value='" + value + "'></div>";
        if (desc.length() > 0) page += "<div class='help-text'>" + desc + "</div>";
    };
    
    auto addSelectField = [&](const String& id, const String& label, const String& value, const std::vector<std::pair<String, String>>& options, const String& desc = "") {
        page += "<div class='field'><label for='" + id + "'>" + label + "</label>";
        page += "<select id='" + id + "' name='" + id + "'>";
        for (const auto& opt : options) {
            String selected = (opt.first == value) ? "selected" : "";
            page += "<option value='" + opt.first + "' " + selected + ">" + opt.second + "</option>";
        }
        page += "</select></div>";
        if (desc.length() > 0) page += "<div class='help-text'>" + desc + "</div>";
    };

    // Sektionen
    page += "<div class='card'><h2>WLAN Einstellungen</h2>";
    int n = WiFi.scanNetworks();
    page += "<datalist id='wifi-networks'>";
    for (int i = 0; i < n; ++i) {
        page += "<option value='" + WiFi.SSID(i) + "'>";
    }
    page += "</datalist>";
    WiFi.scanDelete();

    page += "<div class='field'><label for='WIFI_SSID'>Netzwerk Name</label>";
    page += "<input type='text' id='WIFI_SSID' name='WIFI_SSID' value='" + config.wifi_ssid + "' list='wifi-networks'></div>";
    page += "<div class='help-text'>Wie heißt dein normales WLAN zu Hause?</div>";
    addPasswordField("WIFI_PASS", "Passwort", config.wifi_pass, "Das Passwort für dein WLAN, damit die Box online gehen kann.");
    page += "</div>";

    page += "<div class='card'><h2>Administrator & Sicherheit</h2>";
    addPasswordField("ADMIN_PASS", "Webinterface Passwort", config.admin_pass, "Sichert diese Weboberfläche mit einem Passwort ab (optional, Nutzername ist immer 'admin').");
    page += "</div>";

    page += "<div class='card'><h2>Home Assistant (MQTT)</h2>";
    addCheckbox("MQTT_INTEGRATION", "MQTT Aktivieren", config.homeassistant_mqtt_enabled, "Aktiviert die Steuerung und Statusmeldungen für Smart Home Systeme.");
    addTextField("MQTT_SERVER", "Broker Adresse", config.mqtt_server);
    addNumberField("MQTT_PORT", "Port", config.mqtt_port);
    addTextField("MQTT_USER", "Benutzername", config.mqtt_user);
    addPasswordField("MQTT_PASS", "Passwort", config.mqtt_pass);
    addTextField("MQTT_CLIENT_ID", "Client ID", config.mqtt_client_id, "Einzigartiger Name dieser Box im Netzwerk.");
    addTextField("MQTT_BASE_TOPIC", "Basis-Pfad (Topic)", config.mqtt_base_topic, "Der Haupt-Pfad, über den Home Assistant mit der Box spricht.");
    page += "</div>";

    page += "<div class='card'><h2>Freundschaftslampe</h2>";
    addCheckbox("FRIENDLAMP_ENABLE", "LED Hardware aktivieren", config.friendlamp_enabled, "Nur anhaken, wenn ein LED-Ring angeschlossen ist!");
    addCheckbox("FRIENDLAMP_MQTT_INTEGRATION", "MQTT Modus aktivieren", config.friendlamp_mqtt_enabled, "Vernetzt deine Box über das Internet mit den Boxen deiner Freunde.");
    addColorPicker("FRIENDLAMP_COLOR", "Wähle deine Farbe", config.friendlamp_color, "In dieser Farbe leuchten die Lampen deiner Freunde, wenn DU vor deiner Box stehst.");
    addTextField("FRIENDLAMP_TOPIC", "Topic Freundschaft", config.friendlamp_topic, "Das Topic zum Senden/Empfangen der Signale deiner Freunde wenn sie eine Freundschaftslampe haben.");
    addTextField("ZWITSCHERBOX_TOPIC", "Topic Zwitscherbox", config.zwitscherbox_topic, "Das Topic zum Senden/Empfangen der Signale deiner Freunde wenn sie eine Zwitscherbox haben.");
    addCheckbox("LED_FADE_EFFECT", "Sanftes Ein-/Ausblenden", config.led_fade_effect, "Nutzt weiche Übergänge für die LEDs anstatt sie hart ein- und auszuschalten.");
    addNumberField("LED_FADE_DURATION", "Dauer (ms)", config.fadeDuration, "Dauer des Farbwechsels in Millisekunden (1000 = 1 Sekunde).");
    addNumberField("LED_BRIGHTNESS", "Helligkeit (0-255)", config.led_brightness, "Maximale Helligkeit des LED-Rings.");
    addNumberField("LED_COUNT", "Anzahl NeoPixel LEDs", config.led_count, "Anzahl der verlöteten LEDs auf dem verbauten Ring.");
    page += "</div>";

    page += "<div class='card'><h2>Ruhezeit (Bitte nicht st&ouml;ren)</h2>";
    std::vector<std::pair<String, String>> tzOptions = {
        {"CET-1CEST,M3.5.0,M10.5.0/3", "Europa/Berlin (CET/CEST)"},
        {"GMT0BST,M3.5.0/1,M10.5.0", "Europa/London (GMT/BST)"},
        {"EST5EDT,M3.2.0,M11.1.0", "USA/New York (EST/EDT)"},
        {"PST8PDT,M3.2.0,M11.1.0", "USA/Los Angeles (PST/PDT)"},
        {"AEST-10AEDT,M10.1.0,M4.1.0/3", "Australien/Sydney (AEST/AEDT)"}
    };
    addSelectField("TIMEZONE", "Zeitzone", config.timezone, tzOptions, "Notwendig f&uuml;r korrekte Sommer-/Winterzeit.");
    addCheckbox("QUIET_TIME_ENABLED", "Ruhezeit aktivieren", config.quiet_time_enabled, "In dieser Zeit werden keine eingehenden Farben von au&szlig;en angezeigt. (NTP Server notwendig)");
    addTimeField("QUIET_TIME_START", "Start", config.quiet_time_start, "Ab dieser Uhrzeit bleibt die Lampe stumm (z.B. 22:00).");
    addTimeField("QUIET_TIME_END", "Ende", config.quiet_time_end, "Ab dieser Uhrzeit werden wieder Farben angezeigt (z.B. 08:00).");
    page += "</div>";

    page += "<div class='card'><h2>Externer Broker (Optional)</h2>";
    addTextField("FRIENDLAMP_MQTT_SERVER", "Server-URL", config.friendlamp_mqtt_server, "Trage hier deinen eigenen Internet-Broker ein (falls genutzt).");
    addNumberField("FRIENDLAMP_MQTT_PORT", "Port", config.friendlamp_mqtt_port);
    addTextField("FRIENDLAMP_MQTT_USER", "Benutzer", config.friendlamp_mqtt_user);
    addPasswordField("FRIENDLAMP_MQTT_PASS", "Passwort", config.friendlamp_mqtt_pass);
    addCheckbox("FRIENDLAMP_MQTT_TLS_ENABLED", "TLS Verschlüsselung nutzen", config.friendlamp_mqtt_tls_enabled, "Sichert die Verbindung ab. In der Regel für öffentliche MQTT Broker empfohlen!");
    String ca = config.mqtt_root_ca_content.length() > 0 ? config.mqtt_root_ca_content : DEFAULT_ROOT_CA;
    addTextArea("FRIENDLAMP_MQTT_ROOT_CA", "Root CA Zertifikat", ca, "Das Stammzertifikat des Servers für die verschlüsselte Verbindung.");
    page += "</div>";

    page += "<input type='submit' value='Konfiguration Speichern'>";
    page += "</form><div style='height:40px;'></div></body></html>";
    return page;
}

String WebManager::getFileManagerHtml() {
    String page;
    page.reserve(8000);
    page += "<!DOCTYPE html><html><head><title>Dateimanager</title>";
    page += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    page += "<style>";
    page += "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background-color:#f0f2f5;color:#1c1e21;margin:0;padding:20px;}";
    page += "h1{color:#2e7d32;margin-bottom:10px;}";
    page += ".card{background:#fff;padding:20px;border-radius:12px;box-shadow:0 2px 4px rgba(0,0,0,0.1);margin-bottom:20px;}";
    page += ".btn{background:#2e7d32;color:white;padding:10px 15px;border:none;border-radius:8px;cursor:pointer;font-weight:bold;margin-right:10px;}";
    page += ".btn-danger{background:#d32f2f;}";
    page += "li{padding:10px;border-bottom:1px solid #ddd;display:flex;justify-content:space-between;align-items:center;}";
    page += "ul{list-style:none;padding:0;}";
    page += "#progress{width:100%;background:#e0e0e0;border-radius:8px;margin-top:10px;display:none;}";
    page += "#bar{width:0%;height:20px;background:#4CAF50;border-radius:8px;}";
    page += "input[type=file], input[type=text]{padding:10px;margin-bottom:10px;width:calc(100% - 20px);border:1px solid #ccc;border-radius:8px;}";
    page += "</style></head><body>";
    page += "<div class='card'>";
    page += "<div style='display:flex;justify-content:space-between;align-items:center;'><h1>SD-Karten Manager</h1><a href='/' class='btn' style='text-decoration:none;'>Setup</a></div>";
    page += "<h3>Aktueller Pfad: <span id='currentPath'>/</span></h3>";
    page += "<button class='btn' onclick='loadDir(\"/\")'>Root</button>";
    page += "<button class='btn' onclick='goUp()'>Nach Oben</button></div>";

    page += "<div class='card'><h3>📂 Ordner erstellen</h3>";
    page += "<input type='text' id='newDirName' placeholder='Name des neuen Ordners'>";
    page += "<button class='btn' onclick='createFolder()'>Erstellen</button></div>";

    page += "<div class='card'><h3>⬆️ MP3 Hochladen</h3>";
    page += "<input type='file' id='fileInput'>";
    page += "<button class='btn' onclick='uploadFile()'>Hochladen</button>";
    page += "<div id='progress'><div id='bar'></div></div></div>";

    page += "<div class='card'><h3>Inhalt</h3>";
    page += "<ul id='fileList'><li>Lade...</li></ul></div>";

    page += "<script>";
    page += "let currentPath = '/';";
    page += "function goUp() { let parts = currentPath.split('/'); parts.pop(); parts.pop(); let p = parts.join('/') + '/'; loadDir(p.length > 1 ? p : '/'); }";
    page += "function loadDir(path) { currentPath = path.endsWith('/') ? path : path + '/'; document.getElementById('currentPath').innerText = currentPath;";
    page += "fetch('/api/list?dir=' + encodeURIComponent(currentPath)).then(r=>r.json()).then(data=>{";
    page += "let html=''; data.forEach(item=>{";
    page += "let isDir = item.isDir; let size = item.size?Math.round(item.size/1024)+' KB':'';";
    page += "let icon = isDir ? '📁' : '🎵';";
    page += "let action = isDir ? `<button class='btn' onclick='loadDir(\"${currentPath}${item.name}\")'>&Ouml;ffnen</button>` : '';";
    page += "html+=`<li><span>${icon} ${item.name} <small>${size}</small></span><div>${action}<button class='btn btn-danger' onclick='deleteItem(\"${currentPath}${item.name}\")'>X</button></div></li>`;";
    page += "}); document.getElementById('fileList').innerHTML=html; }); }";
    
    page += "function deleteItem(path) { if(confirm('Sicher l\\u00f6schen? ' + path)){ fetch('/api/delete?path='+encodeURIComponent(path),{method:'DELETE'}).then(r=>{if(r.ok)loadDir(currentPath);else alert('Fehler beim L\\u00f6schen');}); } }";
    
    page += "function createFolder() { let name = document.getElementById('newDirName').value; if(name){ fetch('/api/mkdir?path='+encodeURIComponent(currentPath+name),{method:'POST'}).then(r=>{if(r.ok){document.getElementById('newDirName').value='';loadDir(currentPath);}else alert('Fehler');}); } }";
    
    page += "function uploadFile() { let file = document.getElementById('fileInput').files[0]; if(!file)return; let data = new FormData(); data.append('file', file, currentPath + file.name);";
    page += "let r = new XMLHttpRequest(); r.open('POST', '/api/upload');";
    page += "r.upload.onprogress = function(e){ if(e.lengthComputable){ document.getElementById('progress').style.display='block'; let p = (e.loaded/e.total)*100; document.getElementById('bar').style.width=Math.round(p)+'%'; } };";
    page += "r.onload = function(){ document.getElementById('progress').style.display='none'; document.getElementById('bar').style.width='0%'; loadDir(currentPath); };";
    page += "r.send(data); }";

    page += "window.onload = function() { loadDir('/'); };";
    page += "</script></body></html>";
    return page;
}

void WebManager::handleSave(AsyncWebServerRequest *request) {
    File configFile = SD.open("/config.txt", FILE_WRITE);
    if (!configFile) {
        request->send(500, "text/plain", "Fehler beim Öffnen der config.txt zum Schreiben.");
        return;
    }
    configFile.println("# Zwitscher - Configuration File (Web-Generated)");
    
    auto writeParam = [&](const String& key, const String& webName) {
        if (request->hasParam(webName.c_str(), true)) {
            String value = request->getParam(webName.c_str(), true)->value();
            configFile.println(key + "=" + value);
        }
    };
    auto writeCheckbox = [&](const String& key, const String& webName) {
        String value = "0";
        if (request->hasParam(webName.c_str(), true)) {
             if(request->getParam(webName.c_str(), true)->value() == "1") value = "1";
        }
         configFile.println(key + "=" + value);
    };

    // WLAN
    writeParam("WIFI_SSID", "WIFI_SSID");
    writeParam("WIFI_PASS", "WIFI_PASS");
    
    // Administrator
    writeParam("ADMIN_PASS", "ADMIN_PASS");

    // Home Assistant
    writeCheckbox("MQTT_INTEGRATION", "MQTT_INTEGRATION");
    writeParam("MQTT_SERVER", "MQTT_SERVER");
    writeParam("MQTT_PORT", "MQTT_PORT");
    writeParam("MQTT_USER", "MQTT_USER");
    writeParam("MQTT_PASS", "MQTT_PASS");
    writeParam("MQTT_CLIENT_ID", "MQTT_CLIENT_ID");
    writeParam("MQTT_BASE_TOPIC", "MQTT_BASE_TOPIC");

    // Freundschaftslampe
    writeCheckbox("FRIENDLAMP_ENABLE", "FRIENDLAMP_ENABLE");
    writeCheckbox("FRIENDLAMP_MQTT_INTEGRATION", "FRIENDLAMP_MQTT_INTEGRATION");
    if (request->hasParam("FRIENDLAMP_COLOR", true)) {
        String colorValue = request->getParam("FRIENDLAMP_COLOR", true)->value();
        if (colorValue.startsWith("#")) colorValue = colorValue.substring(1); // '#' entfernen
        colorValue.toUpperCase(); // Konsistent in Großbuchstaben umwandeln
        configFile.println("FRIENDLAMP_COLOR=" + colorValue);
    }
    writeParam("FRIENDLAMP_TOPIC", "FRIENDLAMP_TOPIC");
    writeParam("ZWITSCHERBOX_TOPIC", "ZWITSCHERBOX_TOPIC");
    writeCheckbox("LED_FADE_EFFECT", "LED_FADE_EFFECT");
    writeParam("LED_FADE_DURATION", "LED_FADE_DURATION");
    writeParam("LED_BRIGHTNESS", "LED_BRIGHTNESS");
    writeParam("LED_COUNT", "LED_COUNT");

    // Ruhezeit
    writeParam("TIMEZONE", "TIMEZONE");
    writeCheckbox("QUIET_TIME_ENABLED", "QUIET_TIME_ENABLED");
    writeParam("QUIET_TIME_START", "QUIET_TIME_START");
    writeParam("QUIET_TIME_END", "QUIET_TIME_END");

    // Externer Broker
    writeParam("FRIENDLAMP_MQTT_SERVER", "FRIENDLAMP_MQTT_SERVER");
    writeParam("FRIENDLAMP_MQTT_PORT", "FRIENDLAMP_MQTT_PORT");
    writeParam("FRIENDLAMP_MQTT_USER", "FRIENDLAMP_MQTT_USER");
    writeParam("FRIENDLAMP_MQTT_PASS", "FRIENDLAMP_MQTT_PASS");
    writeCheckbox("FRIENDLAMP_MQTT_TLS_ENABLED", "FRIENDLAMP_MQTT_TLS_ENABLED");
    if (request->hasParam("FRIENDLAMP_MQTT_ROOT_CA", true)) {
        configFile.println("BEGIN_CERT");
        configFile.print(request->getParam("FRIENDLAMP_MQTT_ROOT_CA", true)->value());
        configFile.println("\nEND_CERT");
    }

    configFile.close();
    
    String response = "<html><head><title>Gespeichert</title><meta charset='UTF-8'></head><body><h1>Konfiguration gespeichert!</h1><p>Das Ger&auml;t wird jetzt neu gestartet. Bitte verbinde dich mit deinem normalen WLAN und schlie&szlig;e dieses Fenster.</p>";
    response += "<p>In 5 Sekunden wird versucht, die Seite neu zu laden...</p><meta http-equiv='refresh' content='5;url=/' /></body></html>";
    request->send(200, "text/html", response);

    // KORREKTUR: Den ESP über den main loop neu starten, damit der Browser die Bestätigung empfangen kann!
    pendingRestart = true;
    restartTime = millis() + 2000; // Dem Server 2 Sekunden Zeit geben, die Seite zu schicken
}

bool WebManager::checkAuth(AsyncWebServerRequest *request) {
    if (config.admin_pass.length() > 0 && !request->authenticate("admin", config.admin_pass.c_str())) {
        request->requestAuthentication();
        return false;
    }
    return true;
}

void WebManager::setupWebServer() {
  server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    request->send(200, "text/html", getHtmlPage());
  });

  server.on("/save", HTTP_POST, [this](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    handleSave(request);
  });

  server.on("/files", HTTP_GET, [this](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    request->send(200, "text/html", getFileManagerHtml());
  });

  server.on("/api/list", HTTP_GET, [this](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    String dirPath = request->hasParam("dir") ? request->getParam("dir")->value() : "/";
    File dir = SD.open(dirPath);
    if (!dir || !dir.isDirectory()) return request->send(400, "application/json", "[]");
    String json = "[";
    File file = dir.openNextFile();
    bool first = true;
    while(file){
        if(!first) json += ",";
        String name = file.name();
        if(name.lastIndexOf('/') >= 0) name = name.substring(name.lastIndexOf('/')+1);
        json += "{\"name\":\"" + name + "\",\"isDir\":" + (file.isDirectory()?"true":"false") + ",\"size\":" + String(file.size()) + "}";
        first = false;
        file = dir.openNextFile();
    }
    json += "]";
    request->send(200, "application/json", json);
  });

  server.on("/api/mkdir", HTTP_POST, [this](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    if(request->hasParam("path")){
        String path = request->getParam("path")->value();
        if(SD.mkdir(path)) request->send(200);
        else request->send(500);
    } else request->send(400);
  });

  server.on("/api/delete", HTTP_DELETE, [this](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    if(request->hasParam("path")){
        String path = request->getParam("path")->value();
        File f = SD.open(path);
        bool success = false;
        if(f){
            bool isDir = f.isDirectory();
            f.close();
            if(isDir) success = SD.rmdir(path);
            else success = SD.remove(path);
        }
        if(success) request->send(200);
        else request->send(500);
    } else request->send(400);
  });

  server.on("/api/upload", HTTP_POST, [this](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    request->send(200, "text/plain", "Upload done");
  }, [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    if (config.admin_pass.length() > 0 && !request->authenticate("admin", config.admin_pass.c_str())) return;
    File *f = (File *)request->_tempObject;
    if(!index){
        if(!filename.startsWith("/")) filename = "/" + filename;
        f = new File(SD.open(filename, FILE_WRITE));
        request->_tempObject = (void*)f;
    }
    if (f) {
        if(len) f->write(data, len);
        if(final){
            f->close();
            delete f;
            request->_tempObject = NULL;
        }
    }
  });
  
  server.onNotFound([this](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    request->send(200, "text/html", getHtmlPage());
  });

  server.begin();
  Serial.println("HTTP server started.");
}

void WebManager::startConfigPortal(){
  apMode = true;
  ledCtrl.setApModeLed(true);
  Serial.println("Starting Access Point 'Zwitscherbox'");
  Serial.println("Password for Access Point is: 12345678");
  WiFi.softAP("Zwitscherbox", "12345678");
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  dns.start(53, "*", IP);

  setupWebServer();
}
