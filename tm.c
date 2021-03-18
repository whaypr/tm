#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <err.h>
#include "tm.h"

int qflag = 0;
int tflag = 0;
int Tflag = 0;

static void
usage()
{
	fprintf(stderr, "usage: tm [-qtT] machine\n");
}

struct inst*
mkinst(char R, char W, char M, struct state *t)
{
	struct inst *i = NULL;
	if (t == NULL) {
		warn("jump state missing");
		return NULL;
	}
	if ((i = calloc(1, sizeof(struct inst))) == NULL)
		err(1, NULL);
	i->r = R;
	i->w = W;
	i->m = M;
	i->t = t;
	return i;
}

struct inst*
getinst(struct inst *list, char R)
{
	struct inst *i;
	if (list == NULL)
		return NULL;
	for (i = list; i; i = i->next)
		if (i->r == R)
			return i;
	return NULL;
}

int
addinst(struct state* s, struct inst *i)
{
	if (s == NULL)
		return -1;
	if (s->inst == NULL) {
		s->inst = i;
		s->last = i;
	} else {
		s->last->next = i;
		s->last = i;
	}
	return 0;
}

void
prstat(struct state *s)
{
	struct inst *i;
	if (s == NULL)
		return;
	for (i = s->inst; i; i = i->next)
		printf("%c%c%c%c%c\n", s->s, i->r, i->w, i->m, i->t->s);
}

struct state*
mkstat(char S)
{
	struct state *s = NULL;
	if ((s = calloc(1, sizeof(struct state))) == NULL)
		err(1, NULL);
	s->s = S;
	return s;
}

struct state*
getstat(struct state *list, char S)
{
	struct state *s;
	if (list == NULL)
		return NULL;
	for (s = list; s; s = s->next)
		if (s->s == S)
			return s;
	return NULL;
}

int
addstat(struct tm* tm, struct state *s)
{
	if (tm == NULL)
		return -1;
	if (tm->list == NULL) {
		tm->list = s;
		tm->last = s;
	} else {
		tm->last->next = s;
		tm->last = s;
	}
	return 0;
}

int
add(struct tm* tm, char S, char R, char W, char M, char T)
{
	struct inst  *i;
	struct state *s, *t;
	if (tm == NULL)
		return -1;
	if ((s = getstat(tm->list, S))) {
		if ((i = getinst(s->inst, R))) {
			warnx("conflicting instructions for (%c,%c)", S, R);
			warnx("old: %c%c%c%c%c", S, R, i->w, i->m, i->t->s);
			warnx("new: %c%c%c%c%c", S, R, W, M, T);
			return -1;
		}
	} else {
		s = mkstat(S);
		addstat(tm, s);
	}
	if ((t = getstat(tm->list, T)) == NULL) {
		t = mkstat(T);
		addstat(tm, t);
	}
	i = mkinst(R, W, M, t);
	addinst(s, i);
	return 0;
}

void
prtape(struct tm *tm)
{
	char *c;
	if (tm == NULL || tm->tape == NULL)
		return;
	if (Tflag) {
		for (c = tm->tape; c && *c; c++) {
			if (c == tm->head) {
				printf("\033[1m%c\033[22m", *c);
			} else {
				putchar(*c);
			}
		}
		putchar('\n');
	} else {
		printf("%s\n", tm->tape);
	}
		
}

/* Given an input line, prepare the machine's tape.
 * Point the head to the first 1 (it there is one).
 * Return length for success, 0 for empty line, -1 for error. */
int
mktape(struct tm *tm, char* line)
{
	char *c;
	char *h = NULL;
	size_t len = 0;
	if (tm == NULL)
		return -1;
	if (line == NULL || *line == '\n' || *line == 0)
		return 0;
	for (c = line, len = 0; c; c++, len++) {
		if (*c == '\n') {
			*c = 0;
			break;
		}
		if (!isalnum(*c)) {
			warnx("'%c' is not a valid tape symbol", *c);
			return -1;
		}
		if ((*c == '1') && (h == NULL)) {
			/* point the head to the first 1 */
			h = c;
		}
	}
	tm->tape = strdup(line);
	tm->head = h ? tm->tape + (h - line) : tm->tape;
	tm->tlen = len;
	return len;
}

void
prtm(struct tm *tm)
{
	struct state *s;
	if (tm == NULL)
		return;
	for (s = tm->list; s; s = s->next)
		prstat(s);
}

struct tm*
mktm(const char* file)
{
	FILE* f;
	char s, r, w, m, t;
	struct tm *tm = NULL;
	if ((f = fopen(file, "r")) == NULL)
		err(1, "%s", file);
	if ((tm = calloc(1, sizeof(struct tm))) == NULL)
		err(1, NULL);
	while (fscanf(f, "%c%c%c%c%c\n", &s, &r, &w, &m, &t) == 5) {
		if (!(isalnum(s))) {
			warnx("'%c' is not a valid state name", s);
			goto bad;
		}
		if (!(isalnum(r))) {
			warnx("'%c' is not a valid tape symbol", r);
			goto bad;
		}
		if (!(isalnum(w))) {
			warnx("'%c' is not a valid tape symbol", w);
			goto bad;
		}
		if (!(ISMOVE(m))) {
			warnx("'%c' is not a valid move", m);
			goto bad;
		}
		if (!(isalnum(t))) {
			warnx("'%c' is not a valid state name", t);
			goto bad;
		}
		if (add(tm, s, r, w, m, t) == -1) {
			warnx("could not add %c%c%c%c%c", s, r, w, m, t);
			goto bad;
		}
	}
	tm->s = tm->list;
	return tm;
bad:
	free(tm);
	return NULL;
}

void
reset(struct tm* tm)
{
	if (tm == NULL)
		return;
	tm->s = tm->list;
	free(tm->tape);
	tm->tlen = 0;
}

int
run
(struct tm *tm)
{
	struct inst *i;
	do {
		if (tflag)
			prtape(tm);
		if ((i = getinst(tm->s->inst, *tm->head)) == NULL) {
			/* halt */
			break;
		}
		*tm->head = i->w;
		tm->head += i->m == 'L' ? -1 : +1;
		tm->s = i->t;
	} while (1);
	if (!tflag && !qflag)
		prtape(tm);
	return 0;
}

int
main(int argc, char** argv)
{
	int c;
	struct tm *tm;
	ssize_t len = 0;
	size_t size = 0;
	char *line = NULL;

	while ((c = getopt(argc, argv, "qtT")) != -1) switch (c) {
		case 'q':
			qflag = 1;
			break;
		case 't':
			tflag = 1;
			break;
		case 'T':
			Tflag = 1;
			tflag = 1;
			break;
		default:
			usage();
			return -1;
	}
	argc -= optind;
	argv += optind;

	if (argc != 1) {
		usage();
		return -1;
	}

	if ((tm = mktm(*argv)) == NULL) {
		warnx("error parsing instructions");
		return -1;
	}

	while ((len = getline(&line, &size, stdin)) != -1) {
		switch (mktape(tm, line)) {
			case -1:
				warnx("invalid tape: %s", line);
				break;
			case 0:
				/* empty */
				break;
			default:
				run(tm);
				break;
		}
		reset(tm);
	}

	free(line);
	return 0;
}
