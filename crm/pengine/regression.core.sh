#!/bin/bash

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

verbose=$1
io_dir=testcases
diff_opts="--ignore-all-space -1 -u"
failed=.regression.failed
# zero out the error log
> $failed

function do_test {

    base=$1;
    name=$2;
    input=$io_dir/${base}.xml
    output=$io_dir/${base}.out
    expected=$io_dir/${base}.exp

    if [ ! -f $input ]; then
	echo "Test $name	($base)...	Error ($input)";
	return;
    fi

    if [ "$create_mode" != "true" -a ! -f $expected ]; then
	echo "Test $name	($base)...	Error ($expected)";
#	return;
    fi

    ./ptest < $input 2>/dev/null 2>/dev/null > $output

    if [ ! -s $output ]; then
	echo "Test $name	($base)...	Error ($output)";
	rm $output
	return;
    fi

    ./fix_xml.pl $output

    if [ ! -s $output ]; then
	echo "Test $name	($base)...	Error (fixed $output)";
	rm $output
	return;
    fi

    if [ "$create_mode" = "true" -a ! -f $expected ]; then
	cp "$output" "$expected"
    fi

    if [ -f $expected ]; then
	diff $diff_opts -q $expected $output >/dev/null
	rc=$?
    fi

    if [ "$create_mode" = "true" ]; then
	echo "Test $name	($base)...	Created expected output" 
    elif [ ! -f $expected ]; then
	echo "==== Raw results for test ($base) ====" >> $failed
	cat $output 2>/dev/null >> $failed
    elif [ "$rc" = 0 ]; then
	echo "Test $name	($base)...	Passed";
    elif [ "$rc" = 1 ]; then
	echo "Test $name	($base)...	* Failed";
	diff $diff_opts $expected $output 2>/dev/null >> $failed
    else 
	echo "Test $name	($base)...	Error (diff: $rc)";
	echo "==== Raw results for test ($base) ====" >> $failed
	cat $output 2>/dev/null >> $failed
    fi
    
    rm $output
}

function test_results {

    if [ -s $failed ]; then
	if [ "$verbose" = "-v" ]; then
	    echo "Results of failed tests...."
	    less $failed
	else
	    echo "Results of failed tests are in $failed...."
	    echo "Use $0 -v to display them automatically."
	fi
    else
	rm $failed
    fi
}

