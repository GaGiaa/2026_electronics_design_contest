"""CanMV K230 steel-ball detector configuration."""


# The 01Studio board camera is normally connected to CSI2.
CAMERA_ID = 2
CAMERA_FPS = 30
FRAME_WIDTH = 320
FRAME_HEIGHT = 240
CV_LITE_FRAME_WIDTH = 320
CV_LITE_FRAME_HEIGHT = 240
IMAGE_FRAME_WIDTH = 320
IMAGE_FRAME_HEIGHT = 240
CV_LITE_DISPLAY_WIDTH = 640
CV_LITE_DISPLAY_HEIGHT = 480
CV_LITE_DISPLAY_QUALITY = 80

# "auto" uses cv_lite when the firmware provides it, otherwise image.find_circles.
# main.py automatically selects RGB565 or RGB888 for the selected backend.
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

# A target is accepted after 3 matching observations in the latest 5 frames.
CONFIRM_WINDOW = 5
CONFIRM_HITS = 3
CENTER_TOLERANCE = 20
RADIUS_TOLERANCE = 8
LOST_AFTER = 3

# Parameters used only when DETECTOR_BACKEND is "cv_lite".
CV_LITE_DP = 1
CV_LITE_MIN_DISTANCE = 30
CV_LITE_PARAM1 = 80
CV_LITE_PARAM2 = 20

DISPLAY_TEXT_SIZE = 24
