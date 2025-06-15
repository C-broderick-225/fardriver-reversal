import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.patches import Rectangle
import pandas as pd
import numpy as np
from collections import deque
import threading
import time
import queue
import tkinter as tk
from tkinter import ttk, messagebox
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
        

        self.current_values = {
            'battery': 0, 'rpm': 0, 'load_voltage': 0, 'speed_mode': 0,
            'reverse': 0, 'brake': 0, 'regen': 0, 'power_state': 'IDLE',
            'b2_direction': 0, 'est_power': 0.0
        }
        

        self.raw_bytes = [0] * 12
        self.setup_gui()
        self.setup_plots()
        
    def setup_gui(self):
        self.root = tk.Tk()
        self.root.title("SIF Vehicle Data Dashboard")
        self.root.geometry("1400x800")
        
        control_frame = ttk.Frame(self.root)
        control_frame.pack(pady=10)
        
        ttk.Label(control_frame, text="COM Port:").pack(side=tk.LEFT, padx=5)
        self.port_var = tk.StringVar(value="COM6")
        port_entry = ttk.Entry(control_frame, textvariable=self.port_var, width=10)
        port_entry.pack(side=tk.LEFT, padx=5)
        
        self.connect_btn = ttk.Button(control_frame, text="Connect", command=self.toggle_connection)
        self.connect_btn.pack(side=tk.LEFT, padx=5)
        
        self.status_label = ttk.Label(control_frame, text="Disconnected", foreground="red")
        self.status_label.pack(side=tk.LEFT, padx=10)
        
        data_frame = ttk.LabelFrame(self.root, text="Current Values", padding=10)
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
        
        status_frame = ttk.LabelFrame(self.root, text="Status Indicators", padding=10)
        status_frame.pack(fill=tk.X, padx=10, pady=5)
        
        self.status_indicators = {}
        for i, status in enumerate(['reverse', 'brake', 'regen']):
            self.status_indicators[status] = tk.Label(status_frame, text=status.upper(), 
                                                    width=10, height=2, relief='raised')
            self.status_indicators[status].pack(side=tk.LEFT, padx=5)
        
        raw_frame = ttk.LabelFrame(self.root, text="Raw SIF Bytes (Hex)", padding=10)
        raw_frame.pack(fill=tk.X, padx=10, pady=5)
        
        self.hex_labels = []
        for i in range(12):
            label = tk.Label(raw_frame, text=f"B{i}: 00", font=('Courier', 10), width=8)
            label.pack(side=tk.LEFT, padx=2)
            self.hex_labels.append(label)
    
    def setup_plots(self):
        self.fig, self.axes = plt.subplots(2, 3, figsize=(15, 8))
        self.fig.suptitle('SIF Vehicle Data Live Dashboard', fontsize=16, fontweight='bold')
        
        self.axes[0,0].set_title('Battery %')
        self.axes[0,0].set_ylabel('Percentage')
        self.axes[0,0].set_ylim(0, 100)
        self.axes[0,0].grid(True)
        
        self.axes[0,1].set_title('RPM')
        self.axes[0,1].set_ylabel('RPM')
        self.axes[0,1].set_ylim(0, 3000)
        self.axes[0,1].grid(True)
        
        self.axes[0,2].set_title('Load Voltage')
        self.axes[0,2].set_ylabel('Voltage')
        self.axes[0,2].set_ylim(0, 15)
        self.axes[0,2].grid(True)
        
        self.axes[1,0].set_title('Power State')
        self.axes[1,0].set_ylabel('State')
        self.axes[1,0].set_ylim(-0.5, 3.5)
        self.axes[1,0].set_yticks([0, 1, 2, 3])
        self.axes[1,0].set_yticklabels(['IDLE', 'COAST', 'LOAD', 'REGEN'])
        self.axes[1,0].grid(True)
        
        self.axes[1,1].set_title('Estimated Power')
        self.axes[1,1].set_ylabel('Power')
        self.axes[1,1].set_ylim(0, 100)
        self.axes[1,1].grid(True)
        
        self.axes[1,2].set_title('Direction & Status')
        self.axes[1,2].set_ylabel('Status')
        self.axes[1,2].set_ylim(-0.5, 1.5)
        self.axes[1,2].set_yticks([0, 1])
        self.axes[1,2].set_yticklabels(['Stopped', 'Moving'])
        self.axes[1,2].grid(True)
        
        self.lines = {
            'battery': self.axes[0,0].plot([], [], 'g-', linewidth=2)[0],
            'rpm': self.axes[0,1].plot([], [], 'b-', linewidth=2)[0],
            'load_voltage': self.axes[0,2].plot([], [], 'r-', linewidth=2)[0],
            'power_state': self.axes[1,0].plot([], [], 'o-', linewidth=2, markersize=4)[0],
            'est_power': self.axes[1,1].plot([], [], 'm-', linewidth=2)[0],
            'direction': self.axes[1,2].plot([], [], 'c-', linewidth=2)[0]
        }
        
        plt.tight_layout()
        
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
            self.status_label.config(text="Connected", foreground="green")
            
            self.ani = animation.FuncAnimation(self.fig, self.update_plots, interval=100, blit=False)
            plt.show()
            
        except Exception as e:
            messagebox.showerror("Connection Error", f"Failed to connect: {str(e)}")
    
    def disconnect_serial(self):
        self.running = False
        if self.serial_port:
            self.serial_port.close()
        
        self.connect_btn.config(text="Connect")
        self.status_label.config(text="Disconnected", foreground="red")
    
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
                    'est_power': est_power
                }
                
                self.data_queue.put(data_point)
                
        except Exception as e:
            print(f"Parse error: {e}")
    
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
                self.status_indicators[status].config(bg='red', fg='white')
            else:
                self.status_indicators[status].config(bg='lightgray', fg='black')
        
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
        print("Missing fucking packages")
        sys.exit(1)
    
    
    dashboard = SIFDashboard()
    dashboard.run()