import json
import time
from datetime import timedelta

import joblib
import pandas as pd
import paho.mqtt.client as mqtt


# -----------------------------
# CONFIGURACIÓN
# -----------------------------
CSV_DATOS = r"data/dataset_estacion.csv"
MODELO = r"modelo/modelo_temperatura_1h_v2.pkl"

MQTT_BROKER = "********"   # IP del PC donde está Mosquitto
MQTT_PORT = 1883
MQTT_TOPIC = "estacionmeteorologica/prediccion"

INTERVALO_SEGUNDOS = 300  # 5 minutos


# -----------------------------
# FUNCIÓN PARA CALCULAR ESTADO
# -----------------------------
def calcular_estado(temperatura, humedad, presion, luz):
    """
    Calcula un estado ambiental aproximado usando la misma lógica
    general que el sistema Arduino.
    """

    if (humedad >= 75.0 and luz < 100) or (humedad >= 78.0 and presion < 1010.0):
        return "LLUVIA"

    elif luz > 700:
        return "NOCHE"

    elif luz <= 5:
        return "SOLEADO"

    else:
        return "NUBLADO"


# -----------------------------
# CARGAR MODELO
# -----------------------------
def cargar_modelo():
    paquete = joblib.load(MODELO)
    modelo = paquete["modelo"]
    columnas_entrada = paquete["columnas_entrada"]

    print("[INFO] Modelo cargado correctamente")
    print("[INFO] Columnas esperadas:", columnas_entrada)

    return modelo, columnas_entrada


# -----------------------------
# LEER ÚLTIMO DATO DEL CSV
# -----------------------------
def leer_ultimo_dato():
    df = pd.read_csv(CSV_DATOS)

    df["timestamp"] = pd.to_datetime(df["timestamp"])
    df = df.sort_values("timestamp")

    ultimo = df.iloc[-1].copy()

    return ultimo


# -----------------------------
# GENERAR PREDICCIÓN
# -----------------------------
def generar_prediccion(modelo, columnas_entrada):
    ultimo = leer_ultimo_dato()

    timestamp = ultimo["timestamp"]

    temperatura = float(ultimo["temperatura"])
    humedad = float(ultimo["humedad"])
    presion = float(ultimo["presion"])
    luz = int(ultimo["luz"])

    hora = timestamp.hour
    minuto_dia = timestamp.hour * 60 + timestamp.minute
    dia_semana = timestamp.dayofweek

    entrada = pd.DataFrame([{
        "temperatura": temperatura,
        "humedad": humedad,
        "presion": presion,
        "luz": luz,
        "hora": hora,
        "minuto_dia": minuto_dia,
        "dia_semana": dia_semana
    }])

    entrada = entrada[columnas_entrada]

    delta_predicho = float(modelo.predict(entrada)[0])
    temperatura_prevista = temperatura + delta_predicho

    # En esta versión solo se predice la temperatura.
    # Humedad y presión se mantienen con el último valor medido.
    humedad_prevista = humedad
    presion_prevista = presion

    estado_previsto = calcular_estado(
        temperatura_prevista,
        humedad_prevista,
        presion_prevista,
        luz
    )

    mensaje = {
        "horizonte": "1h",
        "temperatura_prevista": round(temperatura_prevista, 2),
        "humedad_prevista": round(humedad_prevista, 2),
        "presion_prevista": round(presion_prevista, 2),
        "estado_previsto": estado_previsto
    }

    return mensaje


# -----------------------------
# PUBLICAR POR MQTT
# -----------------------------
def publicar_mqtt(client, mensaje):
    payload = json.dumps(mensaje, ensure_ascii=False)

    result = client.publish(
        MQTT_TOPIC,
        payload,
        qos=0,
        retain=True
    )

    result.wait_for_publish()

    if result.rc == mqtt.MQTT_ERR_SUCCESS:
        print("[OK] Predicción publicada por MQTT:")
        print(payload)
    else:
        print(f"[ERROR] No se pudo publicar el mensaje MQTT. Código: {result.rc}")


# -----------------------------
# MAIN
# -----------------------------
def main():
    modelo, columnas_entrada = cargar_modelo()

    client = mqtt.Client()
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    client.loop_start()

    print(f"[INFO] Conectado a MQTT en {MQTT_BROKER}:{MQTT_PORT}")
    print(f"[INFO] Publicando en topic: {MQTT_TOPIC}")
    print("[INFO] Sistema de predicción en ejecución...")

    while True:
        try:
            mensaje = generar_prediccion(modelo, columnas_entrada)
            publicar_mqtt(client, mensaje)

        except Exception as e:
            print("[ERROR] Fallo generando/publicando predicción:", e)

        time.sleep(INTERVALO_SEGUNDOS)


if __name__ == "__main__":
    main()