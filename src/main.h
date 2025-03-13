#ifndef IBUTTON_SCHLIESSSYSTEM_MAIN_H
#define IBUTTON_SCHLIESSSYSTEM_MAIN_H

#include <Arduino.h>

#include <HDLC.h>
#include <CRC16_CCITT.h>

#include "IButtonReader.h"
#include "hdlc_commands.h"


#define DEBUG 1


#ifdef DEBUG
#   define debug_init() Serial.begin(115200);
#   define debug_println(...) Serial.println(__VA_ARGS__)
#   define debug_print(...) Serial.print(__VA_ARGS__)
#else
#   define debug_init() (void)0
#   define debug_println(...) (void)0
#   define debug_print(...) (void)0
#endif

void hdlc_handle_ping(CommandPing *command);
void hdlc_handle_get_status(CommandGetStatus *command);
void hdlc_handle_unlock_door(CommandUnlockDoor *command);
void hdlc_handle_reject_key(CommandRejectKey *command);


#endif //IBUTTON_SCHLIESSSYSTEM_MAIN_H
