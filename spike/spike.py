import cv2

img_square = cv2.imread('../resources/square.png')

img = cv2.imread('../resources/brussels.ppm')
(_,_,min_loc,_) = cv2.minMaxLoc(cv2.matchTemplate(img, img_square, cv2.TM_SQDIFF))
rv = cv2.rectangle(img, min_loc, (min_loc[0]+50, min_loc[1]+50),(0,255,0),3)
cv2.imshow('img', img)


img_noise = cv2.imread('../resources/brusselsNoise50.ppm')
(_,_,min_loc,_) = cv2.minMaxLoc(cv2.matchTemplate(img_noise, img_square, cv2.TM_SQDIFF))
rv = cv2.rectangle(img_noise, min_loc, (min_loc[0]+50, min_loc[1]+50),(0,255,0),3)
cv2.imshow('img_noise', img_noise)
cv2.waitKey(5)