#include <errno.h>
#include <error.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

bool dflag = false;
bool nflag = false;
bool pflag = false;

static bool is_hex(char c) {
	return ('0' <= c && c <= '9') || ('A' <= c && c <= 'F') || ('a' <= c && c <= 'f');
}

static int xtoi(char c) {
	if ('0' <= c && c <= '9') {
		return c - '0';
	} else if ('A' <= c && c <= 'F') {
		return (c - 'A') + 10;
	} else if ('a' <= c && c <= 'f') {
		return (c - 'a') + 10;
	} else {
		return -1;
	}
}

size_t decode_file(FILE *file, char *inbuf, size_t incap, size_t inlen) {
	unsigned char outbuf[incap];
	size_t outlen = 0;

	for (;;) {
		size_t rlen = fread(inbuf + inlen, sizeof(unsigned char), incap - inlen, file);
		inlen += rlen;

		bool leftover = false;
		for (size_t i = 0; i < inlen; i++) {
			if (pflag && inbuf[i] == '+') {
				outbuf[outlen++] = ' ';
			} else if (inbuf[i] != '%') {
				outbuf[outlen++] = inbuf[i];
			} else if (i + 2 >= inlen) {
				// prepend remainder to the next input buffer
				inlen -= i;
				memmove(inbuf, inbuf + i, inlen);
				leftover = true;
				break;
			} else if (!is_hex(inbuf[i + 1]) || !is_hex(inbuf[i + 2])) {
				// parse error; log and output as-is
				error(0, 0, "parse error: invalid percent encoding \"%%%c%c\"", inbuf[i + 1], inbuf[i + 2]);
				outbuf[outlen++] = inbuf[i++];
				outbuf[outlen++] = inbuf[i++];
				outbuf[outlen++] = inbuf[i++];
			} else {
				// percent-decode
				unsigned char n = 0;
				n += xtoi(inbuf[++i]) << 4;
				n += xtoi(inbuf[++i]);
				outbuf[outlen++] = n;
			}
		}
		if (!leftover) {
			inlen = 0;
		}

		size_t wlen = fwrite(outbuf, sizeof(char), outlen, stdout);
		if (wlen < outlen) {
			if (ferror(stdout)) {
				error(1, errno, "error writing to stdout");
			} else {
				error(1, 0, "error writing to stdout: stdout reached EOF");
			}
		}
		outlen = 0;

		if (rlen < incap) {
			break;
		}
	}

	return inlen;
}

int decode(char **args, size_t nargs) {
	unsigned char buf[BUFSIZ];
	size_t len = 0;

	for (size_t i = 0; i < nargs; i++) {
		FILE *file;

		char *fname = args[i];
		if (strcmp(fname, "-") == 0) {
			fname = "stdin";
			file = stdin;
		} else {
			file = fopen(args[i], "r");
			if (file == NULL) {
				error(0, errno, "error reading from \"%s\"", fname);
				continue;
			}
		}

		len = decode_file(file, buf, BUFSIZ, len);
		if (ferror(file)) {
			error(0, errno, "error reading from \"%s\"", fname);
		}

		int not_ok = fclose(file);
		if (not_ok) {
			error(0, errno, "error reading from \"%s\"", fname);
		}
	}

	if (len != 0) {
		unsigned char errbuf[len + 1];
		memcpy(errbuf, buf, len);
		errbuf[len] = '\0';
		error(0, 0, "parse error: incomplete percent encoding \"%s\"", errbuf);
		fwrite(buf, sizeof(unsigned char), len, stdout);
	}

	return 0;
}

static bool is_printable(char c) {
	return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || c == '-' || c == '.' || c == '_' || c == '~';
}

void encode_file(FILE *file) {
	unsigned char inbuf[BUFSIZ];
	char outbuf[BUFSIZ * 3];
	size_t outlen = 0;

	for (;;) {
		size_t inlen = fread(inbuf, sizeof(unsigned char), BUFSIZ, file);
		for (size_t i = 0; i < inlen; i++) {
			if (is_printable(inbuf[i]) || (nflag && inbuf[i] == '\n')) {
				outbuf[outlen++] = inbuf[i];
			} else if (pflag && inbuf[i] == ' ') {
				outbuf[outlen++] = '+';
			} else {
				const char *hexdigits = "0123456789ABCDEF";
				outbuf[outlen++] = '%';
				outbuf[outlen++] = hexdigits[inbuf[i] >> 4];
				outbuf[outlen++] = hexdigits[inbuf[i] & 0x0F];
			}
		}

		size_t wlen = fwrite(outbuf, sizeof(char), outlen, stdout);
		if (wlen < outlen) {
			if (ferror(stdout)) {
				error(1, errno, "error writing to stdout");
			} else {
				error(1, 0, "error writing to stdout: stdout reached EOF");
			}
		}
		outlen = 0;

		if (inlen < BUFSIZ) {
			break;
		}
	}
}

void encode(char **args, size_t nargs) {
	for (size_t i = 0; i < nargs; i++) {
		FILE *file;

		char *fname = args[i];
		if (strcmp(fname, "-") == 0) {
			fname = "stdin";
			file = stdin;
		} else {
			file = fopen(args[i], "r");
			if (file == NULL) {
				error(0, errno, "error reading from \"%s\"", fname);
				continue;
			}
		}

		encode_file(file);
		if (ferror(file)) {
			error(0, errno, "error reading from \"%s\"", fname);
		}

		int not_ok = fclose(file);
		if (not_ok) {
			error(0, errno, "error reading from \"%s\"", fname);
		}
	}
}

int main(int argc, char **argv) {
	int opt;
	while ((opt = getopt(argc, argv, "dnph")) != -1) {
		switch (opt) {
		case 'd':
			dflag = true;
			break;
		case 'n':
			nflag = true;
			break;
		case 'p':
			pflag = true;
			break;
		case 'h':
			printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
			printf("Percent-encode (or decode) FILEs\n");
			printf("\n");
			printf("With no FILE, or when FILE is -, encode STDIN.\n");
			printf("\n");
			printf("\t-d\tDecode\n");
			printf("\t-n\tpreserve Newlines\n");
			printf("\t-p\tPlus-encode spaces\n");
			printf("\t-h\tprint this Help and exit\n");
			exit(EXIT_SUCCESS);
			break;
		default: /* '?' */
			fprintf(stderr, "Usage: %s [OPTION]... [FILE]...\n", argv[0]);
			fprintf(stderr, "try %s -h for more help\n", argv[0]);
			exit(EXIT_FAILURE);
			break;
		}
	}

	size_t nargs = argc - optind;
	char **args;
	if (nargs < 1) {
		nargs = 1;
		args = (char *[]){"-"};
	} else {
		args = argv + optind;
	}

	if (dflag) {
		decode(args, nargs);
	} else {
		encode(args, nargs);
	}

	return 0;
}
