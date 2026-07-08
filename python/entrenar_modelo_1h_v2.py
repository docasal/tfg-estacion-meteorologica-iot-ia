import pandas as pd
import joblib
import math

from sklearn.ensemble import RandomForestRegressor
from sklearn.metrics import mean_absolute_error, mean_squared_error, r2_score


# -----------------------------
# CONFIGURACIÓN
# -----------------------------
CSV_ENTRADA = r"C:\Users\zurine\Documents\tfg\dataset_ia_1h_v2.csv"
MODELO_SALIDA = r"C:\Users\zurine\Documents\tfg\modelo_temperatura_1h_v2.pkl"


# -----------------------------
# CARGAR DATASET
# -----------------------------
df = pd.read_csv(CSV_ENTRADA)
df["timestamp"] = pd.to_datetime(df["timestamp"])

print("Registros cargados:", len(df))


# -----------------------------
# LIMPIEZA DE CAMBIOS ANÓMALOS
# -----------------------------
# Se eliminan cambios de temperatura demasiado bruscos para una hora,
# que pueden deberse a calentamiento directo del sensor, manipulación
# o condiciones no representativas del entorno habitual.

q1 = df["delta_temperatura_1h"].quantile(0.25)
q3 = df["delta_temperatura_1h"].quantile(0.75)
iqr = q3 - q1

limite_inferior = q1 - 1.5 * iqr
limite_superior = q3 + 1.5 * iqr

df = df[
    (df["delta_temperatura_1h"] >= limite_inferior) &
    (df["delta_temperatura_1h"] <= limite_superior)
]

print("Registros tras limpieza de cambios anómalos:", len(df))
print(f"Rango aceptado delta 1h: {limite_inferior:.2f} a {limite_superior:.2f} ºC")


# -----------------------------
# VARIABLES DE ENTRADA Y SALIDA
# -----------------------------
columnas_entrada = [
    "temperatura",
    "humedad",
    "presion",
    "luz",
    "hora",
    "minuto_dia",
    "dia_semana"
]

columna_objetivo = "delta_temperatura_1h"

X = df[columnas_entrada]
y = df[columna_objetivo]


# -----------------------------
# DIVISIÓN TEMPORAL TRAIN / TEST
# -----------------------------
# Al ser datos temporales, se entrena con el 80% inicial
# y se prueba con el 20% final.

split_index = int(len(df) * 0.8)

X_train = X.iloc[:split_index]
X_test = X.iloc[split_index:]

y_train = y.iloc[:split_index]
y_test = y.iloc[split_index:]

temperatura_actual_test = df["temperatura"].iloc[split_index:]
temperatura_real_1h_test = df["temperatura_1h"].iloc[split_index:]

print("Muestras entrenamiento:", len(X_train))
print("Muestras prueba:", len(X_test))


# -----------------------------
# ENTRENAMIENTO DEL MODELO
# -----------------------------
modelo = RandomForestRegressor(
    n_estimators=300,
    random_state=42,
    max_depth=8,
    min_samples_leaf=2
)

modelo.fit(X_train, y_train)


# -----------------------------
# PREDICCIÓN
# -----------------------------
delta_predicho = modelo.predict(X_test)

# Temperatura final predicha = temperatura actual + cambio predicho
temperatura_predicha_1h = temperatura_actual_test + delta_predicho


# -----------------------------
# EVALUACIÓN
# -----------------------------
mae = mean_absolute_error(temperatura_real_1h_test, temperatura_predicha_1h)
rmse = math.sqrt(mean_squared_error(temperatura_real_1h_test, temperatura_predicha_1h))
r2 = r2_score(temperatura_real_1h_test, temperatura_predicha_1h)

print("\nRESULTADOS DEL MODELO RANDOM FOREST")
print("-----------------------------------")
print(f"MAE  : {mae:.3f} ºC")
print(f"RMSE : {rmse:.3f} ºC")
print(f"R2   : {r2:.3f}")


# -----------------------------
# MODELO BASELINE
# -----------------------------
# Baseline: asumir que dentro de 1 hora la temperatura será igual a la actual.
baseline_pred = temperatura_actual_test

mae_baseline = mean_absolute_error(temperatura_real_1h_test, baseline_pred)
rmse_baseline = math.sqrt(mean_squared_error(temperatura_real_1h_test, baseline_pred))

print("\nCOMPARACIÓN CON MODELO SIMPLE")
print("-----------------------------------")
print(f"Baseline MAE  : {mae_baseline:.3f} ºC")
print(f"Baseline RMSE : {rmse_baseline:.3f} ºC")

if mae < mae_baseline:
    print("\nEl modelo mejora la predicción simple.")
else:
    print("\nEl modelo no mejora la predicción simple, pero sirve como primera aproximación.")


# -----------------------------
# IMPORTANCIA DE VARIABLES
# -----------------------------
importancias = pd.DataFrame({
    "variable": columnas_entrada,
    "importancia": modelo.feature_importances_
}).sort_values(by="importancia", ascending=False)

print("\nIMPORTANCIA DE VARIABLES")
print("-----------------------------------")
print(importancias)


# -----------------------------
# GUARDAR MODELO FINAL
# -----------------------------
# Una vez evaluado, se reentrena con todos los datos limpios
# para aprovechar el máximo dataset disponible.

modelo_final = RandomForestRegressor(
    n_estimators=300,
    random_state=42,
    max_depth=8,
    min_samples_leaf=2
)

modelo_final.fit(X, y)

paquete_modelo = {
    "modelo": modelo_final,
    "columnas_entrada": columnas_entrada,
    "horizonte": "1 hora",
    "objetivo": "delta_temperatura_1h"
}

joblib.dump(paquete_modelo, MODELO_SALIDA)

print("\nModelo final guardado en:")
print(MODELO_SALIDA)