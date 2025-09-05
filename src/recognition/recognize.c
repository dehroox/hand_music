// evil-er mathematics

#include "recognize.h"

#include <stdlib.h>
#include <string.h>

#include "types.h"

static inline double crossProduct(const Point po1, const Point po2,
				  const Point po3) {
    return ((po2.x - po1.x) * (po3.y - po1.y)) -
	   ((po2.y - po1.y) * (po3.x - po1.x));
}

static inline double distanceSquared(const Point po1, const Point po2) {
    const double distanceX = po2.x - po1.x;
    const double distanceY = po2.y - po1.y;
    return (distanceX * distanceX) + (distanceY * distanceY);
}

static int comparePoints(const void *poA, const void *poB) {
    const Point *pointA = (const Point *)poA;
    const Point *pointB = (const Point *)poB;
    if (pointA->x == pointB->x) {
	return pointA->y - pointB->y;
    }
    return pointA->x - pointB->x;
}

void thresholdImage(const unsigned char *const grayInput,
		    unsigned char *const binaryOutput,
		    const FrameDimensions dimensions,
		    const unsigned char threshold) {
    const unsigned int width = dimensions.width;
    const unsigned int height = dimensions.height;

    for (unsigned int row = 0; row < height; ++row) {
	for (unsigned int column = 0; column < width; ++column) {
	    const unsigned int index = (row * width) + column;
	    binaryOutput[index] = (grayInput[index] > threshold) ? 255 : 0;
	}
    }
}

int traceContour(const unsigned char *const binaryInput,
		 Point *const contourOutput, const FrameDimensions dimensions,
		 const int maxPoints) {
    Point startPoint = {-1, -1};

    for (int row = 0; row < (int)dimensions.height && startPoint.x == -1;
	 ++row) {
	for (int column = 0;
	     column < (int)dimensions.width && startPoint.x == -1; ++column) {
	    const int index = (row * (int)dimensions.width) + column;
	    if (binaryInput[index] == 255) {
		startPoint.x = column;
		startPoint.y = row;
		break;
	    }
	}
    }

    if (startPoint.x == -1) {
	return 0;
    }

    static const Point neighborOffsets[8] = {
	{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};

    int contourCount = 0;
    int neighborIndex = 0;
    Point currentPoint = startPoint;

    do {
	if (contourCount < maxPoints) {
	    contourOutput[contourCount++] = currentPoint;
	}
	int foundNext = 0;
	for (int i = 0; i < 8; ++i) {
	    int testIndex = (neighborIndex + i) % 8;
	    Point neighbor = {0};

	    neighbor.x = currentPoint.x + neighborOffsets[testIndex].x;
	    neighbor.y = currentPoint.y + neighborOffsets[testIndex].y;

	    if (neighbor.x >= 0 && neighbor.x < (int)dimensions.width &&
		neighbor.y >= 0 && neighbor.y < (int)dimensions.height &&
		binaryInput[(neighbor.y * (int)dimensions.width) +
			    neighbor.x] == 255) {
		currentPoint.x = neighbor.x;
		currentPoint.y = neighbor.y;
		foundNext = 1;
		break;
	    }
	}

	if (!foundNext) {
	    break;
	}
    } while (currentPoint.x != -1 && currentPoint.y != -1);

    return contourCount;
}

int convexHull(const Point *const contourInput, Point *const convexHullOutput,
	       const int pointCount) {
    memcpy(convexHullOutput, contourInput, (size_t)pointCount * sizeof(Point));
    qsort(convexHullOutput, (size_t)pointCount, sizeof(Point), &comparePoints);

    int hullIndex = 0;
    for (int i = 0; i < pointCount; i++) {
	while (hullIndex >= 2 && crossProduct(convexHullOutput[hullIndex - 2],
					      convexHullOutput[hullIndex - 1],
					      convexHullOutput[i]) <= 0) {
	    hullIndex--;
	}
	convexHullOutput[hullIndex++] = convexHullOutput[i];
    }

    int lower_size = hullIndex;
    for (int i = pointCount - 2; i >= 0; i--) {
	while (hullIndex > lower_size &&
	       crossProduct(convexHullOutput[hullIndex - 2],
			    convexHullOutput[hullIndex - 1],
			    convexHullOutput[i]) <= 0) {
	    hullIndex--;
	}
	convexHullOutput[hullIndex++] = convexHullOutput[i];
    }
    return hullIndex > 1 ? hullIndex - 1 : hullIndex;
}

int detectFingertips(const Point *inputConvexHull, int pointCount,
		     Point *fingertipOutput, int fingertipCount) {
    if (pointCount < 3) {
	return 0;
    }

    Point sumPoint = {.x = 0, .y = 0};
    for (int i = 0; i < pointCount; i++) {
	sumPoint.x += inputConvexHull[i].x;
	sumPoint.y += inputConvexHull[i].y;
    }
    Point centroidPoint = {.x = sumPoint.x / pointCount,
			   .y = sumPoint.y / pointCount};

    int fingertipIndex = 0;
    for (int i = 0; i < pointCount && fingertipIndex < fingertipCount; i++) {
	double distance = distanceSquared(centroidPoint, inputConvexHull[i]);
	if (distance > 2000) {
	    fingertipOutput[fingertipIndex++] = inputConvexHull[i];
	}
    }
    return fingertipIndex;
}
