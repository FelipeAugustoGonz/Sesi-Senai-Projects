"""
Decision engine for EcoFlutuador POC.
Implements zone-based navigation logic (left/center/right).
"""
import logging
from typing import List, Optional
from dataclasses import dataclass
from core.detector import Detection


@dataclass
class DecisionConfig:
    frame_width: int = 320
    zones: int = 3
    default_command: str = "s"  # STOP when no detection
    send_only_on_change: bool = True


class DecisionEngine:
    """
    Zone-based navigation decision logic.
    Divides frame horizontally into zones:
    - Left zone: command 'a' (turn left)
    - Center zone: command 'w' (forward)
    - Right zone: command 'd' (turn right)
    - No detection: command 's' (stop)
    """

    def __init__(self, config: DecisionConfig):
        self.config = config
        self._logger = logging.getLogger(__name__)
        self._last_command: Optional[str] = None
        self._zone_width = config.frame_width // config.zones

    def decide(self, detections: List[Detection]) -> str:
        """
        Decide navigation command based on detections.
        Returns command character: 'w', 'a', 'd', 's', 'q', 'e'
        """
        if not detections:
            cmd = self.config.default_command
        else:
            # Pick detection with largest area (closest object)
            best = max(detections, key=lambda d: d.bbox[2] * d.bbox[3])
            x, y, w, h = best.bbox
            cx = x + w // 2  # center X

            # Determine zone
            if cx < self._zone_width:
                cmd = 'a'      # left
            elif cx > 2 * self._zone_width:
                cmd = 'd'      # right
            else:
                cmd = 'w'      # center

        # Only return command if changed (to avoid oscillation)
        if self.config.send_only_on_change and cmd == self._last_command:
            return None  # No change

        self._last_command = cmd
        return cmd

    def reset(self) -> None:
        """Reset last command (e.g., when switching modes)."""
        self._last_command = None

    def get_zones(self) -> dict:
        """Return zone boundaries for debugging."""
        return {
            "left": (0, self._zone_width),
            "center": (self._zone_width, 2 * self._zone_width),
            "right": (2 * self._zone_width, self.config.frame_width),
            "zone_width": self._zone_width
        }