# visualize_data.py
import pandas as pd
import matplotlib.pyplot as plt
import os
import sys

def plot_device_data(device_id=None):
    data_dir = 'lamp_data'
    
    if not os.path.exists(data_dir):
        print(f"Error: Data directory '{data_dir}' not found")
        return
    
    # If no device specified, list available devices
    if device_id is None:
        devices = [f.split('.')[0] for f in os.listdir(data_dir) if f.endswith('.csv')]
        if not devices:
            print("No device data found")
            return
        
        print("Available devices:")
        for i, device in enumerate(devices):
            print(f"{i+1}. {device}")
        
        selection = input("Select device number (or 'all' to plot all): ")
        if selection.lower() == 'all':
            for device in devices:
                plot_device_data(device)
            return
        else:
            try:
                device_id = devices[int(selection) - 1]
            except (ValueError, IndexError):
                print("Invalid selection")
                return
    
    # Load data for the specified device
    file_path = os.path.join(data_dir, f"{device_id}.csv")
    if not os.path.exists(file_path):
        print(f"Error: No data file found for device {device_id}")
        return
    
    df = pd.read_csv(file_path)
    
    # Convert timestamp to datetime
    df['timestamp'] = pd.to_datetime(df['timestamp'])
    
    # Create figure with two subplots
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8), sharex=True)
    
    # Plot voltage
    ax1.plot(df['timestamp'], df['voltage'], 'b-')
    ax1.set_ylabel('Battery Voltage (V)')
    ax1.set_title(f'Device {device_id} - Battery Voltage Over Time')
    ax1.grid(True)
    
    # Plot potentiometer position
    ax2.plot(df['timestamp'], df['position'], 'r-')
    ax2.set_ylabel('Potentiometer Position (%)')
    ax2.set_xlabel('Time')
    ax2.set_title('Potentiometer Position Over Time')
    ax2.grid(True)
    
    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    if len(sys.argv) > 1:
        plot_device_data(sys.argv[1])
    else:
        plot_device_data()