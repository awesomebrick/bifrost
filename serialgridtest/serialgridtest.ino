
// USB stack -> TinyUSB , CPU: 240MHz
//
#include "pio_usb.h"           // Pico-PIO-USB library
#include "Adafruit_TinyUSB.h"  // TinyUSB library

#define HOST_PIN_DP PIN_USB_HOST_DP  // GPIO number for USB D+ pin (D- is the next pin)
#define MAXLEDCOUNT 256 // Maximum number of LEDs (for 16x16 grid)

Adafruit_USBH_Host USBHost;  // USB host object
static uint8_t cdc_idx = 0;  // CDC device index

// Global variables
uint8_t leds[MAXLEDCOUNT] = {0}; // Array to store LED states
uint8_t grid_width = 8;          // Grid width
uint8_t grid_height = 8;         // Grid height
bool gridDirty = false;          // Flag to indicate grid update required

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  Serial.println("Grid - LED and Key Handling Demo");
}

void loop() {
  // Main loop (Core 0)
  delay(10);
}

// Setup PIO USB and TinyUSB on Core1
void setup1() {
  Serial.println("Core1: TinyUSB host with PIO-USB");

  // Configure PIO USB
  pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
  pio_cfg.pin_dp = HOST_PIN_DP;            // Specify D+ pin
  USBHost.configure_pio_usb(1, &pio_cfg);  // Setup PIO USB
  USBHost.begin(1);                        // Start TinyUSB host
}

void loop1() {
  USBHost.task();  // Run USB host tasks
  
  // Check incoming data
  check_incoming_data();
  
  // Refresh grid if dirty
  if (gridDirty) {
    refresh_grid();
  }
  
  delay(1); // Short delay
}

// Check incoming data from CDC
void check_incoming_data() {
  uint8_t buffer[64];
  uint32_t bytes_read = tuh_cdc_read(cdc_idx, buffer, sizeof(buffer));
  
  if (bytes_read > 0) {
    // Process data
    process_monome_data(buffer, bytes_read);
  }
}

// Process Monome data
void process_monome_data(uint8_t* buffer, uint32_t length) {
  if (length < 1) return;
  
  uint8_t cmd = buffer[0];
  
  switch (cmd) {
    case 0x03:  // grid size query
      if (length >= 3) {
        uint8_t xsize = buffer[1];
        uint8_t ysize = buffer[2];
        Serial.printf("Grid size detected: %dx%d\r\n", xsize, ysize);
        grid_width = xsize; //update grid size values for this grid.
        grid_height = ysize;
        Serial.printf("Grid size set: %dx%d\r\n", grid_width, grid_height);
      }
      break;

    case 0x20:  // Grid key up
      if (length >= 3) {
        uint8_t x = buffer[1];
        uint8_t y = buffer[2];
        Serial.printf("Grid key up: x=%d, y=%d\r\n", x, y);
      }
      break;
    
    case 0x21:  // Grid key down
      if (length >= 3) {
        uint8_t x = buffer[1];
        uint8_t y = buffer[2];
        Serial.printf("Grid key down: x=%d, y=%d\r\n", x, y);
        // Toggle LED when key is pressed
        toggle_led(x, y);
      }
      break;
  }
}

// Toggle LED state
void toggle_led(uint8_t x, uint8_t y) {
  if (x >= grid_width || y >= grid_height) return;
  
  uint32_t index = y * grid_width + x;
  if (index < MAXLEDCOUNT) {
    // Invert LED state (0 -> 15, else -> 0)
    leds[index] = (leds[index] == 0) ? 15 : 0;
    gridDirty = true;
    
    Serial.printf("Toggled LED at (%d,%d) to %s\r\n", 
                  x, y, (leds[index] > 0) ? "ON" : "OFF");
  }
}

// Set a single LED
void set_led(uint8_t x, uint8_t y, uint8_t brightness) {
  if (x >= grid_width || y >= grid_height) return;
  
  uint32_t index = y * grid_width + x;
  if (index < MAXLEDCOUNT) {
    leds[index] = brightness;
    gridDirty = true;
  }
}

// Directly set LED via USB
void set_led_direct(uint8_t x, uint8_t y, uint8_t brightness) {
  if (x >= grid_width || y >= grid_height) return;
  
  uint8_t cmd[3];
  if (brightness > 0) {
    cmd[0] = 0x11; // LED ON
  } else {
    cmd[0] = 0x10; // LED OFF
  }
  cmd[1] = x;
  cmd[2] = y;
  
  tuh_cdc_write(cdc_idx, cmd, sizeof(cmd));
  tuh_cdc_write_flush(cdc_idx);
}

// Clear all LEDs
void clear_all_leds() {
  uint8_t cmd[] = { 0x12 }; // ALL LEDs OFF
  tuh_cdc_write(cdc_idx, cmd, sizeof(cmd));
  tuh_cdc_write_flush(cdc_idx);
  
  // Clear internal buffer
  memset(leds, 0, sizeof(leds));
  gridDirty = false;
  
  Serial.println("All LEDs cleared");
}

