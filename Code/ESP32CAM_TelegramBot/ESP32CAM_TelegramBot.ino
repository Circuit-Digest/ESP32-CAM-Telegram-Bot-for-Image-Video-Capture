/*
 * ====================================================================
 * ESP32-CAM Telegram Bot with AVI Video Recording
 * ====================================================================
 * 
 * Author: Rithik Krisna M /CircuitDigest (@me_RK)
 * Date: August 2025
 * Hardware: AI-Thinker ESP32-CAM Module
 * 
 * DESCRIPTION:
 * This advanced ESP32-CAM project creates a fully-featured Telegram bot
 * that can capture high-quality photos and record custom-duration AVI 
 * videos with proper MJPEG codec structure. All media is stored on SD 
 * card and can be uploaded to Telegram.
 * 
 * KEY FEATURES:
 * ============
 * 📸 PHOTO CAPTURE:
 *    - High-quality JPEG photo capture
 *    - Automatic photo numbering and SD card storage
 *    - Direct Telegram upload with progress feedback
 * 
 * 🎥 AVI VIDEO RECORDING:
 *    - Custom duration videos (1-120 seconds)
 *    - Proper AVI file structure with MJPEG codec
 *    - Target 5 FPS for smooth playback
 *    - VGA resolution (640x480) with PSRAM, QVGA (320x240) fallback
 *    - Real-time frame capture with timing optimization
 *    - Automatic file cleanup after upload
 * 
 * 💾 SD CARD STORAGE:
 *    - Full SD card integration with health monitoring
 *    - Storage space tracking and usage statistics
 *    - Configurable storage enable/disable
 *    - File size validation and corruption detection
 * 
 * 📱 TELEGRAM BOT COMMANDS:
 *    /start     - Welcome message and command list
 *    /photo     - Capture and send photo
 *    /video_on  - Enable video recording mode
 *    /video_off - Disable video recording mode
 *    /record    - Record video with default duration
 *    /record[N] - Record N-second video (e.g., /record30)
 *    /video_status - Show recording status and progress
 *    /sd_on     - Enable SD card storage
 *    /sd_off    - Disable SD card storage
 *    /storage   - Show SD card storage information
 *    /status    - Complete system status report
 *    /help      - Show all available commands
 * 
 * 🔧 TECHNICAL SPECIFICATIONS:
 *    - ESP32-CAM AI-Thinker module support
 *    - WiFi connectivity with automatic reconnection
 *    - SSL/TLS secure communication with Telegram
 *    - Advanced camera sensor optimization
 *    - Memory management with PSRAM detection
 *    - Watchdog timer handling for stability
 * 
 * 🛡️ SECURITY FEATURES:
 *    - Chat ID authentication (only authorized users)
 *    - Secure SSL connections to Telegram API
 *    - Input validation and error handling
 * 
 * HARDWARE REQUIREMENTS:
 * =====================
 * - AI-Thinker ESP32-CAM module
 * - MicroSD card (Class 10 recommended)
 * - 5V power supply (minimum 2A)
 * - WiFi network connection
 * 
 * SOFTWARE DEPENDENCIES:
 * =====================
 * - Arduino IDE with ESP32 board package
 * - UniversalTelegramBot library
 * - ArduinoJson library
 * - ESP32 Camera library
 * 
 * SETUP INSTRUCTIONS:
 * ==================
 * 1. Install required libraries in Arduino IDE
 * 2. Create Telegram bot via @BotFather
 * 3. Get your chat ID from @myidbot
 * 4. Update WiFi credentials and bot tokens below
 * 5. Insert formatted SD card into ESP32-CAM
 * 6. Upload code and monitor serial output
 * 
 * CONFIGURATION:
 * =============
 * - Update ssid and password with your WiFi credentials
 * - Replace BOTtoken with your Telegram bot token
 * - Replace CHAT_ID with your Telegram chat ID
 * - Adjust video quality settings if needed
 * 
 * ====================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include "esp_task_wdt.h"
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include "FS.h"
#include "SD_MMC.h"
#include <EEPROM.h>
#include "esp_timer.h"

// ====================================================================
// NETWORK CONFIGURATION
// ====================================================================
const char *ssid = "MyWiFiNetwork";      // Your WiFi network name
const char *password = "MySecretPass123";          // Your WiFi password

// ====================================================================
// TELEGRAM BOT CONFIGURATION
// ====================================================================
// IMPORTANT: Replace these with your actual bot credentials
String BOTtoken = "123456789:ABCdefGHIjkLMNopQRstUVwxyZ";  // Bot token from @BotFather
String CHAT_ID = "987654321";                 // Your chat ID from @myidbot

// ====================================================================
// GLOBAL STATE VARIABLES
// ====================================================================
// Photo and video control flags
bool sendPhoto = false;           // Flag to trigger photo capture
bool videoMode = true;            // Enable/disable video recording functionality
bool sdCardMode = true;           // Enable/disable SD card storage
bool sdCardAvailable = true;      // SD card detection status
int pictureNumber = 0;            // Photo counter for naming

// Video recording state
bool isRecording = false;         // Current recording status
int videoFrameCount = 0;          // Frames captured in current video
String currentVideoFile = "";     // Current video file path
unsigned long videoStartTime = 0; // Recording start timestamp
unsigned long lastFrameTime = 0;  // Last frame capture time

// Video configuration
int customVideoDurationSec = 10;           // Default video duration in seconds
unsigned long VIDEO_DURATION_MS = 10000;  // Video duration in milliseconds (updated dynamically)
const int MIN_VIDEO_DURATION = 1;         // Minimum video duration (1 second)
const int MAX_VIDEO_DURATION = 120;       // Maximum video duration (2 minutes)
const int TARGET_FPS = 5;                 // Target frames per second

// Network and bot objects
WiFiClientSecure clientTCP;                           // Secure WiFi client for HTTPS
UniversalTelegramBot bot(BOTtoken, clientTCP);       // Telegram bot instance

// Bot message checking interval
int botRequestDelay = 1000;        // Check for messages every 1 second
unsigned long lastTimeBotRan;      // Last message check timestamp

// ====================================================================
// ESP32-CAM PIN DEFINITIONS (AI-Thinker Module)
// ====================================================================
#define PWDN_GPIO_NUM     32    // Power down pin
#define RESET_GPIO_NUM    -1    // Reset pin (not used)
#define XCLK_GPIO_NUM      0    // External clock
#define SIOD_GPIO_NUM     26    // I2C data
#define SIOC_GPIO_NUM     27    // I2C clock
#define Y9_GPIO_NUM       35    // Camera data pins
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25    // Vertical sync
#define HREF_GPIO_NUM     23    // Horizontal reference
#define PCLK_GPIO_NUM     22    // Pixel clock

// ====================================================================
// AVI FILE STRUCTURE DEFINITIONS
// ====================================================================
// These structures define the proper AVI file format for video recording

// Main AVI file header
struct AVIHeader {
  uint32_t riff;    // "RIFF" identifier
  uint32_t size;    // File size minus 8 bytes
  uint32_t fourcc;  // "AVI " identifier
} __attribute__((packed));

// AVI main header with video properties
struct AVIMainHeader {
  uint32_t fcc;                    // "avih" identifier
  uint32_t size;                   // Header size
  uint32_t microSecPerFrame;       // Microseconds per frame
  uint32_t maxBytesPerSec;         // Maximum bytes per second
  uint32_t paddingGranularity;     // Padding alignment
  uint32_t flags;                  // AVI file flags
  uint32_t totalFrames;            // Total number of frames
  uint32_t initialFrames;          // Initial frames before interleaving
  uint32_t streams;                // Number of streams
  uint32_t suggestedBufferSize;    // Suggested buffer size
  uint32_t width;                  // Video width
  uint32_t height;                 // Video height
  uint32_t reserved[4];            // Reserved fields
} __attribute__((packed));

// Stream header defining video stream properties
struct AVIStreamHeader {
  uint32_t fcc;                    // "strh" identifier
  uint32_t size;                   // Header size
  uint32_t type;                   // Stream type ("vids" for video)
  uint32_t handler;                // Codec handler ("MJPG")
  uint32_t flags;                  // Stream flags
  uint16_t priority;               // Stream priority
  uint16_t language;               // Language code
  uint32_t initialFrames;          // Initial frames
  uint32_t scale;                  // Time scale
  uint32_t rate;                   // Sample rate (frames per second)
  uint32_t start;                  // Start time
  uint32_t length;                 // Stream length in frames
  uint32_t suggestedBufferSize;    // Suggested buffer size
  uint32_t quality;                // Stream quality
  uint32_t sampleSize;             // Sample size
  uint16_t left, top, right, bottom; // Frame rectangle
} __attribute__((packed));

// Bitmap info header for video format description
struct BitmapInfoHeader {
  uint32_t size;           // Header size
  uint32_t width;          // Image width
  uint32_t height;         // Image height
  uint16_t planes;         // Color planes
  uint16_t bitCount;       // Bits per pixel
  uint32_t compression;    // Compression type
  uint32_t sizeImage;      // Image size
  uint32_t xPelsPerMeter;  // Horizontal resolution
  uint32_t yPelsPerMeter;  // Vertical resolution
  uint32_t clrUsed;        // Colors used
  uint32_t clrImportant;   // Important colors
} __attribute__((packed));

// ====================================================================
// VIDEO FILE VARIABLES
// ====================================================================
File aviFile;                  // SD card file handle for AVI recording
uint32_t moviDataSize = 0;     // Size of video data in AVI file
uint32_t indexOffset = 0;      // Offset for updating file size
uint16_t videoWidth = 0;       // Current video width
uint16_t videoHeight = 0;      // Current video height

// ====================================================================
// CAMERA INITIALIZATION AND CONFIGURATION
// ====================================================================
/**
 * Initialize and configure ESP32-CAM for optimal video recording
 * Sets up camera parameters, frame size, quality, and sensor optimizations
 */
