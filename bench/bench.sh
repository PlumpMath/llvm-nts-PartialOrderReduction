#!/bin/sh
RUNNER=../run/run
bench_file()
{
	FILE_IN=$1
	THREADS=$2

	FILEPR="${FILE_IN}-seq-t${THREADS}"
	FILEOUT_POR_NTS="${FILEPR}-por.nts"
	FILEOUT_POR_LOG="${FILEPR}-por.log"
	FILEOUT_SIMPLE_NTS="${FILEPR}-simple.nts"
	FILEOUT_SIMPLE_LOG="${FILEPR}-simple.log"

	echo file: $FILE_IN
	${RUNNER} --threads $THREADS \
		      --output "$FILEOUT_POR_NTS" \
			  "$FILE_IN" \
			  | tee "$FILEOUT_POR_LOG"

	${RUNNER} --threads $THREADS \
		      --output "$FILEOUT_SIMPLE_NTS" \
			  --no-por \
			  "$FILE_IN" \
			  | tee "$FILEOUT_SIMPLE_LOG"

}

FILE="$1"
if [ -z "$FILE" ] ; then
	echo "usage: ./bench.sh file.ll"
	exit 1
fi;

bench_file "$FILE" "2"
