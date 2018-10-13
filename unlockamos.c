/* unlockamos: unlocks AMOS source files with "locked" procedures */
#include "fileio.c"
#include <string.h>

/* read 16-bit big-endian word from unsigned char[] */
#define amos_deek(a) ((((a)[0])<<8)|((a)[1]))
/* read 32-bit big-endian word from unsigned char[] */
#define amos_leek(a) ((((a)[0])<<24)|(((a)[1])<<16)|(((a)[2])<<8)|((a)[3]))

void AMOS_decrypt_procedure(unsigned char *src) {
    unsigned char *line, *next, *endline;
    unsigned int key, key2, key3, size;

    /* do not operate on compiled procedures */
    if (src[10] & 0x10) return;

    size = amos_leek(&src[4]);
    line = next = &src[src[0] * 2]; /* the line after PROCEDURE */
    endline = &src[size + 8 + 6]; /* the start of the line after END PROC */

    /* initialise keys */
    key = (size << 8) | src[11];
    key2 = 1;
    key3 = amos_deek(&src[8]);

    while (line < endline) {
	line = next; next = &line[line[0] * 2];
	if (!line[0]) return; /* avoid infinite loop on bad data */
	for (line += 4; line < next;) {
	    *line++ ^= (key >> 8) & 0xFF;
	    *line++ ^=  key       & 0xFF;
	    key  += key2;
	    key2 += key3;
	    key = (key >> 1) | (key << 31); /* rotate right one bit */
	}
    }
    src[10] ^= 0x20; /* toggle "is encrypted" bit */
}

int unlock_source(unsigned char *src, size_t len) {
    int locked_procs_found = 0;
    unsigned int x;

    /* go through the lines of source code */
    len = amos_leek(&src[16]);
    for (src += 20; len > 0; src += x, len -= x) {
	x = src[0] * 2;
	if (x > len) {
	    printf("line length error\n");
	    return 0;
	}

	/* does this a line begin with "PROCEDURE" ? */
	if (amos_deek(&src[2]) == 0x0376) {
	    /* decrypt and remove the "is encrypted" flag */
	    if (src[10] & 0x20) {
		if ((size_t)(amos_leek(&src[4]) + 8 + 6) < len) {
		    AMOS_decrypt_procedure(src);
		    locked_procs_found++;
		}
		else {
		    printf("WARNING: locked procedure with bad length found\n");
		}
	    }
	    /* always remove "is locked" flag */
	    if (src[10] & 0x40) {
		src[10] ^= 0x040;
		locked_procs_found++;
	    }
	}
    }
    return locked_procs_found;
}

int main(int argc, char *argv[]) {
    char *fname;
    unsigned char *buf;
    size_t len;

    if (argc <= 1) {
	printf("Usage: %s <file(s).amos>\n", argv[0]);
	return 1;
    }

    for (argv++; (fname = *argv); argv++) {
	if ((buf = read_file(fname, &len))) {
	    if (!memcmp(buf, "AMOS Basic", 10)) {
		if (unlock_source(buf, len)) {
		    printf("%s: unlocked procedures, saving\n", fname);
		    write_file(fname, buf, len);
		}
		else {
		    printf("%s: no locked procedures\n", fname);
		}
	    }
	    else {
		printf("%s: not an AMOS Basic file\n", fname);
	    }
	    free(buf);
	}
    }
    return 0;
}