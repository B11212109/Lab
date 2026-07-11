#include "at32f403a_407_board.h"
#include "at32f403a_407_clock.h"

#include "OLED_UI.h"
#include "OLED_UI_Launcher.h"
#include "OLED_UI_MenuData.h"

int main(void)
{
    system_clock_config();
    delay_init();

    OLED_UI_Init(&MainMenuPage);

    while (1) {
        OLED_UI_MainLoop();

        /*
         * Run the OLED UI service at 100 Hz for smoother menu animation.
         */
        OLED_UI_InterruptHandler();
        OLED_UI_Tick(10U);
        delay_ms(10U);
    }
}
