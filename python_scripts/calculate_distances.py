import numpy as np

def calculate_distances(point, points_array):
    """
    Calculate the Euclidean distances from a single point to an array of points in 3D space.

    Parameters:
    - point: A single point (x, y, z) as a tuple or list.
    - points_array: An array of points [(x1, y1, z1), (x2, y2, z2), ...] as a list of tuples or a 2D numpy array.

    Returns:
    - distances: A numpy array of distances from the given point to each point in points_array.
    """
    point = np.array(point)
    points_array = np.array(points_array)
    distances = np.sqrt(np.sum((points_array - point)**2, axis=1))
    return distances

# Example usage
point = (15.3, 21.5, 10.2)
points_array = [[0, 0, 0], [0, 10, 0], [0, 10, 3], [10, 0, 0], [10, 0, 3]]
distances = calculate_distances(point, points_array)
print("Distances:", distances)