"""CanMV IDE entry point for single steel-ball detection."""

import time

from media.sensor import Sensor
from media.display import Display
from media.media import MediaManager


class _DefaultConfig:
    """Fallback values used when CanMV IDE only uploads main.py."""

    CAMERA_ID = 2
    CAMERA_FPS = 30
    FRAME_WIDTH = 640
    FRAME_HEIGHT = 360
    CV_LITE_FRAME_WIDTH = 320
    CV_LITE_FRAME_HEIGHT = 240
    IMAGE_FRAME_WIDTH = 320
    IMAGE_FRAME_HEIGHT = 240
    CV_LITE_DISPLAY_WIDTH = 640
    CV_LITE_DISPLAY_HEIGHT = 480
    CV_LITE_DISPLAY_QUALITY = 80
    DETECTOR_BACKEND = "auto"
    MIN_RADIUS = 4
    MAX_RADIUS = 120
    HOUGH_THRESHOLD = 2000
    X_STRIDE = 2
    Y_STRIDE = 1
    X_MARGIN = 10
    Y_MARGIN = 10
    R_MARGIN = 10
    R_STEP = 2
    ROI = None
    CONFIRM_WINDOW = 5
    CONFIRM_HITS = 3
    CENTER_TOLERANCE = 20
    RADIUS_TOLERANCE = 8
    LOST_AFTER = 3
    CV_LITE_DP = 1
    CV_LITE_MIN_DISTANCE = 30
    CV_LITE_PARAM1 = 80
    CV_LITE_PARAM2 = 20
    DISPLAY_TEXT_SIZE = 24


try:
    import config
except ImportError:
    config = _DefaultConfig()


try:
    from ball_detector import ConfirmationTracker, find_candidates, select_best_candidate
