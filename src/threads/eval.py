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
tests[1] = ["alarm-negative.result", "alarm-simultaneous.result", "alarm-wait.result", "alarm-zero.result"]
tests[2] = ["priority-change.result", "priority-condvar.result", "priority-fifo.result", "priority-preempt.result", "priority-sema.result", "alarm-priority.result"]
tests[3] = ["priority-donate-chain.result", "priority-donate-lower.result", "priority-donate-multiple.result", "priority-donate-multiple2.result", "priority-donate-nest.result", "priority-donate-one.result", "priority-donate-sema.result"]
tests[4] = ["mlfqs-block.result", "mlfqs-fair.result", "mlfqs-load-1.result", "mlfqs-load-60.result", "mlfqs-load-avg.result", "mlfqs-recent-1.result"]

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
        print color_red + 'Errors encountered in compilation. Will now Stop!' + color_end
        exit()

    if 'warning' in output or 'Warning' in output:
        print color_red + 'Warnings encountered in compilation.' + color_end

def grade(output, test_name):
    test_name = test_name.rsplit('.result', 1)[0]
    ofset = ' ' * (65 - len(test_name))
    if 'pass' in output:
        print 'Test ' + test_name +  ofset + color_green + ' Passed!' + color_end
        return 1
    print 'Test ' + test_name + ofset + color_red + ' Failed!' + color_end
    return 0

# run tests
def test(tests, name):
    count = 0
    print ''
    for test_name in tests:
        run_process("rm " + path + test_name)
        output = run_process("make " + path + test_name)
        count += grade(output, test_name)
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
    for i in range(0, len(args)):
        if args[i] == 'help':
            help()
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
            test(tests[1], tests_name[1])
        elif int(args[i]) is 2:
            test(tests[2], tests_name[2])
        elif int(args[i]) is 3:
            test(tests[3], tests_name[3])
        elif int(args[i]) is 4:
            test(tests[4], tests_name[4])
    print ''

