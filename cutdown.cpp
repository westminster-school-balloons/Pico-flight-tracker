#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "main.h"
#include "misc.h"
#include "cutdown.h"

// Store the geofence - REMEMBER TO UPDATE NUM_POINTS ACCORDINGLY
// TODO: Make this actually good
#define NUM_POINTS 33
const float GEOFENCE[] = {
    -1.0263835, 50.9617035,
    -0.0769043, 50.9447692,
    0.4601635, 50.9900486,
    0.9275644, 51.2404555,
    0.6410757, 51.5331587,
    0.8423013, 51.9324736,
    1.3638430, 52.4988980,
    0.9490685, 52.7124568,
    0.5079889, 52.6783039,
    -0.0054529, 52.7728642,
    -0.4382056, 53.4749700,
    -1.1137481, 54.4013930,
    -1.7382343, 54.3370538,
    -2.4564880, 54.2179219,
    -2.4993045, 53.4947902,
    -2.6181822, 53.2592487,
    -3.2225778, 52.9691035,
    -3.7390613, 52.7900895,
    -3.7170833, 52.2620550,
    -3.7974109, 51.8268558,
    -2.8599404, 51.7752664,
    -2.4415226, 51.9130914,
    -2.1213842, 51.8090696,
    -2.4620569, 51.4892105,
    -2.9367654, 51.0801128,
    -3.9378541, 51.0500343,
    -4.2756888, 50.7808468,
    -4.4570975, 50.5798409,
    -3.8387112, 50.5690840,
    -3.5609884, 50.7345519,
    -3.1003722, 50.8341691,
    -2.4719238, 50.8273464,
    -1.6542023, 50.9176484
};

long cut_altitude = 0;
int test_count = 0;

void init_cutdown() {
    // Perform initialisation here
    gpio_init(CUT_PIN);
    gpio_set_dir(CUT_PIN, GPIO_OUT);
    gpio_put(CUT_PIN, 0);
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

    if (test_count > 5) {
        debug("> Payload CUTDOWN - burn started! \n");
        gpio_put(CUT_PIN, 1);
        state->HasCutDown = 1;
    }

    // Save cut altitude
    cut_altitude = state->Altitude;

    return;

}