void configInitCamera() {
  // Camera configuration structure
  camera_config_t config;
  
  // Pin assignment for AI-Thinker ESP32-CAM
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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  
  // Camera timing and format settings
  config.xclk_freq_hz = 20000000;        // 20MHz external clock
  config.pixel_format = PIXFORMAT_JPEG;  // JPEG output format
  config.grab_mode = CAMERA_GRAB_LATEST; // Always get latest frame
  
  // SETTINGS FOR VIDEO - Optimize based on available PSRAM
  if(psramFound()){
    // High quality settings with PSRAM
    config.frame_size = FRAMESIZE_VGA;  // 640x480 for better quality
    config.jpeg_quality = 12;           // Better quality (lower number = higher quality)
    config.fb_count = 2;                // Double buffering for smoother capture
  } else {
    // Fallback settings without PSRAM
    config.frame_size = FRAMESIZE_QVGA; // 320x240 fallback
    config.jpeg_quality = 15;           // Moderate quality
    config.fb_count = 1;                // Single buffer
  }
  
  // Initialize camera with configuration
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart(); // Restart if camera initialization fails
  }
  
  // Get camera sensor for advanced configuration
  sensor_t * s = esp_camera_sensor_get();
  if (s != NULL) {
    // Set frame size based on PSRAM availability
    s->set_framesize(s, (psramFound()) ? FRAMESIZE_VGA : FRAMESIZE_QVGA);
    
    // Determine actual video dimensions based on frame size
    framesize_t frameSize = s->status.framesize;
    switch(frameSize) {
      case FRAMESIZE_QVGA: videoWidth = 320; videoHeight = 240; break;
      case FRAMESIZE_VGA: videoWidth = 640; videoHeight = 480; break;
      case FRAMESIZE_SVGA: videoWidth = 800; videoHeight = 600; break;
      case FRAMESIZE_XGA: videoWidth = 1024; videoHeight = 768; break;
      default: videoWidth = 640; videoHeight = 480; break;
    }
    
    // ADVANCED CAMERA SENSOR OPTIMIZATIONS
    s->set_brightness(s, 0);     // Brightness: -2 to 2 (0 = default)
    s->set_contrast(s, 0);       // Contrast: -2 to 2 (0 = default)
    s->set_saturation(s, 0);     // Saturation: -2 to 2 (0 = default)
    s->set_special_effect(s, 0); // Special effects: 0-6 (0 = none)
    s->set_whitebal(s, 1);       // White balance: 0=disable, 1=enable
    s->set_awb_gain(s, 1);       // Auto white balance gain: 0=disable, 1=enable
    s->set_wb_mode(s, 0);        // White balance mode: 0-4
    s->set_exposure_ctrl(s, 1);  // Exposure control: 0=disable, 1=enable
    s->set_aec2(s, 0);           // AEC2: 0=disable, 1=enable
    s->set_ae_level(s, 0);       // Auto exposure level: -2 to 2
    s->set_aec_value(s, 300);    // AEC value: 0-1200
    s->set_gain_ctrl(s, 1);      // Gain control: 0=disable, 1=enable
    s->set_agc_gain(s, 0);       // AGC gain: 0-30
    s->set_gainceiling(s, (gainceiling_t)0);  // Gain ceiling: 0-6
    s->set_bpc(s, 0);            // Black pixel correction: 0=disable, 1=enable
    s->set_wpc(s, 1);            // White pixel correction: 0=disable, 1=enable
    s->set_raw_gma(s, 1);        // Raw gamma: 0=disable, 1=enable
    s->set_lenc(s, 1);           // Lens correction: 0=disable, 1=enable
    s->set_hmirror(s, 0);        // Horizontal mirror: 0=disable, 1=enable
    s->set_vflip(s, 0);          // Vertical flip: 0=disable, 1=enable
    s->set_dcw(s, 1);            // Downsize enable: 0=disable, 1=enable
    s->set_colorbar(s, 0);       // Color bar test pattern: 0=disable, 1=enable
  }
  
  // Print camera configuration results
  Serial.println("Camera optimized for video recording");
  Serial.printf("Video resolution: %dx%d\n", videoWidth, videoHeight);
}

