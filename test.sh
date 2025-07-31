#!/bin/sh
set -efu

if ! [ -x penc ]; then
	echo 'penc executable not found' >&2
	exit 1
fi

alias print='printf "%s"'

touch file.txt
trap 'rm file.txt' EXIT

tests=''

tests="$tests test_encode"
test_encode() {
	local actual="$(echo 'hello world' | ./penc)"
	local expected='hello%20world%0A'
	[ "x$actual" = "x$expected" ]
	return $?
}

tests="$tests test_encode_utf8"
test_encode_utf8() {
	local actual="$(echo '你好，世界' | ./penc)"
	local expected='%E4%BD%A0%E5%A5%BD%EF%BC%8C%E4%B8%96%E7%95%8C%0A'
	[ "x$actual" = "x$expected" ]
	return $?
}

tests="$tests test_encode_file"
test_encode_file() {
	echo 'hello world' > file.txt
	local actual="$(./penc file.txt)"
	local expected='hello%20world%0A'
	[ "x$actual" = "x$expected" ]
	return $?
}

tests="$tests test_encode_plus"
test_encode_plus() {
	local actual="$(echo 'hello world' | ./penc -p)"
	local expected='hello+world%0A'
	[ "x$actual" = "x$expected" ]
	return $?
}

tests="$tests test_encode_newline"
test_encode_newline() {
	local actual="$(echo 'hello world' | ./penc -n)"
	local expected='hello%20world'
	[ "x$actual" = "x$expected" ]
	return $?
}

tests="$tests test_decode"
test_decode() {
	local actual="$(echo 'hello%20world' | ./penc -d)"
	local expected='hello world'
	[ "x$actual" = "x$expected" ]
	return $?
}

tests="$tests test_decode_utf8"
test_decode_utf8() {
	local actual="$(echo '%E4%BD%A0%E5%A5%BD%EF%BC%8C%E4%B8%96%E7%95%8C' | ./penc -d)"
	local expected='你好，世界'
	[ "x$actual" = "x$expected" ]
	return $?
}

tests="$tests test_decode_file"
test_decode_file() {
	echo 'hello%20world' > file.txt
	local actual="$(./penc -d file.txt)"
	local expected='hello world'
	[ "x$actual" = "x$expected" ]
	return $?
}

tests="$tests test_decode_plus"
test_decode_plus() {
	local actual="$(echo 'hello+world' | ./penc -dp)"
	local expected='hello world'
	[ "x$actual" = "x$expected" ]
	return $?
}

tests="$tests test_decode_newline"
test_decode_newline() {
	local actual="$(echo 'hello%20world' | ./penc -dn)"
	local expected='hello world'
	[ "x$actual" = "x$expected" ]
	return $?
}

n=0
fails=0
for t in $tests; do
	: $((n += 1))
	if ! eval "$t"; then
		echo "$t failed"
		: $((fails += 1))
	fi
done
if [ "$fails" -eq 0 ]; then
	echo "all $n tests passed"
else
	echo "$fails/$n tests failed"
fi
