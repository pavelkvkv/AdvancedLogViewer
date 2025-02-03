#!/usr/bin/env python3
import tkinter as tk
from tkinter import ttk
import serial
import serial.tools.list_ports
import threading
import queue
import time
import configparser
import struct
import os

class LogPanel(tk.Frame):
    def __init__(self, master, level, color, *args, **kwargs):
        super().__init__(master, *args, **kwargs)
        self.level = level
        self.text_color = color
        self.all_lines = []      # все добавленные строки
        self.filter_text = ""    # текущий фильтр
        self.auto_scroll = tk.BooleanVar(value=True)
        self.collapsed = False   # состояние: свернуто/развернуто

        # Заголовок панели с кнопкой сворачивания/разворачивания
        self.header = tk.Frame(self, bg='#222222')
        self.toggle_button = tk.Button(self.header, text=f"{self.level} лог [-]", command=self.toggle,
                                       bg='#444444', fg='white')
        self.toggle_button.pack(side=tk.LEFT, padx=2, pady=2)
        self.header.pack(fill=tk.X)

        # Основной фрейм содержимого панели
        self.content = tk.Frame(self, bg='#222222')
        # Фрейм для фильтра и галочки автоскроллинга
        filter_frame = tk.Frame(self.content, bg='#222222')
        tk.Label(filter_frame, text="Фильтр:", bg='#222222', fg='white').pack(side=tk.LEFT, padx=2)
        self.filter_entry = tk.Entry(filter_frame)
        self.filter_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=2)
        self.filter_entry.bind("<Return>", self.on_filter_enter)
        self.auto_scroll_cb = tk.Checkbutton(filter_frame, text="Автоскроллинг",
                                              variable=self.auto_scroll,
                                              bg='#222222', fg='white', selectcolor='#222222')
        self.auto_scroll_cb.pack(side=tk.LEFT, padx=2)
        filter_frame.pack(fill=tk.X, pady=2)

        # Текстовое поле с вертикальной прокруткой; фон почти чёрный, минимальная ширина задана width.
        self.text_widget = tk.Text(self.content, bg='#111111', fg=self.text_color,
                                   wrap=tk.NONE, width=40)
        self.text_widget.config(state=tk.DISABLED)
        self.scrollbar = tk.Scrollbar(self.content, command=self.text_widget.yview)
        self.text_widget.configure(yscrollcommand=self.scrollbar.set)
        self.text_widget.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        self.scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.content.pack(fill=tk.BOTH, expand=True)

    def toggle(self):
        if self.collapsed:
            # Разворачиваем: возвращаем content, сбрасываем фиксированную ширину
            self.content.pack(fill=tk.BOTH, expand=True)
            self.toggle_button.config(text=f"{self.level} [-]")
            # Передаем 0, чтобы ширина рассчитывалась автоматически
            self.config(width=0)
            self.pack_configure(expand=True)
            self.pack_propagate(True)
        else:
            # Сворачиваем: скрываем content и устанавливаем ширину равной ширине кнопки
            self.content.forget()
            self.toggle_button.config(text=f"{self.level} [+]")
            self.update_idletasks()  # вычисляем размеры
            min_width = self.toggle_button.winfo_reqwidth()
            self.config(width=min_width)
            self.pack_propagate(False)
            self.pack_configure(expand=False)
        self.collapsed = not self.collapsed

    def on_filter_enter(self, event):
        self.filter_text = self.filter_entry.get()
        self.refresh_text()

    def refresh_text(self):
        self.text_widget.config(state=tk.NORMAL)
        self.text_widget.delete('1.0', tk.END)
        for line in self.all_lines:
            if self.filter_text == "" or self.filter_text in line:
                self.text_widget.insert(tk.END, line)
        if self.auto_scroll.get():
            self.text_widget.see(tk.END)
        self.text_widget.config(state=tk.DISABLED)

    def add_line(self, line):
        self.all_lines.append(line)
        if self.filter_text == "" or self.filter_text in line:
            self.text_widget.config(state=tk.NORMAL)
            self.text_widget.insert(tk.END, line)
            if self.auto_scroll.get():
                self.text_widget.see(tk.END)
            self.text_widget.config(state=tk.DISABLED)