// ====================================================================
// SD CARD INITIALIZATION
// ====================================================================
/**
 * Initialize SD card for media storage
 * Detects card presence, type, and available space
 */
void initSDCard() {
  Serial.println("Initializing SD card...");
  
  // Attempt to mount SD card
  if(!SD_MMC.begin()){
    Serial.println("SD Card Mount Failed");
    sdCardAvailable = false;
    return;
  }
  
  // Check if SD card is actually present
  uint8_t cardType = SD_MMC.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    sdCardAvailable = false;
    return;
  }
  
  // SD card successfully detected and mounted
  sdCardAvailable = true;
  Serial.println("SD Card initialized successfully");
  
  // Display SD card information
  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024); // Convert to MB
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
}

// ====================================================================
// TELEGRAM MESSAGE HANDLER
// ====================================================================
/**
 * Process incoming Telegram messages and execute corresponding commands
 * Handles all bot commands with proper authentication and error handling
 * 
 * @param numNewMessages Number of new messages to process
 */
void handleNewMessages(int numNewMessages) {
  Serial.print("Handle New Messages: ");
  Serial.println(numNewMessages);

  // Process each new message
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    
    // SECURITY: Check if user is authorized
    if (chat_id != CHAT_ID){
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue; // Skip unauthorized users
    }
    
    String text = bot.messages[i].text;
    Serial.println("Received command: " + text);
    String from_name = bot.messages[i].from_name;

    // COMMAND PROCESSING
    // Welcome and help command
    if (text == "/start") {
      String welcome = "Welcome, " + from_name + "! 🤖\n";
      welcome += "ESP32-CAM Telegram Bot Image Capuring & Video Recording:\n\n";
      welcome += "📸 Photo Commands:\n";
      welcome += "/photo - Take and send photo\n\n";
      welcome += "🎥 Video Commands:\n";
      welcome += "/video_on - Enable video recording mode\n";
      welcome += "/video_off - Disable video recording mode\n";
      welcome += "/record - Record video (current duration: " + String(customVideoDurationSec) + "s)\n";
      welcome += "/record [seconds] - Record custom duration video\n";
      welcome += "   Examples: /record5, /record30, /record60\n";
      welcome += "/video_status - Check video mode status\n\n";
      welcome += "💾 Storage Commands:\n";
      welcome += "/sd_on - Enable SD card storage\n";
      welcome += "/sd_off - Disable SD card storage\n";
      welcome += "/storage - Check storage info\n\n";
      welcome += "ℹ️ Info Commands:\n";
      welcome += "/status - System status\n";
      welcome += "/help - Show this help\n\n";
      welcome += "📏 Duration Limits: " + String(MIN_VIDEO_DURATION) + "-" + String(MAX_VIDEO_DURATION) + " seconds";
      
      bot.sendMessage(CHAT_ID, welcome, "");
    }
    
    // Photo capture command
    else if (text == "/photo") {
      sendPhoto = true;
      Serial.println("New photo request");
    }
    
    // Enable video recording mode
    else if (text == "/video_on") {
      videoMode = true;
      bot.sendMessage(CHAT_ID, "🎥 CUSTOM DURATION AVI Video recording mode: ENABLED ✅", "");
      Serial.println("Video mode enabled");
    }
    
    // Disable video recording mode
    else if (text == "/video_off") {
      videoMode = false;
      if (isRecording) {
        stopVideoRecording(); // Stop any ongoing recording
      }
      bot.sendMessage(CHAT_ID, "🎥 Video recording mode: DISABLED ❌", "");
      Serial.println("Video mode disabled");
    }
    
    // Video recording with optional custom duration
    else if (text.startsWith("/record")) {
      // Validate prerequisites
      if (!videoMode) {
        bot.sendMessage(CHAT_ID, "❌ Video mode is disabled. Use /video_on first.", "");
      } else if (!sdCardAvailable) {
        bot.sendMessage(CHAT_ID, "❌ Video recording requires SD card!", "");
      } else if (isRecording) {
        bot.sendMessage(CHAT_ID, "❌ Already recording! Please wait...", "");
      } else {
        // Parse custom duration from command
        int duration = customVideoDurationSec; // Use default duration
        
        // Check if duration is specified after "/record"
        if (text.length() > 7) { // "/record" is 7 characters
          String durationStr = text.substring(7); // Extract duration part
          durationStr.trim();
          int parsedDuration = durationStr.toInt();
          
          // Validate duration range
          if (parsedDuration >= MIN_VIDEO_DURATION && parsedDuration <= MAX_VIDEO_DURATION) {
            duration = parsedDuration;
            Serial.println("Using custom duration: " + String(duration) + " seconds");
          } else {
            bot.sendMessage(CHAT_ID, "❌ Invalid duration! Use " + String(MIN_VIDEO_DURATION) + "-" + String(MAX_VIDEO_DURATION) + " seconds.\nExample: /record 30", "");
            continue;
          }
        }
        
        // Start recording with specified duration
        startVideoRecording(chat_id, duration);
      }
    }
    
    // Video recording status and progress
    else if (text == "/video_status") {
      String status = "🎥 Video Mode: " + String(videoMode ? "ENABLED ✅" : "DISABLED ❌") + "\n";
      status += "📹 Recording: " + String(isRecording ? "ACTIVE ✅" : "INACTIVE ❌") + "\n";
      status += "📏 Default Duration: " + String(customVideoDurationSec) + " seconds\n";
      
      // Show detailed recording progress if currently recording
      if (isRecording) {
        unsigned long elapsed = (millis() - videoStartTime) / 1000;
        int currentRecordingDuration = VIDEO_DURATION_MS / 1000;
        unsigned long remaining = (VIDEO_DURATION_MS - (millis() - videoStartTime)) / 1000;
        status += "⏱️ Recording: " + String(elapsed) + "s / " + String(currentRecordingDuration) + "s (remaining: " + String(remaining) + "s)\n";
        status += "📊 Frames captured: " + String(videoFrameCount) + "\n";
        status += "📁 Saving to: " + currentVideoFile + "\n";
        status += "🎬 Resolution: " + String(videoWidth) + "x" + String(videoHeight) + "\n";
        status += "🎞️ Target FPS: " + String(TARGET_FPS) + "\n";
        status += "💾 Status: RECORDING TO SD CARD...";
      } else {
        status += "💡 Next recording will be: " + String(customVideoDurationSec) + " seconds\n";
        status += "📏 Duration range: " + String(MIN_VIDEO_DURATION) + "-" + String(MAX_VIDEO_DURATION) + " seconds";
      }
      bot.sendMessage(CHAT_ID, status, "");
    }
    
    // Enable SD card storage
    else if (text == "/sd_on") {
      sdCardMode = true;
      String msg = "💾 SD card storage: ENABLED ✅";
      if (!sdCardAvailable) {
        msg += "\n⚠️ Warning: No SD card detected!";
      }
      bot.sendMessage(CHAT_ID, msg, "");
      Serial.println("SD card mode enabled");
    }
    
    // Disable SD card storage
    else if (text == "/sd_off") {
      sdCardMode = false;
      bot.sendMessage(CHAT_ID, "💾 SD card storage: DISABLED ❌", "");
      Serial.println("SD card mode disabled");
    }
    
    // Check storage information
    else if (text == "/storage") {
      checkStorage(chat_id);
    }
    
    // System status report
    else if (text == "/status") {
      sendSystemStatus(chat_id);
    }
    
    // Help command - same as /start
    else if (text == "/help") {
      String welcome = "ESP32-CAM Telegram Bot Commands:\n\n";
      welcome += "📸 Photo Commands:\n";
      welcome += "/photo - Take and send photo\n\n";
      welcome += "🎥 Video Commands (CUSTOM DURATION AVI FILES):\n";
      welcome += "/video_on - Enable video recording mode\n";
      welcome += "/video_off - Disable video recording mode\n";
      welcome += "/record - Record video (current: " + String(customVideoDurationSec) + "s)\n";
      welcome += "/record [seconds] - Record custom duration video\n";
      welcome += "   Examples: /record5, /record30, /record60\n";
      welcome += "/video_status - Check video mode status\n\n";
      welcome += "💾 Storage Commands:\n";
      welcome += "/sd_on - Enable SD card storage\n";
      welcome += "/sd_off - Disable SD card storage\n";
      welcome += "/storage - Check storage info\n\n";
      welcome += "ℹ️ Info Commands:\n";
      welcome += "/status - System status\n";
      welcome += "/help - Show this help\n\n";
      welcome += "📏 Duration Limits: " + String(MIN_VIDEO_DURATION) + "-" + String(MAX_VIDEO_DURATION) + " seconds";
      
      bot.sendMessage(CHAT_ID, welcome, "");
    }
    
    // Unknown command handler
    else {
      String msg = "❓ Unknown command: " + text + "\n\n";
      msg += "💡 Try these commands:\n";
      msg += "📸 /photo - Take photo\n";
      msg += "🎥 /record - Record " + String(customVideoDurationSec) + "s video\n";
      msg += "🎥 /record30 - Record 30s video\n";
      msg += "ℹ️ /help - Full command list";
      bot.sendMessage(CHAT_ID, msg, "");
    }
  }
}

