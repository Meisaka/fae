set -e
echo TODO pass in the built executable

function testcase() {
	set -e
	TESTSPASSED=0
	TESTCOUNT=0
	while read TREEFILE; do
		BASE_FILE="${TREEFILE%%.tree}"
		INFILE="${BASE_FILE}.ffs"
		TESTCOUNT=$(($TESTCOUNT + 1))
		echo -n $BASE_FILE
		if $1 -T $TREEFILE -p $INFILE; then
			echo ' pass'
			TESTSPASSED=$(($TESTSPASSED + 1))
		else
			echo "$BASE_FILE FAIL"
		fi
	done
	echo "done ${TESTSPASSED}/${TESTCOUNT}"
}

find tests/ -name '*.tree' | testcase ./build-n/fae

