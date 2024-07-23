#include "Arduino.h"

#include "esp_now.h"
#include "WiFi.h"
#include "messaging.h"


void messaging::calculateDistances(int id) {
    addError("Calculate distances with id "+String(id)+" called \n");
    orderClaps(id);
    addError("---- after orderclaps----\n");
    addError("---for clap device----");
    addError(printClapTimes(clapDevice.clapTimes.timeStamp, NUM_CLAPS));
    addError("---for host device----");
    addError(printClapTimes(myClapTimes.timeStamp, NUM_CLAPS));
    addError("---for client device "+String(id)+"----");
    addError(printClapTimes(clientAddresses[id].clapTimes.timeStamp, NUM_CLAPS));
    if (id > -1) {
        addError("id > -1 "+String(id)+"\n");
        for (int i = 0; i<clientAddresses[id].clapTimes.clapCounter; i++) {
            if (clientAddresses[id].clapTimes.timeStamp[i] == 0) {continue;}
            else {
                int timeDifference = clientAddresses[id].clapTimes.timeStamp[i]-clapDevice.clapTimes.timeStamp[i];
                clientAddresses[id].distances[i] = 0.0343*(timeDifference);
                addError("distance for "+String(id)+" found "+String(clientAddresses[id].distances[i]));
            }   
        }
        int distcount = 0;
        float distAvg = 0;
        for (int i = 0; i < clientAddresses[id].clapTimes.clapCounter; i++) {
            if (clientAddresses[id].distances[i] != 0) {
                distAvg += clientAddresses[id].distances[i];
                distcount++;
            }
        }
        if (distcount > 0) {
            clientAddresses[id].distanceFromCenter = distAvg/distcount;
        }
        distanceMessage.distance = clientAddresses[id].distanceFromCenter;
        pushDataToSendQueue(clientAddresses[id].address, MSG_DISTANCE, -1);
        
    }
    else {
        addError("id not > -1 "+String(id)+" and clapcounter = "+String(myClapTimes.clapCounter)+"\n");
        addError("calculating for host", false);
        //here somewhere is an error if the orderclaps doesn't do what it's told. especially if there's a zero somewhere. 
        
        for (int i = 0; i<myClapTimes.clapCounter; i++) {
            addError("clap "+String(i)+" and clapcounter  = "+String(myClapTimes.clapCounter)+"\n");
            if (myClapTimes.timeStamp[i] == 0) {continue;}
            else {
                int timeDifference = myClapTimes.timeStamp[i]-clapDevice.clapTimes.timeStamp[i];
                myDistances[i] = 0.0343*(timeDifference);
                addError("distance for host found "+String(myDistances[i])+"\n");
        }   
        
    }
    }
    addError("---calculate distances---\n");
    addError("updating to webserver", false);
    updateAddressesToWebserver();

}


void messaging::orderClaps(int id) {
    message_send_clap_times newClapTimes;
    memset(&newClapTimes, 0, sizeof(newClapTimes));
    addError("orderclaps  clapcounter = "+String(myClapTimes.clapCounter), false);
   if (id == -1) {
        for (int i = 0; i < clapDevice.clapTimes.clapCounter; i++) {
            if (clapDevice.clapTimes.timeStamp[i] == 0) {addError(String(i)+" is zero\n"); return;}
            for (int j = 0; j<myClapTimes.clapCounter; j++) {
                if (myClapTimes.timeStamp[j] == 0) {addError(String(j)+" is zero\n");break;}
                if (myClapTimes.timeStamp[j] <= clapDevice.clapTimes.timeStamp[i]+500000 and myClapTimes.timeStamp[j]>=clapDevice.clapTimes.timeStamp[i] -500000 ) {
                    addError("fits"+String(myClapTimes.timeStamp[j])+"\n");
                        newClapTimes.timeStamp[i] = myClapTimes.timeStamp[j];
                        break;
                }
                else if (myClapTimes.timeStamp[j]<=clapDevice.clapTimes.timeStamp[i]+500000) {
                    addError("smaller\n");
                }
                else if (myClapTimes.timeStamp[j]>=clapDevice.clapTimes.timeStamp[i] -500000 ) {
                    addError("larger\n");
                }
                else {
                    addError("neither" + String(myClapTimes.timeStamp[j])+" - "+String(clapDevice.clapTimes.timeStamp[i])+"\n");
                }
            }
            newClapTimes.clapCounter++;

        }
        memcpy(&myClapTimes, &newClapTimes, sizeof(newClapTimes));
        addError("FROM ORDERCLAPS\n");
        addError(printClapTimes(myClapTimes.timeStamp, NUM_CLAPS));
    }
    else {
        addError("Orderclaps called for "+String(id)+"\n");
        for (int i = 0; i < clapDevice.clapTimes.clapCounter; i++) {
            if (clapDevice.clapTimes.timeStamp[i] == 0) {return;}
            for (int j = 0; j<clientAddresses[id].clapTimes.clapCounter; j++) {
                if (clientAddresses[id].clapTimes.timeStamp[j] == 0) {break;}
                    if (clientAddresses[id].clapTimes.timeStamp[j] < clapDevice.clapTimes.timeStamp[i]+500000 and clientAddresses[id].clapTimes.timeStamp[j]>clapDevice.clapTimes.timeStamp[i] -500000 ) {
                        newClapTimes.timeStamp[i] = clientAddresses[id].clapTimes.timeStamp[j];
                        break;
                }
            }
            newClapTimes.clapCounter++;
        }
        memset(&clientAddresses[id].clapTimes, 0, sizeof(clientAddresses[id].clapTimes));
        memcpy(&clientAddresses[id].clapTimes, &newClapTimes, sizeof(newClapTimes));
    }
}