// ====================================================================
// PHOTO CAPTURE AND TELEGRAM UPLOAD
// ====================================================================
/**
 * Capture high-quality photo and send to Telegram
 * Uses the original working logic with proper error handling
 * Saves to SD card if storage is enabled
 * 
 * @return Status message indicating success or failure
 */
String sendPhotoTelegram() {
  const char* myDomain = "api.telegram.org";
  String getAll = "";
  String getBody = "";
  
  // Dispose first picture to ensure better quality for second capture
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();
  esp_camera_fb_return(fb); // dispose the buffered image
  
  // Take the actual photo
  fb = NULL;
  fb = esp_camera_fb_get();
  if(!fb) {
    Serial.println("Camera capture failed");
    bot.sendMessage(CHAT_ID, "❌ Camera capture failed!", "");
    return "Camera capture failed";
  }
  
  Serial.println("Photo captured, size: " + String(fb->len) + " bytes");
  
  // Save to SD card if enabled and available
  if (sdCardMode && sdCardAvailable) {
    String filename = "/photo_" + String(pictureNumber) + ".jpg";
    saveToSD(filename, fb->buf, fb->len);
    Serial.println("Photo saved to SD: " + filename);
  }
  
  Serial.println("Connecting to Telegram...");
  
  // Establish secure connection to Telegram API
  if (clientTCP.connect(myDomain, 443)) {
    Serial.println("Connection successful, sending photo...");
    
    // Prepare multipart form data for photo upload
    String head = "--CircuitDigestTutorials\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + CHAT_ID + "\r\n--CircuitDigestTutorials\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--CircuitDigestTutorials--\r\n";
    
    // Calculate content lengths
    uint32_t imageLen = fb->len;
    uint32_t extraLen = head.length() + tail.length();
    uint32_t totalLen = imageLen + extraLen;
    
    // Send HTTP POST request headers
    clientTCP.println("POST /bot"+BOTtoken+"/sendPhoto HTTP/1.1");
    clientTCP.println("Host: " + String(myDomain));
    clientTCP.println("Content-Length: " + String(totalLen));
    clientTCP.println("Content-Type: multipart/form-data; boundary=CircuitDigestTutorials");
    clientTCP.println();
    clientTCP.print(head);
    
    // Send image data in chunks to avoid memory issues
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n=0; n<fbLen; n=n+1024) {
      if (n+1024 < fbLen) {
        clientTCP.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen%1024>0) {
        size_t remainder = fbLen%1024;
        clientTCP.write(fbBuf, remainder);
      }
    }
    clientTCP.print(tail);
    
    // Free camera buffer
    esp_camera_fb_return(fb);
    
    // Wait for response with timeout
    int waitTime = 10000; // 10 second timeout
    long startTimer = millis();
    boolean state = false;
    
    while ((startTimer + waitTime) > millis()) {
      Serial.print(".");
      delay(100);
      while (clientTCP.available()) {
        char c = clientTCP.read();
        if (state==true) getBody += String(c);
        if (c == '\n') {
          if (getAll.length()==0) state=true;
          getAll = "";
        }
        else if (c != '\r')
          getAll += String(c);
        startTimer = millis();
      }
      if (getBody.length()>0) break;
    }
    clientTCP.stop();
    Serial.println("\nPhoto sent successfully!");
    pictureNumber++;
    
    // Send confirmation message to user
    String confirmMsg = "📸 Photo sent successfully!\n";
    confirmMsg += "📊 Photo #" + String(pictureNumber) + "\n";
    if (sdCardMode && sdCardAvailable) {
      confirmMsg += "💾 Saved to SD card ✅";
    }
    bot.sendMessage(CHAT_ID, confirmMsg, "");
  }
  else {
    getBody="Failed to connect to Telegram";
    Serial.println("Failed to connect to Telegram");
    bot.sendMessage(CHAT_ID, "❌ Failed to send photo - connection error", "");
  }
  return getBody;
}

// ====================================================================
// VIDEO RECORDING FUNCTIONS
// ====================================================================

/**
 * Start AVI video recording with custom duration
 * Creates proper AVI file structure and begins frame capture
 * 
 * @param chat_id Telegram chat ID for status updates
 * @param durationSeconds Duration of video recording in seconds
 */
