import pandas as pd
from pathlib import Path

# -----------------------------
# CONFIGURACIÓN
# -----------------------------
CSV_ENTRADA = r"data/dataset_estacion.csv"
CSV_SALIDA = r"data/dataset_ia_1h_v2.csv"

# -----------------------------
# CARGA DEL DATASET
# -----------------------------
df = pd.read_csv(CSV_ENTRADA)
df["timestamp"] = pd.to_datetime(df["timestamp"])

print("Registros originales:", len(df))

# -----------------------------
# LIMPIEZA DE VALORES ANÓMALOS
# -----------------------------
df = df[
    (df["temperatura"].between(-20, 50)) &
    (df["humedad"].between(0, 100)) &
    (df["presion"].between(900, 1100)) &
    (df["luz"].between(0, 1023))
]

print("Registros tras limpieza:", len(df))

df = df.sort_values("timestamp")
df = df.drop_duplicates(subset=["timestamp"])

# -----------------------------
# CREAR OBJETIVO A 1 HORA
# -----------------------------
# Para cada lectura buscamos la lectura más cercana a 1 hora después.
df["timestamp_objetivo"] = df["timestamp"] + pd.Timedelta(hours=1)

objetivos = df[["timestamp", "temperatura"]].copy()
objetivos = objetivos.rename(columns={
    "timestamp": "timestamp_objetivo",
    "temperatura": "temperatura_1h"
})

df_modelo = pd.merge_asof(
    df.sort_values("timestamp_objetivo"),
    objetivos.sort_values("timestamp_objetivo"),
    on="timestamp_objetivo",
    direction="nearest",
    tolerance=pd.Timedelta(minutes=3)
)

df_modelo = df_modelo.dropna(subset=["temperatura_1h"])

# -----------------------------
# VARIABLE A PREDECIR
# -----------------------------
df_modelo["delta_temperatura_1h"] = (
    df_modelo["temperatura_1h"] - df_modelo["temperatura"]
)

# -----------------------------
# VARIABLES TEMPORALES
# -----------------------------
df_modelo["hora"] = df_modelo["timestamp"].dt.hour
df_modelo["minuto_dia"] = (
    df_modelo["timestamp"].dt.hour * 60 + df_modelo["timestamp"].dt.minute
)
df_modelo["dia_semana"] = df_modelo["timestamp"].dt.dayofweek

# -----------------------------
# COLUMNAS FINALES
# -----------------------------
columnas_finales = [
    "timestamp",
    "temperatura",
    "humedad",
    "presion",
    "luz",
    "hora",
    "minuto_dia",
    "dia_semana",
    "temperatura_1h",
    "delta_temperatura_1h"
]

df_modelo = df_modelo[columnas_finales]

# -----------------------------
# GUARDAR
# -----------------------------
Path(CSV_SALIDA).parent.mkdir(parents=True, exist_ok=True)
df_modelo.to_csv(CSV_SALIDA, index=False)

print("Dataset IA 1h guardado en:")
print(CSV_SALIDA)
print("Registros finales:", len(df_modelo))

print("\nPrimeras filas:")
print(df_modelo.head())

print("\nÚltimas filas:")
print(df_modelo.tail())