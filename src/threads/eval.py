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

# tests
tests_name = {}
tests_name[1] = "Alarm"
tests_name[2] = "Priority Scheduling"
tests_name[3] = "Priority Donation"
tests_name[4] = "MLFQS Scheduler"

tests = {}
tests[1] = ["alarm-negative", "alarm-simultaneous", "alarm-zero"] # "alarm-wait"
tests[2] = ["priority-change", "priority-condvar", "priority-fifo", "priority-preempt", "priority-sema", "alarm-priority"]
tests[3] = ["priority-donate-chain", "priority-donate-lower", "priority-donate-multiple", "priority-donate-multiple2", "priority-donate-nest", "priority-donate-one", "priority-donate-sema"]
tests[4] = ["mlfqs-block", "mlfqs-fair-2", "mlfqs-fair-20", "mlfqs-nice-10", "mlfqs-nice-2", "mlfqs-load-1", "mlfqs-load-60", "mlfqs-load-avg", "mlfqs-recent-1"]

# path to tests
path = 'build/tests/threads/'

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
    if 'pass' in output:
        print 'Test ' + test_name +  ofset + color_green + ' Passed!' + color_end
        return 1
    print 'Test ' + test_name + ofset + color_red + ' Failed!' + color_end

    if debug is True:
        print output
        exit()

    return 0

# run tests
def test(tests, name, debug):
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
    print 'Eval script for Pintos Help'
    print '_' * 80
    print 'help -> displays this message'
    print 'all -> make clean & make all'
    print 'clean -> make clean'
    print 'make -> make all'
    
    for index in xrange(1, 5):
        print str(index) + ' -> run all ' + tests_name[index] + ' tests'
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
        if args[i] == 'help':
            help()
            continue

        if args[i] == 'd' or args[i] == 'v' or args[i] == 'verbose' or args[i] == 'debug':
            debug = True
            continue

        if args[i] == 'make':
            make()
            continue

        if args[i] == 'all':
            clean()
            make()
            continue

        if args[i] == 'clean':
            clean()
            continue

        if not is_number(args[i]):
            continue

        if int(args[i]) is 1:
            test(tests[1], tests_name[1], debug)
        elif int(args[i]) is 2:
            test(tests[2], tests_name[2], debug)
        elif int(args[i]) is 3:
            test(tests[3], tests_name[3], debug)
        elif int(args[i]) is 4:
            test(tests[4], tests_name[4], debug)
    print ''

