#include "WiFi.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include <ESPAsyncWebServer.h>
#include <StringArray.h>
#include <SPIFFS.h>
#include <FS.h>


// #include "camera_pins.h"

// Replace with your network credentials
const char* ssid = "your-id";
const char* password = "your-password";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

#define CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 21
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27

#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 19
#define Y4_GPIO_NUM 18
#define Y3_GPIO_NUM 5
#define Y2_GPIO_NUM 4
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// Pin definition for TFT HSPI
#define TFT_SCLK 14
#define TFT_MISO -1
#define TFT_MOSI 13
#define TFT_CS 15 // Chip select control pin
#define TFT_DC 32 // Data Command control pin
#define TFT_RST 33

// Pin button
#define PIN_BTN 12

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { text-align:center; }
    .vert { margin-bottom: 10%; }
    .hori{ margin-bottom: 0%; }
  </style>
</head>
<body>
  <div id="container">
    <h2>ESP32-CAM Last Photo</h2>
    <p>The photo might be taken more than 5 seconds ago.</p>
    <p>
      <button onclick="rotatePhoto();">ROTATE</button>
      <button onclick="location.reload();">REFRESH PAGE</button>
    </p>
  </div>
  <div><img src="/image" id="photo" width="70%"></div>
</body>
<script>
  var deg = 0;
  function rotatePhoto() {
    var img = document.getElementById("photo");
    deg += 90;
    if(isOdd(deg/90)){ document.getElementById("container").className = "vert"; }
    else{ document.getElementById("container").className = "hori"; }
    img.style.transform = "rotate(" + deg + "deg)";
  }
  function isOdd(n) { return Math.abs(n % 2) == 1; }
</script>
</html>)rawliteral";

// Check if photo capture was successful
bool checkPhoto( fs::FS &fs ) {
  File f_pic = fs.open( FILE_PHOTO );
  unsigned int pic_sz = f_pic.size();
  return ( pic_sz > 100 );
}


void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);
  delay(5000);
  Serial.println("Setup start");

  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println(String("Connecting to ") + ssid);
  while (WiFi.status() != WL_CONNECTED){
    delay(1000);
    Serial.println("Connecting to WiFi...");
    Serial.print(WiFi.status());
  }
  Serial.println("\nConnected, IP address: ");
  

  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    ESP.restart();
  }
  else {
    delay(500);
    Serial.println("SPIFFS mounted successfully");
  }

  // Print ESP32 Local IP Address
  Serial.println("IP Address: http://");
  Serial.println(WiFi.localIP());

  // Turn-off the 'brownout detector'
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  // OV2640 camera module
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  // config.pin_sscb_sda = SIOD_GPIO_NUM;
  // config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size   = FRAMESIZE_QVGA; // 320x240
//    config.frame_size   = FRAMESIZE_UXGA; // 1600x1200
  config.jpeg_quality = 10;
  config.fb_count     = 2;
  config.fb_location = CAMERA_FB_IN_DRAM;

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html);
  });

  // server.on("/capture", HTTP_GET, [](AsyncWebServerRequest * request) {
  //   takeNewPhoto = true;
  //   request->send_P(200, "text/plain", "Taking Photo");
  // });

  // server.on("/saved-photo", HTTP_GET, [](AsyncWebServerRequest * request) {
  //   request->send(SPIFFS, FILE_PHOTO, "image/jpg", false);
  // });

  server.on("/image", HTTP_GET, [](AsyncWebServerRequest * request) {
    camera_fb_t* fb = esp_camera_fb_get();
    esp_camera_fb_get();
    if(!fb){
      Serial.println("Camera capture failed");
      request->send(500, "text/plain", "Failed to capture");
      return;
    }
    AsyncWebServerResponse* response = request->beginResponse_P(
      200, "image/jpeg", fb->buf, fb->len
    );
    response->addHeader("Content-Disposition", "inline;filename=capture.jpg");
    request->send(response);
    esp_camera_fb_return(fb);
  });

  // Start server
  server.begin();
  Serial.println("Setup End");
}

void loop() {
  // if (takeNewPhoto) {
  //   capturePhotoSaveSpiffs();
  //   takeNewPhoto = false;
  // }
  delay(100);
}
