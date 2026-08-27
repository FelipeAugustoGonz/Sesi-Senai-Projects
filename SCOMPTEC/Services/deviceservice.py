from typing import List

from fastapi import APIRouter, Depends
from sqlalchemy.orm import Session

from app.database.connection import get_db
from app.schemas.device import DeviceCreate, DeviceUpdate, DeviceResponse, DeviceStatusResponse
from app.services import device_service

router = APIRouter(prefix="/devices", tags=["Devices"])


@router.post("", response_model=DeviceResponse, status_code=201)
def create_device(data: DeviceCreate, db: Session = Depends(get_db)):
    return device_service.create_device(db, data)


@router.get("", response_model=List[DeviceResponse])
def list_devices(db: Session = Depends(get_db)):
    return device_service.list_devices(db)


@router.get("/{device_id}", response_model=DeviceResponse)
def get_device(device_id: str, db: Session = Depends(get_db)):
    return device_service.get_device_or_404(db, device_id)


@router.put("/{device_id}", response_model=DeviceResponse)
def update_device(device_id: str, data: DeviceUpdate, db: Session = Depends(get_db)):
    return device_service.update_device(db, device_id, data)


@router.delete("/{device_id}", status_code=204)
def delete_device(device_id: str, db: Session = Depends(get_db)):
    device_service.delete_device(db, device_id)


@router.get("/{device_id}/status", response_model=DeviceStatusResponse)
def get_device_status(device_id: str, db: Session = Depends(get_db)):
    return device_service.get_device_status(db, device_id)