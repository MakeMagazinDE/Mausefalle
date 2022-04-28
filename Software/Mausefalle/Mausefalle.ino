/*********
  Dieses Programm beruht auf einem Sketch von
    
  Rui Santos
  Complete instructions at https://RandomNerdTutorials.com/esp32-cam-projects-ebook/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/

#include "esp_camera.h"
#include "driver/rtc_io.h"
#include "ESP32_MailClient.h"
/*#include <FS.h>*/
/*#include <SPIFFS.h>*/
#include "LITTLEFS.h"
#include <WiFi.h>

// Hier die eigenen WLAN-Einstellungen einsetzen...
const char* ssid = "ssid";
const char* password = "wlan-passwort";

//...und hier die EMail-Einstellungen
#define emailBenutzer "xyz@abc.de"
#define emailPasswort "ihr email passwort"
#define smtpServer    "smtp-serveradresse
#define smtpServerPort xyz
#define emailBetreff  "Tier in Falle"
#define emailAdressat "adresse@xy.ab"
//Bei Bedarf weitere Empfängeradresse eintragen und nummerieren
//#define emailAdressat2 "xyz@abc.de"

// Falls eine andere ESP32-CAM benutzt wird, hier bitte anpassen
//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
//#define CAMERA_MODEL_ESP_EYE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
//#define CAMERA_MODEL_M5STACK_V2_PSRAM // M5Camera version B Has PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_ESP32CAM // No PSRAM
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
//#define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM

#include "camera_pins.h"

SMTPData smtpData;

#define FOTO_NAME "/maus.jpg"

void setup() {
  //WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  
  //ESP alle 30 Minuten wieder einschalten (Zeit ist in Mikrosekunden)
  esp_sleep_enable_timer_wakeup(180000000000); 
  
  Serial.begin(115200);
  Serial.println();

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Verbindungsaufnahme mit WLAN");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  
  if (!SPIFFS.begin(true)) {
    Serial.println("Ein Fehler mit dem SPIFFS ist aufgetreten");
    ESP.restart();
  }
  else {
    delay(500);
    Serial.println("SPIFFS erfolgreich gestartet");
  }
  
  // Print ESP32 Local IP Address
  Serial.print("IP Address: http://");
  Serial.println(WiFi.localIP());
   
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  //UXGA-Format nur bei Kameraboards mit PSRAM
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 1;
    Serial.println("Aufnahmeformat UXGA");
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    Serial.println("Aufnahmeformat SVGA");
  }

  // Kamera initialisieren mit obigen Konfigurationsdaten und Fehlerabfrage
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Kamera meldet Fehler 0x%x", err);
    return;
  }

  delay(5000);
  //Foto aufnehmen...
  capturePhotoSaveSPIFFS();
  
  //...und senden
  sendPhoto();

  //...und schlafen gehen
  Serial.printf("Gehe jetzt 5 Minuten schlafen und schicke dann die nächste EMail");
  esp_deep_sleep_start();
}

void loop() {
//Hier passiert nix, da wir kein ständig sich wiederholendes Programm brauchen  
}

//Funktionsdefinitionen

// Checken, ob ein Foto aufgenommen wurde
bool checkPhoto( fs::FS &fs ) {
  File f_pic = fs.open( FOTO_NAME );
  unsigned int pic_sz = f_pic.size();
  return ( pic_sz > 100 );
}

// Foto aufnehmen und als Datei speichern
void capturePhotoSaveSPIFFS( void ) {
  camera_fb_t * fb = NULL; 
  bool ok = 0; // Boolean indicating if the picture has been taken correctly

  do {
    Serial.println("Achtung, Aufnahme!");
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Aufnahme fehlerhaft");
      return;
    }

    // Photo file name
    Serial.printf("Dateiname des Fotos: %s\n", FOTO_NAME);
    File file = SPIFFS.open(FOTO_NAME, FILE_WRITE);

    // Bilddaten in Fotodatei schreiben
    if (!file) {
      Serial.println("Kann nicht in Fotodatei schreiben");
    }
    else {
      file.write(fb->buf, fb->len);
      Serial.print("Das Bild wurde gespeichert in ");
      Serial.print(FOTO_NAME);
      Serial.print(" - Größe: ");
      Serial.print(file.size());
      Serial.println(" Bytes");
    }
    file.close();
    esp_camera_fb_return(fb);

    // Überprüfen, ob alles richtig gespeichert wurde 
    ok = checkPhoto(SPIFFS);
    //ab do wiederholen, falls kein Foto gespeichert wurde
  } while ( !ok );
}

void sendPhoto( void ) {
  Serial.println("Sende EMail...");
  // Beim SMTP-Server anmelden
  smtpData.setLogin(smtpServer, smtpServerPort, emailBenutzer, emailPasswort);
  //EMail zusammensetzen
  // Absender
  smtpData.setSender("ESP32-CAM", emailBenutzer);
  
  // EMail-Priorität auf Hoch setzen
  smtpData.setPriority("High");

  // Betreffzeile
  smtpData.setSubject(emailBetreff);
    
  // Text der EMail im HTML format
  smtpData.setMessage("<h2>Maus gefangen, bitte schnellstmöglich befreien!</h2>", true);
  
  // Empfängeradresse (können auch mehrere sein, dann ab Zeile 31 weitere Adressen einfügen)
  smtpData.addRecipient(emailAdressat);
  //smtpData.addRecipient(emailAdressat2);

  // Foto als Anhang zu EMail hinzufügen
  smtpData.addAttachFile(FOTO_NAME, "image/jpg");
  smtpData.setFileStorageType(MailClientStorageType::SPIFFS);

  smtpData.setSendCallback(sendCallback);
  
  // Absenden und überprüfen
  if (!MailClient.sendMail(smtpData))
    Serial.println("Fehler beim EMail-Versand, " + MailClient.smtpErrorReason());
  smtpData.empty();
}

// EMail-Status holen
void sendCallback(SendStatus msg) {
   Serial.println(msg.info());
}
