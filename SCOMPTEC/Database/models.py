from datetime import datetime, timezone
from uuid import uuid4

from sqlalchemy import (
    Boolean,
    DateTime,
    ForeignKey,
    JSON,
    String,
    Text,
)
from sqlalchemy.orm import (
    DeclarativeBase,
    Mapped,
    mapped_column,
    relationship,
)


class Base(DeclarativeBase):
    pass


# ============================================================
# CNC
# ============================================================

class CNC(Base):
    __tablename__ = "cncs"

    id: Mapped[str] = mapped_column(
        String(36),
        primary_key=True,
        default=lambda: str(uuid4()),
    )

    code: Mapped[str] = mapped_column(
        String(20),
        unique=True,
        nullable=False,
        index=True,
    )

    name: Mapped[str] = mapped_column(
        String(100),
        nullable=False,
    )

    description: Mapped[str | None] = mapped_column(
        Text,
        nullable=True,
    )

    status: Mapped[str] = mapped_column(
        String(20),
        nullable=False,
        default="UNKNOWN",
    )

    status_since: Mapped[datetime | None] = mapped_column(
        DateTime,
        nullable=True,
    )

    last_seen: Mapped[datetime | None] = mapped_column(
        DateTime,
        nullable=True,
    )

    created_at: Mapped[datetime] = mapped_column(
        DateTime,
        nullable=False,
        default=lambda: datetime.now(timezone.utc),
    )

    devices: Mapped[list["Device"]] = relationship(
        "Device",
        back_populates="cnc",
        cascade="all, delete-orphan",
    )


# ============================================================
# DEVICE / GATEWAY
# ============================================================

class Device(Base):
    __tablename__ = "devices"

    id: Mapped[str] = mapped_column(
        String(36),
        primary_key=True,
        default=lambda: str(uuid4()),
    )

    code: Mapped[str] = mapped_column(
        String(20),
        unique=True,
        nullable=False,
        index=True,
    )

    name: Mapped[str] = mapped_column(
        String(100),
        nullable=False,
    )

    cnc_id: Mapped[str] = mapped_column(
        String(36),
        ForeignKey(
            "cncs.id",
            ondelete="CASCADE",
        ),
        nullable=False,
        index=True,
    )

    firmware_version: Mapped[str | None] = mapped_column(
        String(50),
        nullable=True,
    )

    last_seen: Mapped[datetime | None] = mapped_column(
        DateTime,
        nullable=True,
    )

    created_at: Mapped[datetime] = mapped_column(
        DateTime,
        nullable=False,
        default=lambda: datetime.now(timezone.utc),
    )

    cnc: Mapped["CNC"] = relationship(
        "CNC",
        back_populates="devices",
    )

    telemetry: Mapped[list["Telemetry"]] = relationship(
        "Telemetry",
        back_populates="device",
        cascade="all, delete-orphan",
    )


# ============================================================
# TELEMETRY
# ============================================================

class Telemetry(Base):
    __tablename__ = "telemetry"

    id: Mapped[str] = mapped_column(
        String(36),
        primary_key=True,
        default=lambda: str(uuid4()),
    )

    device_id: Mapped[str] = mapped_column(
        String(36),
        ForeignKey(
            "devices.id",
            ondelete="CASCADE",
        ),
        nullable=False,
        index=True,
    )

    timestamp: Mapped[datetime] = mapped_column(
        DateTime,
        nullable=False,
    )

    received_at: Mapped[datetime] = mapped_column(
        DateTime,
        nullable=False,
        default=lambda: datetime.now(timezone.utc),
    )

    machine_active: Mapped[bool] = mapped_column(
        Boolean,
        nullable=False,
    )

    voltage_24v: Mapped[bool | None] = mapped_column(
        Boolean,
        nullable=True,
    )

    digital_signals: Mapped[dict | None] = mapped_column(
        JSON,
        nullable=True,
    )

    analog_signals: Mapped[dict | None] = mapped_column(
        JSON,
        nullable=True,
    )

    extra_signals: Mapped[dict | None] = mapped_column(
        JSON,
        nullable=True,
    )

    device: Mapped["Device"] = relationship(
        "Device",
        back_populates="telemetry",
    )

    # ============================================================
    # Login
    # ============================================================

    # ============================================================
    # Registro
    # ============================================================