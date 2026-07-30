"""Steel-ball candidate detection and multi-frame confirmation logic."""


def _as_int(value):
    return int(round(value))


def circle_to_candidate(circle):
    """Convert a CanMV circle object to a small, host-testable dictionary."""
    magnitude = getattr(circle, "magnitude", 0)
    score = magnitude() if callable(magnitude) else magnitude
    return {
        "x": _as_int(circle.x()),
        "y": _as_int(circle.y()),
        "radius": _as_int(circle.r()),
        "score": _as_int(score),
    }


def select_best_candidate(candidates, min_radius, max_radius):
    """Filter radius and return the highest-scoring candidate, if any."""
    valid = []
    for candidate in candidates:
        radius = candidate["radius"]
        if min_radius <= radius <= max_radius:
            valid.append(candidate)

    if not valid:
        return None

    return max(valid, key=lambda item: item["score"])


def _compatible(first, second, center_tolerance, radius_tolerance):
    return (
        abs(first["x"] - second["x"]) <= center_tolerance
        and abs(first["y"] - second["y"]) <= center_tolerance
        and abs(first["radius"] - second["radius"]) <= radius_tolerance
    )


class ConfirmationTracker:
    """Confirm one stable candidate across a bounded frame history."""

    def __init__(
        self,
        window=5,
        hits=3,
        center_tolerance=20,
        radius_tolerance=8,
        lost_after=3,
    ):
        if window <= 0 or hits <= 0 or hits > window or lost_after <= 0:
            raise ValueError("invalid confirmation parameters")

        self.window = window
        self.hits = hits
        self.center_tolerance = center_tolerance
        self.radius_tolerance = radius_tolerance
        self.lost_after = lost_after
        self._history = []
        self._confirmed = None
        self._misses = 0

    def _append_history(self, candidate):
        self._history.append(candidate)
        if len(self._history) > self.window:
            self._history.pop(0)

    def _matching_hits(self, candidate):
        return sum(
            1
            for item in self._history
            if item is not None
            and _compatible(
                item,
                candidate,
                self.center_tolerance,
                self.radius_tolerance,
            )
        )

    def update(self, candidate):
        if self._confirmed is not None:
            if candidate is not None and _compatible(
                self._confirmed,
                candidate,
                self.center_tolerance,
                self.radius_tolerance,
            ):
                self._confirmed = candidate
                self._misses = 0
                self._append_history(candidate)
                return {"status": "FOUND", "candidate": candidate}

            self._misses += 1
            self._append_history(None)
            if self._misses < self.lost_after:
                return {"status": "FOUND", "candidate": self._confirmed}

            self._confirmed = None
            self._misses = 0
            self._history = []
            return {"status": "LOST", "candidate": None}

        self._append_history(candidate)
        if candidate is None:
            return {"status": "SEARCHING", "candidate": None}

        if self._matching_hits(candidate) >= self.hits:
            self._confirmed = candidate
            self._misses = 0
            return {"status": "FOUND", "candidate": candidate}

        return {"status": "SEARCHING", "candidate": None}


def find_image_candidates(image, config):
    """Run the built-in CanMV circle detector for one image."""
    kwargs = {
        "x_stride": config.X_STRIDE,
        "y_stride": config.Y_STRIDE,
        "threshold": config.HOUGH_THRESHOLD,
        "x_margin": config.X_MARGIN,
        "y_margin": config.Y_MARGIN,
        "r_margin": config.R_MARGIN,
        "r_min": config.MIN_RADIUS,
        "r_max": config.MAX_RADIUS,
        "r_step": config.R_STEP,
    }
    if config.ROI is not None:
        kwargs["roi"] = config.ROI

    return [circle_to_candidate(circle) for circle in image.find_circles(**kwargs)]


def find_cv_lite_candidates(image, config):
    """Run the optional RGB888 cv_lite detector for one image."""
    import cv_lite

    image_shape = [config.FRAME_HEIGHT, config.FRAME_WIDTH]
    circles = cv_lite.rgb888_find_circles(
        image_shape,
        image.to_numpy_ref(),
        config.CV_LITE_DP,
        config.CV_LITE_MIN_DISTANCE,
        config.CV_LITE_PARAM1,
        config.CV_LITE_PARAM2,
        config.MIN_RADIUS,
        config.MAX_RADIUS,
    )
    candidates = []
    for index in range(0, len(circles), 3):
        candidates.append(
            {
                "x": _as_int(circles[index]),
                "y": _as_int(circles[index + 1]),
                "radius": _as_int(circles[index + 2]),
                "score": 0,
            }
        )
    return candidates


def find_candidates(image, config):
    if config.DETECTOR_BACKEND == "image":
        return find_image_candidates(image, config)
    if config.DETECTOR_BACKEND == "cv_lite":
        return find_cv_lite_candidates(image, config)
    raise ValueError("unsupported detector backend: " + str(config.DETECTOR_BACKEND))
