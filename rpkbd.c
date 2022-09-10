#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"
//#include "bsp/board.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "keyboard_matrix.h"


void hid_task(void);

  #include "pico/time.h"
  static inline uint32_t board_millis(void)
  {
    return to_ms_since_boot(get_absolute_time());
  }


bi_decl(bi_program_description("rpkbd"));
bi_decl(bi_program_description("Firmware for RP2040-based keyboard driver on a Unicomp Model M keyboard."));
bi_decl(bi_program_version_string("0.0.1.meh"));
bi_decl(bi_program_build_date_string("September 2022 I guess."));
bi_decl(bi_1pin_with_name(PICO_DEFAULT_UART_TX_PIN, "UART TX"));
bi_decl(bi_1pin_with_name(PICO_DEFAULT_UART_RX_PIN, "UART RX"));





int main() {
    //set_sys_clock_48mhz();
    set_sys_clock_pll(768000000, 4, 2); // 96 MHz
    //set_sys_clock_pll(768000000, 4, 4); // 48 MHz but slightly lower power
    stdio_init_all(); // Enable UART
    tusb_init();
//    keyboard_init_matrix(); // Initialized upon USB mount.
    puts("Device init 2.\n");
    while (1) {
        __wfi();
        tud_task();
        hid_task();
    }
}


//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
    keyboard_init_matrix();
    puts("Device mount event.\n");
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    keyboard_deinit_matrix();
    puts("Device unmount event.\n");
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    (void) remote_wakeup_en;
    keyboard_deinit_matrix();
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    keyboard_init_matrix();
}


//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

// Every 10ms, we will sent 1 report for each HID profile (keyboard, mouse etc ..)
// tud_hid_report_complete_cb() is used to send the next report after previous one is complete
void hid_task(void)
{
    // Poll every 10ms
    const uint32_t interval_ms = 10;
    static uint32_t start_ms = 0;

    if ( board_millis() - start_ms < interval_ms) return; // not enough time
    if (!check_scan_complete_flag() || scan_in_progress())
        return;
    clear_scan_complete_flag();
    start_ms += interval_ms;
    
    keyboard_scan_matrix();
    keyboard_send_next_report();
}

// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint8_t len)
{
    (void) instance;
    (void) len;
    keyboard_send_next_report();
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    // TODO not Implemented
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;

    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
    (void) instance;

    if (report_type == HID_REPORT_TYPE_OUTPUT)
    {
        // Set keyboard LED e.g Capslock, Numlock etc...
        if (report_id == REPORT_ID_KEYBOARD)
        {
            // bufsize should be (at least) 1
            if ( bufsize < 1 ) return;
            keyboard_set_leds(buffer[0]);
        }
    }
}
