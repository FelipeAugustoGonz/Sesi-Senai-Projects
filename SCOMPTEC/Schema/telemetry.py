from datetime import datetime
from typing import Optional, Dict, Any, List
from pydantic import BaseModel, Field, ConfigDict


class TelemetryCreate(BaseModel):
    machine_active: bool
    voltage_24v: bool
    digital_signals: Optional[Dict[str, Any]] = Field(default_factory=dict)
    analog_signals: Optional[Dict[str, Any]] = Field(default_factory=dict)
    extra_signals: Optional[Dict[str, Any]] = Field(default_factory=dict)
    timestamp: Optional[datetime] = Field(
        default=None,
        description="Timestamp de origem do ESP32. Se omitido, usa-se o horário do servidor.",
    )


class TelemetryResponse(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: str
    device_id: str
    timestamp: datetime
    received_at: datetime
    machine_active: bool
    voltage_24v: bool
    digital_signals: Optional[Dict[str, Any]] = None
    analog_signals: Optional[Dict[str, Any]] = None
    extra_signals: Optional[Dict[str, Any]] = None


class TelemetryHistoryResponse(BaseModel):
    items: List[TelemetryResponse]
    total: int
    page: int
    limit: int