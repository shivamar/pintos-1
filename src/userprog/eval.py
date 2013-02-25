#!/bin/bash
import os, optparse, sys, subprocess

# some nice colors
color_red = "\033[31m"
color_green = "\033[89m"
color_end = "\033[0m"
color_blue = "\033[94m"
color_white = "\033[97m"

# parse arguments
cmdline = optparse.OptionParser(add_help_option=False)
(opts, args) = cmdline.parse_args(sys.argv[1:])

# tests name
tests_name = {}
tests_name[1] = "Arguments"
tests_name[2] = "Breaking"
tests_name[3] = "Process Wait"
tests_name[4] = "Process Exec"
tests_name[5] = "File handling"
tests_name[6] = "SYS Read & Write"
tests_name[7] = "Other SYS Calls"
tests_name[8] = "File System"

# actual tests
tests = {}
tests[1] = ["args-dbl-space", "args-many", "args-multiple", "args-none", "args-single"] 
tests[2] = ["bad-jump", "bad-jump2", "bad-read", "bad-read2", "bad-write", "bad-write2"]
tests[3] = ["wait-bad-pid", "wait-killed", "wait-simple", "wait-twice"]
tests[4] = ["exec-arg", "exec-bad-ptr", "exec-missing", "exec-multiple", "exec-once", "no-vm/multi-oom"]
tests[5] = ["create-bad-ptr", "create-empty", "create-long", "create-null", "create-bound", "create-exists", "create-normal",
    "open-bad-ptr", "open-empty", "open-normal", "open-twice", "open-boundary", "open-missing", "open-null",
    "close-bad-fd", "close-normal", "close-stdin", "close-stdout", "close-twice",
    "rox-child", "rox-multichild", "rox-simple"]
tests[6] = ["read-bad-fd", "read-bad-ptr", "read-boundary", "read-normal", "read-stdout", "read-zero",
    "write-bad-fd", "write-bad-ptr", "write-boundary", "write-normal", "write-stdin", "write-zero"]
tests[7] = ["sc-bad-arg", "sc-bad-sp", "sc-boundary-2", "sc-boundary", "multi-recurse", "multi-child-fd", "halt", "exit"]
tests[8] = ["lg-create", "lg-seq-random", "sm-seq-block", "syn-write", "lg-full", "sm-create", "sm-seq-random",
    "lg-random", "sm-full", "syn-read", "lg-seq-block", "sm-random", "syn-remove"]

# path to tests
paths = {}
for x in xrange(1, 8):
    paths[x] = 'build/tests/userprog/'
paths[8] = 'build/tests/filesys/base/'

class stats:
    total = ok = 0

def run_process(name):
    return subprocess.Popen([name], shell=True, stdout = subprocess.PIPE, stderr = subprocess.STDOUT).communicate()[0]

def clean():
    print ''
    print "Deleting previous build..."
    output = run_process("make clean")

def make():
    print "Compiling pintos..."
    output = run_process("make all")

    if 'error' in output or 'Error' in output:
        print output
        print color_red + 'Errors encountered in compilation. Will now Stop!' + color_end
        exit()

    if 'warning' in output or 'Warning' in output:
        print output
        print color_red + 'Warnings encountered in compilation.' + color_end

def grade(output, test_name, debug):
    ofset = ' ' * (65 - len(test_name))
    stats.total = stats.total + 1
    if 'pass' in output:
        print 'Test ' + test_name +  ofset + color_green + ' Passed!' + color_end
        stats.ok = stats.ok + 1
        return 1
    print 'Test ' + test_name + ofset + color_red + ' Failed!' + color_end

    if debug is True:
        print output
        exit()

    return 0

# run tests
def test(path, tests, name, debug):
    count = 0
    print ''
    for test_name in tests:
        run_process("rm " + path + test_name + ".result")
        output = run_process("make " + path + test_name + ".result")
        count += grade(output, test_name, debug)
    print '_' * 80
    if count == len(tests):
        print color_white + 'Passed ALL ' + name + ' Tests' + color_end
    else:
        print color_white + 'Passed ' + str(count) + ' out of ' + str(len(tests)) + ' ' + name + ' Tests' + color_end

def help():
    print '_' * 80
    print 'Help Menu -- Pintos Grader'
    print '_' * 80
    print 'help -> displays this message'
    print 'all -> make clean & make all'
    print 'clean -> make clean'
    print 'make -> make all'
    
    print ''
    print "Test cases covered"
    for index in xrange(1, len(tests) + 1):
        print "\t" + str(index) + ' -> run all ' + tests_name[index] + ' tests'
        index += 1     
    print ''
    print 'example usage: "eval.py all 1 2"'

def is_number(s):
    try:
        int(s)
        return True
    except ValueError:
        return False

if __name__ == '__main__':
    debug = False

    for i in range(0, len(args)):
        case = int(args[i]) if is_number(args[i]) else -1
 
        if args[i] == 'd' or args[i] == 'v' or args[i] == 'verbose' or args[i] == 'debug':
            debug = True
            continue

        elif args[i] == 'make':
            make()
            continue

        elif args[i] == 'all':
            clean()
            make()
            continue

        elif args[i] == 'clean':
            clean()
            continue

        elif case >= 1 and case <= len(tests):
            test(paths[case], tests[case], tests_name[case], debug)

        else:
            help()
            continue

    print ''
    print '_' * 80
    if stats.total is not 0:
        if stats.total == stats.ok:
            print color_white + 'Passed ALL Tests!' + color_end
        else:
            print color_white + 'Passed ' + str(stats.ok) + ' out of ' + str(stats.total) + ' Tests!' 

