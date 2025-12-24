#pragma once
#ifndef SEMVER_UTILS_H
#define SEMVER_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

int semver_compare(const char *a, const char *b);

#ifdef __cplusplus
}
#endif

#endif // SEMVER_UTILS_H