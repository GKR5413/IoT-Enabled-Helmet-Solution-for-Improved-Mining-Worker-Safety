from flask import Flask, request, jsonify, render_template, redirect, url_for, session
from datetime import datetime, timedelta
import logging
import secrets
import psycopg2
import json

app = Flask(__name__)
app.secret_key = secrets.token_hex(16)

# Configure logging
logging.basicConfig(level=logging.DEBUG, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# Database config
db_config = {
    'user': 'postgres',
    'password': 'Tanuja',  # Replace with your actual password
    'host': 'localhost',
    'port': 5432,
    'database': 'postgres'
}


def get_db_connection():
    try:
        conn = psycopg2.connect(**db_config)
        return conn
    except psycopg2.Error as e:
        logger.error(f"Database connection error: {e}")
        return None


# In-memory sensor data
sensor_data = {}


@app.route('/')
def home():
    if 'username' in session:
        return redirect(url_for('dashboard'))
    return redirect(url_for('login'))


@app.route('/register', methods=['GET', 'POST'])
def register():
    if request.method == 'POST':
        username = request.form['username']
        password = request.form['password']

        if not username or not password:
            return render_template('register.html', message="All fields are required")

        conn = get_db_connection()
        if conn:
            try:
                cur = conn.cursor()
                cur.execute("SELECT username FROM userstable WHERE username = %s", (username,))
                if cur.fetchone():
                    return render_template('register.html', message="Username already exists")
                cur.execute("INSERT INTO userstable (username, password) VALUES (%s, %s)", (username, password))
                conn.commit()
                cur.close()
                conn.close()
                return redirect(url_for('login'))
            except Exception as e:
                logger.error(f"Registration error: {e}")
                return render_template('register.html', message="Error creating user")
        else:
            return render_template('register.html', message="Database error")

    return render_template('register.html')


@app.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        username = request.form['username']
        password = request.form['password']

        conn = get_db_connection()
        if conn:
            try:
                cur = conn.cursor()
                cur.execute("SELECT password FROM userstable WHERE username = %s", (username,))
                user = cur.fetchone()
                cur.close()
                conn.close()
                if user and user[0] == password:
                    session['username'] = username
                    return redirect(url_for('dashboard'))
                else:
                    return render_template('login.html', message="Invalid credentials")
            except Exception as e:
                logger.error(f"Login error: {e}")
                return render_template('login.html', message="Database error")
        else:
            return render_template('login.html', message="Database connection error")

    return render_template('login.html')


@app.route('/dashboard')
def dashboard():
    if 'username' not in session:
        return redirect(url_for('login'))

    # Get latest sensor data for dashboard display
    latest_data = get_latest_sensor_data()
    return render_template('dashboard.html', username=session['username'], sensor_data=latest_data)


# Update get_latest_sensor_data function
def get_latest_sensor_data():
    conn = get_db_connection()
    latest_data = {
        'temperature': None,
        'humidity': None,
        'gas': None,
        'button_pressed': False,
        'buzzer_active': False,
        'rssi': None,
        'timestamp': None
    }

    if conn:
        try:
            cur = conn.cursor()
            cur.execute("""
                SELECT temperature, humidity, gas, button_pressed, buzzer_active, rssi, timestamp 
                FROM final_sensor_data 
                ORDER BY timestamp DESC 
                LIMIT 1
            """)
            data = cur.fetchone()
            cur.close()
            conn.close()

            if data:
                latest_data = {
                    'temperature': data[0],
                    'humidity': data[1],
                    'gas': data[2],
                    'button_pressed': bool(data[3]),
                    'buzzer_active': bool(data[4]),
                    'rssi': data[5],
                    'timestamp': data[6].strftime('%Y-%m-%d %H:%M:%S')
                }
        except Exception as e:
            logger.error(f"Error fetching latest sensor data: {e}")

    return latest_data
# Update the API upload endpoint
@app.route('/api/upload', methods=['POST'])
def upload_data():
    data = request.get_json()
    logger.debug(f"Received JSON payload: {data}")

    temperature = data.get('temperature')
    humidity = data.get('humidity')
    gas = data.get('gas')
    button_pressed = data.get('button_pressed', False)
    buzzer_active = data.get('buzzer_active', False)
    rssi = data.get('distance')  # New field


    if temperature is None or humidity is None or gas is None:
        return jsonify({"error": "Incomplete data"}), 400

    conn = get_db_connection()
    if conn:
        try:
            cur = conn.cursor()
            cur.execute(
                """INSERT INTO final_sensor_data 
                   (temperature, humidity, gas, button_pressed, buzzer_active, rssi) 
                   VALUES (%s, %s, %s, %s, %s, %s)""",
                (temperature, humidity, gas, button_pressed, buzzer_active, rssi)
            )
            conn.commit()
            cur.close()
            conn.close()

            # Update in-memory data for real-time updates
            sensor_data['temperature'] = temperature
            sensor_data['humidity'] = humidity
            sensor_data['gas'] = gas
            sensor_data['button_pressed'] = button_pressed
            sensor_data['buzzer_active'] = buzzer_active
            sensor_data['rssi'] = rssi
            sensor_data['timestamp'] = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

            return jsonify({"message": "Data inserted successfully"}), 200
        except Exception as e:
            logger.error(f"Database insertion error: {e}")
            return jsonify({"error": "Database error"}), 500
    else:
        return jsonify({"error": "Database connection error"}), 500
# Update the API data endpoint
@app.route('/api/data', methods=['GET'])
def get_data():
    if 'username' not in session:
        return jsonify({"error": "Authentication required"}), 401

    time_range = request.args.get('range', 'day')

    conn = get_db_connection()
    if conn:
        try:
            cur = conn.cursor()

            # Define time range filter based on parameter
            if time_range == 'hour':
                time_filter = datetime.now() - timedelta(hours=1)
            elif time_range == 'day':
                time_filter = datetime.now() - timedelta(days=1)
            elif time_range == 'week':
                time_filter = datetime.now() - timedelta(weeks=1)
            else:
                time_filter = datetime.now() - timedelta(days=1)  # Default to 1 day

            cur.execute("""
                SELECT temperature, humidity, gas, button_pressed, buzzer_active, rssi, timestamp 
                FROM final_sensor_data 
                WHERE timestamp > %s
                ORDER BY timestamp DESC
            """, (time_filter,))

            rows = cur.fetchall()
            cur.close()
            conn.close()

            result = []
            for row in rows:
                result.append({
                    'temperature': row[0],
                    'humidity': row[1],
                    'gas': row[2],
                    'button_pressed': row[3],
                    'buzzer_active': row[4],
                    'rssi': row[5],
                    'timestamp': row[6].strftime('%Y-%m-%d %H:%M:%S')
                })

            return jsonify(result)
        except Exception as e:
            logger.error(f"Error retrieving sensor data: {e}")
            return jsonify({"error": "Database error"}), 500
    else:
        return jsonify({"error": "Database connection error"}), 500
@app.route('/api/latest', methods=['GET'])
def get_latest():
    latest_data = get_latest_sensor_data()
    return jsonify(latest_data)


@app.route('/logout')
def logout():
    session.clear()
    return redirect(url_for('login'))

def init_db():
    conn = get_db_connection()
    if conn:
        try:
            cur = conn.cursor()

            # Create users table if not exists
            cur.execute("""
                CREATE TABLE IF NOT EXISTS userstable (
                    id SERIAL PRIMARY KEY,
                    username VARCHAR(50) UNIQUE NOT NULL,
                    password VARCHAR(50) NOT NULL
                )
            """)

            # Create sensor data table with RSSI and distance fields
            cur.execute("""
                CREATE TABLE IF NOT EXISTS final_sensor_data (
                    id SERIAL PRIMARY KEY,
                    temperature FLOAT NOT NULL,
                    humidity FLOAT NOT NULL,
                    gas INTEGER NOT NULL,
                    button_pressed BOOLEAN DEFAULT FALSE,
                    buzzer_active BOOLEAN DEFAULT FALSE,
                    rssi FLOAT DEFAULT NULL,
                    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                )
            """)

            conn.commit()
            cur.close()
            conn.close()
            logger.info("Database initialized successfully")
        except Exception as e:
            logger.error(f"Database initialization error: {e}")


if __name__ == '__main__':
    init_db()  # Initialize database tables
    app.run(host='0.0.0.0', port=8888, debug=True)