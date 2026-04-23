#include "bsp/display.h"
#include "matrix_audio.h"
#include "matrix_font_5x7.h"
#include "matrix_qmi.h"
#include "matrix_rtc.h"
#include "matrix_sdcard.h"
#include "matrix_shtc3.h"
#include "matrix_wifi.h"
#include "matrix_rgbw.h"

#define MATRIX_EXAMPLE_RGBW         0
#define MATRIX_EXAMPLE_FONT_5X7     0
#define MATRIX_EXAMPLE_QMI          0
#define MATRIX_EXAMPLE_RTC          0
#define MATRIX_EXAMPLE_SDCARD       0
#define MATRIX_EXAMPLE_SHTC3        0
#define MATRIX_EXAMPLE_WIFI         0
#define MATRIX_EXAMPLE_AUDIO        0

void app_main(void) {
    init_display();
    
#if MATRIX_EXAMPLE_RGBW
    rgbw_start();
#elif MATRIX_EXAMPLE_FONT_5X7
    font_5x7_start();
#elif MATRIX_EXAMPLE_WIFI
    wifi_start();
#elif MATRIX_EXAMPLE_SHTC3
    shtc3_start();
#elif MATRIX_EXAMPLE_SDCARD
    sdcard_start();
#elif MATRIX_EXAMPLE_RTC
    rtc_start();
#elif MATRIX_EXAMPLE_QMI
    qmi_start();
#elif MATRIX_EXAMPLE_AUDIO
    audio_start();
#elif MATRIX_EXAMPLE_SPECTRUM
    spectrum_start();
#endif
}
