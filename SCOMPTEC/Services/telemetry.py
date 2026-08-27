from fastapi import APIRouter, Depends
from sqlalchemy.orm import Session

from app.database.connection import get_db
from app.schemas.telemetry import TelemetryCreate, TelemetryResponse
from app.services import telemetry_service

router = APIRouter(prefix="/devices", tags=["Telemetry"])


@router.post("/{device_id}/telemetry", response_model=TelemetryResponse, status_code=201)
def create_telemetry(device_id: str, data: TelemetryCreate, db: Session = Depends(get_db)):
    return telemetry_service.create_telemetry(db, device_id, data)