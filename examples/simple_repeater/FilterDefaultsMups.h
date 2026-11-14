#pragma once

// mups: own defaults for the DMC packet filter.
//
// Applied on top of the Dutch-MeshCore defaults (FilterPrefs) on a new install
// (no /filter_prefs yet) and by 'filter reset'. Repeaters with a stored
// /filter_prefs keep their settings. The filter itself stays OFF by default
// ('filter on'); these values only define what it does once switched on.
//
// Kept in its own file so updates of the Dutch filter files do not touch it.
// Change the values here, not in Filter.h.

#define MUPS_FILTER_ADVERT_HOURS   24   // per-origin advert window (h), 0 = off
#define MUPS_FILTER_AGE_MINS       30   // drop readable group texts older than this (min), 0 = off
#define MUPS_FILTER_REQ_HOPS        5   // hop limit for REQ (00), RESPONSE (01), ANON_REQ (07); was 6 in mups18-1
#define MUPS_FILTER_HOPS_OFF       64   // "no hop limit" (flood.max applies)

inline void mupsFilterDefaults(FilterPrefs& p) {
  p.dryrun = true;                 // 'filter on' only counts until 'filter dryrun off'
  p.minimal_hash_bytes = 1;        // accept 1-byte path hashes
  p.filter_malformed = true;       // malformed scan (Public only)
  p.advert_hours = MUPS_FILTER_ADVERT_HOURS;
  p.age_mins = MUPS_FILTER_AGE_MINS;

  for (int t = 0; t < PAYLOAD_TYPE_COUNT; t++) {
    p.payload_prefs[t].rate_limit = 0;    // no rate limits (enable selectively if needed)
    p.payload_prefs[t].rate_secs = 60;
    p.soft_limit[t] = 0;
    p.payload_prefs[t].hops_max = MUPS_FILTER_HOPS_OFF;
  }
  p.payload_prefs[PAYLOAD_TYPE_REQ].hops_max      = MUPS_FILTER_REQ_HOPS;
  p.payload_prefs[PAYLOAD_TYPE_RESPONSE].hops_max = MUPS_FILTER_REQ_HOPS;
  p.payload_prefs[PAYLOAD_TYPE_ANON_REQ].hops_max = MUPS_FILTER_REQ_HOPS;
}
