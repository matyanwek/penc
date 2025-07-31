# Penc

Percent-encode (or -decode) input.

## Installation

```sh
$ make
$ sudo make install
```

## Usage

```sh
# encode 'hello world', including the trailing newline
$ echo 'hello world' | penc

# encode 'hello world', ignoring the trailing newline
$ echo 'hello world' | penc -n

# encode 'hello world' with plus-encoded space
$ echo 'hello world' | penc -p

# decode the contents of file.txt
$ penc -d file.txt
```
