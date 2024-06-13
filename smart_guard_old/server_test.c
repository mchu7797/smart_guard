/*
 * Created by Minseok Chu on 2024-06-09.
 */

#include "server_test.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "packet.h"

int send_ping(int sock, int ping_command, int client_id,
              const char *image_path) {
  struct PingRequest request;
  struct PingResponse response;

  memset(&request, 0, sizeof(request));
  memset(&response, 0, sizeof(response));

  request.PingCommand = ping_command;
  request.ClientId = client_id;

  if (ping_command == PING_COMMAND_INIT || ping_command == PING_COMMAND_EXIT) {
    send(sock, &request, sizeof(request), 0);
    recv(sock, &response, sizeof(response), 0);
    return response.IsOk;
  } else {
    FILE *image_file = fopen(image_path, "rb");
    if (image_file == NULL) {
      return 0;
    }

    fseek(image_file, 0, SEEK_END);
    request.ImageSize = ftell(image_file);
    rewind(image_file);

    send(sock, &request, sizeof(request), 0);
    recv(sock, &response, sizeof(response), 0);

    if (!response.IsOk) {
      fclose(image_file);
      return 0;
    }

    char *image_data = (char *)malloc(request.ImageSize);
    fread(image_data, request.ImageSize, 1, image_file);
    send(sock, image_data, request.ImageSize, 0);
    recv(sock, &response, sizeof(response), 0);

    free(image_data);
    fclose(image_file);

    return response.IsOk;
  }
}

int main() {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1) {
    perror("socket");
    exit(1);
  }

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
  server_addr.sin_port = htons(SERVER_PORT);

  if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
      -1) {
    perror("connect");
    exit(1);
  }

  if (!send_ping(sock, PING_COMMAND_INIT, CLIENT_ID, NULL)) {
    printf("초기화 실패\n");
    close(sock);
    return 1;
  }

  printf("초기화 완료\n");

  const char *image_path = "./test_image.png";

  if (!send_ping(sock, PING_COMMAND_WARNING, CLIENT_ID, image_path)) {
    printf("Warning Ping 전송 실패\n");
  } else {
    printf("Warning Ping 전송 성공\n");
  }

  if (!send_ping(sock, PING_COMMAND_EXIT, CLIENT_ID, NULL)) {
    printf("종료 실패\n");
  } else {
    printf("종료 완료\n");
  }

  close(sock);
  return 0;
}