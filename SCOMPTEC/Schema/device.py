from datetime import datetime
from typing import Optional
from pydantic import BaseModel, Field, ConfigDict


class DeviceCreate(BaseModel):
    name: str = Field(..., min_length=1, max_length=255)
    cnc_id: str = Field(..., description="ID (UUID) da CNC associada")
    firmware_version: Optional[str] = Field(default=None, max_length=50)


class DeviceUpdate(BaseModel):
    name: Optional[str] = Field(default=None, min_length=1, max_length=255)
    firmware_version: Optional[str] = Field(default=None, max_length=50)
    # Permitido: um gateway físico pode ser realocado para outra CNC
    # (troca de armário, substituição de máquina, reorganização de planta).
    cnc_id: Optional[str] = Field(default=None)


class DeviceResponse(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: str
    code: str
    name: str
    cnc_id: str
    firmware_version: Optional[str] = None
    last_seen: Optional[datetime] = None
    created_at: datetime


class DeviceStatusResponse(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: str
    code: str
    name: str
    cnc_id: str
    online: bool
    last_seen: Optional[datetime] = None