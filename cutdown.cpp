#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "main.h"
#include "misc.h"
#include "cutdown.h"

// Store the geofence - REMEMBER TO UPDATE NUM_POINTS ACCORDINGLY
// TODO: Make this actually good
#define NUM_POINTS 22
const float GEOFENCE[] = {
    -0.1524720, 51.4971934,
    -0.1536656, 51.4966181,
    -0.1547782, 51.4974117,
    -0.1583594, 51.4968147,
    -0.1558495, 51.4963606,
    -0.1585310, 51.4957995,
    -0.1562785, 51.4955724,
    -0.1577802, 51.4946506,
    -0.1572414, 51.4937921,
    -0.1563001, 51.4951483,
    -0.1557366, 51.4955449,
    -0.1560183, 51.4934254,
    -0.1538330, 51.4937021,
    -0.1544765, 51.4923260,
    -0.1525029, 51.4926467,
    -0.1530821, 51.4938223,
    -0.1510442, 51.4935818,
    -0.1521168, 51.4942365,
    -0.1491135, 51.4943166,
    -0.1508869, 51.4947926,
    -0.1496284, 51.4957594,
    -0.1522922, 51.4974537
};

long cut_altitude = 0;
int test_count = 0;

void init_cutdown() {
    // Perform initialisation here
    return;
}

bool should_cut(struct STATE * state) {
    // Checks if payload should be cut down

    // If above ceiling, cut
    if (state->Altitude >= CEILING_ALT) {
        return true;
    }

    // If outside geofence, cut
    if (!is_in_geofence(state->Longitude, state->Latitude, GEOFENCE, NUM_POINTS)) {
        return true;
    }

    // Otherwise, return false
    return false;
}

bool is_in_geofence(float longitude, float latitude, const float *geofence, int num_points) {
    // Checks if the payload is inside the geofence using polygon collision
    // Returns true if point is to the left of an odd number of segments

    // If latitude and longitude are both zero (invalid data), return false
    if (latitude == 0.0 && longitude == 0.0) {
        return true;
    }

    int count = 0;

    for (int i = 0; i < NUM_POINTS; i++) {
        int next = (i + 1) % NUM_POINTS;
        if (left_of_line(longitude, latitude, GEOFENCE[2*i], GEOFENCE[2*i + 1], GEOFENCE[2*next], GEOFENCE[2*next + 1])) {
            count++;
        }
    }

    return count % 2 == 1; 
}

bool left_of_line(float px, float py, float lx0, float ly0, float lx1, float ly1) {
    // Checks if a point (px, py) is to the left of a line defined by (lx0, ly0) and (lx1, ly1)
    float minX = lx0 < lx1 ? lx0 : lx1;
    float maxX = lx0 > lx1 ? lx0 : lx1;
    float minY = ly0 < ly1 ? ly0 : ly1;
    float maxY = ly0 > ly1 ? ly0 : ly1;

    // Return false if too high or too low
    if (py < minY || py > maxY) {
       return false; 
    }

    // Deal with edge case if aligned with an endpoint - return true only if other endpoint is below test point
    if (py == ly0 && px < lx0) {
        return ly1 < py;
    }
    if (py == ly1 && px < lx1) {
        return ly0 < py;
    }

    // Return false if to the right of both endpoints and true if to the left of both
    if (px > maxX) {
        return false;
    }
    if (px < minX) {
        return true;
    }

    // Edge case if line is vertical
    if (lx0 == lx1) {
        return px <= lx0;
    }

    // Edge case if line is horiontal
    if (ly0 == ly1) {
        return false;
    }

    // Calculate gradient and x-coord of hitpoint
    float gradient = (ly1-ly0) / (lx1-lx0);
    float hitpoint = lx0 + (py-ly0) / gradient;

    // Return true if hitpoint is to the left
    return px <= hitpoint;
}


void cutdown_check(struct STATE * state){
    // If payload as already cut and altitude is now less than cut altitude (by certain a amount), set gpio pin to low
    if (state->HasCutDown) {

        if (cut_altitude - state->Altitude > 10) {
            debug("> CUTDOWN burn stopped \n");  
            gpio_put(CUT_PIN, 0);
        } else {
            debug("> Altitude not decreasing - burn resuming for another second \n");
        }

        return;
    }

    // Otherwise check if payload should be cut down
    if (should_cut(state)) {
        test_count++;
    } else {
        test_count = 0;
    }

    if (test_count >= 5) {
        debug("> Payload CUTDOWN - burn started! \n");
        gpio_put(CUT_PIN, 1);
        state->HasCutDown = 1;

        // Save cut altitude
        cut_altitude = state->Altitude;
    }

    return;

}