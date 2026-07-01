#ifndef PEPPEL_SST_PROTOCOL_KEYS_H
#define PEPPEL_SST_PROTOCOL_KEYS_H

/*
 * AppMessage key constants mirroring PebbleRequests.kt from
 * mithodin/Android-SimpleTimeTracker (branch: feature/pebble-integration).
 *
 * Sync procedure: diff this file against
 *   features/feature_pebble/src/main/java/com/example/util/simpletimetracker/feature_pebble/PebbleRequests.kt
 * in the simpletimetracker flake input (nix eval .#inputs.simpletimetracker.outPath).
 * Method ids and pagination keys must match exactly.
 */

#define KEY_METHOD_ID        0u

/* Method ids (mirror WearableRequests paths / PebbleRequests.kt) */
#define METHOD_QUERY_ACTIVITIES                  1u
#define METHOD_QUERY_CURRENT_ACTIVITIES          2u
#define METHOD_QUERY_STATISTICS                  3u
#define METHOD_START_ACTIVITY                    4u
#define METHOD_STOP_ACTIVITY                     5u
#define METHOD_REPEAT_ACTIVITY                   6u
#define METHOD_QUERY_TAGS_FOR_ACTIVITY           7u
#define METHOD_QUERY_SHOULD_SHOW_TAG_SELECTION   8u
#define METHOD_QUERY_SHOULD_SHOW_TAG_VALUE_SELECTION 9u
#define METHOD_QUERY_SETTINGS                   10u
#define METHOD_SET_SETTINGS                     11u
#define METHOD_OPEN_PHONE_APP                   12u
#define METHOD_DATA_UPDATED                     13u

/* Pagination keys (in request and response) */
#define KEY_TOTAL_ITEMS     1u
#define KEY_OFFSET          2u
#define KEY_RETURNED_COUNT 3u
#define KEY_ITEMS_START     4u

/* Key 3 is dual-purpose: requested limit (UInt8) in paginated requests,
 * returned count (UInt8) in paginated responses. */
#define KEY_LIMIT           3u

/* Byte budget for response payload (APP_MESSAGE_INBOX_SIZE_MINIMUM) */
#define INBOX_BUDGET_BYTES  124

#endif /* PEPPEL_SST_PROTOCOL_KEYS_H */
