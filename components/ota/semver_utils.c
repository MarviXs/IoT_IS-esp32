#include <ctype.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
    int major, minor, patch;
    bool has_prerelease;
    char prerelease[32]; // enough for "rc.1", "dirty", "gabcdef", etc.
    bool is_dirty;       // true if prerelease contains "dirty"
} semver_t;

static void copy_nul(char *dst, size_t dstsz, const char *src, size_t n) {
    size_t m = (n < dstsz - 1) ? n : (dstsz - 1);
    if (dstsz == 0) return;
    if (m > 0 && src) memcpy(dst, src, m);
    dst[m] = '\0';
}

static bool parse_semver(const char *s, semver_t *out) {
    const char *p, *q;
    long v;
    char *endp;

    if (!s || !out) return false;
    memset(out, 0, sizeof(*out));

    /* skip spaces and optional 'v'/'V' */
    p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == 'v' || *p == 'V') p++;

    /* major */
    v = strtol(p, &endp, 10);
    if (endp == p || v < 0) return false;
    out->major = (int)v;
    if (*endp != '.') return false;
    p = endp + 1;

    /* minor */
    v = strtol(p, &endp, 10);
    if (endp == p || v < 0) return false;
    out->minor = (int)v;
    if (*endp != '.') return false;
    p = endp + 1;

    /* patch */
    v = strtol(p, &endp, 10);
    if (endp == p || v < 0) return false;
    out->patch = (int)v;
    p = endp;

    /* prerelease (starts with '-') stops at '+' or end */
    if (*p == '-') {
        p++;
        q = p;
        while (*q && *q != '+') q++;
        out->has_prerelease = true;
        copy_nul(out->prerelease, sizeof(out->prerelease), p, (size_t)(q - p));
        if (strstr(out->prerelease, "dirty") != NULL) out->is_dirty = true;
    }
    /* ignore build metadata +... */
    return true;
}

/* Return next identifier from prerelease string: token delimited by '.' */
static bool pre_next_token(const char *s, size_t len, size_t *idx, const char **tok, size_t *toklen) {
    size_t i = *idx;
    size_t start;

    if (i >= len) return false;
    start = i;
    while (i < len && s[i] != '.') i++;
    *tok = s + start;
    *toklen = i - start;
    *idx = (i < len) ? (i + 1) : i; /* skip '.' if present */
    return true;
}

static bool token_is_numeric(const char *t, size_t n) {
    size_t i;
    if (n == 0) return false;
    for (i = 0; i < n; ++i) {
        if (!isdigit((unsigned char)t[i])) return false;
    }
    return true;
}

/* SemVer prerelease comparison:
   - Compare dot-separated identifiers left-to-right.
   - Numeric identifiers are compared numerically and have LOWER precedence than non-numeric.
   - If all equal but one has more identifiers, the longer has HIGHER precedence.
   Returns: <0 if a<b, 0 if equal, >0 if a>b
*/
static int compare_prerelease(const char *a, const char *b) {
    size_t ia = 0, ib = 0, la = strlen(a), lb = strlen(b);
    const char *ta, *tb;
    size_t na, nb;

    while (pre_next_token(a, la, &ia, &ta, &na) | pre_next_token(b, lb, &ib, &tb, &nb)) {
        if (na == 0 && nb == 0) return 0;
        if (na == 0) return -1; /* a shorter -> lower */
        if (nb == 0) return  1;

        /* ----- DIRTY WINS rule ----- */
        if (na == 5 && strncmp(ta, "dirty", 5) == 0 && !(nb == 5 && strncmp(tb, "dirty", 5) == 0)) {
            return 1;  /* a has 'dirty' -> higher */
        }
        if (nb == 5 && strncmp(tb, "dirty", 5) == 0 && !(na == 5 && strncmp(ta, "dirty", 5) == 0)) {
            return -1; /* b has 'dirty' -> higher */
        }
        /* --------------------------- */

        {
            bool an = token_is_numeric(ta, na);
            bool bn = token_is_numeric(tb, nb);

            if (an && bn) {
                long ai = strtol(ta, NULL, 10);
                long bi = strtol(tb, NULL, 10);
                if (ai != bi) return (ai > bi) ? 1 : -1;
            } else if (an != bn) {
                /* SemVer: numeric < non-numeric */
                return an ? -1 : 1;
            } else {
                int r;
                size_t n = (na < nb) ? na : nb;
                r = strncmp(ta, tb, n);
                if (r) return (r > 0) ? 1 : -1;
                if (na != nb) return (na > nb) ? 1 : -1;
            }
        }
    }
    return 0;
}

/* returns <0 if a<b, 0 if equal, >0 if a>b */
int semver_compare(const char *a, const char *b) {
    semver_t va, vb;
    bool pa = parse_semver(a, &va);
    bool pb = parse_semver(b, &vb);

    if (pa && pb) {
        if (va.major != vb.major) return (va.major > vb.major) ? 1 : -1;
        if (va.minor != vb.minor) return (va.minor > vb.minor) ? 1 : -1;
        if (va.patch != vb.patch) return (va.patch > vb.patch) ? 1 : -1;

        /* base equal → clean > prerelease; dirty counts as prerelease and is lower */
        if (va.is_dirty != vb.is_dirty) {
            /* DIRTY > CLEAN at same base */
            return va.is_dirty ? 1 : -1;
        }
        if (va.has_prerelease != vb.has_prerelease) {
            return va.has_prerelease ? -1 : 1;
        }
        if (va.has_prerelease && vb.has_prerelease) {
            return compare_prerelease(va.prerelease, vb.prerelease);
        }
        return 0;
    }

    /* Only one parsed: prefer the parsed one */
    if (pa && !pb) return 1;
    if (!pa && pb) return -1;

    /* Fallback: strip leading 'v' and "-dirty" then strcmp */
    {
        char aa[64], bb[64];
        const char *sa = a ? a : "", *sb = b ? b : "";
        size_t i = 0, j = 0;

        /* strip spaces and leading 'v' */
        while (sa[i] && isspace((unsigned char)sa[i])) i++;
        if (sa[i] == 'v' || sa[i] == 'V') i++;
        while (sb[j] && isspace((unsigned char)sb[j])) j++;
        if (sb[j] == 'v' || sb[j] == 'V') j++;

        copy_nul(aa, sizeof(aa), sa + i, strlen(sa + i));
        copy_nul(bb, sizeof(bb), sb + j, strlen(sb + j));
        /* cut at "-dirty" if present */
        char *d = strstr(aa, "-dirty"); if (d) *d = '\0';
        d = strstr(bb, "-dirty"); if (d) *d = '\0';

        int r = strcmp(aa, bb);
        if (r == 0) return 0;
        /* be conservative: assume available (b) is newer */
        return -1;
    }
}