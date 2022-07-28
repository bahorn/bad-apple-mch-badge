import cv2
import sys
import numpy as np
from bitarray import bitarray

# Create a VideoCapture object and read from input file
# If the input is the camera, pass 0 instead of the video file name
cap = cv2.VideoCapture(sys.argv[1], cv2.IMREAD_GRAYSCALE)

# Check if camera opened successfully
if not cap.isOpened():
    print("Error opening video stream or file")

out = open('out.bin', 'wb')

data = bitarray()

count = 0
# Read until video is completed
while(cap.isOpened()):
    # Capture frame-by-frame
    ret, frame = cap.read()
    count += 1
    if ret:
        data += list(cv2.threshold(frame, 128, 255,
                cv2.THRESH_BINARY)[1].flatten()[::3] > 128)

        
        # Press Q on keyboard to  exit
        #if cv2.waitKey(25) & 0xFF == ord('q'):
        #    break
    # Break the loop
    else:
        break

# When everything done, release the video capture object
cap.release()

# Closes all the frames
cv2.destroyAllWindows()

out.write(data.tobytes())
