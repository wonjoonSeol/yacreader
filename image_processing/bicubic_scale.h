#ifndef BICUBIC_SCALE_H
#define BICUBIC_SCALE_H

#include <QImage>

// Catmull-Rom (B = 0, C = 1/2) resampling. The image must be Format_ARGB32 or
// Format_RGB32, and width and height must be positive.
QImage scaleImageBicubic(const QImage &image, int width, int height);

#endif // BICUBIC_SCALE_H
