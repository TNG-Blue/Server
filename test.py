import socket
import sqlite3
from PyQt6.QtWidgets import QApplication, QMainWindow, QVBoxLayout, QWidget, QPushButton, QLabel
from PyQt6.QtCore import QThread, pyqtSignal, QTimer
import os

esp32_ip = "192.168.130.82"  # Địa chỉ IP của ESP32
esp32_port = 80  # Cổng của ESP32

device_id = None


def send_to_esp32(data):
    try:
        client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client_socket.connect((esp32_ip, esp32_port))
        client_socket.send(data.encode())
        client_socket.close()
    except Exception as e:
        print("Error:", str(e))


class DatabaseWatcher(QThread):
    dataChanged = pyqtSignal(tuple)

    def __init__(self, device_id):
        super().__init__()
        self.device_id = device_id

    def run(self):
        try:
            connection = sqlite3.connect("F:/Source/C++/Database_Server/cmake-build-debug/lora.db")
            cursor = connection.cursor()
            cursor.execute(
                f"SELECT device_id, command FROM user_control WHERE device_id = ? ORDER BY timestamp DESC LIMIT 1",
                (self.device_id,))
            row = cursor.fetchone()
            connection.close()
            if row:
                self.dataChanged.emit(row)
        except Exception as e:
            print("Error:", str(e))


class ESP32Control(QMainWindow):
    def __init__(self):
        super().__init__()

        self.command_label = None
        self.label = None
        self.layout = None
        self.central_widget = None
        self.initUI()

        self.db_watchers = {}

        self.init_device_watcher("pump")
        self.init_device_watcher("fan")
        self.init_device_watcher("motor")

        self.timer = QTimer(self)
        self.timer.timeout.connect(self.check_for_data_changes)
        self.timer.start(1000)

    def initUI(self):
        self.setWindowTitle("ESP32 LED Control")
        self.setGeometry(100, 100, 400, 200)

        self.central_widget = QWidget(self)
        self.setCentralWidget(self.central_widget)

        self.layout = QVBoxLayout()
        self.command_label = QLabel("Command Label Text")
        self.layout.addWidget(self.label)

        self.command_label = QLabel("")
        self.layout.addWidget(self.command_label)

        self.central_widget.setLayout(self.layout)

    def init_device_watcher(self, device_id):

        db_watcher = DatabaseWatcher(device_id)
        db_watcher.dataChanged.connect(self.handle_data_changed)
        self.db_watchers[device_id] = db_watcher

    def check_for_data_changes(self):
        for device_id, db_watcher in self.db_watchers.items():
            if not db_watcher.isRunning():
                db_watcher.start()

    def handle_data_changed(self, data):
        global device_id
        device_id, command = data
        self.command_label.setText(f"Command for {device_id}: {command}")
        self.send_command_to_esp32(command)

    @staticmethod
    def send_command_to_esp32(command):
        if command == "on" and device_id == "pump":
            send_to_esp32("PUMP_ON")
        elif command == "off" and device_id == "pump":
            send_to_esp32("PUMP_OFF")
        elif command == "on" and device_id == "fan":
            send_to_esp32("FAN_ON")
        elif command == "off" and device_id == "fan":
            send_to_esp32("FAN_OFF")
        elif command == "on" and device_id == "motor":
            send_to_esp32("MOTOR_ON")
        elif command == "off" and device_id == "motor":
            send_to_esp32("MOTOR_OFF")


if __name__ == "__main__":
    app = QApplication([])
    window = ESP32Control()
    window.show()
    app.exec()