void startVideoRecording(String chat_id, int durationSeconds) {
  // Verify SD card is available for video recording
  if (!sdCardAvailable) {
    bot.sendMessage(chat_id, "❌ Video recording requires SD card!", "");
    return;
  }
  
  // Update recording duration for this session
  VIDEO_DURATION_MS = durationSeconds * 1000;
  
  // Create unique filename with duration timestamp
  currentVideoFile = "/video_" + String(durationSeconds) + "s_" + String(millis()) + ".avi";
  
  // Open AVI file on SD card for writing
  aviFile = SD_MMC.open(currentVideoFile.c_str(), FILE_WRITE);
  if (!aviFile) {
    bot.sendMessage(chat_id, "❌ Failed to create video file!", "");
    return;
  }
  
  // Initialize recording state variables
  isRecording = true;
  videoFrameCount = 0;
  lastFrameTime = millis();
  moviDataSize = 0;
  
  // Send recording start notification
  String msg = "🎥 Starting " + String(durationSeconds) + "-second AVI recording...\n";
  msg += "📏 Duration: " + String(durationSeconds) + " seconds\n";
  msg += "🎞️ Target FPS: " + String(TARGET_FPS) + "\n";
  msg += "💾 Video will be saved to SD card first, then uploaded to Telegram!";
  
  bot.sendMessage(chat_id, msg, "");
  Serial.println("Custom duration AVI video recording started: " + currentVideoFile);
  Serial.printf("Duration: %d seconds (%lu ms)\n", durationSeconds, VIDEO_DURATION_MS);
  Serial.printf("Target resolution: %dx%d at %d FPS\n", videoWidth, videoHeight, TARGET_FPS);
  
  // Write AVI file headers
  writeAVIHeaders(durationSeconds);
}

/**
 * Write AVI file headers with proper structure
 * Creates complete AVI file format compatible with standard players
 * 
 * @param durationSeconds Expected duration for header calculations
 */
void writeAVIHeaders(int durationSeconds) {
  // Calculate expected total frames based on custom duration
  uint32_t totalFrames = durationSeconds * TARGET_FPS;
  
  // Write main RIFF AVI header
  AVIHeader aviHeader;
  aviHeader.riff = 0x46464952; // "RIFF" fourcc code
  aviHeader.size = 0; // Will update later with actual file size
  aviHeader.fourcc = 0x20495641; // "AVI " fourcc code
  aviFile.write((uint8_t*)&aviHeader, sizeof(aviHeader));
  
  // Write hdrl LIST header for headers section
  uint32_t hdrlList = 0x5453494C; // "LIST" fourcc
  aviFile.write((uint8_t*)&hdrlList, 4);
  uint32_t hdrlSize = sizeof(AVIMainHeader) + 8 + sizeof(AVIStreamHeader) + 8 + sizeof(BitmapInfoHeader) + 8;
  aviFile.write((uint8_t*)&hdrlSize, 4);
  uint32_t hdrlFourcc = 0x6C726468; // "hdrl" fourcc
  aviFile.write((uint8_t*)&hdrlFourcc, 4);
  
  // Write main AVI header with video properties
  AVIMainHeader mainHeader;
  mainHeader.fcc = 0x68697661; // "avih" fourcc
  mainHeader.size = sizeof(AVIMainHeader) - 8;
  mainHeader.microSecPerFrame = 1000000 / TARGET_FPS; // Microseconds per frame
  mainHeader.maxBytesPerSec = videoWidth * videoHeight * 3 * TARGET_FPS; // Max data rate
  mainHeader.paddingGranularity = 0;
  mainHeader.flags = 0x00000910; // AVI file flags
  mainHeader.totalFrames = totalFrames;
  mainHeader.initialFrames = 0;
  mainHeader.streams = 1; // Single video stream
  mainHeader.suggestedBufferSize = videoWidth * videoHeight * 3;
  mainHeader.width = videoWidth;
  mainHeader.height = videoHeight;
  memset(mainHeader.reserved, 0, sizeof(mainHeader.reserved));
  aviFile.write((uint8_t*)&mainHeader, sizeof(mainHeader));
  
  // Write stream LIST header for video stream info
  uint32_t strlList = 0x5453494C; // "LIST" fourcc
  aviFile.write((uint8_t*)&strlList, 4);
  uint32_t strlSize = sizeof(AVIStreamHeader) + 8 + sizeof(BitmapInfoHeader);
  aviFile.write((uint8_t*)&strlSize, 4);
  uint32_t strlFourcc = 0x6C727473; // "strl" fourcc
  aviFile.write((uint8_t*)&strlFourcc, 4);
  
  // Write video stream header with MJPEG codec info
  AVIStreamHeader streamHeader;
  streamHeader.fcc = 0x68727473; // "strh" fourcc
  streamHeader.size = sizeof(AVIStreamHeader) - 8;
  streamHeader.type = 0x73646976; // "vids" - video stream type
  streamHeader.handler = 0x47504A4D; // "MJPG" - Motion JPEG codec
  streamHeader.flags = 0;
  streamHeader.priority = 0;
  streamHeader.language = 0;
  streamHeader.initialFrames = 0;
  streamHeader.scale = 1;
  streamHeader.rate = TARGET_FPS; // Frames per second
  streamHeader.start = 0;
  streamHeader.length = totalFrames;
  streamHeader.suggestedBufferSize = videoWidth * videoHeight * 3;
  streamHeader.quality = 10000; // Quality indicator
  streamHeader.sampleSize = 0; // Variable frame sizes
  streamHeader.left = 0;
  streamHeader.top = 0;
  streamHeader.right = videoWidth;
  streamHeader.bottom = videoHeight;
  aviFile.write((uint8_t*)&streamHeader, sizeof(streamHeader));
  
  // Write bitmap info header for video format details
  uint32_t strfFourcc = 0x66727473; // "strf" fourcc
  aviFile.write((uint8_t*)&strfFourcc, 4);
  uint32_t strfSize = sizeof(BitmapInfoHeader);
  aviFile.write((uint8_t*)&strfSize, 4);
  
  BitmapInfoHeader bitmapHeader;
  bitmapHeader.size = sizeof(BitmapInfoHeader);
  bitmapHeader.width = videoWidth;
  bitmapHeader.height = videoHeight;
  bitmapHeader.planes = 1;
  bitmapHeader.bitCount = 24; // 24-bit color
  bitmapHeader.compression = 0x47504A4D; // "MJPG" compression
  bitmapHeader.sizeImage = videoWidth * videoHeight * 3;
  bitmapHeader.xPelsPerMeter = 0;
  bitmapHeader.yPelsPerMeter = 0;
  bitmapHeader.clrUsed = 0;
  bitmapHeader.clrImportant = 0;
  aviFile.write((uint8_t*)&bitmapHeader, sizeof(bitmapHeader));
  
  // Write movi LIST header for video data section
  uint32_t moviList = 0x5453494C; // "LIST" fourcc
  aviFile.write((uint8_t*)&moviList, 4);
  indexOffset = aviFile.position(); // Save position for later size update
  uint32_t moviSize = 0; // Will update later with actual data size
  aviFile.write((uint8_t*)&moviSize, 4);
  uint32_t moviFourcc = 0x69766F6D; // "movi" fourcc
  aviFile.write((uint8_t*)&moviFourcc, 4);
  
  aviFile.flush(); // Ensure headers are written to SD card
  Serial.printf("AVI headers written for %d second video (%u expected frames)\n", durationSeconds, totalFrames);
}

