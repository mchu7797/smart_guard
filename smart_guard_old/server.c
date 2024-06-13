/*
 * Created by Minseok Chu on 2024-06-06.
 */

#include "server.h"

#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "packet.h"

void interruptSignalHandler(int Signal) {
  printf("Received signal %d. Shutting down the server...\n", Signal);

  pthread_mutex_lock(&ClientsMutex);

  for (int i = 0; i < SERVER_MAX_CLIENTS; ++i) {
    if (Clients[i].socket != 0) {
      shutdown(Clients[i].socket, SHUT_RDWR);
      close(Clients[i].socket);
    }
  }

  shutdown(ServerSocket, SHUT_RDWR);
  close(ServerSocket);

  pthread_mutex_unlock(&ClientsMutex);
  pthread_mutex_destroy(&ClientsMutex);

  exit(0);
}

void removeClient(int clientSocket) {
  pthread_mutex_lock(&ClientsMutex);

  for (int i = 0; i < SERVER_MAX_CLIENTS; ++i) {
    if (Clients[i].socket == clientSocket) {
      Clients[i].socket = 0;
      Clients[i].thread = 0;
      break;
    }
  }

  pthread_mutex_unlock(&ClientsMutex);
}

void alertWarning(long ClientId, char *ImagePath) {
  // TODO: 이미지 띄우는 코드 집어넣기

  printf("[WARNING] Thief detected from client %ld!\n", ClientId);
  printf("[IMAGE] %s\n", ImagePath);
}

void *handlePing(void *Arg) {
  /* Packets */
  struct PingRequest Request;
  struct PingResponse Response;

  /* Network stuffs */
  int ClientSocket = *(int *)Arg;
  long ClientId = -1;
  ssize_t ReceiveSize, TempReceiveSize;
  int RetryCount;

  /* Image stuffs */
  char ImagePath[256];
  unsigned char *ImageBuffer;
  unsigned long ImageSize;
  unsigned long FileSize;
  FILE *ImageFile;

  while (true) {
    memset(&Request, 0, sizeof(Request));
    ReceiveSize = recv(ClientSocket, &Request, sizeof(Request), 0);

    /* Handle connection closed */
    if (ReceiveSize <= 0) {
      printf("Client %ld disconnected!\n", ClientId);
      break;
    }

    /* Make response */
    switch (Request.PingCommand) {
    case PING_COMMAND_INIT:
      ClientId = Request.ClientId;
      Response.IsOk = true;
      break;
    case PING_COMMAND_EXIT:
      Response.IsOk = (Request.ClientId == ClientId) ? true : false;
      break;
    case PING_COMMAND_WARNING:
      Response.IsOk = (Request.ClientId == ClientId) ? true : false;
      ImageSize = Request.ImageSize;
      break;
    default:
      Response.IsOk = false;
      break;
    }

    RetryCount = 0;

    while (send(ClientSocket, &Response, sizeof(Response), 0) <
               sizeof(Response) &&
           RetryCount < SERVER_MAX_RETRY_COUNT) {
      ++RetryCount;
    }

    if (RetryCount == SERVER_MAX_RETRY_COUNT) {
      printf("Connection bad! Shutting down connection!\n");
      break;
    }

    if (Request.PingCommand == PING_COMMAND_INIT) {
      printf("Client %ld Connected!\n", ClientId);
    }

    if (Request.PingCommand == PING_COMMAND_WARNING) {
      ImageBuffer = (unsigned char *)malloc(ImageSize);
      ReceiveSize = 0;

      while (ReceiveSize < ImageSize) {
        TempReceiveSize = recv(ClientSocket, ImageBuffer + ReceiveSize,
                               ImageSize - ReceiveSize, 0);

        if (TempReceiveSize <= 0) {
          printf("Client %ld connection bad! Shutting down!\n", ClientId);

          free(ImageBuffer);
          break;
        }

        ReceiveSize += TempReceiveSize;
      }

      if (ReceiveSize == ImageSize) {
        memset(ImagePath, 0, sizeof(ImagePath));
        sprintf(ImagePath, "./Image%ld.png", ClientId);

        ImageFile = fopen(ImagePath, "wb");

        if (ImageFile == NULL) {
          perror("IMAGE-OPEN");

          free(ImageBuffer);
          Response.IsOk = false;
        } else {
          if ((FileSize = fwrite(ImageBuffer, 1, ImageSize, ImageFile)) <
              ImageSize) {
            perror("IMAGE-WRITE");
            printf("Written file size : %ld\n", FileSize);

            Response.IsOk = false;
          }

          fclose(ImageFile);
        }
      } else {
        Response.IsOk = false;
      }

      free(ImageBuffer);

      RetryCount = 0;

      while (send(ClientSocket, &Response, sizeof(Response), 0) <
                 sizeof(Response) &&
             RetryCount < SERVER_MAX_RETRY_COUNT) {
        ++RetryCount;
      }

      if (RetryCount == SERVER_MAX_RETRY_COUNT) {
        printf("Connection bad! Shutting down connection!\n");
        break;
      }

      alertWarning(ClientId, ImagePath);
    }
  }

  shutdown(ClientSocket, SHUT_RDWR);
  close(ClientSocket);

  removeClient(ClientSocket);

  return NULL;
}

int main() {
  struct sockaddr_in ServerAddress;

  signal(SIGINT, interruptSignalHandler);
  signal(SIGTERM, interruptSignalHandler);

  ServerSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (ServerSocket < 0) {
    perror("CREAT-SOCKET-MAIN");
    return 1;
  }

  ServerAddress.sin_family = AF_INET;
  ServerAddress.sin_port = htons(SERVER_PORT);
  ServerAddress.sin_addr.s_addr = INADDR_ANY;

  int OptionValue = 1;
  if (setsockopt(ServerSocket, SOL_SOCKET, SO_REUSEADDR, &OptionValue,
                 sizeof(OptionValue)) < 0) {
    perror("SET-SOCKET-OPTION-MAIN");
    return 1;
  }

  if (bind(ServerSocket, (struct sockaddr *)&ServerAddress,
           sizeof(ServerAddress)) < 0) {
    perror("BIND-SOCKET-MAIN");
    return 1;
  }

  if (listen(ServerSocket, SERVER_MAX_CLIENTS) < 0) {
    perror("LISTEN-SOCKET-MAIN");
    return 1;
  }

  printf("Server started!\n");

  struct sockaddr_in ClientAddress;
  socklen_t ClientAddressSize = sizeof(ClientAddress);

  pthread_mutex_init(&ClientsMutex, NULL);

  while (true) {
    int ClientSocket = accept(ServerSocket, (struct sockaddr *)&ClientAddress,
                              &ClientAddressSize);

    if (ClientSocket < 0) {
      perror("ACCEPT-SOCKET-MAIN");
      continue;
    }

    int index = -1;

    pthread_mutex_lock(&ClientsMutex);

    for (int i = 0; i < SERVER_MAX_CLIENTS; ++i) {
      if (Clients[i].socket == 0) {
        index = i;
        break;
      }
    }

    if (index == -1) {
      printf("Server already full!\n");
      shutdown(ClientSocket, SHUT_RDWR);
      close(ClientSocket);
    } else {
      Clients[index].socket = ClientSocket;
      pthread_create(&Clients[index].thread, NULL, handlePing,
                     &Clients[index].socket);
      pthread_detach(Clients[index].thread);
    }

    pthread_mutex_unlock(&ClientsMutex);
  }
}