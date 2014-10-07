#!/usr/bin/python

# Copyright (c) 2014 by Enea Software AB
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of Enea Software AB nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Rules for adding new test cases:
# Test cases that has a prefix "PART" should be executed on a partitioned
# environment. Test cases that requires a "clean" environment should have
# prefix "NOPART".

import os
import sys
import subprocess
import StringIO
import getopt
import multiprocessing
import time
import re
import signal

# Test constants
PROC_LAST_CPU_ELEM = 38
PROC_RT_PRIO = 39
PROC_SCHED_POLICY = 40
SCHED_NORMAL = 0
SCHED_FIFO = 1
DEFERMENT_TICK_DISABLED = 4294967295
SUCCESS = 0
FAIL = 1
SUPPORTED_TARGETS = [
    "keystone-evm",
    "chiefriver",
    "p2041rdb",
    "crystalforest-server",
    "romley-ivb"
    ]

class targetOptions:
    def __init__(self, target):
        self.rt_mask = {
            "default" : (2 ** (multiprocessing.cpu_count() - 1)),
            "keystone-evm" : 0xe,
            "chiefriver" : 0xe,
            "p2041rdb" : 0xe,
            "crystalforest-server" : 0xaaaa, # NUMA node 1
            "romley-ivb" : 0x820 # CPU 5 and CPU 11 (same core id in cpuinfo)
            }[target]

        # None, or node nr
        self.numa = {
            "default" : None,
            "keystone-evm" : None,
            "chiefriver" :None,
            "p2041rdb" : None,
            "crystalforest-server" : 1,
            "romley-ivb" : None
            }[target]

# Test globals
global verbose
global options
global ref_count_irqs

def print_msg(msg):
    global verbose
    if verbose == True:
        print msg


################################################################################
#                                Helper functions
################################################################################