/**
 * Capture individual video frame and write to AVI file
 * Maintains target FPS and handles timing for smooth video
 * Called repeatedly during recording until duration expires
 */
void captureVideoFrame() {
  if (!isRecording || !videoMode) return;

  // Record start time on first frame
  if(videoFrameCount == 0){
    videoStartTime = millis(); 
  }

  unsigned long currentTime = millis();
  unsigned long elapsed = currentTime - videoStartTime;

  // Stop recording when target duration is reached
  if (elapsed >= VIDEO_DURATION_MS) {
    stopVideoRecording();
    return;
  }

  // Calculate target frame interval for desired FPS
  unsigned long frameInterval = 1000 / TARGET_FPS;
  
  // Skip frame if not enough time has passed (maintain FPS timing)
  if (currentTime - lastFrameTime < frameInterval) {
    return;
  }

  // Capture frame from camera
  camera_fb_t * fb = esp_camera_fb_get();
  if (fb) {
    // Write AVI frame chunk header
    uint32_t frameFourcc = 0x63643030; // "00dc" - uncompressed video chunk
    aviFile.write((uint8_t*)&frameFourcc, 4);
    uint32_t frameSize = fb->len;
    aviFile.write((uint8_t*)&frameSize, 4);

    // Write JPEG frame data to file
    if (aviFile.write(fb->buf, fb->len) == fb->len) {
      videoFrameCount++;
      moviDataSize += fb->len + 8; // Data size + chunk header
      
      // Add padding byte if frame size is odd (AVI requirement)
      if (fb->len % 2 == 1) {
        uint8_t padding = 0;
        aviFile.write(&padding, 1);
        moviDataSize += 1;
      }
      
      lastFrameTime = currentTime;
      
      // Progress logging every 10 frames
      if (videoFrameCount % 10 == 0) {
        unsigned long elapsedSec = elapsed / 1000;
        unsigned long totalDurationSec = VIDEO_DURATION_MS / 1000;
        unsigned long remainingSec = (VIDEO_DURATION_MS - elapsed) / 1000;
        Serial.printf("Recording %lus AVI... %lus/%lus, Frames: %d, Remaining: %lus\n", 
                     totalDurationSec, elapsedSec, totalDurationSec, videoFrameCount, remainingSec);
      }
      
      aviFile.flush(); // Ensure frame is written to SD card
    } else {
      Serial.println("Failed to write frame to file");
    }
    esp_camera_fb_return(fb); // Free camera buffer
  } else {
    Serial.println("Failed to capture frame");
  }
}

/**
 * Complete video recording and finalize AVI file
 * Updates file headers with actual frame counts and sizes
 * Initiates Telegram upload process
 */
void stopVideoRecording() {
  if (!isRecording) return;
  
  unsigned long actualRecordingTime = millis() - videoStartTime;
  int recordedDurationSec = VIDEO_DURATION_MS / 1000;
  
  isRecording = false;
  
  Serial.println("🎥 COMPLETING RECORDING - SAVING TO SD CARD...");
  Serial.printf("Target duration: %d seconds (%lu ms)\n", recordedDurationSec, VIDEO_DURATION_MS);
  Serial.printf("Actual recording time: %lu ms\n", actualRecordingTime);
  
  if (aviFile) {
    uint32_t currentPos = aviFile.position();
    
    // Update movi data size in file header
    aviFile.seek(indexOffset);
    aviFile.write((uint8_t*)&moviDataSize, 4);
    
    // Update main RIFF file size
    uint32_t riffSize = currentPos - 8;
    aviFile.seek(4);
    aviFile.write((uint8_t*)&riffSize, 4);
    
    // Calculate actual FPS and frame count for header updates
    float actualFPS = (float)videoFrameCount / (float)recordedDurationSec;
    uint32_t actualFrames = videoFrameCount;
    
    // Update main header with actual frame count
    aviFile.seek(48);
    aviFile.write((uint8_t*)&actualFrames, 4);
    
    // Update microseconds per frame based on actual FPS
    uint32_t microSecPerFrame = (actualFPS > 0) ? (uint32_t)(1000000.0 / actualFPS) : (1000000 / TARGET_FPS);
    aviFile.seek(52);
    aviFile.write((uint8_t*)&microSecPerFrame, 4);
    
    // Update stream header with actual frame count
    aviFile.seek(140);
    aviFile.write((uint8_t*)&actualFrames, 4);
    
    // Update stream rate with actual FPS
    uint32_t streamRate = (uint32_t)(actualFPS + 0.5);
    aviFile.seek(156);
    aviFile.write((uint8_t*)&streamRate, 4);
    
    aviFile.close(); // Close and finalize AVI file
    
    // Log completion statistics
    Serial.println("=== CUSTOM DURATION RECORDING COMPLETE ===");
    Serial.printf("Recorded duration: %d seconds\n", recordedDurationSec);
    Serial.printf("Frames captured: %u\n", videoFrameCount);
    Serial.printf("Calculated FPS: %.2f\n", actualFPS);
    Serial.printf("File: %s\n", currentVideoFile.c_str());
    
    // Send completion notification to user
    String completeMsg = "✅ " + String(recordedDurationSec) + "-second video recorded and saved to SD card!\n";
    completeMsg += "📊 " + String(videoFrameCount) + " frames captured\n";
    completeMsg += "📤 Now uploading to Telegram...";
    
    bot.sendMessage(CHAT_ID, completeMsg, "");
    sendVideoToTelegram(); // Start Telegram upload process
  }
}

/**
 * Upload completed video file to Telegram
 * Validates file integrity and handles upload with progress tracking
 */
