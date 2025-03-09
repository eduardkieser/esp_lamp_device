from flask import Flask, request, jsonify
import csv
from datetime import datetime
import os

app = Flask(__name__)

# Ensure data directory exists
data_dir = 'lamp_data'
os.makedirs(data_dir, exist_ok=True)

@app.route('/api/log', methods=['POST'])
def log_data():
    try:
        data = request.json
        
        # Validate required fields
        required_fields = ['device_id', 'voltage', 'position']
        for field in required_fields:
            if field not in data:
                return jsonify({'error': f'Missing required field: {field}'}), 400
        
        # Add timestamp
        timestamp = datetime.now().isoformat()
        data['timestamp'] = timestamp
        
        # Save to CSV file
        device_id = data['device_id']
        filename = os.path.join(data_dir, f'{device_id}.csv')
        
        # Check if file exists to write headers
        file_exists = os.path.isfile(filename)
        
        with open(filename, 'a', newline='') as csvfile:
            fieldnames = ['timestamp', 'device_id', 'voltage', 'position']
            writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
            
            if not file_exists:
                writer.writeheader()
            
            writer.writerow(data)
        
        return jsonify({'status': 'success', 'message': 'Data logged successfully'}), 200
    
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@app.route('/api/devices', methods=['GET'])
def list_devices():
    try:
        devices = []
        for filename in os.listdir(data_dir):
            if filename.endswith('.csv'):
                device_id = filename.split('.')[0]
                devices.append(device_id)
        
        return jsonify({'devices': devices}), 200
    
    except Exception as e:
        return jsonify({'error': str(e)}), 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=4999, debug=True)