/*
 * Compatibility entry for the old std/native_support.c path.
 *
 * The driver now links std/support/host_c.c through the selected std backend.
 * Keep this file so older build scripts that compile std/native_support.c still
 * get the same v0 support symbols and legacy wrappers.
 */
#include "support/host_c.c"