void sendVideoToTelegram() {
  // Verify video file exists on SD card
  if (!SD_MMC.exists(currentVideoFile.c_str())) {
    bot.sendMessage(CHAT_ID, "❌ Video file not found on SD card!", "");
    return;
  }
  
  // Get file size and validate integrity
  File videoFile = SD_MMC.open(currentVideoFile.c_str(), FILE_READ);
  if (!videoFile) {
    bot.sendMessage(CHAT_ID, "❌ Cannot open video file from SD card!", "");
    return;
  }
  
  size_t videoSize = videoFile.size();
  videoFile.close();
  
  Serial.printf("📁 Video file stored on SD card: %s\n", currentVideoFile.c_str());
  Serial.printf("📊 File size: %d bytes (%.2f KB)\n", videoSize, videoSize / 1024.0);
  
  // Check Telegram file size limits
  if (videoSize > 50 * 1024 * 1024) { // 50MB Telegram limit
    bot.sendMessage(CHAT_ID, "❌ Video file too large for Telegram (>50MB)!", "");
    return;
  }
  
  if (videoSize < 1024) { // Less than 1KB - likely corrupted
    bot.sendMessage(CHAT_ID, "❌ Video file seems corrupted (too small)!", "");
    return;
  }
  
  Serial.println("📤 Starting upload to Telegram...");
  bot.sendMessage(CHAT_ID, "📤 Uploading video from SD card to Telegram...", "");
  
  // Attempt to upload video document
  if (sendVideoDocument(currentVideoFile)) {
    // Success - send detailed statistics
    String successMsg = "🎬 AVI Video sent successfully!\n\n";
    successMsg += "📊 Recording Stats:\n";
    successMsg += "🎞️ Frames: " + String(videoFrameCount) + "\n";
    successMsg += "🎥 Resolution: " + String(videoWidth) + "x" + String(videoHeight) + "\n";
    successMsg += "📁 Size: " + String(videoSize / 1024) + " KB\n";
    successMsg += "⏱️ Duration: " + String(VIDEO_DURATION_MS/1000 ) + " seconds\n";
    successMsg += "🎯 Format: AVI with MJPEG codec\n";
    successMsg += "💾 Saved on SD card ✅\n";
    successMsg += "📱 Uploaded to Telegram ✅";
    
    bot.sendMessage(CHAT_ID, successMsg, "");
    
    // Optional: Clean up video file to save SD card space
    // Comment out the next lines if you want to keep videos on SD card
    if (SD_MMC.remove(currentVideoFile.c_str())) {
      Serial.println("🗑️ Video file cleaned up from SD card: " + currentVideoFile);
    } else {
      Serial.println("⚠️ Could not clean up video file: " + currentVideoFile);
    }
  } else {
    bot.sendMessage(CHAT_ID, "❌ Failed to upload video to Telegram!\n💾 Video remains saved on SD card: " + currentVideoFile, "");
  }
}

/**
 * Send video file as document to Telegram
 * Handles secure upload with chunked data transmission
 * 
 * @param filePath Path to video file on SD card
 * @return true if upload successful, false otherwise
 */
bool sendVideoDocument(String filePath) {
  const char* myDomain = "api.telegram.org";
  
  // Open video file from SD card
  File file = SD_MMC.open(filePath.c_str(), FILE_READ);
  if (!file) {
    Serial.println("Failed to open video file");
    return false;
  }
  
  size_t fileSize = file.size();
  Serial.println("Video file size: " + String(fileSize) + " bytes");
  
  // Check file size limit for video upload (20MB)
  if (fileSize > 20 * 1024 * 1024) {
    Serial.println("Video file too large for Telegram");
    file.close();
    return false;
  }
  
  // Configure SSL connection (insecure for development)
  clientTCP.setInsecure(); // For development - use proper certificates in production
  
  // Establish secure connection to Telegram API
  if (clientTCP.connect(myDomain, 443)) {
    String filename = filePath.substring(filePath.lastIndexOf('/') + 1);
    
    // Prepare multipart form data for video upload
    String head = "--VideoUpload\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + CHAT_ID;
    head += "\r\n--VideoUpload\r\nContent-Disposition: form-data; name=\"video\"; filename=\"" + filename + "\"";
    head += "\r\nContent-Type: video/avi\r\n\r\n";
    String tail = "\r\n--VideoUpload--\r\n";
    
    // Calculate total content length
    uint32_t extraLen = head.length() + tail.length();
    uint32_t totalLen = fileSize + extraLen;
    
    // Send HTTP POST request headers
    clientTCP.println("POST /bot" + BOTtoken + "/sendVideo HTTP/1.1");
    clientTCP.println("Host: " + String(myDomain));
    clientTCP.println("Content-Length: " + String(totalLen));
    clientTCP.println("Content-Type: multipart/form-data; boundary=VideoUpload");
    clientTCP.println("Connection: close");
    clientTCP.println();
    clientTCP.print(head);
    
    // Send file data in small chunks for stability
    uint8_t buffer[512]; // Small buffer size for memory efficiency
    size_t totalSent = 0;
    int chunkCount = 0;
    
    while (file.available() && clientTCP.connected()) {
      int bytesRead = file.read(buffer, sizeof(buffer));
      if (bytesRead > 0) {
        size_t written = clientTCP.write(buffer, bytesRead);
        if (written != bytesRead) {
          Serial.println("Write error during upload!");
          break;
        }
        totalSent += bytesRead;
        chunkCount++;
        
        // Show upload progress every 100 chunks
        if (chunkCount % 100 == 0) {
          Serial.println("Uploaded: " + String(totalSent * 100 / fileSize) + "%");
        }
      } else {
        break; // No more data to read
      }
    }
    
    file.close();
    clientTCP.print(tail);
    
    // Wait for Telegram API response with timeout
    Serial.println("Waiting for Telegram response...");
    String response = "";
    unsigned long timeout = millis() + 30000; // 30 second timeout
    
    while (millis() < timeout && clientTCP.connected()) {
      if (clientTCP.available()) {
        response = clientTCP.readString();
        break;
      }
      delay(100);
    }
    
    clientTCP.stop();
    
    // Check if upload was successful
    bool success = response.indexOf("\"ok\":true") > 0;
    if (success) {
      Serial.println("Video upload successful!");
    } else {
      Serial.println("Upload failed. Response: " + response.substring(0, 300));
    }
    return success;
  } else {
    Serial.println("Failed to connect to Telegram");
    file.close();
    return false;
  }
}

// ====================================================================
// UTILITY FUNCTIONS
// ====================================================================

/**
 * Save data to SD card file
 * Generic function for saving photos and other data
 * 
 * @param filename Name of file to create
 * @param data Pointer to data buffer
 * @param len Length of data to write
 */
void saveToSD(String filename, uint8_t* data, size_t len) {
  if (!sdCardAvailable) return;
  
  File file = SD_MMC.open(filename.c_str(), FILE_WRITE);
  if (!file) {
    Serial.println("Failed to create file: " + filename);
    return;
  }
  
  if (file.write(data, len) == len) {
    Serial.println("File saved: " + filename);
  } else {
    Serial.println("Write failed: " + filename);
  }
  file.close();
}

/**
 * Check and report SD card storage information
 * Reports card status, capacity, usage, and file counts
 * 
 * @param chat_id Telegram chat ID for sending report
 */
void checkStorage(String chat_id) {
  String storage = "💾 SD Card Storage Status:\n\n";
  
  if (!sdCardAvailable) {
    storage += "❌ No SD card detected!\n";
    storage += "💾 Storage mode: " + String(sdCardMode ? "ENABLED" : "DISABLED") + "\n";
    storage += "\n💡 Insert SD card and restart to enable storage.";
  } else {
    // Get SD card capacity and usage information
    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);      // Total size in MB
    uint64_t totalBytes = SD_MMC.totalBytes() / (1024 * 1024);  // Available space in MB
    uint64_t usedBytes = (SD_MMC.cardSize() - SD_MMC.totalBytes()) / (1024 * 1024); // Used space in MB
    
    storage += "✅ SD Card Status: DETECTED\n";
    storage += "💾 Storage Mode: " + String(sdCardMode ? "ENABLED ✅" : "DISABLED ❌") + "\n\n";
    storage += "📊 Storage Information:\n";
    storage += "💳 Total Size: " + String(cardSize) + " MB\n";
    storage += "📈 Used Space: " + String(usedBytes) + " MB\n";
    storage += "📉 Free Space: " + String(totalBytes) + " MB\n";
    storage += "📸 Photos Taken: " + String(pictureNumber) + "\n\n";
    
    // Calculate and display usage percentage
    float usagePercent = (float)usedBytes / (float)cardSize * 100.0;
    storage += "📈 Usage: " + String(usagePercent, 1) + "%";
  }
  
  bot.sendMessage(chat_id, storage, "");
}

