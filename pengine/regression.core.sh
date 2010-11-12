 # Copyright (C) 2004 Andrew Beekhof <andrew@beekhof.net>
 # 
 # This program is free software; you can redistribute it and/or
 # modify it under the terms of the GNU General Public
 # License as published by the Free Software Foundation; either
 # version 2.1 of the License, or (at your option) any later version.
 # 
 # This software is distributed in the hope that it will be useful,
 # but WITHOUT ANY WARRANTY; without even the implied warranty of
 # MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 # General Public License for more details.
 # 
 # You should have received a copy of the GNU General Public
 # License along with this library; if not, write to the Free Software
 # Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 #

verbose=0
num_failed=0
num_tests=0
force_local=0
VALGRIND_CMD=""
diff_opts="--ignore-all-space -u -N"

test_home=`dirname $0`
test_name=`basename $0`

function info() {
    printf "$*\n"
}

function error() {
    printf "      * ERROR:   $*\n"
}

function failed() {
    printf "      * FAILED:  $*\n"
}

function show_test() {
    name=$1; shift
    printf "  Test %-25s $*\n" "$name:"
}

info "Test home is:\t$test_home"
test_binary=$test_home/ptest

failed=$test_home/.regression.failed.diff

# zero out the error log
> $failed

while true ; do
    case "$1" in
	-V|--verbose) verbose=1; shift;;
	-v|--valgrind) 
	    export G_SLICE=always-malloc
	    VALGRIND_CMD="valgrind -q --log-file=%q{valgrind_output} --show-reachable=no --leak-check=full --trace-children=no --time-stamp=yes --num-callers=20 --suppressions=$test_home/ptest.supp"
	    test_binary=`which ptest`
	    shift;;
	--valgrind-dhat) 
	    VALGRIND_CMD="valgrind --log-file=%q{valgrind_output} --show-top-n=100 --num-callers=4 --time-stamp=yes --trace-children=no --tool=exp-dhat --suppressions=$test_home/ptest.supp"
	    test_binary=`which ptest`
	    shift;;
	--valgrind-skip-output)
	    VALGRIND_SKIP_OUTPUT=1
	    shift;;
	-b|--binary) test_binary=$2; shift; shift;;
	-?|--help) echo "$0 [--binary name] [--force-local]"; shift; exit 0;;
	--) shift ; break ;;
	"") break;;
	*) echo "unknown option: $1"; exit 1;;
    esac
done

if [ ! -x $test_binary ]; then
    test_binary=`which ptest`
fi

if [ "x$test_binary" = "x" ]; then
    info "ptest not installed. Aborting."
    exit 1
fi

info "Test binary is:\t$test_binary"
if [ "x$VALGRIND_CMD" != "x" ]; then
    info "Activating memory testing with valgrind";
fi

info " "

test_cmd="$VALGRIND_CMD $test_binary"
#echo $test_cmd

if [ `whoami` != root ]; then
    declare -x CIB_shadow_dir=/tmp
fi

