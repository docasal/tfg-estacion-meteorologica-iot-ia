# Sistema de monitorización ambiental IoT con predicción mediante inteligencia artificial

Este repositorio contiene el código fuente y los archivos necesarios para reproducir el sistema desarrollado en el Trabajo Fin de Grado: un prototipo de monitorización ambiental basado en Arduino GIGA R1 WiFi, comunicación MQTT, almacenamiento de datos en CSV y predicción preliminar de temperatura a una hora mediante un modelo Random Forest.

## Estructura del repositorio

- `arduino/`: código cargado en la placa Arduino GIGA R1 WiFi.
- `python/`: scripts de almacenamiento, preparación del dataset, entrenamiento del modelo y publicación de predicciones.
- `config/`: archivo de configuración del broker MQTT Mosquitto.
- `data/`: datasets de ejemplo.
- `models/`: modelo entrenado o generado durante el entrenamiento.
- `docs/`: diagramas y capturas complementarias.

## Hardware utilizado

- Arduino GIGA R1 WiFi.
- GIGA Display Shield.
- Sensor BME280 para temperatura, humedad y presión.
- Sensor LDR para luminosidad.
- Ordenador con broker MQTT Mosquitto.

## Dependencias de Python

Instalar dependencias:

```bash
pip install -r requirements.txt
```
## Configuración MQTT

El sistema utiliza los siguientes topics:
```bash
Topic	-- Descripción
estacionmeteorologica	-- Datos actuales enviados por Arduino
estacionmeteorologica/prediccion -- Predicción generada por Python
```
## Ejecutar Mosquitto:
```bash
mosquitto -c config/mosquitto.conf -v
```
## Ejecución del sistema
```bash
1. Cargar el código Arduino desde arduino/estacion_iot/estacion_iot.ino.
2. Iniciar el broker Mosquitto.
3. Ejecutar el script de almacenamiento:

python python/guardar_dataset_mqtt.py

4. Preparar el dataset para IA:
python python/preparar_dataset_ia_1h_v2.py

5. Entrenar el modelo:
python python/entrenar_modelo_1h_v2.py

6. Publicar predicciones mediante MQTT:
python python/predecir_y_publicar_mqtt.py
```

## Modelo predictivo

El modelo implementado utiliza Random Forest para estimar la temperatura prevista a una hora a partir de las variables recogidas por el sistema: temperatura, humedad, presión, luminosidad y variables temporales derivadas del timestamp.

## Dataset

Se incluye un dataset de ejemplo para permitir la reproducción parcial del flujo de preparación, entrenamiento y predicción.

## Nota sobre configuración

Antes de ejecutar el sistema, deben ajustarse en los scripts y en el código Arduino las rutas locales, la red WiFi y la dirección IP del broker MQTT.

## Licencia

Este proyecto se distribuye bajo licencia MIT con fines académicos.
