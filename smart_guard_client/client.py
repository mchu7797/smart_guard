from cv2 import cv2
from smbus2 import SMBus
from gpiozero import DistanceSensor
from socket import socket, AF_INET, SOCK_STREAM
from enum import Enum
from struct import pack, unpack
from os.path import getsize
from time import sleep
from datetime import datetime, timezone, timedelta

BH_1750_DEVICE = 0x23
BH_1750_POWER_DOWN = 0x00
BH_1750_POWER_ON = 0x01
BH_1750_RESET = 0x07
BH_1750_CONTINUOUS_HIGH_RES_MODE = 0x10
HC_SR04_TRIGGER_PIN = 17
HC_SR04_ECHO_PIN = 18

SERVER_IP = "220.66.59.110"
SERVER_PORT = 12877
CLIENT_ID = 1


class PingMethods(Enum):
    INIT = 0
    WARNING = 1
    EXIT = 2


i2c_bus = SMBus(1)
i2c_bus.write_byte(BH_1750_DEVICE, BH_1750_POWER_ON)
i2c_bus.write_byte(BH_1750_DEVICE, BH_1750_RESET)

distance_sensor = DistanceSensor(trigger=HC_SR04_TRIGGER_PIN, echo=HC_SR04_ECHO_PIN)

server_socket = socket(AF_INET, SOCK_STREAM)
server_socket.connect((SERVER_IP, SERVER_PORT))


def get_light():
    data = i2c_bus.read_i2c_block_data(
        BH_1750_DEVICE, BH_1750_CONTINUOUS_HIGH_RES_MODE, 2
    )
    return (data[1] + (256 * data[0])) / 1.2


def get_distance():
    return distance_sensor.distance * 10


def take_picture(image_path: str):
    capture = cv2.VideoCapture(0)

    capture.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    capture.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

    result, frame = capture.read()

    cv2.imwrite(image_path, frame)

    capture.release()

    cv2.destroyAllWindows()


def send_ping(ping_method: PingMethods, client_id: int, image_path: str = None):
    current_time = int(datetime.now(timezone(timedelta(hours=9))).timestamp())

    if ping_method == PingMethods.INIT or ping_method == PingMethods.EXIT:
        request_packet = pack("iqqq", ping_method.value, client_id, 0, current_time)
        server_socket.send(request_packet)
        result = server_socket.recv(1)

        if result != 1:
            return True

        return False
    elif ping_method == PingMethods.WARNING:
        request_packet = pack("iqqq", ping_method.value, client_id, getsize(image_path), current_time)
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
    try:
        current_light = 0
        current_distance = 0

        if not send_ping(PingMethods.INIT, CLIENT_ID):
            exit(1)

        while True:
            if current_light == 0:
                current_light = get_light()
            if current_distance == 0:
                current_distance = get_distance()

            light = get_light()
            distance = get_distance()

            if abs(current_light - light) > 40 or abs(current_distance - distance) > 40:
                take_picture("warning.png")
                send_ping(PingMethods.WARNING, CLIENT_ID, "warning.png")
                print("Something detected!")
            else:
                current_light = light
                current_distance = distance

            sleep(2)
    finally:
        send_ping(PingMethods.EXIT, CLIENT_ID)
        server_socket.close()

        i2c_bus.close()
        distance_sensor.close()
