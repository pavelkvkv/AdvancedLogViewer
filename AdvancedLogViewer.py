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
import re
import os
from datetime import datetime

class LogPanel(tk.Frame):
    def __init__(self, master, app, level, color, *args, **kwargs):
        super().__init__(master, *args, **kwargs)
        self.app = app  # ссылка на главный объект для доступа к общим настройкам
        self.level = level
        self.text_color = color
        self.entries = []  # список записей: (timestamp, message), timestamp – int или None
        self.filter_text = ""
        self.auto_scroll = tk.BooleanVar(value=True)
        self.collapsed = False

        # Заголовок панели с кнопкой сворачивания/разворачивания
        self.header = tk.Frame(self, bg='#222222')
        self.toggle_button = tk.Button(self.header, text=f"{self.level} логи [-]", command=self.toggle,
                                       bg='#444444', fg='white')
        self.toggle_button.pack(side=tk.LEFT, padx=2, pady=2)
        self.header.pack(fill=tk.X)

        # Фрейм с фильтром и основным текстовым полем
        self.content = tk.Frame(self, bg='#222222')
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

        # Текстовое поле для журнала с вертикальной прокруткой (почти чёрный фон)
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
            self.content.pack(fill=tk.BOTH, expand=True)
            self.toggle_button.config(text=f"{self.level} логи [-]")
            self.config(width=0)  # 0 – авторасчёт
            self.pack_propagate(True)
            self.pack_configure(expand=True)
        else:
            self.content.forget()
            self.toggle_button.config(text=f"{self.level} логи [+]")
            self.update_idletasks()
            min_width = self.toggle_button.winfo_reqwidth()
            self.config(width=min_width)
            self.pack_propagate(False)
            self.pack_configure(expand=False)
        self.collapsed = not self.collapsed

    def on_filter_enter(self, event):
        self.filter_text = self.filter_entry.get()
        self.refresh_text()

    def format_entry(self, timestamp, message):
        # Форматирует запись с учетом выбранного режима отображения времени
        if timestamp is not None:
            hh = timestamp // 3600000
            mm = (timestamp % 3600000) // 60000
            ss = (timestamp % 60000) // 1000
            ms = timestamp % 1000
            tf = self.app.time_format.get()  # "full", "short", "none"
            if tf == "full":
                time_str = f"({hh:02d}:{mm:02d}:{ss:02d}:{ms:03d}) "
            elif tf == "short":
                time_str = f"({ss:02d}:{ms:03d}) "
            elif tf == "none":
                time_str = ""
            else:
                time_str = f"({hh:02d}:{mm:02d}:{ss:02d}:{ms:03d}) "
        else:
            time_str = ""
        return time_str + message

    def refresh_text(self):
        self.text_widget.config(state=tk.NORMAL)
        self.text_widget.delete('1.0', tk.END)
        for timestamp, message in self.entries:
            # Фильтрация по содержимому сообщения (без времени)
            if self.filter_text == "" or self.filter_text in message:
                line = self.format_entry(timestamp, message)
                self.text_widget.insert(tk.END, line + "\n")
        if self.auto_scroll.get():
            self.text_widget.see(tk.END)
        self.text_widget.config(state=tk.DISABLED)

    def add_entry(self, timestamp, message):
        self.entries.append((timestamp, message))
        if self.filter_text == "" or self.filter_text in message:
            line = self.format_entry(timestamp, message)
            self.text_widget.config(state=tk.NORMAL)
            self.text_widget.insert(tk.END, line + "\n")
            if self.auto_scroll.get():
                self.text_widget.see(tk.END)
            self.text_widget.config(state=tk.DISABLED)
        # Автосохранение: дописываем запись в файл
        if self.level in self.app.log_files and self.app.log_files[self.level]:
            try:
                self.app.log_files[self.level].write(self.format_entry(timestamp, message) + "\n")
                self.app.log_files[self.level].flush()
            except Exception:
                pass

    def clear_entries(self):
        self.entries.clear()
        self.text_widget.config(state=tk.NORMAL)
        self.text_widget.delete('1.0', tk.END)
        self.text_widget.config(state=tk.DISABLED)

