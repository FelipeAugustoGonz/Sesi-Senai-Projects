from typing import List, Optional
from datetime import datetime

from fastapi import APIRouter, Depends, Query
from sqlalchemy.orm import Session

from app.database.connection import get_db
from app.schemas.cnc import CNCCreate, CNCUpdate, CNCResponse, CNCStatusResponse
from app.schemas.telemetry import TelemetryHistoryResponse
from app.services import cnc_service, telemetry_service

router = APIRouter(prefix="/cncs", tags=["CNC"])


@router.post("", response_model=CNCResponse, status_code=201)
def create_cnc(data: CNCCreate, db: Session = Depends(get_db)):
    return cnc_service.create_cnc(db, data)


@router.get("", response_model=List[CNCResponse])
def list_cncs(db: Session = Depends(get_db)):
    return cnc_service.list_cncs(db)


@router.get("/{cnc_id}", response_model=CNCResponse)
def get_cnc(cnc_id: str, db: Session = Depends(get_db)):
    return cnc_service.get_cnc_or_404(db, cnc_id)


@router.put("/{cnc_id}", response_model=CNCResponse)
def update_cnc(cnc_id: str, data: CNCUpdate, db: Session = Depends(get_db)):
    return cnc_service.update_cnc(db, cnc_id, data)


@router.delete("/{cnc_id}", status_code=204)
def delete_cnc(cnc_id: str, db: Session = Depends(get_db)):
    cnc_service.delete_cnc(db, cnc_id)


@router.get("/{cnc_id}/status", response_model=CNCStatusResponse)
def get_cnc_status(cnc_id: str, db: Session = Depends(get_db)):
    return cnc_service.get_cnc_status(db, cnc_id)


@router.get("/{cnc_id}/history", response_model=TelemetryHistoryResponse)
def get_cnc_history(
    cnc_id: str,
    start: Optional[datetime] = Query(default=None),
    end: Optional[datetime] = Query(default=None),
    page: int = Query(default=1, ge=1),
    limit: int = Query(default=50, ge=1, le=500),
    db: Session = Depends(get_db),
):
    cnc_service.get_cnc_or_404(db, cnc_id)
    items, total = telemetry_service.get_history(db, cnc_id, start, end, page, limit)
    return TelemetryHistoryResponse(items=items, total=total, page=page, limit=limit)