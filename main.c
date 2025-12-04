#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define arrlen(arr) (sizeof(arr) / sizeof(*(arr)))

#define Void void
typedef bool Bool;
typedef unsigned char Byte;
typedef ssize_t Int;

#define maxByte ((Byte)~0)

typedef struct {
	Byte *data;
	Int len;
} String;

Bool parseHex(String str, Int *num) {
	*num = 0;
	for (Int i = 0; i < str.len; i++) {
		*num <<= 4;
		Byte c = str.data[i];
		if ('0' <= c && c <= '9') {
			*num |= c - '0';
		} else if ('A' <= c && c <= 'F') {
			*num |= (c - 'A') + 10;
		} else if ('a' <= c && c <= 'f') {
			*num |= (c - 'a') + 10;
		} else {
			return false;
		}
	}
	return true;
}

Byte hexBuf[2] = {0};

// WARNING: DO NOT COPY-PASTE; the String returned will last only until the next call to toHex()
String toHex(Byte c) {
	const Byte digits[0xF + 1] = "0123456789ABCDEF";
	// low digit
	memset(hexBuf, 0, sizeof(hexBuf));
	hexBuf[1] = digits[c & 0xF];
	c >>= 4;
	// high digit
	hexBuf[0] = digits[c & 0xF];
	return (String){.data = hexBuf, .len = 2};
}

typedef struct {
	FILE *f;
	Byte buf[BUFSIZ];
	Int len; // amount of data loaded; reset when buf is loaded
	Int pos; // cursor position; increases when reading from buf, reset when buf is loaded
	Bool ok; // ok to continue reading
	Bool eof; // reached EOF
} FileBuffer;

FileBuffer newFileBuffer(FILE *f) {
	FileBuffer fb = {0};
	fb.f = f;
	fb.ok = true;
	return fb;
}

Bool load(FileBuffer *fb) {
	if (!fb->ok) {
		return false;
	} else if (fb->eof) {
		fb->ok = false;
		return false;
	}

	// reset currently loaded data
	memmove(fb->buf, fb->buf + fb->pos, fb->len - fb->pos);
	fb->len -= fb->pos;
	fb->pos = 0;

	// load more data
	Int len = sizeof(fb->buf) - fb->len;
	Int n = fread(fb->buf + fb->pos, 1, len, fb->f);
	fb->len += n;

	if (n < len || feof(fb->f)) {
		fb->eof = true;
	}

	if (ferror(fb->f)) {
		fb->ok = false;
	}

	return true;
}

// WARNING: DO NOT COPY-PASTE; the String returned will last only until the next call to load()
// NOTE: this function will only return a maximum string length of sizeof(buf->buf);
// this is sufficient for the purposes of this program
String readString(FileBuffer *fb, Int n) {
	if (fb->len - fb->pos < n) {
		load(fb);
	}

	// len is minimum of n or unread portion of fb->buf
	Int len = n < fb->len - fb->pos ? n : fb->len - fb->pos;
	String str = (String){.data = fb->buf + fb->pos, .len = len};
	fb->pos += len;
	if (fb->pos >= fb->len) {
		fb->ok = false;
	}

	return str;
}

Byte readByte(FileBuffer *fb) {
	if (fb->len - fb->pos < 1) {
		Bool ok = load(fb);
		if (!ok) {
			// no byte loaded into fb->buf
			return 0;
		}
	}

	Byte c = fb->buf[fb->pos++];
	if (fb->pos >= fb->len) {
		fb->ok = false;
	}

	return c;
}

Bool encodeFile(FILE *f, Bool preserveNewlines, Bool plusEncode) {
	FileBuffer fb = newFileBuffer(f);
	while (fb.ok) {
		Byte c = readByte(&fb);
		if (isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~' || (preserveNewlines && c == '\n')) {
			// printable char
			Int ret = putc(c, stdout);
			if (ret == EOF) {
				return false;
			}

		} else if (plusEncode && c == ' ') {
			// plus-encoded space
			Int ret = putc('+', stdout);
			if (ret == EOF) {
				return false;
			}

		} else {
			// percent-encoded char
			Int ret = putc('%', stdout);
			if (ret == EOF) {
				return false;
			}

			String str = toHex(c);
			Int n = fwrite(str.data, 1, str.len, stdout);
			if (n < str.len) {
				return false;
			}
		}
	}

	return !ferror(f);
}