# Run shell command. Will tell the world about it if verbose is true
def run(cmd, func=None):
    print_msg("Executing: '" + cmd + "'")
    return subprocess.Popen(cmd, shell=True, preexec_fn=func, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


# Convert Linux CPU list (i.e: 3,4,5-7) to bitmask. Return -1 on failure
def liststr2mask(liststr):
    try:
        valid_array = True
        mask = 0

        for elem in liststr.split(','):
            range_str = elem.split('-')
            if len(range_str) > 1:
                for idx in range(int(range_str[0]),int(range_str[1]) + 1):
                    if idx >= 0:
                        mask |= (1 << idx)
                    else:
                        print_msg("liststr2mask: Illegal range:" + range_str)
                        valid_array = False
                        break
            else:
                idx = int(range_str[0])
                if idx >= 0:
                    mask |= (1 << idx)
                else:
                    print_msg("liststr2mask: Illegal index:" + str(idx))
                    valid_array = False
                    break
        if valid_array == True:
            return mask
        else:
            return -1
    except:
        print_msg ("liststr2mask: Failed because of exception: " +
                   str(sys.exc_info()[1]))
        return -1

# Function that returns cpuset_dir and cpuset prefix
def get_cpusets():
    if os.path.isdir("/sys/fs/cgroup/cpuset/"):
        cpuset_dir = "/sys/fs/cgroup/cpuset/"
        cpuset_prefix = "cpuset."
        return cpuset_dir, cpuset_prefix
    else:
        print_msg("get_cpusets: Kernel is lacking support for cpuset")
        return "", ""

# Check if bad parameter is detected
def bad_parameter(cmd, tc_name):
    try:
        p = run(cmd)
        (stdout, stderr) = p.communicate()
        if p.returncode == 0:
            print_msg(tc_name + ": Failed: " + cmd +
                      ": incorrectly returned normal")
            return FAIL
        else:
            return SUCCESS
    except:
        print_msg ("bad_parameter: Failed because of exception: " +
                   str(sys.exc_info()[1]))
        return FAIL

# Get process affinity. Returns task_name, CPU mask and last_cpu
def get_task_info(pid):
    try:
        task_name = ""
        affinity = -1
        last_cpu = -1
        policy = -1
        prio = -1

        with open("/proc/" + str(pid) + "/status") as f:
            for line in f.readlines():
                if "Name:" in line:
                    task_name = line.split()[1]
                if "Cpus_allowed:" in line:
                    affinity = int(line.split()[1], base=16)

        # Check on what CPU the process was executed most recently
        with open("/proc/" + str(pid) + "/stat") as f:
            elements =  f.readline().split()
            last_cpu = int(elements[PROC_LAST_CPU_ELEM])
            policy = int(elements[PROC_SCHED_POLICY])
            prio = int(elements[PROC_RT_PRIO])

            print_msg("get_task_info(" + str(pid) + "): Task name: " + task_name + ", PID: " + str(pid) + ", affinity: " + str(affinity) + ", last CPU: " + str(last_cpu) + ", policy: " + str(policy) + ", prio: " + str(prio))

        return task_name, affinity, last_cpu, policy, prio

    except:
        print_msg ("get_task_info: Failed because of exception: " +
                   str(sys.exc_info()[1]))
        return "", -1, -1, -1, -1

# Read the content of a comma separated file and return the result as an
# array of integers.
def read_cpumask(file_str):
    try:
        if os.path.isfile(file_str):
            with open(file_str) as f:
                content = f.readline();
                val = content.rstrip().split(',');
                val = [int(x, base=16) for x in val];
                return val

    except:
        print_msg ("read_cpumask: Failed because of exception: " +
                   str(sys.exc_info()[1]))
        return 0

# Check if file has expected value. If not return FAIL. If file does not exist
# or if file has expected value, return SUCCESS
# This file is expected to have a value on the format 00000000,00000fff
# The parameter expected_val is an array whith the least significant numbers in
def check_file_cpumask(file_str, expected_val):
    try:
        val = read_cpumask(file_str)
        lendiff = len(val) - len(expected_val);
        expected_val = ([0] * lendiff) + expected_val; # Pad with zero
        if val != expected_val:
            print_msg("check_file Failed: " + file_str +
                      " has value: " +
                      str(','.join([str(x) for x in val])) + " Expected: " +
                      str(','.join([str(x) for x in expected_val])))
            return FAIL

        return SUCCESS

    except:
        print_msg ("check_file: Failed because of exception: " +
                   str(sys.exc_info()[1]))
        return FAIL

# Check if file has expected value. If not return FAIL. If file does not exist
# or if file has expected value, return SUCCESS
def check_file(file_str, expected_val, val_base):
    try:
        if os.path.isfile(file_str):
            with open(file_str) as f:
                val = int(f.readline(), base=val_base)
                if val != expected_val:
                    print_msg("check_file Failed: " + file_str +
                              " has value: " + str(val) + " Expected: " +
                              str(expected_val))
                    return FAIL

        return SUCCESS

    except:
        print_msg ("check_file: Failed because of exception: " +
                   str(sys.exc_info()[1]))
        return FAIL

# Check that the undo sub-command does what it should. Takes expected
# environment as input parameters
def check_env(sched_rt_runtime_us, sched_tick_max_deferment, stat_interval,
                  numa_affinity, watchdog, cpumask, check_interval):
    try:

        # Check sched_rt_runtime_us cleanup
        if (check_file("/proc/sys/kernel/sched_rt_runtime_us",
                       sched_rt_runtime_us, 10) == FAIL):
            return FAIL

        # Check sched_tick_max_deferment cleanup
        # Check only if kernel configured for NO_HZ_FULL with patched tick
        # deferment
        if (check_file("/sys/kernel/debug/sched_tick_max_deferment",
                       sched_tick_max_deferment, 10) == FAIL):
            return FAIL

        # Check vmstat_interval cleanup
        if (check_file("/proc/sys/vm/stat_interval",
                       stat_interval, 10) == FAIL):
            return FAIL

        # Get numa_affinity
        if (check_file("/sys/bus/workqueue/devices/writeback/numa",
                       numa_affinity, 10) == FAIL):
            return FAIL

        # Check watchdog cleanup
        if (check_file("/proc/sys/kernel/watchdog",
                       watchdog, 10) == FAIL):
            return FAIL

        # Check BWQ cleanup
        if (check_file_cpumask("/sys/bus/workqueue/devices/writeback/cpumask",
                               [cpumask]) == FAIL):
            return FAIL

        # Check machine check cleanup (only CPU 0)
        if (check_file(
                "/sys/devices/system/machinecheck/machinecheck0/check_interval",
                check_interval, 10) == FAIL):
            return FAIL

        return SUCCESS

    except:
        print_msg("check_env Failed because of exception: " +
                  str(sys.exc_info()[1]))
        return FAIL

# Get environment settings
def get_env():
    try:
        sched_rt_runtime_us = 0
        sched_tick_max_deferment = 0
        stat_interval = 0
        numa_affinity = 0
        watchdog = 0
        check_interval = 0

        # Get RT throtteling values
        if os.path.isfile("/proc/sys/kernel/sched_rt_runtime_us"):
            with open("/proc/sys/kernel/sched_rt_runtime_us") as f:
                sched_rt_runtime_us = int(f.readline())

        # Get sched_tick_max_deferment
        if os.path.isfile("/sys/kernel/debug/sched_tick_max_deferment"):
            with open("/sys/kernel/debug/sched_tick_max_deferment") as f:
                sched_tick_max_deferment = int(f.readline())

        # Get vmstat_interval
        if os.path.isfile("/proc/sys/vm/stat_interval"):
            with open("/proc/sys/vm/stat_interval") as f:
                stat_interval = int(f.readline())

        # Get numa_affinity
        if os.path.isfile("/sys/bus/workqueue/devices/writeback/numa"):
            with open("/sys/bus/workqueue/devices/writeback/numa") as f:
                numa_affinity = int(f.readline())

        # Get watchdog
        if os.path.isfile("/proc/sys/kernel/watchdog"):
            with open("/proc/sys/kernel/watchdog") as f:
                watchdog = int(f.readline())

        # Get machine check interval (only check CPU 0)
        file_str = ("/sys/devices/system/machinecheck/machinecheck0/" +
                    "check_interval")
        if os.path.isfile(file_str):
            with open(file_str) as f:
                check_interval = int(f.readline(), base=10)

        return (sched_rt_runtime_us, sched_tick_max_deferment, stat_interval,
                numa_affinity, watchdog, check_interval)

    except:
        print_msg("get_env Failed because of exception: " +
                  str(sys.exc_info()[1]))
        return -1,-1,-1,-1,-1,-1

# Check that partitions have been removed.
def check_cpuset_cleanup(rt_partition, nrt_partition):
    try:
        # Check that default irq affinity cleanup
        with open("/proc/irq/default_smp_affinity") as f:
            cpumask = 2 ** multiprocessing.cpu_count() - 1
            default_affinity = int(f.readline(), base=16) & cpumask

            if (default_affinity != cpumask):
                print_msg("check_cpuset_cleanup: Bad default IRQ affinity:" +
                          " Expected " + hex(cpumask) + " got " +
                          hex(default_affinity))
                return FAIL

        # Check cpuset cleanup
        cpuset_dir, cpuset_prefix = get_cpusets()

        if os.path.isdir(cpuset_dir + "/" + rt_partition):
            print_msg("check_cpuset_cleanup: Failed: " + cpuset_dir +
                      "/" + rt_partition + " is still present")
            return FAIL

        if os.path.isdir(cpuset_dir + "/" + nrt_partition):
            print_msg("check_cpuset_cleanup: Failed: " + cpuset_dir +
                      "/" + nrt_partition + " is still present")
            return FAIL

        return SUCCESS

    except:
        print_msg("check_cpuset_cleanup Failed because of exception: " +
                  str(sys.exc_info()[1]))
        return FAIL

# Count the number of IRQs that includes the RT CPUs in its affinity
# mask.
def count_irgs_in_rt():
    global options
    try:
        n = 0;
        rt_mask = options.rt_mask

        for irqvector in os.listdir("/proc/irq/"):
            if os.path.isdir("/proc/irq/" + irqvector):
                with open("/proc/irq/" + irqvector + "/smp_affinity") as f:
                    affinity = int(f.readline(), base=16)
                    if (affinity & rt_mask != 0):
                        n += 1;

        return n

    except:
        return -1


################################################################################
#                                Test cases
################################################################################

# PART_TC_0
# Preparation for test cases needed to be done before partrt create
# Calculate ref_count_irqs, needed by PART_TC_2_2
def part_tc_0_1_irq_affinity():
    global ref_count_irqs;
    ref_count_irqs = count_irgs_in_rt()
    if (ref_count_irqs == -1):
        print_msg("part_tc_0_1 Failed because of exception: " +
                  str(sys.exc_info()[1]))
        return FAIL
    return SUCCESS

# PART_TC_1
# Run partition and check return code and check affinity in stderr.
# Leaves system in a partitioned state.
def part_tc_1_1_prepare():
    try:
        if options.numa is not None:
            cmd = ("partrt create -n " + str(options.numa))
        else:
            cmd = ("partrt create " + hex(options.rt_mask))

        p = run(cmd)

        (stdout, stderr) = p.communicate()

        if p.returncode != 0:
            print_msg(stdout + stderr)
            print_msg("part_tc_1_1: Failed: partrt returned non-zero value: " +
                      str(p.returncode))
            return FAIL

        rt_mask = options.rt_mask
        nrt_mask = ~rt_mask & (2 ** multiprocessing.cpu_count() - 1)

        for line in stdout.splitlines():
            if "Isolated CPUs (rt):" in line:
                if rt_mask != liststr2mask(line.split(':')[1]):
                    print_msg("part_tc_1_1 : Failed, partrt returned bad RT CPU" +
                              " list:" + line.split(':')[1])
                    return FAIL

            if "Non-isolated CPUs (nrt):" in line:
                if nrt_mask != liststr2mask(line.split(':')[1]):
                    print_msg("part_tc_1_1 : Failed, partrt returned bad NRT" +
                              " CPU list:" + line.split(':')[1])
                    return FAIL

        return SUCCESS

    except:
        print_msg("part_tc_1_1 Failed because of exception: " +
                  str(sys.exc_info()[1]))
        return FAIL

# PART_TC_1_2 Test the list sub-command
def part_tc_1_2_prepare():
    try:
        cmd = "partrt list"
        p = run(cmd, func=os.setsid)
        (stdout, stderr) = p.communicate()

        if p.returncode != 0:
            print_msg(stdout + stderr)
            print_msg("part_tc_1_2 Failed: " + cmd +
                   " returned with abnormal code: " + str(p.returncode))
            return FAIL

        found_rt = False
        found_nrt = False

        for line in stdout.splitlines():
            if "Name:rt" in line:
                real_mask = liststr2mask(line.split(":")[2])
                rt_mask = options.rt_mask
                if real_mask != rt_mask:
                    print_msg("part_tc_1_2 Failed: rt partition has CPU mask " +
                              hex(real_mask) + " expected: " + hex (rt_mask))
                    return FAIL

                found_rt = True

            if "Name:nrt" in line:
                real_mask = liststr2mask(line.split(":")[2])
                nrt_mask = (~options.rt_mask &
                             (2 ** multiprocessing.cpu_count() - 1))
                if real_mask != nrt_mask:
                    print_msg("part_tc_1_2 Failed: nrt partition has CPU mask "
                              + hex(real_mask) + " expected: " + hex (nrt_mask))
                    return FAIL

                found_nrt = True

        if not(found_rt and found_nrt):
                print_msg("part_tc_1_2 Failed: Could not find all partitions")
                return FAIL

        return SUCCESS

    except:
        print_msg("part_tc_1_2 Failed because of exception: " +
                  str(sys.exc_info()[1]))
        return FAIL

# PART_TC_2_1
# Check that the default irq affinity is the NRT affinity mask
def part_tc_2_1_irq_affinity():
    try:
        with open("/proc/irq/default_smp_affinity") as f:
            default_affinity = int(f.readline(), base=16)

        rt_mask = options.rt_mask
        nrt_mask = ~rt_mask & (2 ** multiprocessing.cpu_count() - 1)

        if (default_affinity != nrt_mask):
            print_msg("part_tc_2_1: Bad default IRQ affinity: Expected " +
                      hex(nrt_mask) + " got " + hex(default_affinity))
            return FAIL

        return SUCCESS

    except:
        print_msg("part_tc_2_1 Failed because of exception: " +
                  str(sys.exc_info()[1]))
        return FAIL

# PART_TC_2_2
# Check that at least one less IRQ includes the RT CPUs in its affinity
# mask.
def part_tc_2_2_irq_affinity():
    global ref_count_irqs;
    n = count_irgs_in_rt()
    if (n == -1):
        print_msg("part_tc_2_2 Failed because of exception: " +
                  str(sys.exc_info()[1]))
        return FAIL
    elif (n >= ref_count_irqs):
        print_msg("part_tc_2_2: No IRQ was migrated")
        return FAIL
    else:
        return SUCCESS

# PART_TC_3_1
# Check that load balancing only is enabled for the nrt cpuset
def part_tc_3_1_proc_affinity():
    try:

        (cpuset_dir, cpuset_prefix) = get_cpusets()

        if len(cpuset_dir) == 0:
            print_msg("part_tc_3_1: Kernel is lacking support for cpuset")
            return FAIL

        # Check root cpuset
        sched_load_balance = (cpuset_dir + cpuset_prefix +
                              "sched_load_balance")

        with open(sched_load_balance) as f:
            load_balance = int(f.readline())

        if load_balance != 0:
            print_msg(
                "part_tc_3_1: Load balance is not disabled in root cpuset")
            return FAIL

        # Check rt cpuset
        sched_load_balance = (cpuset_dir + "rt/" + cpuset_prefix +
                   "sched_load_balance")
        with open(sched_load_balance) as f:
            load_balance = int(f.readline())

        if load_balance != 0:
            print_msg("part_tc_3_1: Load balance is not disabled in RT cpuset")
            return FAIL

        # Check nrt cpuset
        sched_load_balance = (cpuset_dir + "nrt/" + cpuset_prefix +
                              "sched_load_balance")
        with open(sched_load_balance) as f:
            load_balance = int(f.readline())

        if load_balance != 1:
            print_msg("part_tc_3_1: Load balance is disabled in NRT cpuset")
            return FAIL

        return SUCCESS

    except:
        print_msg("part_tc_3_1 Failed because of exception: " +
                  str(sys.exc_info()[1]))
        return FAIL

    return SUCCESS

# PART_TC_3_2
# Check that no tasks are in rt cpuset
def part_tc_3_2_proc_affinity():
    try:
        cpuset_dir, cpuset_prefix = get_cpusets()

        if len(cpuset_dir) == 0:
            print_msg("part_tc_3_2: Kernel is lacking support for cpuset")
            return FAIL

        tasks_path = cpuset_dir + "rt/tasks"

        with open(tasks_path) as f:
            rt_tasks = f.readline()

        if len(rt_tasks) != 0:
            print_msg("part_tc_3_2: There are tasks in the RT cpuset")
            return FAIL

        return SUCCESS

    except:
        print_msg("part_tc_3_2 Failed because of exception: " +
                  str(sys.exc_info()[1]))
        return FAIL

# PART_TC_3_3
# Check that at least one process was migrated. I.e. there are tasks in nrt
def part_tc_3_3_proc_affinity():
    try:
        cpuset_dir, cpuset_prefix = get_cpusets()

        if len(cpuset_dir) == 0:
            print_msg("part_tc_3_3: Kernel is lacking support for cpuset")
            return FAIL

        tasks_path = cpuset_dir + "nrt/tasks"

        with open(tasks_path) as f:
            nrt_tasks = f.readline()

        if len(nrt_tasks) == 0:
            print_msg("part_tc_3_3: No tasks where migrated")
            return FAIL

        return SUCCESS

    except:
        print_msg("part_tc_3_3 Failed because of exception: " +
                  str(sys.exc_info()[1]))
        return FAIL

# PART_TC_4_1
# Test the partrt run rt command. Check that command is executed in correct
# RT partition.
def part_tc_4_1_run():
    try:
        rt_mask = options.rt_mask

        cmd = "partrt run -f 60 rt watch ls"
        p = run(cmd, func=os.setsid)

        time.sleep(1)

        returncode = p.poll()
        if returncode is not None:
            print ("part_tc_4_1: " + cmd + " unexpectedly returned with code: "
                   + str(p.returncode))
            return FAIL

        (task_name, affinity, last_cpu, policy, prio) = get_task_info(p.pid)

        if affinity != rt_mask:
            print_msg("part_tc_4_1: RT task: " + task_name +
                      " has bad affinity:" + hex(affinity))
            return FAIL

        if rt_mask & (1 << last_cpu) == 0:
            print_msg("part_tc_4_1: RT task: " + task_name +
                      "executes on nrt CPU: " + str(last_cpu))
            return FAIL

        if policy != SCHED_FIFO:
            print_msg("part_tc_4_1: RT task: " + task_name +
                      " has sched policy: " + str(policy))
            return FAIL

        if prio != 60:
            print_msg("part_tc_4_1: RT task: " + task_name +
                      " has wrong priority: " + str(prio))
            return FAIL

        return SUCCESS

    except:
        print_msg("part_tc_4_1 Failed because of exception: " +
                  str(sys.exc_info()[1]))
        return FAIL

    finally:
        if p is not None and p.poll() is None:
            os.killpg(p.pid, signal.SIGTERM)

# PART_TC_4_2
# Test the partrt run nrt command. Check that command is executed in correct
# NRT partition.
def part_tc_4_2_run():
    p = None
    try:
       rt_mask = options.rt_mask
       nrt_mask = ~rt_mask & (2 ** multiprocessing.cpu_count() - 1)

       cmd = "partrt run nrt watch ls"
       p = run(cmd, func=os.setsid)

       time.sleep(1)

       returncode = p.poll()
       if returncode is not None:
           print ("part_tc_4_2: " + cmd + " unexpectedly returned with code: "
                  + str(returncode))
           return FAIL

       (task_name, affinity, last_cpu, policy, prio) = get_task_info(p.pid)

       if affinity != nrt_mask:
           print_msg("part_tc_4_2: Invalid nrt task affinity:" + hex(affinity))
           return FAIL

       if nrt_mask & (1 << last_cpu) == 0:
           print_msg("part_tc_4_2: NRT task executes on nrt CPU: " + last_cpu)
           return FAIL

       if policy != SCHED_NORMAL:
           print_msg("part_tc_4_2: NRT task has sched policy: " +
                     str(policy))
           return FAIL

       if prio != 0:
           print_msg("part_tc_4_2: NRT task has wrong priority: " +
                     str(prio))
           return FAIL

       return SUCCESS

    except:
        print_msg("part_tc_4_2 Failed because of exception: " +
                  str(sys.exc_info()[1]))
        return FAIL

    finally:
        if p is not None and p.poll() is None:
            os.killpg(p.pid, signal.SIGTERM)

# PART_TC_4_3
# Test the -c flag of the run subcommand
def part_tc_4_3_run():
    p = None
    try:
        rt_mask = options.rt_mask

        bit = 0
        while (rt_mask & (0x1 << bit)) == 0:
            bit = bit + 1

        cpu = 2 ** bit

        cmd = "partrt run -c " + hex(cpu) + " -f 60 rt watch ls"
        p = run(cmd, func=os.setsid)

        time.sleep(1)

        returncode = p.poll()
        if returncode is not None:
            print ("part_tc_4_3: " + cmd + " unexpectedly returned with code: "
                   + str(p.returncode))
            return FAIL

        (task_name, affinity, last_cpu, policy, prio) = get_task_info(p.pid)

        if affinity != cpu:
            print_msg("part_tc_4_3: RT task: " + task_name +
                      " has bad affinity: " + hex(affinity) + " expected: " +
                      hex(cpu))
            return FAIL

        return SUCCESS

    except:
        print_msg("part_tc_4_3 Failed because of exception: " +
                  str(sys.exc_info()[1]))
        return FAIL

    finally:
        if p is not None and p.poll() is None:
            os.killpg(p.pid, signal.SIGTERM)

# PART_TC_5
# Check that the tick has been disabled
def part_tc_5_tick_deferment():
    try:
        # Check only if kernel configured for NO_HZ_FULL with patched tick
        # deferment
        if os.path.isfile("/sys/kernel/debug/sched_tick_max_deferment"):
            with open("/sys/kernel/debug/sched_tick_max_deferment") as f:
                if int(f.readline()) != DEFERMENT_TICK_DISABLED:
                    print_msg("part_tc_5 Failed: sched_tick_max_deferment" +
                              " is not equal to " +
                              str(DEFERMENT_TICK_DISABLED))
                    return FAIL

        return SUCCESS

    except:
        print_msg("part_tc_5 Failed because of exception: " +
                  str(sys.exc_info()[1]))
        return FAIL

# PART_TC_6
# Check that RT throttling has been disabled
def part_tc_6_rt_throttle():
    try:
        with open("/proc/sys/kernel/sched_rt_runtime_us") as f:
            if int(f.readline()) != -1:
                print_msg("part_tc_6 Failed: RT Throttling is not disabled")
                return FAIL
            return SUCCESS
    except:
        print_msg("part_tc_6 Failed because of exception: " +
                  str(sys.exc_info()[1]))
        return FAIL

# PART_TC_7
# Test bad parameters on partitioned system
def part_tc_7_bad_parameters():
    p = None

    try:
        part_tc_name = "part_tc_7"

        cmd = "partrt"
        if bad_parameter(cmd, part_tc_name):
            return FAIL

        cmd = "partrt asdfasdf"
        if bad_parameter(cmd, part_tc_name):
            return FAIL

        cmd = "partrt create 8"
        if bad_parameter(cmd, part_tc_name):
            return FAIL

        cmd = "partrt run asdfasdf"
        if bad_parameter(cmd, part_tc_name):
            return FAIL

        cmd = "partrt run rt asdfasdf"
        if bad_parameter(cmd, part_tc_name):
            return FAIL

        cmd = "partrt run rt asdfasdf watch ls"
        if bad_parameter(cmd, part_tc_name):
            return FAIL

        cmd = "partrt run rt asdfasdf 50 watch ls"
        if bad_parameter(cmd, part_tc_name):
            return FAIL

        cmd = "partrt run rt -f 60 ls"
        if bad_parameter(cmd, part_tc_name):
            return FAIL

        cmd = "partrt run ls"
        if bad_parameter(cmd, part_tc_name):
            return FAIL

        cmd = "partrt run -f 22 ls -l"
        if bad_parameter(cmd, part_tc_name):
            return FAIL

        cmd = "partrt move asdf 1234"
        if bad_parameter(cmd, part_tc_name):
            return FAIL

        #################################
        # Create process that we can fiddle with
        cmd = "while true; do sleep .1; done"
        p = run(cmd, func=os.setsid)

        cmd = "partrt move " + str(p.pid) + " asdf"

        if bad_parameter(cmd, part_tc_name):
            return FAIL

        #################################

        cmd = "partrt mov asdf rt"
        if bad_parameter(cmd, part_tc_name):
            return FAIL

        return SUCCESS

    except:
        print_msg("part_tc_7 Failed because of exception: " +
                  str(sys.exc_info()[1]))
        return FAIL

    finally:
        if p is not None and p.poll() is None:
            os.killpg(p.pid, signal.SIGTERM)

# PART_TC_8
# Test that it is possible to move a process into the RT partition
# and that the process gets correct affinity
def part_tc_8_mov():
    p1 = None

    try:
        rt_mask = options.rt_mask
        nrt_mask = ~rt_mask & (2 ** multiprocessing.cpu_count() - 1)

        cmd = "while true; do sleep .1; done"
        p1 = run(cmd, func=os.setsid)

        time.sleep(1)

        returncode = p1.poll()
        if returncode is not None:
            print ("part_tc_8: " + cmd + " unexpectedly returned with code: "
                   + str(returncode))
            return FAIL

        # Check that the process executes within the NRT partition
        (task_name, affinity, last_cpu, policy, prio) = get_task_info(p1.pid)

        if affinity != nrt_mask:
            print_msg("part_tc_8: NRT process has bad affinity: " +
                      hex(affinity) + " expected: " + hex(nrt_mask))
            return FAIL

        if rt_mask & (1 << last_cpu) != 0:
            print_msg("part_tc_8: NRT process executes on RT CPU: " +
                      str(last_cpu))
            return FAIL

        # Move the process
        bit = 0
        while (rt_mask & (0x1 << bit)) == 0:
            bit = bit + 1

        cpu = 2 ** bit

        cmd = ("partrt move -c " + hex(cpu) + " " + str(p1.pid) + " rt")

        p2 = run(cmd, func=os.setsid)

        p2.wait()

        if p2.returncode != 0:
            print ("part_tc_8: " + cmd + "Returned with abnormal code: "
                   + str(p2.returncode))
            return FAIL

        time.sleep(1)

        # Check that the process executes within the RT partition
        (task_name, affinity, last_cpu, policy, prio) = get_task_info(p1.pid)

        if affinity != cpu:
            print_msg("part_tc_8: RT process has bad affinity: " +
                      hex(affinity) + " expected: " + hex(cpu))
            return FAIL

        if rt_mask & (1 << last_cpu) == 0:
            print_msg("part_tc_8: RT process executes on NRT CPU: "
                      + str(last_cpu))
            return FAIL

        return SUCCESS

    except:
        print_msg("part_tc_8 Failed because of exception: " +
                  str(sys.exc_info()[1]))
        return FAIL

    finally:
        if p1 is not None and p1.poll() is None:
            os.killpg(p1.pid, signal.SIGTERM)

# PART_TC_9 Check that the environment has been changed
def part_tc_9_check_env():
    try:
        sched_rt_runtime_us = -1
        sched_tick_max_deferment = DEFERMENT_TICK_DISABLED
        stat_interval = 1000
        numa_affinity = 0
        watchdog = 0
        cpumask = (~options.rt_mask &
                    (2 ** multiprocessing.cpu_count() - 1))
        check_interval = 0

        return check_env(sched_rt_runtime_us, sched_tick_max_deferment,
                         stat_interval, numa_affinity, watchdog, cpumask,
                         check_interval)

    except:
        print_msg("part_tc_9 Failed because of exception: " +
                  str(sys.exc_info()[1]))
        return FAIL

# PART_TC_10 Check that partition undo restore environment to default values
def part_tc_10_cleanup():
    try:
        cmd = "partrt undo"
        p = run(cmd, func=os.setsid)
        (stdout, stderr) = p.communicate()

        if p.returncode != 0:
            print_msg(stdout + stderr)
            print_msg("part_tc_10 Failed: " + cmd +
                   " returned with abnormal code: " + str(p.returncode))
            return FAIL

        cpumask = 2 ** multiprocessing.cpu_count() - 1

        # Check cpusets and environment
        if check_cpuset_cleanup("rt", "nrt") != SUCCESS:
            return FAIL
        if check_env(950000, 100, 1, 1, 1, cpumask, 300) != SUCCESS:
            return FAIL

        return SUCCESS

    except:
        print_msg("part_tc_10 Failed because of exception: " +
                  str(sys.exc_info()[1]))
        return FAIL


# NOPART_TC_1_1 Check that partition undo restores environment from file
# Leaves system in unpartitioned state
def nopart_tc_1_1_cleanup():
    try:

        cpumask = 2 ** multiprocessing.cpu_count() - 1

        # Get environment
        (sched_rt_runtime_us, sched_tick_max_deferment, stat_interval,
         numa_affinity, watchdog, check_interval) = get_env()

        # Create partitions
        cmd = ("partrt create " + hex(options.rt_mask))
        p = run(cmd, func=os.setsid)
        (stdout, stderr) = p.communicate()

        if p.returncode != 0:
            print_msg(stdout + stderr)
            print_msg("nopart_tc_1_1 Failed: " + cmd +
                   " returned with abnormal code: " + str(p.returncode))
            return FAIL

        # Remove partitions
        cmd = "partrt undo -s /tmp/partrt_env"
        p = run(cmd, func=os.setsid)
        (stdout, stderr) = p.communicate()

        if p.returncode != 0:
            print_msg(stdout + stderr)
            print_msg("nopart_tc_1_1 Failed: " + cmd +
                   " returned with abnormal code: " + str(p.returncode))
            return FAIL

        # Check cpusets and environment
        if check_cpuset_cleanup("rt", "nrt") != SUCCESS:
            return FAIL

        if  check_env(sched_rt_runtime_us, sched_tick_max_deferment,
                      stat_interval, numa_affinity, watchdog,
                      cpumask, check_interval) != SUCCESS:
            return FAIL

        return SUCCESS

    except:
        print_msg("nopart_tc_1_1 Failed because of exception: " +
                  str(sys.exc_info()[1]))
        return FAIL

# NOPART_TC_1_2 Check that the stanndard options and the create cmd-options
# works. Leaves system in unpartitioned state
def nopart_tc_1_2_cleanup():
    try:

        cpumask = 2 ** multiprocessing.cpu_count() - 1
        # Get environment
        (sched_rt_runtime_us, sched_tick_max_deferment, stat_interval,
         numa_affinity, watchdog, check_interval) = get_env()

        # Create partitions
        cmd = ("partrt -r rt1 -n nrt1 create -a -b -c -d -m -r -t -w " +
               hex(options.rt_mask))

        p = run(cmd, func=os.setsid)

        (stdout, stderr) = p.communicate()

        if p.returncode != 0:
            print_msg(stdout + stderr)
            print_msg("nopart_tc_1_2 Failed: " + cmd +
                   " returned with abnormal code: " + str(p.returncode))
            return FAIL

        # Check that the environment was unmodified
        if (check_env(sched_rt_runtime_us, sched_tick_max_deferment,
                      stat_interval, numa_affinity, watchdog, cpumask,
                      check_interval)
            != SUCCESS):
            return FAIL

        # Remove partitions
        cmd = "partrt -r rt1 -n nrt1 undo"
        p = run(cmd, func=os.setsid)

        (stdout, stderr) = p.communicate()

        if p.returncode != 0:
            print_msg(stdout + stderr)
            print_msg("nopart_tc_1_2 Failed: " + cmd +
                   " returned with abnormal code: " + str(p.returncode))
            return FAIL

        # Check cpuset cleanup
        if check_cpuset_cleanup("rt1", "nrt1") != SUCCESS:
            return FAIL

        # Check again that the environment was unmodified
        if  check_env(sched_rt_runtime_us, sched_tick_max_deferment,
                      stat_interval, numa_affinity, watchdog, cpumask,
                      check_interval) != SUCCESS:
            return FAIL

        return  SUCCESS

    except:
        print_msg("nopart_tc_1_2 Failed because of exception: " +
                  str(sys.exc_info()[1]))
        return FAIL

# NOPART_TC_2_1
# Check that help text is displayed
def nopart_tc_2_1_help_text():
    try:

        cmd = "partrt -h"
        p = run(cmd, func=os.setsid)

        (stdout, stderr) = p.communicate()

        if p.returncode != 0:
            print_msg(stdout + stderr)
            print_msg(
                "nopart_tc_2: Failed: partrt returned with abnormal code: ",
                str(p.returncode))
            return FAIL

        found_usage = False

        for line in stdout.splitlines():
            if  "Usage:" in line:
                found_usage = True
                break

        if found_usage == False:
            print_msg("nopart_tc_2: Failed: partrt returned corrupt help text")
            return FAIL

        return SUCCESS

    except:
        print_msg("nopart_tc_2 Failed because of exception: " +
                  str(sys.exc_info()[1]))
        return FAIL

# NOPART_TC_2_2
# Check that help text is displayed for sub command
def nopart_tc_2_2_help_text():
    try:
        cmd = "partrt run -h"
        p = run(cmd, func=os.setsid)
        (stdout, stderr) = p.communicate()

        if p.returncode != 0:
            print_msg(stdout + stderr)
            print_msg(
                "nopart_tc_2_2: Failed: partrt returned with abnormal code: ",
                       p.returncode)
            return FAIL

        found_usage = False

        for line in stdout.splitlines():
            if  "Usage:" in line:
                found_usage = True
                break

        if found_usage == False:
            print_msg(
                "nopart_tc_2_2: Failed: partrt returned corrupt help text")
            return FAIL

        return SUCCESS

    except:
        print_msg("nopart_tc_2_2 Failed because of exception: " +
                  str(sys.exc_info()[1]))
        return FAIL

# NOPART_TC_3
# Test bad parameters on unpartitioned system
def nopart_tc_3_bad_parameters():
    try:
        nopart_tc_name = "nopart_tc_3"

        cmd = "partrt"
        if bad_parameter(cmd, nopart_tc_name):
            return FAIL

        cmd = "partrt asdfasdf"
        if bad_parameter(cmd, nopart_tc_name):
            return FAIL

        cmd = "partrt --asdfasdf"
        if bad_parameter(cmd, nopart_tc_name):
            return FAIL

        cmd = "partrt -asdfasdf"
        if bad_parameter(cmd, nopart_tc_name):
            return FAIL

        cmd = "partrt create -1"
        if bad_parameter(cmd, nopart_tc_name):
            return FAIL

        # This has to be updated for really large systems
        cmd = "partrt create fffffffffffffffffffffffffffffff"
        if bad_parameter(cmd, nopart_tc_name):
            return FAIL

        cmd = "partrt -z run rt watch ls"
        if bad_parameter(cmd, nopart_tc_name):
            return FAIL

        cmd = "partrt run rt watch ls"
        if bad_parameter(cmd, nopart_tc_name):
            return FAIL

        cmd = "partrt run -c 1234 rt watch ls"
        if bad_parameter(cmd, nopart_tc_name):
            return FAIL

        cmd = "partrt run -c asdf rt watch ls"
        if bad_parameter(cmd, nopart_tc_name):
            return FAIL

        cmd = "partrt run -c rt watch ls"
        if bad_parameter(cmd, nopart_tc_name):
            return FAIL

        cmd = "partrt run -f 60 - rt watch ls"
        if bad_parameter(cmd, nopart_tc_name):
            return FAIL

        cmd = "partrt run nrt watch ls"
        if bad_parameter(cmd, nopart_tc_name):
            return FAIL

        cmd = "partrt run -z nrt watch ls"
        if bad_parameter(cmd, nopart_tc_name):
            return FAIL

        cmd = "partrt move 0 rt"
        if bad_parameter(cmd, nopart_tc_name):
            return FAIL

        cmd = "partrt move -c 1234 0 rt"
        if bad_parameter(cmd, nopart_tc_name):
            return FAIL

        cmd = "partrt move -c asdf 0 rt"
        if bad_parameter(cmd, nopart_tc_name):
            return FAIL

        cmd = "partrt move -c 0 rt"
        if bad_parameter(cmd, nopart_tc_name):
            return FAIL

        return SUCCESS

    except:
        print_msg("nopart_tc_3 Failed because of exception: " +
                  str(sys.exc_info()[1]))
        return FAIL

# NOPART_TC_4
# Check that Enea copyright is present
def nopart_tc_4_copyright():
    try:
        found_copyright = False
        cmd = "partrt -V"
        p = run(cmd, func=os.setsid)
        (stdout, stderr) = p.communicate()

        if p.returncode != 0:
            print_msg(stdout + stderr)
            print_msg("nopart_tc_4: Failed: " + cmd +
                      ": returned with abnormal code: ", p.returncode)
            return FAIL

        for line in stdout.splitlines():
            found = re.search("Copyright \(C\) (.*) by Enea Software AB", line)
            if found:
                found_copyright = True
                break

        if found_copyright == False:
            print_msg("nopart_tc_4: Failed: " + cmd +
                      ": Could not find copyright text")
            return FAIL

        return SUCCESS

    except:
        print_msg("nopart_tc_4 Failed because of exception: " +
                  str(sys.exc_info()[1]))
        return FAIL

def run_tc(nopart_tc_func, nopart_tc_name, expected_result):
    print_msg(nopart_tc_name + ": Executing")

    result = nopart_tc_func()
    if result != expected_result:
        test_result = 1
        print_msg(nopart_tc_name + ": Failed")
        return FAIL
    else:
        print_msg(nopart_tc_name + ": Passed")
        return SUCCESS

def cleanup():
    try:
        cmd = "partrt undo"
        p = run(cmd, func=os.setsid)
        (stdout, stderr) = p.communicate()

        if p.returncode != 0:
            print_msg(stdout + stderr)
            print("test_partition: Failed: " + cmd +
                  ": returned with abnormal code: ", p.returncode)
            sys.exit(1)

        cmd = "partrt -r rt1 -n nrt1 undo"
        p = run(cmd, func=os.setsid)
        (stdout, stderr) = p.communicate()

        if p.returncode != 0:
            print_msg(stdout + stderr)
            print("test_partition: Failed: " + cmd +
                  ": returned with abnormal code: ", p.returncode)
            return sys.exit(1)

    except:
        print("test_partition: Failed because of exception " +
              str(sys.exc_info()[1]))
        sys.exit(1)

# input: programdir, gnuplotdir
def usage():
    print 'Usage:'
    print '\tpartition_test <target> [-v] [-h]'
    print '\tOptions:'
    print '\t\t--help, -h:'
    print '\t\t\tShow help text'
    print '\t\t--verbose, -v:'
    print '\t\t\tExtra verbose output'
    print '\t\t\tSave test results'
    print ''
    print 'If <target> is not one of the preconfigured targets:'
    print SUPPORTED_TARGETS
    print 'a default configuration will be used.'

def main(argv):
    global verbose
    global options

    verbose = False
    target = ""

    # Get mandatory parameter
    if len(argv) == 0:
        print 'Missing target parameter'
        print ''
        usage()
        exit(1)
    else:
        target = argv[0]

    # Compare target against supported targets
    if not(target in SUPPORTED_TARGETS):
        msg = "Unknown target: " + target
        msg += ": Trying default configuration"
        target = "default"
        print msg

    options = targetOptions(target)

    # Get optional parameters
    try:
        opts, args = getopt.getopt(argv[1:], "hs:vk", ["help",
                                                       "verbose"])
    except getopt.GetoptError as err:
        usage()
        exit(-1)

    for o, a in opts:
        if o in ("-v", "verbose"):
            verbose = True
        elif o in ("-h", "help"):
            usage()
            exit(0)
        else:
            print 'Unknown parameter: ', o
            usage()
            exit(-1)

    test_result = 0

    # Remove any pre existing partition
    cleanup()

    # Run the tests

    ############# PART_TC_0_1 #############
    test_result = (test_result | run_tc(part_tc_0_1_irq_affinity,
                                       "PART_TC_0_1",
                                       SUCCESS))

    ############# PART_TC_1_1 #############
    test_result = (test_result | run_tc(part_tc_1_1_prepare,
                                       "PART_TC_1_1",
                                       SUCCESS))

    ############# PART_TC_1_2 #############
    test_result = (test_result | run_tc(part_tc_1_2_prepare,
                                       "PART_TC_1_2",
                                       SUCCESS))

    ############# PART_TC_2_1 #############
    test_result = (test_result | run_tc(part_tc_2_1_irq_affinity,
                                       "PART_TC_2_1",
                                       SUCCESS))

    ############# PART_TC_2_2 ##############
    test_result = (test_result | run_tc(part_tc_2_2_irq_affinity,
                                       "PART_TC_2_2",
                                       SUCCESS))

    ############# PART_TC_3_1 ##############
    test_result = (test_result | run_tc(part_tc_3_1_proc_affinity,
                                       "PART_TC_3_1",
                                       SUCCESS))

    ############# PART_TC_3_2 ##############
    test_result = (test_result | run_tc(part_tc_3_2_proc_affinity,
                                       "PART_TC_3_2",
                                       SUCCESS))

    ############# PART_TC_3_3 ##############
    test_result = (test_result | run_tc(part_tc_3_3_proc_affinity,
                                       "PART_TC_3_3",
                                       SUCCESS))

    ############# PART_TC_4_1 ##############
    test_result = (test_result | run_tc(part_tc_4_1_run,
                                       "PART_TC_4_1",
                                       SUCCESS))

    ############# PART_TC_4_2 ##############
    test_result = (test_result | run_tc(part_tc_4_2_run,
                                       "PART_TC_4_2",
                                       SUCCESS))

    ############# PART_TC_4_3 ##############
    test_result = (test_result | run_tc(part_tc_4_3_run,
                                       "PART_TC_4_3",
                                       SUCCESS))

    ############# PART_TC_5 ##############
    test_result = (test_result | run_tc(part_tc_5_tick_deferment,
                                       "PART_TC_5",
                                       SUCCESS))

    ############# PART_TC_6 ##############
    test_result = (test_result | run_tc(part_tc_6_rt_throttle,
                                       "PART_TC_6",
                                       SUCCESS))

    ############# PART_TC_7 ##############
    test_result = (test_result | run_tc(part_tc_7_bad_parameters,
                                       "PART_TC_7",
                                       SUCCESS))

    ############# PART_TC_8 ##############
    test_result = (test_result | run_tc(part_tc_8_mov,
                                       "PART_TC_8",
                                       SUCCESS))

    ############# PART_TC_9 ##############
    test_result = (test_result | run_tc(part_tc_9_check_env,
                                        "PART_TC_9",
                                        SUCCESS))

    ############# PART_TC_10 ##############
    test_result = (test_result | run_tc(part_tc_10_cleanup,
                                        "PART_TC_10",
                                        SUCCESS))

    ############# NOPART_TC_1_1 ##############
    test_result = (test_result | run_tc(nopart_tc_1_1_cleanup,
                                        "NOPART_TC_1_1",
                                        SUCCESS))

    ############# NOPART_TC_1_2 ##############
    test_result = (test_result | run_tc(nopart_tc_1_2_cleanup,
                                        "NOPART_TC_1_2",
                                        SUCCESS))

    ############# NOPART_TC_2_1 ##############
    test_result = (test_result | run_tc(nopart_tc_2_1_help_text,
                                       "NOPART_TC_2_1",
                                       SUCCESS))

    ############# NOPART_TC_2_2 ##############
    test_result = (test_result | run_tc(nopart_tc_2_2_help_text,
                                       "NOPART_TC_2_2",
                                       SUCCESS))

    ############# NOPART_TC_3 ##############
    test_result = (test_result | run_tc(nopart_tc_3_bad_parameters,
                                       "NOPART_TC_3",
                                       SUCCESS))

    ############# NOPART_TC_4 ##############
    test_result = (test_result | run_tc(nopart_tc_4_copyright,
                                       "NOPART_TC_4",
                                       SUCCESS))

    # Do final cleanup in case any cleanup test case failed and prevented
    # cleanup. This could ofcourse fail if partrt undo is broken
    cleanup()

    if test_result == 0:
        print "SUCCESS"
    else:
        print "FAIL"

    sys.exit(test_result)

if __name__ == "__main__":
    main(sys.argv[1:])