int messaging::checkAndAverage(float x0[3], float x1[3], float x2[3]) {
     int returnval = -1;
    for (int i = 0; i < 2; ++i) { // Loop through both dimensions
        // Check if x0 is the outlier
        if (std::abs(x0[i] - x1[i]) > 3 && std::abs(x0[i] - x2[i]) > 3 && std::abs(x1[i] - x2[i])  <= 3) {
            returnval = 0;
        }
        // Check if x1 is the outlier
        else if (std::abs(x1[i] - x0[i]) > 3 && std::abs(x1[i] - x2[i]) > 3 && std::abs(x0[i] - x2[i]) <= 3) {
            returnval = 1;
        }
        // Check if x2 is the outlier
        else if (std::abs(x2[i] - x0[i]) > 3 && std::abs(x2[i] - x1[i]) > 3 && std::abs(x0[i] - x1[i]) <= 3) {
            returnval = 2;
        }
        // No outlier found
    }
    return returnval;
}
void messaging::unifyDistances(int id) {
    // todoclapdevice claptimes
    addError("CalculatingDistances\n");
    for (int i = 0; i < clapDevice.clapTimes.clapCounter; i++) {
        float averages = 0.0;
        int counter = 0;
       for (int j = i; j < clapDevice.clapTimes.clapCounter; j++) {
        if (clientAddresses[id].distances[j] == 0) {
            continue;
        }
        if (arePointsEqual(clapDeviceLocations[j], clapDeviceLocations[i])) {
            averages += clientAddresses[id].distances[j];
            clientAddresses[id].distances[j] = 0;
            counter++;
        }
       }
       if (averages != 0 and counter != 0) {
              clientAddresses[id].distances[i] = averages/counter;
       }
    }
}

bool messaging::arePointsEqual(clap_device_location &point1, clap_device_location &point2) {
    if (point1.xLoc == point2.xLoc and point1.yLoc == point2.yLoc and point1.zLoc == point2.zLoc) {
        return true;
    }
    return false;
}

bool messaging::arePointsEqual(float point1[3], clap_device_location &point2) {
    if (point1[0] == point2.xLoc and point1[1] == point2.yLoc and point1[2] == point2.zLoc) {
        return true;
    }
    return false;
}

void messaging::groupPoints( int indices[NUM_CLAPS][CLAPS_PER_POINT]) {
    bool used[NUM_CLAPS] = {0};
    for (int i = 0; i < NUM_CLAPS; i++) {
        bool skip = false;
        if (clapDeviceLocations[i].xLoc == 0 and clapDeviceLocations[i].yLoc == 0 and clapDeviceLocations[i].zLoc == 0) {
            continue;
        }
        for (int j = 0; j < NUM_CLAPS; ++j) {
           if (used[j]) {
            skip = true;
           }
           break;
        }
        if (skip) {
            continue;
        }
        for (int j = i; j < NUM_CLAPS; j++) {
            if (arePointsEqual(clapDeviceLocations[i], clapDeviceLocations[j])) {
                for (int k = 0; k < CLAPS_PER_POINT; k++) {
                    if (indices[i][k] == 0) {
                        indices[i][k] = j;
                        used[j] = true;
                        break;
                    }
                }
            }
        }
        if (skip) {
            continue;
        }
    }
}

