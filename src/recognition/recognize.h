#pragma once

#include "types.h"
typedef struct {
    int x;
    int y;
} __attribute__((aligned(8))) Point;

void thresholdImage(const unsigned char* grayInput, unsigned char* binaryOutput,
		    FrameDimensions dimensions, unsigned char threshold);

int traceContour(const unsigned char* binaryInput, Point* contourOutput,
		 FrameDimensions dimensions, int maxPoints);

int convexHull(const Point* contourInput, Point* convexHullOutput,
	       int pointCount);

int detectFingertips(const Point* inputConvexHull, int pointCount,
		     Point* fingertipOutput, int fingertipCount);
