from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from Database.Connection import create_tables
from Routers import cnc, devices, telemetry

app = FastAPI(
    title="SCOMPTEC - CNC Monitor",
    description="API de monitoramento de máquinas CNC industriais via gateways ESP32.",
    version="1.0.0",
)

# ATENÇÃO: CORS permissivo apenas para esta versão, evita dor de cabeça
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


@app.on_event("startup")
def on_startup():
    create_tables()


app.include_router(cnc.router)
app.include_router(devices.router)
app.include_router(telemetry.router)