function do_test {

    did_fail=0
    expected_rc=0
    num_tests=`expr $num_tests + 1`

    base=$1; shift
    name=$1; shift

    input=$io_dir/${base}.xml
    output=$io_dir/${base}.out
    expected=$io_dir/${base}.exp

    dot_png=$io_dir/${base}.png
    dot_expected=$io_dir/${base}.dot
    dot_output=$io_dir/${base}.pe.dot

    scores=$io_dir/${base}.scores
    score_output=$io_dir/${base}.scores.pe
    valgrind_output=$io_dir/${base}.valgrind
    export valgrind_output
    stderr_output=$io_dir/${base}.stderr

    if [ "x$1" = "x--rc" ]; then
	expected_rc=$2
	shift; shift;
    fi

    show_test "$base" "$name"

    if [ ! -f $input ]; then
	error "No input";
	did_fail=1
	num_failed=`expr $num_failed + $did_fail`
	return;
    fi

    if [ "$create_mode" != "true" -a ! -f $expected ]; then
	error "no stored output";
#	return;
    fi

#    ../admin/crm_verify -X $input
    CIB_shadow_dir=$io_dir $test_cmd -x $input -D $dot_output -G $output -S -s $* 2> $stderr_output > $score_output
    rc=$?
    if [ $rc != $expected_rc ]; then
	failed "Test returned: $rc";
	did_fail=1
	echo "CIB_shadow_dir=$io_dir $test_cmd -x $input -D $dot_output -G $output -S -s $*"
    fi

    if [ -z "$VALGRIND_SKIP_OUTPUT" ]; then
	if [ -s ${valgrind_output} ]; then
	    error "Valgrind reported errors";
	    did_fail=1
	    cat ${valgrind_output}
	fi
	rm -f ${valgrind_output}
    fi

    if [ -s core ]; then
	error "Core-file detected: core.${base}";
	did_fail=1
	rm -f $test_home/core.$base
	mv core $test_home/core.$base
    fi

    if [ -s $stderr_output ]; then
	error "Output was written to stderr"
	did_fail=1
	cat $stderr_output
    fi
    rm -f $stderr_output

    if [ ! -s $output ]; then
	error "No graph produced";
	did_fail=1
	num_failed=`expr $num_failed + $did_fail`
	rm -f $output
	return;
#    else
#	mv $output $output.sed
#	cat $output.sed | sed 's/id=.[0-9]*.\ //g' >> $output
    fi

    if [ ! -s $dot_output ]; then
	error "No dot-file summary produced";
	did_fail=1
	num_failed=`expr $num_failed + $did_fail`
	rm -f $output
	return;
    else
	echo "digraph \"g\" {" > $dot_output.sort
	LC_ALL=POSIX sort -u $dot_output | grep -v -e ^}$ -e digraph >> $dot_output.sort
	echo "}" >> $dot_output.sort
	mv -f $dot_output.sort $dot_output
    fi

    if [ ! -s $score_output ]; then
	error "No allocation scores produced";
	did_fail=1
	num_failed=`expr $num_failed + $did_fail`
	rm $output
	return;
    else
	#LC_ALL=POSIX sort $score_output > $score_output.sorted
	#mv -f $score_output.sorted $score_output
	touch $score_output
    fi

    if [ "$create_mode" = "true" ]; then
	cp "$output" "$expected"
	cp "$dot_output" "$dot_expected"
	cp "$score_output" "$scores"
	info "	Created expected outputs" 
    fi

    diff $diff_opts $dot_expected $dot_output >/dev/null
    rc=$?
    if [ $rc != 0 ]; then
	failed "dot-file summary changed";
	diff $diff_opts $dot_expected $dot_output 2>/dev/null >> $failed
	echo "" >> $failed
	did_fail=1
    else 
	rm -f $dot_output
    fi

    diff $diff_opts $expected $output >/dev/null
    rc2=$?
    if [ $rc2 != 0 ]; then
	failed "xml-file changed";
	diff $diff_opts $expected $output 2>/dev/null >> $failed
	echo "" >> $failed
	did_fail=1
    fi
    
    diff $diff_opts $scores $score_output >/dev/null
    rc=$?
    if [ $rc != 0 ]; then
	failed "scores-file changed";
	diff $diff_opts $scores $score_output 2>/dev/null >> $failed
	echo "" >> $failed
	did_fail=1
    fi
    rm -f $output  $score_output
    num_failed=`expr $num_failed + $did_fail`
}

function test_results {
    if [ $num_failed != 0 ]; then
	if [ -s $failed ]; then
	    if [ "$verbose" = "1" ]; then
		error "Results of $num_failed failed tests (out of $num_tests)...."
		less $failed
	    else
		error "Results of $num_failed failed tests (out of $num_tests) are in $failed...."
		error "Use $0 -V to display them automatically."
	    fi
	else
	    error "$num_failed (of $num_tests) tests failed (no diff results)"
	    rm $failed
	fi
    fi
    exit $num_failed
}

