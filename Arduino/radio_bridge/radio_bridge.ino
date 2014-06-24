#include <SPI.h>

#include <RFM69.h>
#include <RFM69Config.h>

#define B_START 255
#define B_END 254
#define B_OK 253
#define B_ERR_UNKNOWN 210
#define B_ERR_TO 211  // time out
#define B_ERR_OOR 212  // escape out of range
#define B_ERR_START 213  // unexpected start byte
#define B_ERR_END 214  // end without start
#define B_ERR_OVERFLOW 215 // buffer overflow
#define B_ESCAPE 200

uint8_t rfm_power = 20; // dBmW
boolean led = false;

RFM69 radio(9.6);

void setup() {
  Serial.begin(115200);

  while (!radio.init()){
    delay(100);
  }
  
  radio.setMode(RFM69_MODE_RX);

  pinMode(13, OUTPUT);
  led = true;
  toggleLed();
}

void toggleLed() {
  digitalWrite(13, led ? HIGH : LOW);

  led = !led;
}

void loop() {
  if (radio.checkRx()) {
    toggleLed();
    
    uint8_t buf[64];
    uint8_t len = sizeof(buf);

    radio.recv(buf, &len);

    uint8_t out[64 * 2 + 4];
    int out_offset = 0;

    out[out_offset++] = B_START;  // start

    for(int i=0; i<len; i++) {
      if (buf[i] >= B_ESCAPE) { // need to escape
        out[out_offset++] = B_ESCAPE; // escape
        out[out_offset++] = buf[i] - B_ESCAPE;
      }
      else {
        out[out_offset++] = buf[i];
      }
    }
    out[out_offset++] = B_END; // end

    Serial.write(out, out_offset);

    toggleLed();
  }
  else if (Serial.available()) {
    toggleLed();

    uint8_t out[64];
    int out_offset = 0;
    boolean escape = false, ok = false, first = true;
    int fail_code = B_ERR_UNKNOWN;
    
    unsigned long start = millis();
    
    for(;;) {
      unsigned long diff = 0;
      
      while(!Serial.available() && diff < 1000) {
        diff = millis() - start;
      }
      
      if (diff >= 1000) {
        fail_code = B_ERR_TO;
        break;
      }
      
      int c = Serial.read();
      
      if (c == -1)
        continue;
        
      if (escape) {
        int temp = c + B_ESCAPE;
        
        if (temp > 255) {
          fail_code = B_ERR_OOR;
          break;
        }
          
        if (out_offset == 64) {
          fail_code = B_ERR_OVERFLOW;
          break;
        }

        out[out_offset++] = temp;
        
        escape = false;
      }
      else if (c == B_START) {  // start
        if (!first) {
          fail_code = B_ERR_START;
          break;
        }
          
        first = false;
      }
      else if (c == B_END) {  // end
        if (first) {
          fail_code = B_ERR_END;
          break;
        }
        ok = true;
        break;
      }
      else if (c == B_ESCAPE)  // escape
        escape = true;
      else {
        if (out_offset == 64) {
          fail_code = B_ERR_OVERFLOW;
          break;
        }
        
        out[out_offset++] = c;
      }

    }
    
    if (ok) {
      radio.setMode(RFM69_MODE_TX);
      radio.send((uint8_t *)out, out_offset, rfm_power);
      delay(1);
      radio.setMode(RFM69_MODE_RX);
      delay(9);
  
      unsigned char x_ok = B_OK;
      Serial.write(&x_ok, 1);
    }
    else {
      
      unsigned char x_fail = (unsigned char)fail_code;
      Serial.write(&x_fail, 1);
    }

    toggleLed();
  }
}

