#ifndef MUON_INCLUDED
#define MUON_INCLUDED

#define SIGNAL_THRESHOLD = 30 * 8;
#define RESET_THRESHOLD = 15 * 8;

void initMuon();
void readMuon(struct STATE *state);

#endif