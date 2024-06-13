/*
 * Created by Minseok Chu on 2024-06-06.
 */

#ifndef SMART_GUARD_CLIENT_H
#define SMART_GUARD_CLIENT_H

#include <gpiod.h>
#include <signal.h>
#include <stdbool.h>

#define SERVER_PORT 12877

#define BH1750_ADDR 0x23
#define BH1750_POWER_ON 0x01
#define BH1750_CONTINUOUS_HIGH_RES_MODE 0x10

#define HC_SR04_TRIGGER_PIN 17
#define HC_SR04_ECHO_PIN 18

#define LUX_THRESHOLD 30
#define DISTANCE_THRESHOLD 30

#define PING_COMMAND_INIT 0
#define PING_COMMAND_WARNING 1
#define PING_COMMAND_EXIT 2

#define PING_IMAGE_BASE_DIRECTORY "./Image/"

/* Five Seconds */
#define LOOP_DELAY 5

#define I2C_DEFAULT_TIMEOUT 2000

volatile sig_atomic_t IsRunning = true;

void interruptSignalHandler();

double readLux(int SensorFile);

double readDistance(struct gpiod_line *TriggerPin, struct gpiod_line *EchoPin);

bool sendPing(int Socket, int PingCommand, long ClientId, char *ImagePath);

#endif // SMART_GUARD_CLIENT_H
