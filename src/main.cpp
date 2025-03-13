#include "main.h"

#ifndef DEVICE_ID
#define DEVICE_ID 0
#endif

#ifndef RELAY_ON_TIME
#define RELAY_ON_TIME 5000
#endif

#define LED_ON_TIME 5000

#define PIN_RELAY 10
#define PIN_1WIRE 15
#define PIN_TX 1
#define PIN_DE 3
#define PIN_RE 2
#define PIN_LED_RED A2
#define PIN_LED_GREEN A3

#define LED_STATE_OFF 0
#define LED_STATE_GREEN 1
#define LED_STATE_RED 2

#define IBUTTON_SEARCH_INTERVAL 50
#define IBUTTON_KEEP_INTERVAL 2000  // fotget ibutton after xx ms if not present anymore


int hdlc_read_byte();
void hdlc_write_byte(uint8_t data);
void hdlc_message_handler(uint8_t *data, uint16_t length);

HDLC<&hdlc_read_byte, &hdlc_write_byte, 16, CRC16_CCITT> hdlc;
IButtonReader ibutton_reader(PIN_1WIRE, IBUTTON_SEARCH_INTERVAL, IBUTTON_KEEP_INTERVAL);

unsigned long previous_millis;

unsigned long relay_on_millis = 0;
bool relay_state = false;

unsigned long led_on_millis = 0;
byte led_state = LED_STATE_OFF;


void hdlc_write_byte(uint8_t data) {
    Serial1.write(data);
}

int hdlc_read_byte() {
    return Serial1.read();
}

void hdlc_receive() {
    if(hdlc.receive() != 0U) {
        uint8_t buff[hdlc.RXBFLEN];
        uint16_t size = hdlc.copyReceivedMessage(buff);
        hdlc_message_handler(buff, size);
    }
}

void hdlc_message_handler(uint8_t *data, uint16_t length) {
    debug_print("message(");
    debug_print(length);
    debug_print("): ");
    for(byte i = 0; i < length; i++) {
        debug_print(data[i], HEX);
    }
    debug_println("");
    if(length < 3) {
        debug_println("too short");
        return;
    }
    if(data[0] != PROTOCOL_VERSION) {
        debug_println("invalid protocol");
        return;
    }
    if(data[1] != DEVICE_ID) {
        debug_println("invalid device id");
        return;
    }
    switch(data[2]) {
        case COMMAND_PING:
            if(length != sizeof(CommandPing) + 2) {
                debug_println("invalid command length");
                return;
            }
            debug_println("ping");
            hdlc_handle_ping((CommandPing *) &data[2]);
            break;
        case COMMAND_GET_STATUS:
            if(length != sizeof(CommandGetStatus) + 2) {
                debug_println("invalid command length");
                return;
            }
            debug_println("get status");
            hdlc_handle_get_status((CommandGetStatus *) &data[2]);
            break;
        case COMMAND_UNLOCK_DOOR:
            if(length != sizeof(CommandUnlockDoor) + 2) {
                debug_println("invalid command length");
                return;
            }
            debug_println("unlock door");
            hdlc_handle_unlock_door((CommandUnlockDoor *) &data[2]);
            break;
        case COMMAND_REJECT_KEY:
            if(length != sizeof(CommandRejectKey) + 2) {
                debug_println("invalid command length");
                return;
            }
            debug_println("reject key");
            hdlc_handle_reject_key((CommandRejectKey *) &data[2]);
            break;
        default:
            debug_println("invalid command");
    }
}

void hdlc_transmit_response(void *response, size_t length) {
    digitalWrite(PIN_DE, HIGH);
    digitalWrite(PIN_RE, HIGH);
    hdlc.transmitStart();
    hdlc.transmitByte(PROTOCOL_VERSION);
    hdlc.transmitByte(DEVICE_ID);
    hdlc.transmitBytes(response, length);
    hdlc.transmitEnd();
    Serial1.flush();
    digitalWrite(PIN_DE, LOW);
    digitalWrite(PIN_RE, LOW);
}

void hdlc_handle_ping(CommandPing *command) {
    ResponsePong response;
    memcpy(response.pong_data, command->ping_data, sizeof(command->ping_data));
    hdlc_transmit_response(&response, sizeof(response));
}

void hdlc_handle_get_status(CommandGetStatus *command) {
    ResponseStatus response;
    response.ibutton_available = ibutton_reader.ibutton_is_available;
    response.family_code = ibutton_reader.family_code;
    memcpy(&response.serial_number, &ibutton_reader.serial_number, sizeof(ibutton_reader.serial_number));
    hdlc_transmit_response(&response, sizeof(response));
}

void hdlc_handle_unlock_door(CommandUnlockDoor *command) {
    debug_println("relay on");
    relay_state = true;
    digitalWrite(PIN_RELAY, HIGH);
    relay_on_millis = millis();

    debug_println("green led on");
    led_state = LED_STATE_GREEN;
    digitalWrite(PIN_LED_RED, LOW);
    digitalWrite(PIN_LED_GREEN, HIGH);
    led_on_millis = millis();

    ResponseUnlockDoor response;
    hdlc_transmit_response(&response, sizeof(response));
}

void hdlc_handle_reject_key(CommandRejectKey *command) {
    debug_println("red led on");
    led_state = LED_STATE_RED;
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED, HIGH);
    led_on_millis = millis();

    ResponseRejectKey response;
    hdlc_transmit_response(&response, sizeof(response));
}


void setup(void) {
    pinMode(PIN_RELAY, OUTPUT);
    pinMode(PIN_DE, OUTPUT);
    pinMode(PIN_RE, OUTPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_TX, OUTPUT);

    digitalWrite(PIN_RELAY, LOW);
    digitalWrite(PIN_DE, LOW);
    digitalWrite(PIN_RE, LOW);
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED, LOW);

    Serial1.begin(115200);

    debug_init();
    debug_println("starting");
}

void loop(void) {
    hdlc_receive();
    ibutton_reader.update();

    unsigned long current_millis = millis();
    if (current_millis - previous_millis >= 100) {
        previous_millis = current_millis;
        if (ibutton_reader.ibutton_is_available) {
            debug_print("serial number: ");
            byte i;
            for (i = 0; i < SERIAL_NUMBER_LENGTH; i++) {
                debug_print(ibutton_reader.serial_number[i], HEX);
                debug_print(" ");
            }
            debug_print("  family code: 0x");
            debug_println(ibutton_reader.family_code, HEX);
        }
    }

    if(relay_state) {
        digitalWrite(PIN_RELAY, HIGH);
        if(current_millis - relay_on_millis >= RELAY_ON_TIME) {
            debug_println("relay off");
            relay_state = false;
        }
    } else {
        digitalWrite(PIN_RELAY, LOW);
    }

    if(led_state == LED_STATE_GREEN || led_state == LED_STATE_RED) {
        if(led_state == LED_STATE_GREEN) {
            digitalWrite(PIN_LED_GREEN, HIGH);
        }
        if(led_state == LED_STATE_RED) {
            digitalWrite(PIN_LED_RED, HIGH);
        }
        if(current_millis - led_on_millis >= LED_ON_TIME) {
            debug_println("led off");
            led_state = LED_STATE_OFF;
        }
    } else {
        digitalWrite(PIN_LED_GREEN, LOW);
        digitalWrite(PIN_LED_RED, LOW);
    }
}