class LogViewerApp:
    def __init__(self, master):
        self.master = master
        master.title("Логгер UART")
        master.configure(bg='#111111')

        # Глобальная настройка отображения времени: "full", "short", "none"
        self.time_format = tk.StringVar(value="full")

        # Фрейм для горизонтального размещения панелей
        self.panels_frame = tk.Frame(master, bg='#111111')
        self.panels_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # Создаем четыре панели для логов D, I, W, E; передаем self для доступа к настройкам
        self.panels = {}
        self.panels['D'] = LogPanel(self.panels_frame, self, "D", "white", bd=1, relief=tk.SOLID)
        self.panels['I'] = LogPanel(self.panels_frame, self, "I", "green", bd=1, relief=tk.SOLID)
        self.panels['W'] = LogPanel(self.panels_frame, self, "W", "orange", bd=1, relief=tk.SOLID)
        self.panels['E'] = LogPanel(self.panels_frame, self, "E", "red", bd=1, relief=tk.SOLID)
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

        # Фрейм с настройками отображения времени и кнопкой "Очистить всё"
        self.options_frame = tk.Frame(master, bg='#111111')
        self.options_frame.pack(pady=5)
        tk.Label(self.options_frame, text="Отображение времени:", bg='#111111', fg='white').pack(side=tk.LEFT, padx=5)
        tk.Radiobutton(self.options_frame, text="Полное", variable=self.time_format, value="full",
                       command=self.refresh_all_logs, bg='#111111', fg='white').pack(side=tk.LEFT, padx=5)
        tk.Radiobutton(self.options_frame, text="Сократить", variable=self.time_format, value="short",
                       command=self.refresh_all_logs, bg='#111111', fg='white').pack(side=tk.LEFT, padx=5)
        tk.Radiobutton(self.options_frame, text="Скрыть", variable=self.time_format, value="none",
                       command=self.refresh_all_logs, bg='#111111', fg='white').pack(side=tk.LEFT, padx=5)
        self.clear_button = tk.Button(self.options_frame, text="Очистить всё", command=self.clear_all_logs,
                                      bg='#444444', fg='white')
        self.clear_button.pack(side=tk.LEFT, padx=20)

        self.load_settings()

        # Настройка автосохранения: открываем файлы для каждого уровня в папке logs
        self.log_files = {}
        self.setup_log_files()

        self.serial_port = None
        self.ser_thread = None
        self.running = False
        self.queue = queue.Queue()

        self.master.after(100, self.poll_queue)
        master.protocol("WM_DELETE_WINDOW", self.on_close)

    def setup_log_files(self):
        logs_dir = os.path.join(os.getcwd(), "logs")
        if not os.path.exists(logs_dir):
            os.makedirs(logs_dir)
        timestamp_str = datetime.now().strftime("%Y%m%d_%H%M%S")
        for level in ['D', 'I', 'W', 'E']:
            file_path = os.path.join(logs_dir, f"{timestamp_str}_{level}.log")
            try:
                self.log_files[level] = open(file_path, "a", encoding="utf-8")
            except Exception as e:
                self.log_files[level] = None

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
                self.queue.put(("D", None, "Ошибка чтения: " + str(e)))

    def parse_line(self, raw_bytes):
        if not raw_bytes:
            return None
        try:
            decoded_line = raw_bytes.decode('utf-8', errors='replace')
        except Exception:
            decoded_line = str(raw_bytes)
        first_byte = raw_bytes[0]
        # Строковое время: проверяем шаблон "X (hh:mm:ss:ms)"
        if first_byte in (ord('D'), ord('I'), ord('W'), ord('E')):
            pattern = r"^([DIWE]) \((\d{2}):(\d{2}):(\d{2}):(\d{3})\)(.*)$"
            m = re.match(pattern, decoded_line)
            if m:
                letter = m.group(1)
                try:
                    h = int(m.group(2))
                    m_val = int(m.group(3))
                    s = int(m.group(4))
                    ms_val = int(m.group(5))
                    timestamp = h * 3600000 + m_val * 60000 + s * 1000 + ms_val
                except Exception:
                    return ("D", None, decoded_line)
                message = m.group(6).lstrip()
                return (letter, timestamp, message)
            else:
                return ("D", None, decoded_line)
        # Цифровое время: первый байт в {0x11,0x12,0x13,0x14}, 4 байта времени, 5-й байт – пробел
        elif first_byte in (0x11, 0x12, 0x13, 0x14):
            mapping = {0x11: 'D', 0x12: 'I', 0x13: 'W', 0x14: 'E'}
            letter = mapping.get(first_byte, 'D')
            if len(raw_bytes) < 6 or raw_bytes[5:6] != b' ':
                return ("D", None, decoded_line)
            try:
                timestamp = struct.unpack('<I', raw_bytes[1:5])[0]
            except Exception:
                return ("D", None, decoded_line)
            try:
                message = raw_bytes[6:].decode('utf-8', errors='replace')
            except Exception:
                message = str(raw_bytes[6:])
            return (letter, timestamp, message)
        else:
            return ("D", None, decoded_line)

    def poll_queue(self):
        while not self.queue.empty():
            item = self.queue.get()
            if item:
                level, timestamp, message = item
                if level in self.panels:
                    self.panels[level].add_entry(timestamp, message)
                else:
                    self.panels['D'].add_entry(timestamp, message)
        self.master.after(50, self.poll_queue)

    def log_to_all(self, message):
        for panel in self.panels.values():
            panel.add_entry(None, message)

    def clear_all_logs(self):
        for panel in self.panels.values():
            panel.clear_entries()

    def refresh_all_logs(self):
        for panel in self.panels.values():
            panel.refresh_text()

    def on_close(self):
        self.disconnect()
        for f in self.log_files.values():
            if f:
                f.close()
        self.master.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    app = LogViewerApp(root)
    root.mainloop()