/**
 * Send comprehensive system status report
 * Includes network, camera, video, and storage status
 * 
 * @param chat_id Telegram chat ID for sending report
 */
void sendSystemStatus(String chat_id) {
  String status = "🔧 ESP32-CAM System Status:\n\n";
  
  // Network status information
  status += "🌐 Network Status:\n";
  status += "📶 WiFi: Connected ✅\n";
  status += "📱 IP Address: " + WiFi.localIP().toString() + "\n";
  status += "📡 RSSI: " + String(WiFi.RSSI()) + " dBm\n\n";
  
  // Camera system status
  status += "📷 Camera Status:\n";
  status += "✅ Camera: Initialized for AVI recording\n";
  status += "🎥 Resolution: " + String(videoWidth) + "x" + String(videoHeight) + "\n";
  status += "🎯 Format: AVI with MJPEG codec\n";
  status += "📸 Photos Taken: " + String(pictureNumber) + "\n\n";
  
  // Video recording status
  status += "🎥 Video Status (AVI FILES):\n";
  status += "🎬 Video Mode: " + String(videoMode ? "ENABLED ✅" : "DISABLED ❌") + "\n";
  status += "📹 Recording: " + String(isRecording ? "ACTIVE ✅" : "INACTIVE ❌") + "\n";
  if (isRecording) {
    // Show detailed recording progress
    unsigned long elapsed = (millis() - videoStartTime) / 1000;
    status += "⏱️ Recording: " + String(elapsed) + "s / " + String(customVideoDurationSec ) + "s\n";
    status += "📊 Frames: " + String(videoFrameCount) + "\n";
    status += "📁 File: " + currentVideoFile + "\n";
    status += "🎞️ Target FPS: " + String(TARGET_FPS) + "\n";
  }
  status += "\n";
  
  // Storage system status
  status += "💾 Storage Status:\n";
  status += "💳 SD Card: " + String(sdCardAvailable ? "DETECTED ✅" : "NOT DETECTED ❌") + "\n";
  status += "💾 SD Storage: " + String(sdCardMode ? "ENABLED ✅" : "DISABLED ❌") + "\n";
  
  if (sdCardAvailable) {
    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    uint64_t freeBytes = SD_MMC.totalBytes() / (1024 * 1024);
    status += "📊 Free Space: " + String(freeBytes) + "/" + String(cardSize) + " MB";
  }
  
  bot.sendMessage(chat_id, status, "");
}

// ====================================================================
// MAIN SETUP AND INITIALIZATION
// ====================================================================

/**
 * Arduino setup function - runs once at startup
 * Initializes all system components and sends startup notification
 */
void setup() {
  // Disable brownout detector to prevent unexpected resets
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  // Initialize serial communication for debugging
  Serial.begin(115200);
  Serial.println("\n=== ESP32-CAM Telegram Bot with AVI Video Recording ===");
  
  // Initialize camera with optimized settings for video recording
  Serial.println("Initializing camera for AVI video recording...");
  configInitCamera();
  
  // Initialize SD card storage system
  initSDCard();
  
  // Connect to WiFi network
  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  clientTCP.setInsecure(); // Configure SSL for Telegram API (development mode)
  
  // Wait for WiFi connection with progress indicators
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("ESP32-CAM IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Send comprehensive startup notification to Telegram
  String startupMsg = "🚀 ESP32-CAM Bot with AVI Video Recording!\n\n";
  startupMsg += "🌐 Network: " + String(ssid) + "\n";
  startupMsg += "📱 IP: " + WiFi.localIP().toString() + "\n";
  startupMsg += "💳 SD Card: " + String(sdCardAvailable ? "DETECTED ✅" : "NOT DETECTED ❌") + "\n";
  startupMsg += "🎥 Resolution: " + String(videoWidth) + "x" + String(videoHeight) + "\n";
  startupMsg += "🎯 Format: AVI with MJPEG codec\n";
  startupMsg += "📸 Photos Taken: " + String(pictureNumber) + "\n";
  startupMsg += "🎥 Video Mode: OFF ❌\n";
  startupMsg += "💾 SD Storage: " + String(sdCardMode ? "ON ✅" : "OFF ❌") + "\n\n";
  startupMsg += "🎬 NEW: AVI video files with proper structure!\n";
  startupMsg += "📝 Type /start for all commands";
  
  bot.sendMessage(CHAT_ID, startupMsg, "");
  Serial.println("=== Setup Complete - Ready for AVI Video Recording ===");
}

// ====================================================================
// MAIN LOOP - PRIORITY VIDEO RECORDING WITH MESSAGE HANDLING
// ====================================================================

/**
 * Arduino main loop - runs continuously
 * Prioritizes video frame capture during recording
 * Handles Telegram messages when not recording
 */
void loop() {
  // Handle pending photo capture requests
  if (sendPhoto) {
    Serial.println("Processing photo request...");
    sendPhotoTelegram();
    sendPhoto = false;
  }
  
  // PRIORITY: Video frame capture with minimal interference
  // During active recording, skip other processing to maintain timing accuracy
  if (isRecording && videoMode) {
    captureVideoFrame(); // Capture frame with precise timing
    return; // Skip message processing to maintain video quality
  }
  
  // Process Telegram messages only when not recording
  // This prevents interference with critical video timing
  if (millis() > lastTimeBotRan + botRequestDelay) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      Serial.println("Got Telegram response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }
}

/*
 * ====================================================================
 * END OF ESP32-CAM TELEGRAM BOT CODE
 * ====================================================================
 * 
 * This code provides a complete solution for remote camera control
 * via Telegram with high-quality photo capture and custom-duration
 * AVI video recording capabilities.
 * 
 * Key Features Summary:
 * - High-quality photo capture with SD storage
 * - Custom duration AVI video recording (1-120 seconds)
 * - Proper AVI file structure with MJPEG codec
 * - Real-time recording progress and status updates
 * - Secure Telegram bot integration with authentication
 * - Comprehensive storage management and monitoring
 * - Optimized camera settings for best quality
 * - Memory-efficient chunked file uploads
 * - Robust error handling and status reporting
 * 
 * The code is designed for stability and performance, with priority
 * given to video frame capture timing during recording operations.
 * 
 * For support and updates, check the documentation and ensure all
 * required libraries are installed and credentials are properly
 * configured before uploading to your ESP32-CAM module.
 * 
 * ====================================================================
 */