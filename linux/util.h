/* $scrotwm: util.h,v 1.3 2010/01/11 20:44:54 marco Exp $ */

#define FPARSELN_UNESCESC	0x01
#define FPARSELN_UNESCCONT	0x02
#define FPARSELN_UNESCCOMM	0x04
#define FPARSELN_UNESCREST	0x08
#define FPARSELN_UNESCALL	0x0f

size_t	strlcpy(char *, const char *, size_t);
size_t	strlcat(char *, const char *, size_t);

char   *fgetln(FILE *, size_t *);
char   *fparseln(FILE *, size_t *, size_t *, const char [3], int);

long long strtonum(const char *, long long, long long, const char **);

int	fmt_scaled(long long number, char *result);

#ifndef WAIT_ANY
#define WAIT_ANY		(-1)
#endif

/* there is no limit to ulrich drepper's crap */
#ifndef TAILQ_END
#define	TAILQ_END(head)			NULL
#endif

/*
 * fmt_scaled(3) specific flags. (from OpenBSD util.h)
 */
#define FMT_SCALED_STRSIZE	7	/* minus sign, 4 digits, suffix, null byte */

int getpeereid(int s, uid_t *euid, gid_t *gid);

