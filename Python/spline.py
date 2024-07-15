from tinyspline import *
import matplotlib.pyplot as plt
import numpy as np

spline = BSpline.interpolate_cubic_natural(
  [
     0, 00, # P1
    0.25,  0.1, # P2
     0.5,  0.5, # P3
     0.75,  0.9, # P4
     1,  1  # P5
  ], 2) # <- dimensionality of the points

# Draw spline as polyline.
points = spline.sample(999)
x = points[0::2]
y = points[1::2]
plt.plot(x, y)
plt.show()
# https://github.com/msteinbeck/tinyspline?tab=readme-ov-file#getting-started
# https://pythonguides.com/python-write-array-to-csv/ 
array = np.array(y)

array.tofile('y.csv', sep = ',')
