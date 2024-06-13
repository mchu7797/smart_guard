/*
 * Created by Minseok Chu on 2024-06-09.
 */

#ifndef SMART_GUARD_SERVER_TEST_H
#define SMART_GUARD_SERVER_TEST_H

#define SERVER_IP "192.168.50.95"
#define SERVER_PORT 12877
#define CLIENT_ID 1

#define PING_COMMAND_INIT 0
#define PING_COMMAND_EXIT 2
#define PING_COMMAND_WARNING 1

int send_ping(int sock, int ping_command, int client_id,
              const char *image_path);

#endif // SMART_GUARD_SERVER_TEST_H