"""
Core state dataclass for EcoFlutuador POC.
Represents the current detection/decision state, serializable to JSON.
"""
from dataclasses import dataclass, asdict
from typing import Optional, List
import time


@dataclass
class DetectionState:
    """Current state of the detection and decision pipeline."""
    timestamp: float
    detected: bool
    object: Optional[str] = None
    confidence: float = 0.0
    center_x: float = 0.0
    center_y: float = 0.0
    bbox: Optional[List[int]] = None
    decision: str = "s"
    inference_ms: float = 0.0
    fps: float = 0.0

    def to_json(self) -> dict:
        """Convert to JSON-serializable dict."""
        return asdict(self)

    @classmethod
    def empty(cls, timestamp: float = None, fps: float = 0.0) -> "DetectionState":
        """Create an empty state (no detection)."""
        return cls(
            timestamp=timestamp or time.time(),
            detected=False,
            decision="s",
            fps=fps
        )

    @classmethod
    def from_detection(cls, box: List[int], class_name: str, confidence: float,
                       decision: str, inference_ms: float, fps: float,
                       frame_width: int, frame_height: int) -> "DetectionState":
        """Create state from detection results."""
        x, y, w, h = box
        return cls(
            timestamp=time.time(),
            detected=True,
            object=class_name,
            confidence=round(confidence, 3),
            center_x=round((x + w / 2) / frame_width, 3),
            center_y=round((y + h / 2) / frame_height, 3),
            bbox=[x, y, w, h],
            decision=decision,
            inference_ms=round(inference_ms, 1),
            fps=round(fps, 1)
        )