Bool decodeFile(FILE *f, Bool plusEncode) {
	FileBuffer fb = newFileBuffer(f);
	while (fb.ok) {
		Byte c = readByte(&fb);
		if (plusEncode && c == '+') {
			// plus-encoded space
			Int ret = putc(' ', stdout);
			if (ret == EOF) {
				return false;
			}

		} else if (c != '%') {
			// printable char
			Int ret = putc(c, stdout);
			if (ret == EOF) {
				return false;
			}

		} else {
			// percent-encoded char
			String str = readString(&fb, 2);
			if (str.len < 2) {
				return false;
			}

			Int num;
			Bool ok = parseHex(str, &num);
			if (!ok || num > maxByte) {
				return false;
			}

			Int ret = putc((Byte)num, stdout);
			if (ret == EOF) {
				return false;
			}
		}
	}

	return !ferror(f);
}

#define usage "Usage: %s [OPTION]... [FILE]...\n"

#define help \
	"Percent-encode (or decode) FILEs\n" \
	"\n" \
	"With no FILE, or when FILE is -, encode STDIN.\n" \
	"\n" \
	"\t-d\tDecode\n" \
	"\t-n\tpreserve Newlines\n" \
	"\t-p\tPlus-encode spaces\n" \
	"\t-h\tprint this Help and exit\n" \

#define errmsg(...) do { \
		fprintf(stderr, "%s: ", argv[0]); \
		fprintf(stderr, __VA_ARGS__); \
		if (errno) { \
			fprintf(stderr, ": %s\n", strerror(errno)); \
		} else { \
			fprintf(stderr, "\n"); \
		} \
	} while (0);

int main(int argc, char **argv) {
	Int opt;
	Bool doDecode = false;
	Bool preserveNewlines = false;
	Bool plusEncode = false;
	while ((opt = getopt(argc, argv, "dnph")) != -1) {
		switch (opt) {
		case 'd':
			doDecode = true;
			break;
		case 'n':
			preserveNewlines = true;
			break;
		case 'p':
			plusEncode = true;
			break;
		case 'h':
			printf(usage, argv[0]);
			printf(help);
			return 0;
		default:
			fprintf(stderr, usage, argv[0]);
			fprintf(stderr, "try %s -h for help\n", argv[0]);
			return 1;
		}
	}

	Byte *altArgv[] = {argv[0], "-"};
	if (argc - optind < 1) {
		argv = (char **)altArgv;
		argc = arrlen(altArgv);
		optind = 1;
	}

	int ret = 0;
	for (Int i = optind; i < argc; i++) {
		Byte *arg = argv[i];
		FILE *f;
		if (strcmp(arg, "-") == 0) {
			f = stdin;
		} else {
			f = fopen(arg, "r");
			if (f == NULL) {
				ret = 1;
				errmsg("error opening \"%s\"", arg);
				continue;
			}
		}

		if (doDecode) {
			Bool ok = decodeFile(f, plusEncode);
			if (!ok) {
				ret = 1;
				errmsg("error decoding \"%s\"", f == stdin ? "standard input" : (char *)arg);
			}
		} else {
			Bool ok = encodeFile(f, preserveNewlines, plusEncode);
			if (!ok) {
				ret = 1;
				errmsg("error encoding \"%s\"", f == stdin ? "standard input" : (char *)arg);
			}
		}

		if (f != stdin) {
			Bool ok = !fclose(f);
			if (!ok) {
				ret = 1;
				errmsg("error closing \"%s\"", arg);
			}
		}
	}

	return ret;
}