void messaging::estimatePoints(int id, float* x0) {
    calculation_struct pointsDistances[CLAPS_PER_POINT];
    for (int i = 0; i < CLAPS_PER_POINT; i++) {
        for (int j = 0; j < NUM_CLAPS; j++) {
            pointsDistances[i].distances[j] = -1;
            pointsDistances[i].points[j][0] = -1.0f;
            pointsDistances[i].points[j][1] = -1.0f;
            pointsDistances[i].points[j][2] = -1.0f;
        }
    }
    clap_device_location last_point {0, 0, 0, 0};
    int j = 0;

    for (int i = 0; i < NUM_CLAPS;i++) {
        if (clientAddresses[0].distances[i] == 0) {
            continue;
        }
        else {
            if (arePointsEqual(clapDeviceLocations[i], last_point)) {
                j++;
                }  
            else {
                last_point = clapDeviceLocations[i];
                j = 0;
            }
            for (int k = 0; k < NUM_CLAPS; k++) {
                if (pointsDistances[j].distances[k] == -1) {
                    pointsDistances[j].distances[k] = clientAddresses[0].distances[i];
                    pointsDistances[j].points[k][0] = clapDeviceLocations[i].xLoc;
                    pointsDistances[j].points[k][1] = clapDeviceLocations[i].yLoc;
                    pointsDistances[j].points[k][2] = clapDeviceLocations[i].zLoc;
                    pointsDistances[j].numPoints++;
                    break;
                }
            }
        }

    }

    //const float weights[NUM_CLAPS] = {1, 1, 1, 1}; // Equal weights for simplicity

    // Initial guess for the unknown point

    // Optimization loop to minimize the weighted distances (improved version)
    float learning_rate = 0.01;
    float prev_error = std::numeric_limits<float>::max();
    float tolerance = 0.0001;
    int max_iterations = 1000;
    float x1[CLAPS_PER_POINT][3];
    for (int j = 0; j < CLAPS_PER_POINT; j++) {
        if (pointsDistances[j].numPoints <= 3) {
            x1[j][0] = -1;
            x1[j][1] = -1;
            x1[j][2] = -1;
            continue;
        }
        for (int iter = 0; iter < max_iterations; ++iter) {
        float gradients[3] = {0, 0, 0};
        float result[NUM_CLAPS];
        weighted_distances_to_points(pointsDistances[j].numPoints, x1[j], pointsDistances[j], result);
        float error = 0;

        for (int i = 0; i < pointsDistances[j].numPoints; ++i) {
            error += result[i] * result[i];
            gradients[0] += result[i] * (x1[j][0] - pointsDistances[j].points[i][0]);
            gradients[1] += result[i] * (x1[j][1] - pointsDistances[j].points[i][1]);
            gradients[2] += result[i] * (x1[j][2] - pointsDistances[j].points[i][2]);
        }

        error = sqrt(error);

        // Normalize gradients to prevent overflow
        float norm = sqrt(gradients[0] * gradients[0] + gradients[1] * gradients[1] + gradients[2] * gradients[2]);
        if (norm > 0) {
            gradients[0] /= norm;
            gradients[1] /= norm;
            gradients[2] /= norm;
        }

        // Adaptive learning rate
        if (error < prev_error) {
            learning_rate *= 1.05; // Increase learning rate if error decreased
            prev_error = error;
        } else {
            learning_rate *= 0.5; // Decrease learning rate if error increased
        }

        // Update the guess
        x1[j][0] -= learning_rate * gradients[0];
        x1[j][1] -= learning_rate * gradients[1];
        x1[j][2] -= learning_rate * gradients[2];


        // Check for convergence
        if (error < tolerance) {
            break;
        }
    }
    if (x1[j][2]<0) {
      x1[j][2] = 0- x1[j][2];
    } 
    }
    // Average the results
    int i = checkAndAverage(x1[0], x1[1], x1[2]);
    if (i == -1) {
        x0[0] = (x1[0][0] + x1[1][0] + x1[2][0]) / 3;
        x0[1] = (x1[0][1] + x1[1][1] + x1[2][1]) / 3;
        x0[2] = (x1[0][2] + x1[1][2] + x1[2][2]) / 3;
    } else {
        x0[0] = x1[i][0];
        x0[1] = x1[i][1];
        x0[2] = x1[i][2];
    }


}
void messaging::weighted_distances_to_points(int numClaps, const float x[3], const calculation_struct pointsDistances, float result[NUM_CLAPS]) {
    for (int i = 0; i < pointsDistances.numPoints; ++i) {
        float dx = pointsDistances.points[i][0] - x[0];
        float dy = pointsDistances.points[i][1] - x[1];
        float dz = pointsDistances.points[i][2] - x[2];
        float distance = sqrt(dx * dx + dy * dy + dz * dz);
        result[i] = distance - pointsDistances.distances[i]; 
    }
}


