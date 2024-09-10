#ifndef SNTP_H
#define SNTP_H

#include <stdint.h>

/**
 * @brief Initialize SNTP (Simple Network Time Protocol) synchronization.
 * 
 * This function sets up a task that periodically synchronizes the system time
 * with an SNTP server.
 * 
 * @param syncIntervalMs The interval in milliseconds between synchronization attempts.
 */
void initSNTP(uint32_t syncIntervalMs);

#endif
