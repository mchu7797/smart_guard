import cv2

from socket import socket, AF_INET, SOCK_STREAM
from enum import Enum
from struct import pack, unpack
from os.path import getsize
from sys import argv
from time import sleep, time
from random import randint

SERVER_IP = "127.0.0.1"
SERVER_PORT = 12877


class PingMethods(Enum):
    INIT = 0
    WARNING = 1
    EXIT = 2


server_socket = socket(AF_INET, SOCK_STREAM)
server_socket.connect((SERVER_IP, SERVER_PORT))

capture = cv2.VideoCapture(0)
capture.set(cv2.CAP_PROP_FRAME_WIDTH, 1920)
capture.set(cv2.CAP_PROP_FRAME_HEIGHT, 1080)


def take_picture(image_path: str):
    result, frame = capture.read()
    cv2.imwrite(image_path, frame)


def send_ping(ping_method: PingMethods, client_id: int, image_path: str = None):
    if ping_method == PingMethods.INIT or ping_method == PingMethods.EXIT:
        request_packet = pack("iqqq", ping_method.value, client_id, 0, int(time()))
        server_socket.send(request_packet)
        result = server_socket.recv(1)

        if result != 1:
            return True

        return False
    elif ping_method == PingMethods.WARNING:
        request_packet = pack(
            "iqqq", ping_method.value, client_id, getsize(image_path), int(time())
        )
        server_socket.send(request_packet)

        if server_socket.recv(1)[0] != 1:
            return False

        with open(image_path, "rb") as image:
            image_binary = image.read()
            server_socket.send(image_binary)

        if server_socket.recv(1)[0] != 1:
            return False

        return True
    else:
        return False


if __name__ == "__main__":
    CLIENT_ID = int(argv[1])

    try:
        if not send_ping(PingMethods.INIT, CLIENT_ID):
            exit(1)

        for _ in range(5):
            sleep(randint(1, 5) * 0.5)
            take_picture("test_image_{}.png".format(CLIENT_ID))
            if not send_ping(
                PingMethods.WARNING, CLIENT_ID, "test_image_{}.png".format(CLIENT_ID)
            ):
                break

        sleep(2)
        send_ping(PingMethods.EXIT, CLIENT_ID)

    finally:
        server_socket.close()
        capture.release()
        cv2.destroyAllWindows()
