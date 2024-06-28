import numpy as np
from scipy.optimize import least_squares

# Known points
points = np.array([[0, 0, 0], [0, 10, 0], [0, 10, 3], [10, 0, 0], [10, 0, 3]])

# Distances to the unknown point (example values)
distances = np.array([5, 7, 6, 8, 9])  # Replace with actual distances

# Function to calculate distances from a point to the known points
def distances_to_points(x, points, distances):
    return np.sqrt(np.sum((points - x)**2, axis=1)) - distances

# Initial guess for the unknown point
x0 = np.array([0, 0, 0])

# Solve for the unknown point
result = least_squares(distances_to_points, x0, args=(points, distances))

# The estimated coordinates of the unknown point
print("Estimated coordinates:", result.x)