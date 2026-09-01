"""
Core package for EcoFlutuador POC.
"""
from core.camera import Camera, CameraConfig, list_cameras
from core.detector import Detector, ModelConfig, Detection
from core.decision import DecisionEngine, DecisionConfig
from core.serial_link import SerialLink, SerialConfig, encode_command, encode_power, parse_response
from core.state import DetectionState

__all__ = [
    "Camera",
    "CameraConfig",
    "list_cameras",
    "Detector",
    "ModelConfig",
    "Detection",
    "DecisionEngine",
    "DecisionConfig",
    "SerialLink",
    "SerialConfig",
    "encode_command",
    "encode_power",
    "parse_response",
    "DetectionState",
]