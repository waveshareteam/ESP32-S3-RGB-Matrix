/*********************************************************************
 * AnimatedGif LED Matrix Panel example where the GIFs are
 * stored on a SD card connected to the ESP32-S3 using
 * SD_MMC 1-bit mode.
 *
 * Put the gifs into a directory called 'gifs' (case sensitive) on
 * a FAT32 formatted SDcard.
 ********************************************************************/
#include "FS.h"
#include "SD_MMC.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h> 
#include <AnimatedGIF.h>

/**** SD_MMC (1-bit) GPIO mappings ****/
#define BSP_SD_D0       17
#define BSP_SD_CMD      44
#define BSP_SD_CLK      1


/***************************************************************
 * HUB 75 LED DMA Matrix Panel Configuration
 **************************************************************/
#define PANEL_RES_X 64      // Number of pixels wide of each INDIVIDUAL panel module. 
#define PANEL_RES_Y 32     // Number of pixels tall of each INDIVIDUAL panel module.
#define PANEL_CHAIN 1      // Total number of panels chained one to another

/**************************************************************/

AnimatedGIF gif;
MatrixPanel_I2S_DMA *dma_display = nullptr;

static int totalFiles = 0; // GIF files count

static File FSGifFile; // temp gif file holder
static File GifRootFolder; // directory listing

std::vector<std::string> GifFiles; // GIF files path

const int maxGifDuration    = 30000; // ms, max GIF duration

#include "gif_functions.hpp"
#include "sdcard_functions.hpp"
 

/**************************************************************/
void draw_test_patterns();
int gifPlay( const char* gifPath )
{ // 0=infinite

  if( ! gif.open( gifPath, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw ) ) {
    log_n("Could not open gif %s", gifPath );
  }

  Serial.print("Playing: "); Serial.println(gifPath);

  int frameDelay = 0; // store delay for the last frame
  int then = 0; // store overall delay

  while (gif.playFrame(true, &frameDelay)) {

    then += frameDelay;
    if( then > maxGifDuration ) { // avoid being trapped in infinite GIF's
      //log_w("Broke the GIF loop, max duration exceeded");
      break;
    }
  }

  gif.close();

  return then;
}


void setup()
{
    Serial.begin(115200);

    // **************************** Setup SD Card access via SD_MMC 1-bit ****************************
    if (!SD_MMC.setPins(BSP_SD_CLK, BSP_SD_CMD, BSP_SD_D0)) {
        Serial.println("SD_MMC setPins Failed");
        return;
    }

    if(!SD_MMC.begin("/sdcard", true)){
        Serial.println("Card Mount Failed");
        return;
    }
    uint8_t cardType = SD_MMC.cardType();

    if(cardType == CARD_NONE){
        Serial.println("No SD card attached");
        return;
    }

    Serial.print("SD Card Type: ");
    if(cardType == CARD_MMC){
        Serial.println("MMC");
    } else if(cardType == CARD_SD){
        Serial.println("SDSC");
    } else if(cardType == CARD_SDHC){
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }

    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);

    //listDir(SD_MMC, "/", 1, false);

    Serial.printf("Total space: %lluMB\n", SD_MMC.totalBytes() / (1024 * 1024));
    Serial.printf("Used space: %lluMB\n", SD_MMC.usedBytes() / (1024 * 1024));



    // **************************** Setup DMA Matrix ****************************
    HUB75_I2S_CFG mxconfig(
      PANEL_RES_X,   // module width
      PANEL_RES_Y,   // module height
      PANEL_CHAIN    // Chain length
    );

    // Keep ESP32-S3 default HUB75 mapping to avoid Flash/PSRAM reserved pins.

    //mxconfig.clkphase = false;
    //mxconfig.driver = HUB75_I2S_CFG::FM6126A;

    // Display Setup
    dma_display = new MatrixPanel_I2S_DMA(mxconfig);

    // Allocate memory and start DMA display
    if( not dma_display->begin() )
        Serial.println("****** !KABOOM! HUB75 memory allocation failed ***********");
 
    dma_display->setBrightness8(128); //0-255
    dma_display->clearScreen();


    // **************************** Setup Sketch ****************************
    Serial.println("Starting AnimatedGIFs Sketch");

    // SD CARD STOPS WORKING WITH DMA DISPLAY ENABLED>...

    File root = SD_MMC.open("/gifs");
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(!file.isDirectory())
        {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());

            std::string filename = "/gifs/" + std::string(file.name());
            Serial.println(filename.c_str());
            
            GifFiles.push_back( filename );
         //   Serial.println("Adding to gif list:" + String(filename));
            totalFiles++;
    
        }
        file = root.openNextFile();
    }

    file.close();
    Serial.printf("Found %d GIFs to play.", totalFiles);
    //totalFiles = getGifInventory("/gifs");



  // This is important - Set the right endianness.
  gif.begin(LITTLE_ENDIAN_PIXELS);

}

void loop(){

      // Iterate over a vector using range based for loop
    for(auto & elem : GifFiles)
    {
        gifPlay( elem.c_str() );
        gif.reset();
        delay(500);
    }

}

void draw_test_patterns()
{
 // fix the screen with green
  dma_display->fillRect(0, 0, dma_display->width(), dma_display->height(), dma_display->color444(0, 15, 0));
  delay(500);

  // draw a box in yellow
  dma_display->drawRect(0, 0, dma_display->width(), dma_display->height(), dma_display->color444(15, 15, 0));
  delay(500);

  // draw an 'X' in red
  dma_display->drawLine(0, 0, dma_display->width()-1, dma_display->height()-1, dma_display->color444(15, 0, 0));
  dma_display->drawLine(dma_display->width()-1, 0, 0, dma_display->height()-1, dma_display->color444(15, 0, 0));
  delay(500);

  // draw a blue circle
  dma_display->drawCircle(10, 10, 10, dma_display->color444(0, 0, 15));
  delay(500);

  // fill a violet circle
  dma_display->fillCircle(40, 21, 10, dma_display->color444(15, 0, 15));
  delay(500);
  delay(1000);

}