class LogViewerApp:
    def __init__(self, master):
        self.master = master
        master.title("Логгер UART")
        master.configure(bg='#111111')  # фон главного окна ещё темнее

        # Фрейм для горизонтального размещения панелей
        self.panels_frame = tk.Frame(master, bg='#111111')
        self.panels_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # Создаем четыре панели для логов D, I, W, E с заданными цветами
        self.panels = {}
        self.panels['D'] = LogPanel(self.panels_frame, "D", "white", bd=1, relief=tk.SOLID)
        self.panels['I'] = LogPanel(self.panels_frame, "I", "green", bd=1, relief=tk.SOLID)
        self.panels['W'] = LogPanel(self.panels_frame, "W", "orange", bd=1, relief=tk.SOLID)
        self.panels['E'] = LogPanel(self.panels_frame, "E", "red", bd=1, relief=tk.SOLID)
        # Размещаем панели горизонтально
        self.panels['D'].pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=5, pady=5)
        self.panels['I'].pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=5, pady=5)
        self.panels['W'].pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=5, pady=5)
        self.panels['E'].pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=5, pady=5)

        # Панель настроек подключения (по умолчанию скрыта)
        self.conn_settings_visible = False
        self.conn_frame = tk.Frame(master, bg='#222222', bd=2, relief=tk.RIDGE)
        tk.Label(self.conn_frame, text="Порт:", bg='#222222', fg='white').grid(row=0, column=0, sticky=tk.W, padx=2, pady=2)
        self.port_var = tk.StringVar()
        self.port_menu = ttk.Combobox(self.conn_frame, textvariable=self.port_var, values=self.get_serial_ports())
        self.port_menu.grid(row=0, column=1, sticky=tk.W, padx=2, pady=2)
        tk.Label(self.conn_frame, text="Скорость:", bg='#222222', fg='white').grid(row=1, column=0, sticky=tk.W, padx=2, pady=2)
        self.baud_var = tk.StringVar()
        self.baud_entry = tk.Entry(self.conn_frame, textvariable=self.baud_var)
        self.baud_entry.grid(row=1, column=1, sticky=tk.W, padx=2, pady=2)
        self.connect_button = tk.Button(self.conn_frame, text="Подключиться", command=self.toggle_connection,
                                        bg='#444444', fg='white')
        self.connect_button.grid(row=2, column=0, columnspan=2, padx=2, pady=2)
        self.settings_button = tk.Button(master, text="Настройки подключения", command=self.toggle_conn_settings,
                                         bg='#444444', fg='white')
        self.settings_button.pack(pady=5)

        self.load_settings()

        self.serial_port = None
        self.ser_thread = None
        self.running = False
        self.queue = queue.Queue()

        self.master.after(100, self.poll_queue)
        master.protocol("WM_DELETE_WINDOW", self.on_close)

    def get_serial_ports(self):
        ports = serial.tools.list_ports.comports()
        return [port.device for port in ports]

    def toggle_conn_settings(self):
        if self.conn_settings_visible:
            self.conn_frame.pack_forget()
        else:
            self.conn_frame.pack(pady=5)
        self.conn_settings_visible = not self.conn_settings_visible

    def load_settings(self):
        self.config = configparser.ConfigParser()
        if os.path.exists("settings.ini"):
            self.config.read("settings.ini")
        else:
            self.config['Connection'] = {}
        self.port_var.set(self.config['Connection'].get("port", ""))
        self.baud_var.set(self.config['Connection'].get("baudrate", "9600"))

    def save_settings(self):
        if 'Connection' not in self.config:
            self.config['Connection'] = {}
        self.config['Connection']['port'] = self.port_var.get()
        self.config['Connection']['baudrate'] = self.baud_var.get()
        with open("settings.ini", "w") as configfile:
            self.config.write(configfile)

    def toggle_connection(self):
        if self.serial_port and self.serial_port.is_open:
            self.disconnect()
        else:
            self.connect()

    def connect(self):
        port = self.port_var.get()
        try:
            baudrate = int(self.baud_var.get())
        except ValueError:
            self.log_to_all("Неверная скорость передачи: " + self.baud_var.get() + "\n")
            return
        try:
            self.serial_port = serial.Serial(port, baudrate, timeout=1)
        except Exception as e:
            self.log_to_all("Ошибка подключения: " + str(e) + "\n")
            return
        self.running = True
        self.ser_thread = threading.Thread(target=self.read_serial)
        self.ser_thread.daemon = True
        self.ser_thread.start()
        self.connect_button.config(text="Отключиться")
        self.save_settings()

    def disconnect(self):
        self.running = False
        if self.serial_port:
            try:
                self.serial_port.close()
            except Exception as e:
                self.log_to_all("Ошибка отключения: " + str(e) + "\n")
        self.serial_port = None
        self.connect_button.config(text="Подключиться")

    def read_serial(self):
        while self.running:
            try:
                if self.serial_port.in_waiting:
                    raw_line = self.serial_port.readline()
                    parsed = self.parse_line(raw_line)
                    if parsed:
                        self.queue.put(parsed)
                else:
                    time.sleep(0.01)
            except Exception as e:
                self.queue.put(("D", "Ошибка чтения: " + str(e) + "\n"))
        # Выход из потока

    def parse_line(self, raw_bytes):
        if not raw_bytes:
            return None
        first_byte = raw_bytes[0]
        # Если строка начинается на ASCII-букву D, I, W или E, отображаем её как есть.
        if first_byte in (ord('D'), ord('I'), ord('W'), ord('E')):
            try:
                line_str = raw_bytes.decode('utf-8', errors='replace')
            except Exception:
                line_str = str(raw_bytes)
            return (chr(first_byte), line_str)
        # Если первый байт равен 0x11, 0x12, 0x13 или 0x14 – байт-кодированное время
        elif first_byte in (0x11, 0x12, 0x13, 0x14):
            level_mapping = {0x11: 'D', 0x12: 'I', 0x13: 'W', 0x14: 'E'}
            level_char = level_mapping.get(first_byte, 'D')
            if len(raw_bytes) < 5:
                try:
                    line_str = raw_bytes.decode('utf-8', errors='replace')
                except Exception:
                    line_str = str(raw_bytes)
                return ("D", line_str)
            try:
                timestamp_ms = struct.unpack('<I', raw_bytes[1:5])[0]
            except Exception:
                try:
                    line_str = raw_bytes.decode('utf-8', errors='replace')
                except Exception:
                    line_str = str(raw_bytes)
                return ("D", line_str)
            hours = (timestamp_ms // (3600 * 1000)) % 24
            minutes = (timestamp_ms // (60 * 1000)) % 60
            seconds = (timestamp_ms // 1000) % 60
            milliseconds = timestamp_ms % 1000
            time_str = f"{hours:02d}:{minutes:02d}:{seconds:02d}:{milliseconds:03d}"
            try:
                rest = raw_bytes[5:].decode('utf-8', errors='replace')
            except Exception:
                rest = str(raw_bytes[5:])
            line_str = f"{level_char} ({time_str}){rest}"
            return (level_char, line_str)
        else:
            # Если не соответствует ни одному из требований – выводим в журнал D
            try:
                line_str = raw_bytes.decode('utf-8', errors='replace')
            except Exception:
                line_str = str(raw_bytes)
            return ("D", line_str)

    def poll_queue(self):
        while not self.queue.empty():
            item = self.queue.get()
            if item:
                level, line = item
                if level in self.panels:
                    self.panels[level].add_line(line)
                else:
                    self.panels['D'].add_line(line)
        self.master.after(50, self.poll_queue)

    def log_to_all(self, message):
        for panel in self.panels.values():
            panel.add_line("D " + message)

    def on_close(self):
        self.disconnect()
        self.master.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    app = LogViewerApp(root)
    root.mainloop()
