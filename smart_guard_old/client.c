/*
 * Created by Minseok Chu on 2024-06-06.
 */

#include "client.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <gpiod.h>
#include <linux/i2c-dev.h>
#include <math.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "image.h"
#include "packet.h"

void interruptSignalHandler() { IsRunning = false; }

/* 조도 센서로 측정한 밝기 (단위: lux) */
double readLux(int SensorFile) {
  unsigned char Data[2];

  if (read(SensorFile, Data, 2) != 2) {
    perror("BH1750-READ");

    return 0;
  }

  return (Data[0] << 8 | Data[1]) / 1.2;
}

/* 초음파 센서로 측정한 거리 (단위: cm) */
double readDistance(struct gpiod_line *TriggerPin, struct gpiod_line *EchoPin) {
  struct timespec Start, End;

  gpiod_line_set_value(TriggerPin, 1);
  usleep(10);
  gpiod_line_set_value(TriggerPin, 0);

  while (gpiod_line_get_value(EchoPin) == 0)
    ;

  clock_gettime(CLOCK_MONOTONIC, &Start);

  while (gpiod_line_get_value(EchoPin) == 1)
    ;

  clock_gettime(CLOCK_MONOTONIC, &End);

  time_t Duration = (End.tv_sec - Start.tv_sec) * 1000000 +
                    (End.tv_nsec - Start.tv_nsec) / 1000;

  return (double)Duration * 0.034 / 2;
}

bool sendPing(int Socket, int PingCommand, long ClientId, char *ImagePath) {
  struct PingRequest Request;
  struct PingResponse Response;
  memset(&Request, 0, sizeof(Request));

  Request.PingCommand = PingCommand;
  Request.ClientId = ClientId;

  if (PingCommand == PING_COMMAND_INIT || PingCommand == PING_COMMAND_EXIT) {
    send(Socket, &Request, sizeof(Request), 0);
    recv(Socket, &Response, sizeof(Response), 0);

    if (Response.IsOk) {
      return true;
    }

    return false;
  } else {
    if (access(ImagePath, F_OK) != 0) {
      return false;
    }

    FILE *ImageFile = fopen(ImagePath, "rb");

    fseek(ImageFile, 0, SEEK_END);
    Request.ImageSize = ftell(ImageFile);
    rewind(ImageFile);

    send(Socket, &Request, sizeof(Request), 0);
    recv(Socket, &Response, sizeof(Response), 0);

    if (!Response.IsOk) {
      return false;
    }

    unsigned char *ImageBinary = (unsigned char *)malloc(Request.ImageSize);

    if (ImageBinary == NULL) {
      fclose(ImageFile);
      free(ImageBinary);
      return false;
    }

    size_t BytesRead = fread(ImageBinary, Request.ImageSize, 0, ImageFile);

    if (BytesRead != Request.ImageSize) {
      fclose(ImageFile);
      free(ImageBinary);
      return false;
    }

    int RetryCount = 0;

    do {
      send(Socket, ImageBinary, sizeof(ImageBinary), 0);
      recv(Socket, &Response, sizeof(Response), 0);

      ++RetryCount;
    } while (!Response.IsOk || RetryCount < 5);

    fclose(ImageFile);
    free(ImageBinary);

    if (RetryCount == 5) {
      return false;
    }
  }

  return true;
}

