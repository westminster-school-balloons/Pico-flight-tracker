#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "main.h"
#include "misc.h"
#include "cutdown.h"

// Store the geofence
// TODO: Make this actually good
#define NUM_POINTS 20
const float GEOFENCE[] = {
    0.9705386, 51.1641209,
    0.1136050, 51.4388757,
    0.6189761, 51.6506805,
    1.1792788, 52.3741918,
    1.0254702, 52.6749843,
    0.2564272, 52.6083195,
    -0.4686704, 52.7548480,
    -0.0731626, 53.1914890,
    -0.3697935, 53.6237282,
    -1.0399595, 53.9223934,
    -2.1166196, 53.8706042,
    -2.7757993, 53.3819533,
    -3.3580747, 52.8279280,
    -3.2591978, 51.8818499,
    -2.1605650, 51.9496158,
    -1.8419614, 51.6370454,
    -2.8307310, 51.1158697,
    -3.4569517, 50.8114203,
    -1.4354673, 50.9638944,
    -0.0511900, 51.0468518
};

long cut_altitude = 0;

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
        return false;
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
    if (py > maxX) {
        return false;
    }
    if (py < minX) {
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
    // If payload as already cut and altitude is now less than cut altitude, set gpio pin to low
    if (state->HasCutDown) {

        if (state->Altitude < cut_altitude) {
            debug("> CUTDOWN burn stopped \n");  
            gpio_put(CUT_PIN, 0);
        } else {
            debug("> Altitude not decreasing - burn resuming for another second \n");
        }

        return;
    }

    // Otherwise check if payload should be cut down
    if (should_cut(state)) {
        debug("> Payload CUTDOWN - burn started! \n");
        gpio_put(CUT_PIN, 1);
        state->HasCutDown = true;

        // Save cut altitude
        cut_altitude = state->Altitude;
    }

    return;

}