// Turn on all LEDs
void set_all_leds() {
  uint8_t cmd[] = { 0x13 }; // ALL LEDs ON
  tuh_cdc_write(cdc_idx, cmd, sizeof(cmd));
  tuh_cdc_write_flush(cdc_idx);
  
  // Update internal buffer
  memset(leds, 15, sizeof(leds));
  gridDirty = false;
  
  Serial.println("All LEDs set");
}

// Refresh entire grid
void refresh_grid() {
  // Update by 8x8 blocks
  for (uint8_t y_offset = 0; y_offset < grid_height; y_offset += 8) {
    for (uint8_t x_offset = 0; x_offset < grid_width; x_offset += 8) {
      update_grid_block(x_offset, y_offset);
    }
  }
  
  gridDirty = false;
  Serial.println("Grid refreshed");
}

// Update 8x8 grid block
void update_grid_block(uint8_t x_offset, uint8_t y_offset) {
  // level/map command for 8x8 block
  uint8_t buf[35];
  buf[0] = 0x1A;        // level/map command
  buf[1] = x_offset;    // x offset
  buf[2] = y_offset;    // y offset
  
  int ind = 3;
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x += 2) {
      uint8_t x_pos = x_offset + x;
      uint8_t y_pos = y_offset + y;
      
      uint32_t led1_index = y_pos * grid_width + x_pos;
      uint32_t led2_index = y_pos * grid_width + (x_pos + 1);
      
      uint8_t val1 = (x_pos < grid_width && y_pos < grid_height && led1_index < MAXLEDCOUNT) ? leds[led1_index] : 0;
      uint8_t val2 = ((x_pos + 1) < grid_width && y_pos < grid_height && led2_index < MAXLEDCOUNT) ? leds[led2_index] : 0;
      
      buf[ind++] = (val1 << 4) | val2;  // Store brightness in upper and lower 4 bits
    }
  }
  
  // Send data
  tuh_cdc_write(cdc_idx, buf, sizeof(buf));
  tuh_cdc_write_flush(cdc_idx);
}

// Display test pattern
void show_test_pattern() {
  Serial.println("Displaying test pattern...");
  
  // Clear all LEDs
  clear_all_leds();
  delay(500);
  
  // Turn on all LEDs
  set_all_leds();
  delay(1000);
  
  // Clear all LEDs again
  clear_all_leds();
  delay(500);
  
  // Diagonal pattern
  for (int i = 0; i < min(grid_width, grid_height); i++) {
    set_led(i, i, 15);
  }
  gridDirty = true;
  refresh_grid();
  delay(1000);
  
  // Cross pattern
  clear_all_leds();
  for (int i = 0; i < grid_width; i++) {
    set_led(i, grid_height/2, 15);
  }
  for (int i = 0; i < grid_height; i++) {
    set_led(grid_width/2, i, 15);
  }
  gridDirty = true;
  refresh_grid();
  delay(1000);
  
  // Random pattern
  clear_all_leds();
  for (int i = 0; i < 16; i++) {
    uint8_t x = random(grid_width);
    uint8_t y = random(grid_height);
    set_led(x, y, 15);
  }
  gridDirty = true;
  refresh_grid();
  
  Serial.println("Test pattern complete");
}

// Called when CDC device is mounted
void tuh_cdc_mount_cb(uint8_t idx) {
  Serial.printf("CDC mounted, idx = %d\r\n", idx);
  cdc_idx = idx;       // Save index
  
  delay(200); // Wait for initialization
  
  // Send system query
  uint8_t query_cmd[] = { 0x00 };
  tuh_cdc_write(cdc_idx, query_cmd, sizeof(query_cmd));
  tuh_cdc_write_flush(cdc_idx);
  Serial.println("Sent system query");
  delay(100);

  check_incoming_data();
  
  delay(100);
  
  // Send size query
  uint8_t size_cmd[] = { 0x05 };
  tuh_cdc_write(cdc_idx, size_cmd, sizeof(size_cmd));
  tuh_cdc_write_flush(cdc_idx);
  Serial.println("Sent size query");
  delay(100);
  check_incoming_data();
  
  delay(100);
  
  // Show test pattern
  //show_test_pattern();
  clear_all_leds();
  
  Serial.println("Grid ready for interaction. Press keys to toggle LEDs.");
}

// Called when CDC device is unmounted
void tuh_cdc_umount_cb(uint8_t idx) {
  Serial.printf("CDC unmounted, idx = %d\r\n", idx);
  if (idx == cdc_idx) {
    cdc_idx = 0;
    // Reset internal state
    memset(leds, 0, sizeof(leds));
    gridDirty = false;
  }
}