int main(int Argc, char *Argv[]) {
  if (Argc != 3) {
    puts("Usage: ./smart-guard-client [ClientId] [IP Address]");
    return 1;
  }

  long ClientId = strtol(Argv[1], NULL, 10);

  /* Setting up BH-1750 */
  int SensorFile = open("/dev/i2c-1", O_RDWR);

  if (SensorFile < 0) {
    perror("BH1750-OPEN");
    return 1;
  }

  ioctl(SensorFile, I2C_TIMEOUT, I2C_DEFAULT_TIMEOUT);

  if (ioctl(SensorFile, I2C_SLAVE, BH1750_ADDR) < 0) {
    perror("BH1750-SET-SLAVE");
    close(SensorFile);
    return 1;
  }

  unsigned char ConfigData;

  ConfigData = BH1750_POWER_ON;
  if (write(SensorFile, &ConfigData, 1) != 1) {
    perror("BH1750-POWER-ON");
    close(SensorFile);
    return 1;
  }

  ConfigData = BH1750_CONTINUOUS_HIGH_RES_MODE;
  if (write(SensorFile, &ConfigData, 1) != 1) {
    perror("BH1750-SET-MODE");
    close(SensorFile);
    return 1;
  }

  puts("BH-1750 Setup done!");

  /* Setting up HC-SR04 */
  struct gpiod_chip *Chip = gpiod_chip_open("/dev/gpiochip0");
  struct gpiod_line *TriggerPin =
      gpiod_chip_get_line(Chip, HC_SR04_TRIGGER_PIN);
  struct gpiod_line *EchoPin = gpiod_chip_get_line(Chip, HC_SR04_ECHO_PIN);

  gpiod_line_request_output(TriggerPin, "TriggerPin", 0);
  gpiod_line_request_input(EchoPin, "EchoPin");

  puts("HC-SR04 Setup done!");

  /* Connecting to Server */
  int ServerSocket;
  struct sockaddr_in ServerAddress;

  if ((ServerSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("SOCKET");
    return 1;
  }

  struct timeval Timeout;
  Timeout.tv_sec = 5;
  Timeout.tv_usec = 0;

  setsockopt(ServerSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&Timeout, sizeof(Timeout));
  setsockopt(ServerSocket, SOL_SOCKET, SO_SNDTIMEO, (char *)&Timeout, sizeof(Timeout));

  ServerAddress.sin_family = AF_INET;
  ServerAddress.sin_port = htons(SERVER_PORT);
  ServerAddress.sin_addr.s_addr = inet_addr(Argv[2]);

  if (connect(ServerSocket, (struct sockaddr *)&ServerAddress,
              sizeof(ServerAddress)) == -1) {
    perror("CONNECT");
    return 1;
  }

  /* If server cannot respond */
  if (sendPing(ServerSocket, PING_COMMAND_INIT, ClientId, NULL) == false) {
    close(SensorFile);
    close(ServerSocket);

    gpiod_line_release(TriggerPin);
    gpiod_line_release(EchoPin);
    gpiod_chip_close(Chip);

    return 1;
  }

  puts("Network setup done!");

  /* Signal action setup */
  struct sigaction SignalAction;

  memset(&SignalAction, 0, sizeof(SignalAction));
  SignalAction.sa_handler = interruptSignalHandler;

  if (sigaction(SIGINT, &SignalAction, NULL) != 0) {
    perror("SIGACTION_SIGINT");
    return 1;
  }

  if (sigaction(SIGTERM, &SignalAction, NULL) != 0) {
    perror("SIGACTION_SIGTERM");
    return 1;
  }

  /* Routine start */
  bool NetworkErrorOccurred = false;
  double CurrentLux = 0;
  double CurrentDistance = 0;
  char ImagePath[256];

  sprintf(ImagePath, "%s/Image%ld.png", PING_IMAGE_BASE_DIRECTORY, ClientId);

  while (IsRunning) {
    if (NetworkErrorOccurred) {
      sendPing(ServerSocket, PING_COMMAND_WARNING, ClientId, ImagePath);
      NetworkErrorOccurred = false;
    }

    if (CurrentDistance == 0) {
      CurrentDistance = readDistance(TriggerPin, EchoPin);
      printf("Current Distance %lf\n", CurrentDistance);
    }

    if (CurrentLux == 0) {
      CurrentLux = readLux(SensorFile);
      printf("Current Lux: %lf\n", CurrentLux);
    }

    bool LuxTriggered = fabs(CurrentLux - readLux(SensorFile)) >= LUX_THRESHOLD;
    bool DistanceTriggered =
        fabs(CurrentDistance - readDistance(TriggerPin, EchoPin)) >=
        DISTANCE_THRESHOLD;

    printf("Lux %d, Distance %d\n", LuxTriggered, DistanceTriggered);

    if (LuxTriggered || DistanceTriggered) {
      if (!takePicture(ImagePath)) {
        perror("TAKE_PICTURE");
        continue;
      }

      NetworkErrorOccurred =
          !sendPing(ServerSocket, PING_COMMAND_WARNING, ClientId, ImagePath);
    }

    sleep(LOOP_DELAY);
  }

  while (!sendPing(ServerSocket, PING_COMMAND_EXIT, ClientId, NULL))
    ;

  close(SensorFile);
  close(ServerSocket);

  gpiod_line_release(TriggerPin);
  gpiod_line_release(EchoPin);
  gpiod_chip_close(Chip);
}
