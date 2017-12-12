#include "main.h"
#include "hdlc_commands.h"

#define DEVICE_ID 1

#define RELAY_ON_TIME 4000

#define PIN_RELAY 13
#define PIN_1WIRE 6
#define PIN_TX 1

#define IBUTTON_SEARCH_INTERVAL 50
#define IBUTTON_KEEP_INTERVAL 2000  // fotget ibutton after xx ms if not present anymore

int hdlc_read_byte();
void hdlc_write_byte(uint8_t data);
void hdlc_message_handler(uint8_t *data, uint16_t length);

HDLC<hdlc_read_byte, hdlc_write_byte, 16, CRC16_CCITT> hdlc;
IButtonReader ibutton_reader(PIN_1WIRE, IBUTTON_SEARCH_INTERVAL, IBUTTON_KEEP_INTERVAL);

unsigned long previous_millis;

unsigned long relay_on_millis = 0;
bool relay_state = false;

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
        default:
            debug_println("invalid command");
    }
}

void hdlc_transmit_response(void *response, size_t length) {
    hdlc.transmitStart();
    hdlc.transmitByte(PROTOCOL_VERSION);
    hdlc.transmitByte(DEVICE_ID);
    hdlc.transmitBytes(response, length);
    hdlc.transmitEnd();
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
    ResponseUnlockDoor response;
    debug_println("relay on");
    relay_state = true;
    digitalWrite(PIN_RELAY, HIGH);
    relay_on_millis = millis();
    hdlc_transmit_response(&response, sizeof(response));
}


void setup(void) {
    pinMode(PIN_RELAY, OUTPUT);
    digitalWrite(PIN_RELAY, LOW);

    pinMode(PIN_TX, OUTPUT); // Serial port TX to output
    Serial1.begin(9600);

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
}
