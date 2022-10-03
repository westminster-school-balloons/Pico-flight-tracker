#ifndef CUTDOWN_INCLUDED
#define CUTDOWN_INCLUDED

#define CEILING_ALT 25500

void init_cutdown();

bool is_in_geofence(float latitude, float longitude, const float *geofence, int num_points);
bool left_of_line(float px, float py, float lx0, float ly0, float lx1, float ly1);

bool should_cut(struct STATE * state);
void cutdown_check(struct STATE * state);

#endif