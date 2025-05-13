import sys
import serial
import threading
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QGridLayout, QPushButton, QWidget
)
from PyQt5.QtCore import Qt


class FireDetectionGUI(QMainWindow):
    def __init__(self, total_areas=32):
        super().__init__()
        self.setWindowTitle("Fire Detection System")
        self.total_areas = total_areas

        # Dictionary to store blocks with their area numbers
        self.area_blocks = {}

        # Serial connection parameters (update as needed)
        self.com_port = "/dev/ttyUSB0"  # Update for Raspberry Pi Zero
        self.baud_rate = 9600
        self.serial_connection = None
        self.running = False
        self.lock = threading.Lock()  # Ensure thread safety

        # Create the GUI layout
        self.init_ui()
        self.showFullScreen()
        # Initialize serial connection and start the thread
        self.initialize_serial()
        self.start_serial_thread()

    def init_ui(self):
        """
        Initialize the user interface and create buttons for each area.
        """
        # Main widget and layout
        widget = QWidget()
        self.setCentralWidget(widget)

        layout = QGridLayout()
        widget.setLayout(layout)

        # Create buttons (blocks) for each area
        for i in range(self.total_areas):
            area_label = f"Area {i + 1}"
            block = QPushButton(area_label)
            block.setStyleSheet("background-color: rgb(109,214,135);color:black;")
            block.setEnabled(False)  # Disable interaction
            block.setFixedSize(80, 80)  # Smaller size for Raspberry Pi screen
            self.area_blocks[i + 1] = block
            layout.addWidget(block, i // 8, i % 8)  # 8 blocks per row

    def initialize_serial(self):
        """
        Initialize the serial connection.
        """
        try:
            self.serial_connection = serial.Serial(self.com_port, self.baud_rate, timeout=1)
            print(f"Serial connection opened on {self.com_port} at {self.baud_rate} baud.")
        except serial.SerialException as e:
            print(f"Error opening serial port: {e}")
            self.serial_connection = None

    def start_serial_thread(self):
        """
        Start a thread to read data from the serial connection.
        """
        if self.serial_connection:
            self.running = True
            threading.Thread(target=self.read_serial_data, daemon=True).start()

    def read_serial_data(self):
        """
        Continuously read data from the serial connection in a separate thread.
        """
        buffer = ""
        try:
            while self.running:
                with self.lock:
                    if self.serial_connection and self.serial_connection.in_waiting > 0:
                        buffer += self.serial_connection.read(self.serial_connection.in_waiting).decode("utf-8")
                        if "Fire cleared, system reset to normal." in buffer:
                            # Reset all blocks to white
                            self.reset_blocks()
                            buffer = ""
                        elif "Sending SMS: Fire detected in area" in buffer:
                            # Extract fire area and update block color
                            area = self.extract_fire_area(buffer)
                            if area:
                                self.update_block_color(area, "red")
                            buffer = ""
        except Exception as e:
            print(f"Error reading from serial: {e}")
        finally:
            self.close_serial_connection()

    def extract_fire_area(self, data):
        """
        Extract the area number from fire detection messages.
        """
        import re
        match = re.search(r"Sending SMS: Fire detected in area (\d+)", data)
        if match:
            return int(match.group(1))
        return None

    def update_block_color(self, area, color):
        """
        Update the color of the block representing the specified area.
        """
        if area in self.area_blocks:
            self.area_blocks[area].setStyleSheet(f"background-color: {color};color:black;")

    def reset_blocks(self):
        """
        Reset all blocks to white.
        """
        for block in self.area_blocks.values():
            block.setStyleSheet("background-color: rgb(109,214,135);color:black;")

    def close_serial_connection(self):
        """
        Close the serial connection if it is open.
        """
        with self.lock:
            if self.serial_connection and self.serial_connection.is_open:
                try:
                    self.serial_connection.close()
                    print("Serial connection closed.")
                except Exception as e:
                    print(f"Error closing serial connection: {e}")

    def closeEvent(self, event):
        """
        Handle application close event.
        """
        print("Closing application...")
        self.running = False
        self.close_serial_connection()
        event.accept()


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = FireDetectionGUI(total_areas=32)
    window.show()
    sys.exit(app.exec_())