except ImportError:
    def _as_int(value):
        return int(round(value))


    def _circle_to_candidate(circle):
        magnitude = getattr(circle, "magnitude", 0)
        score = magnitude() if callable(magnitude) else magnitude
        return {
            "x": _as_int(circle.x()),
            "y": _as_int(circle.y()),
            "radius": _as_int(circle.r()),
            "score": _as_int(score),
        }


    def select_best_candidate(candidates, min_radius, max_radius):
        valid = [
            candidate
            for candidate in candidates
            if min_radius <= candidate["radius"] <= max_radius
        ]
        return max(valid, key=lambda item: item["score"]) if valid else None


    def _compatible(first, second, center_tolerance, radius_tolerance):
        return (
            abs(first["x"] - second["x"]) <= center_tolerance
            and abs(first["y"] - second["y"]) <= center_tolerance
            and abs(first["radius"] - second["radius"]) <= radius_tolerance
        )


    class ConfirmationTracker:
        def __init__(
            self,
            window=5,
            hits=3,
            center_tolerance=20,
            radius_tolerance=8,
            lost_after=3,
        ):
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

            matches = sum(
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
            if matches >= self.hits:
                self._confirmed = candidate
                return {"status": "FOUND", "candidate": candidate}

            return {"status": "SEARCHING", "candidate": None}


    def _find_image_candidates(image):
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
        return [_circle_to_candidate(circle) for circle in image.find_circles(**kwargs)]


    def find_candidates(image, detector_config):
        if detector_config.DETECTOR_BACKEND == "image":
            return _find_image_candidates(image)
        if detector_config.DETECTOR_BACKEND == "cv_lite":
            import cv_lite

            circles = cv_lite.rgb888_find_circles(
                [detector_config.FRAME_HEIGHT, detector_config.FRAME_WIDTH],
                image.to_numpy_ref(),
                detector_config.CV_LITE_DP,
                detector_config.CV_LITE_MIN_DISTANCE,
                detector_config.CV_LITE_PARAM1,
                detector_config.CV_LITE_PARAM2,
                detector_config.MIN_RADIUS,
                detector_config.MAX_RADIUS,
            )
            return [
                {
                    "x": _as_int(circles[index]),
                    "y": _as_int(circles[index + 1]),
                    "radius": _as_int(circles[index + 2]),
                    "score": 0,
                }
                for index in range(0, len(circles), 3)
            ]
        raise ValueError("unsupported detector backend: " + str(detector_config.DETECTOR_BACKEND))


def _resolve_backend():
    backend = config.DETECTOR_BACKEND
    if backend == "auto":
        try:
            import cv_lite
            backend = "cv_lite"
        except ImportError:
            backend = "image"

    config.DETECTOR_BACKEND = backend
    if backend == "cv_lite":
        config.FRAME_WIDTH = config.CV_LITE_FRAME_WIDTH
        config.FRAME_HEIGHT = config.CV_LITE_FRAME_HEIGHT
    elif backend == "image":
        config.FRAME_WIDTH = config.IMAGE_FRAME_WIDTH
        config.FRAME_HEIGHT = config.IMAGE_FRAME_HEIGHT
    else:
        raise ValueError("unsupported detector backend: " + str(backend))

    return backend


def _configure_sensor():
    sensor = Sensor(id=config.CAMERA_ID, fps=config.CAMERA_FPS)
    sensor.reset()
    sensor.set_framesize(width=config.FRAME_WIDTH, height=config.FRAME_HEIGHT)
    if config.DETECTOR_BACKEND == "cv_lite":
        sensor.set_pixformat(Sensor.RGB888)
    else:
        sensor.set_pixformat(Sensor.RGB565)
    return sensor


def _format_status(result, fps, backend, candidate_count):
    candidate = result["candidate"]
    if candidate is None:
        return "{} {}x{} candidates={} fps={:.1f}".format(
            result["status"],
            backend,
            config.FRAME_WIDTH,
            candidate_count,
            fps,
        )
    return "{} {}x{} candidates={} x={} y={} r={} s={} fps={:.1f}".format(
        result["status"],
        backend,
        config.FRAME_WIDTH,
        candidate_count,
        candidate["x"],
        candidate["y"],
        candidate["radius"],
        candidate["score"],
        fps,
    )


def _draw_result(image, result, fps, backend, candidate_count):
    candidate = result["candidate"]
    if result["status"] == "FOUND" and candidate is not None:
        image.draw_circle(
            candidate["x"],
            candidate["y"],
            candidate["radius"],
            color=(255, 0, 0),
            thickness=3,
        )

    image.draw_string_advanced(
        8,
        8,
        config.DISPLAY_TEXT_SIZE,
        _format_status(result, fps, backend, candidate_count),
        color=(255, 255, 255),
    )


def _init_display(backend):
    if backend == "cv_lite":
        try:
            Display.init(
                Display.ST7701,
                width=config.CV_LITE_DISPLAY_WIDTH,
                height=config.CV_LITE_DISPLAY_HEIGHT,
                to_ide=True,
                quality=config.CV_LITE_DISPLAY_QUALITY,
            )
            return "st7701"
        except Exception as error:
            try:
                Display.deinit()
            except Exception:
                pass
            print("display=virt fallback reason={}".format(error))

    Display.init(
        Display.VIRT,
        config.FRAME_WIDTH,
        config.FRAME_HEIGHT,
        to_ide=True,
    )
    return "virt"


def _show_image(image, display_mode):
    if display_mode == "st7701":
        Display.show_image(
            image,
            x=round((config.CV_LITE_DISPLAY_WIDTH - config.FRAME_WIDTH) / 2),
            y=round((config.CV_LITE_DISPLAY_HEIGHT - config.FRAME_HEIGHT) / 2),
        )
    else:
        Display.show_image(image)


def main():
    sensor = None
    display_started = False
    media_started = False

    tracker = ConfirmationTracker(
        window=config.CONFIRM_WINDOW,
        hits=config.CONFIRM_HITS,
        center_tolerance=config.CENTER_TOLERANCE,
        radius_tolerance=config.RADIUS_TOLERANCE,
        lost_after=config.LOST_AFTER,
    )

    try:
        backend = _resolve_backend()
        print(
            "detector={} frame={}x{}".format(
                backend,
                config.FRAME_WIDTH,
                config.FRAME_HEIGHT,
            )
        )
        sensor = _configure_sensor()
        display_mode = _init_display(backend)
        display_started = True
        MediaManager.init()
        media_started = True
        sensor.run()

        clock = time.clock()
        while True:
            clock.tick()
            image = sensor.snapshot()
            candidates = find_candidates(image, config)
            candidate = select_best_candidate(
                candidates,
                config.MIN_RADIUS,
                config.MAX_RADIUS,
            )
            result = tracker.update(candidate)
            fps = clock.fps()
            _draw_result(image, result, fps, backend, len(candidates))
            _show_image(image, display_mode)
            print(_format_status(result, fps, backend, len(candidates)))
    finally:
        if sensor is not None:
            sensor.stop()
        if display_started:
            Display.deinit()
        if media_started:
            MediaManager.deinit()


main()
