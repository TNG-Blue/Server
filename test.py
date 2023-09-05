import socket
import sqlite3
from PyQt6.QtWidgets import QApplication, QMainWindow, QVBoxLayout, QWidget, QPushButton, QLabel
from PyQt6.QtCore import QThread, pyqtSignal, QTimer

esp32_ip = "192.168.38.82"  # Địa chỉ IP của ESP32
esp32_port = 80  # Cổng của ESP32


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
            connection = sqlite3.connect("cmake-build-debug/lora.db")
            cursor = connection.cursor()
            cursor.execute(f"SELECT device_id, command FROM user_control WHERE device_id = ? ORDER BY timestamp DESC LIMIT 1", (self.device_id,))
            row = cursor.fetchone()
            connection.close()
            if row:
                self.dataChanged.emit(row)
        except Exception as e:
            print("Error:", str(e))

class ESP32Control(QMainWindow):
    def __init__(self):
        super().__init__()

        self.initUI()

        self.device_id = "pump"
        self.db_watcher = DatabaseWatcher(self.device_id)
        self.db_watcher.dataChanged.connect(self.handle_data_changed)

        self.timer = QTimer(self)
        self.timer.timeout.connect(self.check_for_data_changes)
        self.timer.start(1000)  # Kiểm tra sự thay đổi trong cơ sở dữ liệu mỗi giây

    def initUI(self):
        self.setWindowTitle("ESP32 LED Control")
        self.setGeometry(100, 100, 400, 200)

        self.central_widget = QWidget(self)
        self.setCentralWidget(self.central_widget)

        self.layout = QVBoxLayout()

        self.label = QLabel("Current command for 'pump':")
        self.layout.addWidget(self.label)

        self.command_label = QLabel("")
        self.layout.addWidget(self.command_label)

        self.central_widget.setLayout(self.layout)

    def check_for_data_changes(self):
        self.db_watcher.start()

    def handle_data_changed(self, data):
        device_id, command = data
        self.command_label.setText(f"Command: {command}")
        self.send_command_to_esp32(command)

    def send_command_to_esp32(self, command):
        if command == "on":
            send_to_esp32("ON")
        elif command == "off":
            send_to_esp32("OFF")


if __name__ == "__main__":
    app = QApplication([])
    window = ESP32Control()
    window.show()
    app.exec()
