import numpy as np
from scipy.optimize import least_squares

def calculate_distances(point, points_array):

    distances = np.sqrt(np.sum((points_array - point)**2, axis=1))
    return distances

def scaling_factor(value):
    # Define the range and scaling
    min_val, max_val = 1, 50
    max_inaccuracy, min_inaccuracy = 0.3, 0.05
    
    # Normalize value to range [0, 1]
    normalized = (value - min_val) / (max_val - min_val)
    
    # Calculate inaccuracy - linear interpolation between max_inaccuracy and min_inaccuracy
    inaccuracy = max_inaccuracy - (max_inaccuracy - min_inaccuracy) * normalized
    
    # Generate a random scaling factor within the inaccuracy range
    return np.random.uniform(1 - inaccuracy, 1 + inaccuracy)


point_searched = np.array([40, 50, 4])
# Known points
points = np.array([[0, 0, 0], [0, 10, 0], [0, 10, 3], [10, 0, 0], [10, 0, 3]])
# Distances to the unknown point (example values)
distances = np.array(calculate_distances(point_searched, points))  # Replace with actual distances
print("Distances:", distances)
scaling_factors = np.vectorize(scaling_factor)(distances)
print("Multiplicator:", scaling_factors)
distances = distances * scaling_factors
print("Distances:", distances)
# Weights for each measurement (example values, replace with actual weights if known)
weights = np.array([1, 1, 1, 1, 1])  # Equal weights for simplicity

# Function to calculate weighted distances from a point to the known points
def weighted_distances_to_points(x, points, distances, weights):
    return weights * (np.sqrt(np.sum((points - x)**2, axis=1)) - distances)

# Initial guess for the unknown point
x0 = np.array([0, 0, 0])

# Solve for the unknown point using weighted least squares
result = least_squares(weighted_distances_to_points, x0, args=(points, distances, weights))

# The estimated coordinates of the unknown point
print("Estimated coordinates:", result.x)