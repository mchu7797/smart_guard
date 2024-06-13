/*
 * Created by Minseok Chu on 2024-06-06.
 */

#ifndef SMART_GUARD_SERVER_H
#define SMART_GUARD_SERVER_H

#include <pthread.h>
#include <signal.h>
#include <stdbool.h>

#define SERVER_PORT 12877
#define SERVER_MAX_CLIENTS 10
#define SERVER_MAX_RETRY_COUNT 5

#define PING_COMMAND_INIT 0
#define PING_COMMAND_WARNING 1
#define PING_COMMAND_EXIT 2

#define LOOP_DELAY 3

typedef struct {
  int socket;
  pthread_t thread;
} ClientInfo;

int ServerSocket;
ClientInfo Clients[SERVER_MAX_CLIENTS];
pthread_mutex_t ClientsMutex;

void interruptSignalHandler(int Signal);

void removeClient(int clientSocket);

void alertWarning(long ClientId, char *ImagePath);

void *handlePing(void *Arg);

#endif // SMART_GUARD_SERVER_H