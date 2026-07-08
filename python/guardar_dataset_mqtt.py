import csv
import json
import os
from datetime import datetime

import paho.mqtt.client as mqtt

# -----------------------------
# CONFIGURACION
# -----------------------------
MQTT_BROKER = "********"   # IP de tu PC
MQTT_PORT = 1883
MQTT_TOPIC = "estacionmeteorologica"

# ⚠️ RUTA ABSOLUTA (IMPORTANTE)
CSV_FILE = r"data/dataset_estacion.csv"

CSV_HEADERS = [
    "timestamp",
    "temperatura",
    "humedad",
    "presion",
    "luz"
]

# -----------------------------
# INICIALIZACION CSV
# -----------------------------
def inicializar_csv():
    print("CSV_FILE =", CSV_FILE)
    print("Ruta absoluta =", os.path.abspath(CSV_FILE))
    print("Existe ya? ", os.path.exists(CSV_FILE))

    # Crear carpeta si no existe
    os.makedirs(os.path.dirname(CSV_FILE), exist_ok=True)

    # Crear archivo si no existe
    if not os.path.exists(CSV_FILE):
        with open(CSV_FILE, mode="w", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            writer.writerow(CSV_HEADERS)
        print("[INFO] CSV creado con cabecera")
    else:
        print("[INFO] CSV ya existe")


# -----------------------------
# GUARDAR FILA (CON VERIFICACION)
# -----------------------------
def guardar_fila(data):
    try:
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

        fila = [
            timestamp,
            data.get("temperatura"),
            data.get("humedad"),
            data.get("presion"),
            data.get("luz"),
        ]

        # Escritura real
        with open(CSV_FILE, mode="a", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            writer.writerow(fila)
            f.flush()
            os.fsync(f.fileno())

        # Verificación inmediata
        with open(CSV_FILE, mode="r", encoding="utf-8") as f:
            lineas = f.readlines()

        print(f"[OK] Dato guardado: {fila}")
        print(f"[DEBUG] Total lineas en CSV: {len(lineas)}")

        if lineas:
            print(f"[DEBUG] Ultima linea real: {lineas[-1].strip()}")

    except Exception as e:
        print("[ERROR] Fallo guardando datos:", e)


# -----------------------------
# CALLBACKS MQTT
# -----------------------------
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("[INFO] Conectado a MQTT correctamente")
        client.subscribe(MQTT_TOPIC)
        print(f"[INFO] Suscrito a: {MQTT_TOPIC}")
    else:
        print("[ERROR] Fallo conexion MQTT, codigo:", rc)


def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode("utf-8")
        print(f"[MQTT] Mensaje recibido: {payload}")

        data = json.loads(payload)

        campos = {"temperatura", "humedad", "presion", "luz"}

        if not campos.issubset(data.keys()):
            print("[WARN] Mensaje incompleto, no se guarda")
            return

        guardar_fila(data)

    except json.JSONDecodeError:
        print("[ERROR] JSON invalido")
    except Exception as e:
        print("[ERROR] Procesando mensaje:", e)


# -----------------------------
# MAIN
# -----------------------------
def main():
    inicializar_csv()

    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message

    print(f"[INFO] Conectando a {MQTT_BROKER}:{MQTT_PORT} ...")
    client.connect(MQTT_BROKER, MQTT_PORT, 60)

    print("[INFO] Esperando datos MQTT...")
    client.loop_forever()


if __name__ == "__main__":
    main()