void messaging::testTrilateration() {


    for (int z = 0; z < 100; z++) {
    float point_searched[3];

        for (int i = 0; i < 12; i++) {
            clapDeviceLocations[i].xLoc= random(1, 51); // Random value between 1 and 50
            clapDeviceLocations[i+1].xLoc= clapDeviceLocations[i].xLoc;
            clapDeviceLocations[i+2].xLoc= clapDeviceLocations[i].xLoc;
            clapDeviceLocations[i].yLoc = random(1, 51); // Random value between 1 and 50
            clapDeviceLocations[i+1].yLoc = clapDeviceLocations[i].yLoc;
            clapDeviceLocations[i+2].yLoc = clapDeviceLocations[i].yLoc;
            clapDeviceLocations[i].zLoc = random(1, 6);  // Random value between 1 and 5
            clapDeviceLocations[i+1].zLoc = clapDeviceLocations[i].zLoc;
            clapDeviceLocations[i+2].zLoc = clapDeviceLocations[i].zLoc;
            i+=2;
        }
        point_searched[0] = random(1,51);
        point_searched[1] = random(1,51);
        point_searched[2] = random(1,6);

    calculate_distances(point_searched);
   
        for (int i = 0; i < 12; ++i) {
            //Serial.println("Distance before "+String(i)+": "+clientAddresses[i].distances[i]);

            clientAddresses[0].distances[i] *= scaling_factor(clientAddresses[0].distances[i]);
            //Serial.println("Distance after "+String(i)+": "+clientAddresses[i].distances[i]);

        }

    float x0[3] = {0, 0, 0};

    estimatePoints(0, x0);
      Serial.print("Point searched: ");
      Serial.print(point_searched[0]);
      Serial.print(", ");
      Serial.print(point_searched[1]);
      Serial.print(", ");
      Serial.println(point_searched[2]);

      Serial.print("Estimated coordinates 1: ");
      Serial.print(x0[0]);
      Serial.print(", ");
      Serial.print(x0[1]);
      Serial.print(", ");
      Serial.println(x0[2]);
      Serial.print("Difference: ");
      Serial.print(x0[0] - point_searched[0]);
      Serial.print(", ");
      Serial.print(x0[1] - point_searched[1]);
      Serial.print(", ");
      Serial.println(x0[2] - point_searched[2]);
      Serial.println("--------------\n");
      delay(1000);
    }
}


void messaging::calculate_distances(const float point[3]) {
    for (int i = 0; i < NUM_CLAPS; ++i) {
        if (clapDeviceLocations[i].xLoc == 0 and clapDeviceLocations[i].yLoc == 0 and clapDeviceLocations[i].zLoc == 0) {
            continue;
        }
        float dx = clapDeviceLocations[i].xLoc - point[0];
        float dy = clapDeviceLocations[i].yLoc - point[1];
        float dz = clapDeviceLocations[i].zLoc - point[2];
        clientAddresses[0].distances[i] = sqrt(dx * dx + dy * dy + dz * dz);
    }


}

float messaging::scaling_factor(float value) {
    // Define the range and scaling
    float min_val = 1.0, max_val = 50.0;
    float max_inaccuracy = 0.15, min_inaccuracy = 0.05;

    // Normalize value to range [0, 1]
    float normalized = (value - min_val) / (max_val - min_val);

    // Calculate inaccuracy - linear interpolation between max_inaccuracy and min_inaccuracy
    float inaccuracy = max_inaccuracy - (max_inaccuracy - min_inaccuracy) * normalized;

    // Generate a random scaling factor within the inaccuracy range
    return 1.0 + ((float)random(-1000, 1000) / 1000.0) * inaccuracy;
}


void messaging::setDistanceFromCenter() {
    int validClaps = 0;
    float averageDistance;
    for (int i = 0; i < addressCounter; i++ ) {
        if (clapDeviceLocations[i].xLoc == -1 && clapDeviceLocations[i].yLoc == -1 && clapDeviceLocations[i].zLoc == -1) {
            for (int j = 0; j < NUM_CLAPS; j++) {
                if (clientAddresses[i].distances[j] != 0) {
                    validClaps++;
                    averageDistance += clientAddresses[i].distances[j];
                }
            }
            float lowerThreshold = averageDistance * 0.8;
            float upperThreshold = averageDistance * 1.2;
            validClaps = 0;
            averageDistance = 0;
            for (int j = 0; j<NUM_CLAPS; j++) {
                if (clientAddresses[i].distances[j] != 0 && clientAddresses[i].distances[j] >= lowerThreshold && clientAddresses[i].distances[j] <= upperThreshold) {
                    validClaps++;
                    averageDistance += clientAddresses[i].distances[j];
                }
            }
            clientAddresses[i].distanceFromCenter = averageDistance/validClaps;
        }
    }
}
