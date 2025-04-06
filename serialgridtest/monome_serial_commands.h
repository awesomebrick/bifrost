// defines the command names and hex codes for each command.
// for ease of coding the firmware, and just quicker reference.


#define sys_query {0x00} //request device information
#define sys_id {0x01} //request device ID string
#define sys_size {0x05} //request grid size
#define sys_request_firmware_version {0x0F}

// led-grid
#define grid_set_led_off {0x10}
#define grid_set_led_on {0x11}
#define grid_led_set_all_off {0x12}
#define grid_led_set_all_on {0x13}
#define grid_led_set_row_off 
#define grid_led_set_row_on
#define grid_led_set_col_off
#define grid_led_set_col_on


