import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.patches import Rectangle
import matplotlib
matplotlib.use('TkAgg')  # Ensure compatibility with tkinter
import pandas as pd
import numpy as np
from collections import deque
import threading
import time
import queue
import tkinter as tk
from tkinter import ttk, messagebox, Scale
import sys

class SIFDashboard:
    def __init__(self):
        self.serial_port = None
        self.serial_thread = None
        self.running = False
        
        self.max_points = 1000
        self.data_queue = queue.Queue()
        
        self.timestamps = deque(maxlen=self.max_points)
        self.battery_data = deque(maxlen=self.max_points)
        self.rpm_data = deque(maxlen=self.max_points)
        self.load_voltage_data = deque(maxlen=self.max_points)
        self.power_state_data = deque(maxlen=self.max_points)
        self.b2_direction = deque(maxlen=self.max_points)
        self.est_power = deque(maxlen=self.max_points)
        
        # New: Temperature candidate data
        self.byte0_data = deque(maxlen=self.max_points)
        self.byte1_data = deque(maxlen=self.max_points)
        self.byte2_data = deque(maxlen=self.max_points)
        self.byte3_data = deque(maxlen=self.max_points)
        
        self.current_values = {
            'battery': 0, 'rpm': 0, 'load_voltage': 0, 'speed_mode': 0,
            'reverse': 0, 'brake': 0, 'regen': 0, 'power_state': 'IDLE',
            'b2_direction': 0, 'est_power': 0.0,
            # New: Temperature candidates
            'byte0_raw': 0, 'byte1_raw': 0, 'byte2_raw': 0, 'byte3_raw': 0,
            'byte0_temp_c': 0, 'byte1_temp_c': 0, 'byte2_temp_c': 0, 'byte3_temp_c': 0
        }
        self.manual_values = {
            'battery': 50.0,
            'rpm': 0,
            'speed_mode': 1,
            'reverse': 0,
            'brake': 0,
            'regen': 0,
            'current': 0,
            'voltage': 12.0
        }
        
        self.raw_bytes = [0] * 12
        self.setup_gui()
        self.setup_plots()
        
    def setup_gui(self):
        self.root = tk.Tk()
        self.root.title("SIF Vehicle Data Dashboard with Temperature Analysis")
        self.root.geometry("1800x1000")
        self.bg_color = '#1e1e1e'
        self.fg_color = '#ffffff'
        self.frame_bg = '#2d2d2d'
        self.entry_bg = '#3d3d3d'
        self.button_bg = '#404040'
        self.button_active = '#505050'
        self.accent_color = '#007acc'
        self.success_color = '#4CAF50'
        self.error_color = '#f44336'
        self.warning_color = '#ff9800'
        self.root.configure(bg=self.bg_color)
        style = ttk.Style()
        style.theme_use('clam')
        style.configure('TFrame', background=self.bg_color)
        style.configure('TLabel', background=self.bg_color, foreground=self.fg_color)
        style.configure('TLabelframe', background=self.frame_bg, foreground=self.fg_color, bordercolor=self.button_bg)
        style.configure('TLabelframe.Label', background=self.frame_bg, foreground=self.fg_color)
        style.configure('TButton', background=self.button_bg, foreground=self.fg_color, borderwidth=0)
        style.map('TButton', background=[('active', self.button_active)])
        style.configure('TEntry', fieldbackground=self.entry_bg, background=self.entry_bg, foreground=self.fg_color, borderwidth=0)
        style.configure('TCheckbutton', background=self.frame_bg, foreground=self.fg_color, focuscolor='none')
        style.map('TCheckbutton', background=[('active', self.frame_bg)])
        style.configure('Accent.TButton', background=self.accent_color, foreground=self.fg_color)
        style.map('Accent.TButton', background=[('active', '#005a9e')])
        style.configure('Success.TButton', background=self.success_color, foreground=self.fg_color)
        style.map('Success.TButton', background=[('active', '#45a049')])
        
        main_container = ttk.Frame(self.root, style='TFrame')
        main_container.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        left_frame = ttk.Frame(main_container, style='TFrame')
        left_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(0, 5))
        control_frame = ttk.Frame(left_frame, style='TFrame')
        control_frame.pack(pady=10)
        
        ttk.Label(control_frame, text="COM Port:").pack(side=tk.LEFT, padx=5)
        self.port_var = tk.StringVar(value="COM6")
        port_entry = ttk.Entry(control_frame, textvariable=self.port_var, width=10)
        port_entry.pack(side=tk.LEFT, padx=5)
        
        self.connect_btn = ttk.Button(control_frame, text="Connect", command=self.toggle_connection)
        self.connect_btn.pack(side=tk.LEFT, padx=5)
        
        self.status_label = ttk.Label(control_frame, text="Disconnected", foreground=self.error_color)
        self.status_label.pack(side=tk.LEFT, padx=10)
        
        data_frame = ttk.LabelFrame(left_frame, text="Current Values", padding=10)
        data_frame.pack(fill=tk.X, padx=10, pady=5)
        
        self.value_labels = {}
        row = 0
        col = 0
        for key in ['battery', 'rpm', 'load_voltage', 'speed_mode', 'power_state', 'est_power']:
            ttk.Label(data_frame, text=f"{key.replace('_', ' ').title()}:").grid(row=row, column=col*2, sticky='w', padx=5)
            self.value_labels[key] = ttk.Label(data_frame, text="0", font=('Arial', 12, 'bold'))
            self.value_labels[key].grid(row=row, column=col*2+1, sticky='w', padx=5)
            col += 1
            if col >= 3:
                col = 0
                row += 1
        
        status_frame = ttk.LabelFrame(left_frame, text="Status Indicators", padding=10)
        status_frame.pack(fill=tk.X, padx=10, pady=5)
        
        self.status_indicators = {}
        for i, status in enumerate(['reverse', 'brake', 'regen']):
            self.status_indicators[status] = tk.Label(status_frame, text=status.upper(), 
                                                    width=10, height=2, relief='raised',
                                                    bg=self.button_bg, fg=self.fg_color)
            self.status_indicators[status].pack(side=tk.LEFT, padx=5)
        
        # NEW: Temperature Analysis Frame
        temp_frame = ttk.LabelFrame(left_frame, text="Temperature Candidates (Unknown Bytes)", padding=10)
        temp_frame.pack(fill=tk.X, padx=10, pady=5)
        
        self.temp_labels = {}
        for i in range(4):
            byte_frame = ttk.Frame(temp_frame)
            byte_frame.pack(fill=tk.X, pady=2)
            
            # Byte label
            ttk.Label(byte_frame, text=f"Byte {i}:", width=8).pack(side=tk.LEFT, padx=2)
            
            # Raw value
            self.temp_labels[f'byte{i}_raw'] = ttk.Label(byte_frame, text="Raw: 0", width=10, font=('Courier', 10))
            self.temp_labels[f'byte{i}_raw'].pack(side=tk.LEFT, padx=2)
            
            # Direct Celsius
            self.temp_labels[f'byte{i}_direct'] = ttk.Label(byte_frame, text="0°C", width=8, font=('Arial', 10))
            self.temp_labels[f'byte{i}_direct'].pack(side=tk.LEFT, padx=2)
            
            # Offset Celsius (-40)
            self.temp_labels[f'byte{i}_offset'] = ttk.Label(byte_frame, text="0°C", width=8, font=('Arial', 10))
            self.temp_labels[f'byte{i}_offset'].pack(side=tk.LEFT, padx=2)
            
            # Scaled
            self.temp_labels[f'byte{i}_scaled'] = ttk.Label(byte_frame, text="0°C", width=8, font=('Arial', 10))
            self.temp_labels[f'byte{i}_scaled'].pack(side=tk.LEFT, padx=2)
            
            # Hex value
            self.temp_labels[f'byte{i}_hex'] = ttk.Label(byte_frame, text="0x00", width=8, font=('Courier', 10))
            self.temp_labels[f'byte{i}_hex'].pack(side=tk.LEFT, padx=2)
        
        # Legend for temperature display
        legend_frame = ttk.Frame(temp_frame)
        legend_frame.pack(fill=tk.X, pady=5)
        ttk.Label(legend_frame, text="Legend: Raw | Direct°C | Offset°C (-40) | Scaled°C (×0.5-40) | Hex", 
                 font=('Arial', 9), foreground=self.accent_color).pack()
        
        raw_frame = ttk.LabelFrame(left_frame, text="Raw SIF Bytes (Hex)", padding=10)
        raw_frame.pack(fill=tk.X, padx=10, pady=5)
        
        self.hex_labels = []
        for i in range(12):
            label = tk.Label(raw_frame, text=f"B{i}: 00", font=('Courier', 10), width=8,
                           bg=self.frame_bg, fg=self.fg_color)
            label.pack(side=tk.LEFT, padx=2)
            self.hex_labels.append(label)
        
        right_frame = ttk.Frame(main_container, style='TFrame')
        right_frame.pack(side=tk.RIGHT, fill=tk.Y, padx=(5, 0))
        control_panel = ttk.LabelFrame(right_frame, text="Manual Controls", padding=10)
        control_panel.pack(fill=tk.BOTH, expand=True)
        self.sliders = {}
        battery_frame = ttk.Frame(control_panel)
        battery_frame.pack(fill=tk.X, pady=5)
        ttk.Label(battery_frame, text="Battery %:", width=15).pack(side=tk.LEFT)
        self.sliders['battery'] = Scale(battery_frame, from_=0, to=100, orient=tk.HORIZONTAL,
                                       bg=self.entry_bg, fg=self.fg_color, highlightthickness=0,
                                       troughcolor=self.button_bg, activebackground=self.accent_color,
                                       borderwidth=0, sliderlength=20,
                                       command=lambda v: self.update_manual_value('battery', float(v)))
        self.sliders['battery'].set(50)
        self.sliders['battery'].pack(side=tk.LEFT, fill=tk.X, expand=True)
        self.battery_label = ttk.Label(battery_frame, text="50.0%", width=8)
        self.battery_label.pack(side=tk.LEFT)
        rpm_frame = ttk.Frame(control_panel)
        rpm_frame.pack(fill=tk.X, pady=5)
        ttk.Label(rpm_frame, text="RPM:", width=15).pack(side=tk.LEFT)
        self.sliders['rpm'] = Scale(
            rpm_frame,
            from_=0,
            to=10000,
            orient=tk.HORIZONTAL,
            resolution=1,
            bg=self.entry_bg,
            fg=self.fg_color,
            highlightthickness=0,
            troughcolor=self.button_bg,
            activebackground=self.accent_color,
            borderwidth=0,
            sliderlength=20,
            command=lambda v: self.update_manual_value('rpm', int(float(v)))
        )
        self.sliders['rpm'].set(0)
        self.sliders['rpm'].pack(side=tk.LEFT, fill=tk.X, expand=True)
        self.rpm_label = ttk.Label(rpm_frame, text="0", width=8)
        self.rpm_label.pack(side=tk.LEFT)
        speed_frame = ttk.Frame(control_panel)
        speed_frame.pack(fill=tk.X, pady=5)
        ttk.Label(speed_frame, text="Speed Mode:", width=15).pack(side=tk.LEFT)
        self.sliders['speed_mode'] = Scale(speed_frame, from_=1, to=5, orient=tk.HORIZONTAL,
                                          bg=self.entry_bg, fg=self.fg_color, highlightthickness=0,
                                          troughcolor=self.button_bg, activebackground=self.accent_color,
                                          borderwidth=0, sliderlength=20,
                                          command=lambda v: self.update_manual_value('speed_mode', int(float(v))))
        self.sliders['speed_mode'].set(1)
        self.sliders['speed_mode'].pack(side=tk.LEFT, fill=tk.X, expand=True)
        self.speed_mode_label = ttk.Label(speed_frame, text="1", width=8)
        self.speed_mode_label.pack(side=tk.LEFT)
        current_frame = ttk.Frame(control_panel)
        current_frame.pack(fill=tk.X, pady=5)
        ttk.Label(current_frame, text="Current (A):", width=15).pack(side=tk.LEFT)
        self.sliders['current'] = Scale(current_frame, from_=-100, to=200, orient=tk.HORIZONTAL,
                                       bg=self.entry_bg, fg=self.fg_color, highlightthickness=0,
                                       troughcolor=self.button_bg, activebackground=self.accent_color,
                                       borderwidth=0, sliderlength=20,
                                       command=lambda v: self.update_manual_value('current', int(float(v))))
        self.sliders['current'].set(0)
        self.sliders['current'].pack(side=tk.LEFT, fill=tk.X, expand=True)
        self.current_label = ttk.Label(current_frame, text="0A", width=8)
        self.current_label.pack(side=tk.LEFT)
        voltage_frame = ttk.Frame(control_panel)
        voltage_frame.pack(fill=tk.X, pady=5)
        ttk.Label(voltage_frame, text="Voltage (V):", width=15).pack(side=tk.LEFT)
        self.sliders['voltage'] = Scale(voltage_frame, from_=0, to=60, resolution=0.1, orient=tk.HORIZONTAL,
                                       bg=self.entry_bg, fg=self.fg_color, highlightthickness=0,
                                       troughcolor=self.button_bg, activebackground=self.accent_color,
                                       borderwidth=0, sliderlength=20,
                                       command=lambda v: self.update_manual_value('voltage', float(v)))
        self.sliders['voltage'].set(12.0)
        self.sliders['voltage'].pack(side=tk.LEFT, fill=tk.X, expand=True)
        self.voltage_label = ttk.Label(voltage_frame, text="12.0V", width=8)
        self.voltage_label.pack(side=tk.LEFT)
        bool_frame = ttk.LabelFrame(control_panel, text="Status Controls", padding=10)
        bool_frame.pack(fill=tk.X, pady=10)
        
        self.bool_vars = {}
        for control in ['reverse', 'brake', 'regen']:
            self.bool_vars[control] = tk.BooleanVar()
            cb = ttk.Checkbutton(bool_frame, text=control.title(), 
                                variable=self.bool_vars[control],
                                command=lambda c=control: self.update_manual_value(c, int(self.bool_vars[c].get())))
            cb.pack(anchor='w', pady=2)
        send_frame = ttk.Frame(control_panel)
        send_frame.pack(fill=tk.X, pady=10)
        
        self.auto_send_var = tk.BooleanVar()
        ttk.Checkbutton(send_frame, text="Auto Send", variable=self.auto_send_var).pack(side=tk.LEFT)
        
        self.send_btn = ttk.Button(send_frame, text="Send Data", command=self.send_manual_data,
                                  style='Accent.TButton')
        self.send_btn.pack(side=tk.RIGHT, padx=5)
        format_frame = ttk.LabelFrame(control_panel, text="Data Format", padding=5)
        format_frame.pack(fill=tk.X, pady=5)
        
        self.format_label = ttk.Label(format_frame, text="", font=('Courier', 9), foreground=self.accent_color)
        self.format_label.pack()
        self.update_format_display()
        self.auto_send_timer()
    
    def convert_temp_candidates(self, raw_byte):
        """Convert raw byte to potential temperature values using common automotive methods"""
        return {
            'raw': raw_byte,
            'direct_c': raw_byte,  # Direct Celsius
            'offset_c': raw_byte - 40,  # Common automotive offset (-40°C)
            'scaled_c': (raw_byte * 0.5) - 40,  # Scaled with offset
            'adc_c': (raw_byte / 255.0) * 150 - 40,  # ADC scaled to reasonable temp range
            'hex': f"0x{raw_byte:02X}"
        }
    
    def setup_plots(self):
        plt.style.use('dark_background')
        
        self.fig, self.axes = plt.subplots(2, 3, figsize=(15, 8), facecolor=self.bg_color)
        self.fig.suptitle('SIF Vehicle Data Live Dashboard', fontsize=16, fontweight='bold', color=self.fg_color)
        self.axes[0,0].set_title('Battery %', color=self.fg_color)
        self.axes[0,0].set_ylabel('Percentage', color=self.fg_color)
        self.axes[0,0].set_ylim(0, 100)
        self.axes[0,0].grid(True, alpha=0.3)
        self.axes[0,0].set_facecolor(self.frame_bg)
        
        self.axes[0,1].set_title('RPM', color=self.fg_color)
        self.axes[0,1].set_ylabel('RPM', color=self.fg_color)
        self.axes[0,1].set_ylim(0, 10000)
        self.axes[0,1].grid(True, alpha=0.3)
        self.axes[0,1].set_facecolor(self.frame_bg)
        
        self.axes[0,2].set_title('Load Voltage', color=self.fg_color)
        self.axes[0,2].set_ylabel('Voltage', color=self.fg_color)
        self.axes[0,2].set_ylim(0, 60)
        self.axes[0,2].grid(True, alpha=0.3)
        self.axes[0,2].set_facecolor(self.frame_bg)
        
        self.axes[1,0].set_title('Power State', color=self.fg_color)
        self.axes[1,0].set_ylabel('State', color=self.fg_color)
        self.axes[1,0].set_ylim(-0.5, 3.5)
        self.axes[1,0].set_yticks([0, 1, 2, 3])
        self.axes[1,0].set_yticklabels(['IDLE', 'COAST', 'LOAD', 'REGEN'])
        self.axes[1,0].grid(True, alpha=0.3)
        self.axes[1,0].set_facecolor(self.frame_bg)
        
        self.axes[1,1].set_title('Estimated Power', color=self.fg_color)
        self.axes[1,1].set_ylabel('Power', color=self.fg_color)
        self.axes[1,1].set_ylim(0, 100)
        self.axes[1,1].grid(True, alpha=0.3)
        self.axes[1,1].set_facecolor(self.frame_bg)
        
        self.axes[1,2].set_title('Direction & Status', color=self.fg_color)
        self.axes[1,2].set_ylabel('Status', color=self.fg_color)
        self.axes[1,2].set_ylim(-0.5, 1.5)
        self.axes[1,2].set_yticks([0, 1])
        self.axes[1,2].set_yticklabels(['Stopped', 'Moving'])
        self.axes[1,2].grid(True, alpha=0.3)
        self.axes[1,2].set_facecolor(self.frame_bg)
        for ax in self.axes.flat:
            ax.tick_params(colors=self.fg_color)
            ax.spines['bottom'].set_color(self.button_bg)
            ax.spines['top'].set_color(self.button_bg)
            ax.spines['right'].set_color(self.button_bg)
            ax.spines['left'].set_color(self.button_bg)
        
        self.lines = {
            'battery': self.axes[0,0].plot([], [], '#4CAF50', linewidth=2)[0],  # Green
            'rpm': self.axes[0,1].plot([], [], '#2196F3', linewidth=2)[0],      # Blue
            'load_voltage': self.axes[0,2].plot([], [], '#FF5722', linewidth=2)[0],  # Red-orange
            'power_state': self.axes[1,0].plot([], [], 'o-', color='#9C27B0', linewidth=2, markersize=4)[0],  # Purple
            'est_power': self.axes[1,1].plot([], [], '#FF9800', linewidth=2)[0],     # Orange
            'direction': self.axes[1,2].plot([], [], '#00BCD4', linewidth=2)[0]      # Cyan
        }
        
        plt.tight_layout()
    
    def update_manual_value(self, key, value):
        self.manual_values[key] = value
        if key == 'battery':
            self.battery_label.config(text=f"{value:.1f}%")
        elif key == 'rpm':
            self.rpm_label.config(text=str(value))
        elif key == 'speed_mode':
            self.speed_mode_label.config(text=str(value))
        elif key == 'current':
            self.current_label.config(text=f"{value}A")
        elif key == 'voltage':
            self.voltage_label.config(text=f"{value:.1f}V")
        
        self.update_format_display()
    
    def update_format_display(self):
        format_str = (f"DATA,{self.manual_values['battery']:.1f},"
                     f"{self.manual_values['rpm']},"
                     f"{self.manual_values['speed_mode']},"
                     f"{self.manual_values['reverse']},"
                     f"{self.manual_values['brake']},"
                     f"{self.manual_values['regen']},"
                     f"{self.manual_values['current']},"
                     f"{self.manual_values['voltage']:.1f}")
        self.format_label.config(text=format_str)
    
    def send_manual_data(self):
        if self.serial_port and self.serial_port.is_open:
            try:
                data_str = (f"DATA,{self.manual_values['battery']:.1f},"
                           f"{self.manual_values['rpm']},"
                           f"{self.manual_values['speed_mode']},"
                           f"{self.manual_values['reverse']},"
                           f"{self.manual_values['brake']},"
                           f"{self.manual_values['regen']},"
                           f"{self.manual_values['current']},"
                           f"{self.manual_values['voltage']:.1f}\n")
                
                self.serial_port.write(data_str.encode())
                self.send_btn.config(text="Sent!", style='Success.TButton')
                self.root.after(500, lambda: self.send_btn.config(text="Send Data", style='Accent.TButton'))
                
            except Exception as e:
                messagebox.showerror("Send Error", f"Failed to send data: {str(e)}")
        else:
            messagebox.showwarning("Not Connected", "Please connect to serial port first")
    
    def auto_send_timer(self):
        if self.auto_send_var.get() and self.serial_port and self.serial_port.is_open:
            self.send_manual_data()
        self.root.after(100, self.auto_send_timer)  # Check every 100ms
        
    def toggle_connection(self):
        if not self.running:
            self.connect_serial()
        else:
            self.disconnect_serial()
    
    def connect_serial(self):
        try:
            self.serial_port = serial.Serial(self.port_var.get(), 115200, timeout=1)
            self.running = True
            self.serial_thread = threading.Thread(target=self.read_serial_data, daemon=True)
            self.serial_thread.start()
            
            self.connect_btn.config(text="Disconnect")
            self.status_label.config(text="Connected", foreground=self.success_color)
            
            self.ani = animation.FuncAnimation(self.fig, self.update_plots, interval=100, blit=False)
            if hasattr(self.fig.canvas.manager, 'window'):
                self.fig.canvas.manager.window.wm_attributes('-topmost', False)
            plt.show()
            
        except Exception as e:
            messagebox.showerror("Connection Error", f"Failed to connect: {str(e)}")
    
    def disconnect_serial(self):
        self.running = False
        if self.serial_port:
            self.serial_port.close()
        
        self.connect_btn.config(text="Connect")
        self.status_label.config(text="Disconnected", foreground=self.error_color)
    
    def read_serial_data(self):
        header_found = False
        
        while self.running:
            try:
                if self.serial_port and self.serial_port.in_waiting:
                    line = self.serial_port.readline().decode('utf-8').strip()
                    
                    if not header_found:
                        if line.startswith("Timestamp"):
                            header_found = True
                        continue
                    
                    if line and not line.startswith("SIF") and not line.startswith("Waiting"):
                        self.parse_data_line(line)
                        
            except Exception as e:
                print(f"Serial read error: {e}")
                time.sleep(0.1)
    
    def parse_data_line(self, line):
        try:
            parts = line.split(',')
            if len(parts) >= 25:  
                timestamp = float(parts[0])
                raw_bytes = [int(parts[i]) for i in range(1, 13)]
                battery = int(parts[13])
                load_voltage = int(parts[14])
                rpm = int(parts[15])
                speed_mode = int(parts[16])
                reverse = int(parts[17])
                brake = int(parts[18])
                regen = int(parts[19])
                power_state = parts[20]
                b2_direction = int(parts[21])
                est_power = float(parts[22])

                # NEW: Extract temperature candidate bytes
                byte0, byte1, byte2, byte3 = raw_bytes[0], raw_bytes[1], raw_bytes[2], raw_bytes[3]

                data_point = {
                    'timestamp': timestamp,
                    'raw_bytes': raw_bytes,
                    'battery': battery,
                    'load_voltage': load_voltage,
                    'rpm': rpm,
                    'speed_mode': speed_mode,
                    'reverse': reverse,
                    'brake': brake,
                    'regen': regen,
                    'power_state': power_state,
                    'b2_direction': b2_direction,
                    'est_power': est_power,
                    # NEW: Temperature candidates
                    'byte0_raw': byte0,
                    'byte1_raw': byte1,
                    'byte2_raw': byte2,
                    'byte3_raw': byte3,
                    'byte0_temp_direct': byte0,
                    'byte1_temp_direct': byte1,
                    'byte2_temp_direct': byte2,
                    'byte3_temp_direct': byte3,
                    'byte0_temp_offset': byte0 - 40,
                    'byte1_temp_offset': byte1 - 40,
                    'byte2_temp_offset': byte2 - 40,
                    'byte3_temp_offset': byte3 - 40,
                    'byte0_temp_scaled': (byte0 * 0.5) - 40,
                    'byte1_temp_scaled': (byte1 * 0.5) - 40,
                    'byte2_temp_scaled': (byte2 * 0.5) - 40,
                    'byte3_temp_scaled': (byte3 * 0.5) - 40
                }
                
                self.data_queue.put(data_point)
                
        except Exception as e:
            print(f"Parse error: {e}")
    
    def get_temp_color(self, temp_value, method='offset'):
        """Color code temperature values based on reasonableness"""
        if method == 'offset':
            # For offset method (-40), reasonable motor/battery temps
            if 15 <= temp_value <= 85:
                return 'green'  # Good temperature range
            elif temp_value > 85:
                return 'red'    # Hot
            elif temp_value < 0:
                return 'blue'   # Cold
            else:
                return 'orange' # Questionable but possible
        elif method == 'direct':
            # For direct Celsius
            if 20 <= temp_value <= 100:
                return 'green'
            elif temp_value > 100:
                return 'red'
            else:
                return 'blue'
        else:
            return 'white'
    
    def update_gui_values(self, data):
        self.current_values.update(data)
        
        for key, label in self.value_labels.items():
            if key in data:
                if key == 'est_power':
                    label.config(text=f"{data[key]:.1f}")
                else:
                    label.config(text=str(data[key]))
        
        for status in ['reverse', 'brake', 'regen']:
            if data[status]:
                color = self.error_color if status in ['reverse', 'brake'] else self.success_color
                self.status_indicators[status].config(bg=color, fg=self.fg_color)
            else:
                self.status_indicators[status].config(bg=self.button_bg, fg=self.fg_color)
        
        # NEW: Update temperature candidate displays
        for i in range(4):
            if f'byte{i}_raw' in data:
                raw_val = data[f'byte{i}_raw']
                direct_temp = data[f'byte{i}_temp_direct']
                offset_temp = data[f'byte{i}_temp_offset']
                scaled_temp = data[f'byte{i}_temp_scaled']
                
                # Raw value
                self.temp_labels[f'byte{i}_raw'].config(text=f"Raw: {raw_val}")
                
                # Direct temperature
                direct_color = self.get_temp_color(direct_temp, 'direct')
                self.temp_labels[f'byte{i}_direct'].config(
                    text=f"{direct_temp:.0f}°C", 
                    foreground=direct_color
                )
                
                # Offset temperature (-40)
                offset_color = self.get_temp_color(offset_temp, 'offset')
                self.temp_labels[f'byte{i}_offset'].config(
                    text=f"{offset_temp:.0f}°C", 
                    foreground=offset_color
                )
                
                # Scaled temperature
                scaled_color = self.get_temp_color(scaled_temp, 'offset')
                self.temp_labels[f'byte{i}_scaled'].config(
                    text=f"{scaled_temp:.1f}°C", 
                    foreground=scaled_color
                )
                
                # Hex value
                self.temp_labels[f'byte{i}_hex'].config(text=f"0x{raw_val:02X}")
        
        for i, byte_val in enumerate(data['raw_bytes']):
            self.hex_labels[i].config(text=f"B{i}: {byte_val:02X}")
    
    def update_plots(self, frame):
        while not self.data_queue.empty():
            try:
                data = self.data_queue.get_nowait()
                
                self.timestamps.append(data['timestamp'])
                self.battery_data.append(data['battery'])
                self.rpm_data.append(data['rpm'])
                self.load_voltage_data.append(data['load_voltage'])
                self.b2_direction.append(data['b2_direction'])
                self.est_power.append(data['est_power'])
                
                # NEW: Add temperature candidate data
                self.byte0_data.append(data['byte0_raw'])
                self.byte1_data.append(data['byte1_raw'])
                self.byte2_data.append(data['byte2_raw'])
                self.byte3_data.append(data['byte3_raw'])
                
                power_state_num = {'IDLE': 0, 'COAST': 1, 'LOAD': 2, 'REGEN': 3}.get(data['power_state'], 0)
                self.power_state_data.append(power_state_num)
                
                self.root.after_idle(self.update_gui_values, data)
                
            except queue.Empty:
                break
        
        if len(self.timestamps) > 0:
            times = list(self.timestamps)
            
            self.lines['battery'].set_data(times, list(self.battery_data))
            self.lines['rpm'].set_data(times, list(self.rpm_data))
            self.lines['load_voltage'].set_data(times, list(self.load_voltage_data))
            self.lines['power_state'].set_data(times, list(self.power_state_data))
            self.lines['est_power'].set_data(times, list(self.est_power))
            self.lines['direction'].set_data(times, list(self.b2_direction))
            
            for ax in self.axes.flat:
                if len(times) > 1:
                    ax.set_xlim(min(times), max(times))
                ax.relim()
                ax.autoscale_view(scalex=True, scaley=False)
        
        return list(self.lines.values())
    
    def run(self):
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)
        self.root.mainloop()
    
    def on_closing(self):
        self.disconnect_serial()
        plt.close('all')
        self.root.destroy()

if __name__ == "__main__":
    required_packages = ['serial', 'matplotlib', 'pandas', 'numpy']
    missing_packages = []
    
    for package in required_packages:
        try:
            if package == 'serial':
                import serial
            else:
                __import__(package)
        except ImportError:
            missing_packages.append('pyserial' if package == 'serial' else package)
    
    if missing_packages:
        print("Missing packages:", missing_packages)
        print("Install with: pip install", " ".join(missing_packages))
        sys.exit(1)
    
    dashboard = SIFDashboard()
    dashboard.run()