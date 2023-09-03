from flask import Flask, request, jsonify
from flask_socketio import SocketIO
import sqlite3
import shutil
import json
import threading

app = Flask(__name__)
socketio = SocketIO(app)
local_storage = threading.local()

def get_db_connection():
    """Get a thread-local SQLite connection."""
    if not hasattr(local_storage, 'conn'):
        local_storage.conn = sqlite3.connect('cmake-build-debug/lora.db')
    return local_storage.conn

def get_db_cursor():
    """Get a thread-local SQLite cursor."""
    conn = get_db_connection()
    if not hasattr(local_storage, 'cursor'):
        local_storage.cursor = conn.cursor()
    return local_storage.cursor

@app.route('/add_sensor_data', methods=['POST'])
def add_sensor_data():
    try:
        data = request.get_json()
        device_id = data['device_id']
        light_intensity = data['light_intensity']
        temperature = data['temperature']
        air_humidity = data['air_humidity']
        soil_humidity = data['soil_humidity']
        prediction = data['prediction']
        timestamp = data['timestamp']
        note = data['note']

        conn = get_db_connection()
        cursor = conn.cursor()

        cursor.execute("INSERT INTO sensor_data (device_id, light_intensity, temperature, air_humidity, soil_humidity, timestamp, prediction, note) VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
                       (device_id, light_intensity, temperature, air_humidity, soil_humidity, timestamp, prediction, note))
        conn.commit()

        socketio.emit('sensor_update', data)

        return jsonify({'message': 'Sensor data added successfully!'}), 201
    except Exception as e:
        return jsonify({'error': str(e)}), 400

@app.route('/update_sensor_data/<int:sensor_id>', methods=['PUT'])
def update_sensor_data(sensor_id):
    try:
        data = request.get_json()

        fields_to_update = {
            'light_intensity': data.get('light_intensity'),
            'temperature': data.get('temperature'),
            'air_humidity': data.get('air_humidity'),
            'soil_humidity': data.get('soil_humidity'),
            'prediction': data.get('prediction'),
            'timestamp': data.get('timestamp'),
            'note': data.get('note')
        }

        update_query = ", ".join([f"{field} = ?" for field in fields_to_update.keys()])
        values = tuple(fields_to_update.values())

        query = f"UPDATE sensor_data SET {update_query} WHERE id = ?"
        values += (sensor_id,)

        conn = get_db_connection()
        cursor = conn.cursor()

        cursor.execute(query, values)
        conn.commit()

        socketio.emit('sensor_update', data)

        return jsonify({'message': 'Sensor data updated successfully!'}), 200
    except Exception as e:
        return jsonify({'error': str(e)}), 400

@app.route('/get_sensor_data', methods=['GET'])
def get_sensor_data():
    try:
        conn = get_db_connection()
        cursor = conn.cursor()

        cursor.execute("SELECT * FROM sensor_data")
        data = cursor.fetchall()

        sensor_data_list = []
        for row in data:
            sensor_data = {
                'id': row[0],
                'device_id': row[1],
                'light_intensity': row[2],
                'temperature': row[3],
                'air_humidity': row[4],
                'soil_humidity': row[5],
                'prediction': row[6],
                'timestamp': row[7],
                'note': row[8]
            }
            sensor_data_list.append(sensor_data)

        return jsonify(sensor_data_list), 200
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@app.route('/backup_database', methods=['POST'])
def backup_database():
    try:
        src_file = 'cmake-build-debug/lora.db'
        backup_file = 'backup/lora_backup.db'

        shutil.copy(src_file, backup_file)

        return jsonify({'message': 'Database backup created successfully!'}), 200
    except Exception as e:
        return jsonify({'error': str(e)}), 500

# Route để thêm dữ liệu vào bảng user_control
@app.route('/add_user_control', methods=['POST'])
def add_user_control():
    try:
        data = request.get_json()
        device_id = data['device_id']
        command = data['command']
        timestamp_utc7 = data['timestamp']

        conn = get_db_connection()
        cursor = conn.cursor()

        cursor.execute("INSERT INTO user_control (device_id, command, timestamp) VALUES (?, ?, ?)",
                       (device_id, command, timestamp_utc7))
        conn.commit()

        return jsonify({'message': 'User control data added successfully!'}), 201
    except Exception as e:
        return jsonify({'error': str(e)}), 400

# Route để lấy dữ liệu từ bảng user_control
@app.route('/get_user_control', methods=['GET'])
def get_user_control():
    try:
        conn = get_db_connection()
        cursor = conn.cursor()

        cursor.execute("SELECT * FROM user_control")
        data = cursor.fetchall()

        user_control_list = []
        for row in data:
            user_control_data = {
                'id': row[0],
                'device_id': row[1],
                'command': row[2],
                'timestamp': row[3]
            }
            user_control_list.append(user_control_data)

        return jsonify(user_control_list), 200
    except Exception as e:
        return jsonify({'error': str(e)}), 500

# Route để cập nhật dữ liệu trong bảng user_control dựa trên ID
@app.route('/update_user_control/<int:user_control_id>', methods=['PUT'])
def update_user_control(user_control_id):
    try:
        data = request.get_json()

        fields_to_update = {
            'device_id': data.get('device_id'),
            'command': data.get('command'),
            'timestamp': data.get('timestamp')
        }

        update_query = ", ".join([f"{field} = ?" for field in fields_to_update.keys()])
        values = tuple(fields_to_update.values())

        query = f"UPDATE user_control SET {update_query} WHERE id = ?"
        values += (user_control_id,)

        conn = get_db_connection()
        cursor = conn.cursor()

        cursor.execute(query, values)
        conn.commit()

        return jsonify({'message': 'User control data updated successfully!'}), 200
    except Exception as e:
        return jsonify({'error': str(e)}), 400


if __name__ == '__main__':
    socketio.run(app, debug=True)
