#!/usr/bin/python3
# coding: utf-8

import socket
import time
import argparse
import subprocess
import threading
import shlex
import fnmatch
import os
import traceback
import re
import signal
from random import random
from math import sqrt
from glob import glob
from shutil import rmtree
import xml.etree.ElementTree as ET
from xml.dom.minidom import parseString
#import networkx as nx
import matplotlib.pyplot as plt
from multiprocessing import Process


# Looking for default values
curr = os.getcwd()
# st_files = []
sim_files = []
# vid_files = []
for root, dirnames, filenames in os.walk(curr):
    # for filename in fnmatch.filter(filenames, 'st-packet'):
    #     st_files.append(os.path.join(os.path.relpath(root, curr),
    #                     filename) if root != curr else filename)
    for filename in fnmatch.filter(filenames, '*.csc'):
        sim_files.append(os.path.join(os.path.relpath(
            root, curr), filename) if root != curr else filename)
    # for filename in fnmatch.filter(filenames, '*.avi'):
    #     vid_files.append(os.path.join(os.path.relpath(
    #         root, curr), filename) if root != curr else filename)
# if len(st_files) == 0:
#     st_files.append(None)
if len(sim_files) == 0:
    sim_files.append(None)
# if len(vid_files) == 0:
#     vid_files.append(None)


# Add every argument
parser = argparse.ArgumentParser(description='Send commands to the source mote',
                                 formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument('-ht', '--host', type=str, nargs='?',
                    help="Host address to connect to", default='localhost')
parser.add_argument('-rcv', '--sink_port', type=int, nargs='?',
                    help="The port of the sink depending on the simulation file", default=60001)
parser.add_argument('-src', '--source_port', type=int, nargs='?',
                    help="The port of the source depending on the simulation file", default=60002)
# parser.add_argument('-st', '--st_file', type=str, nargs='?',
#                     help="The st-packet file to send", default=st_files[0])
parser.add_argument('-csc', '--sim_file', type=str, nargs='?',
                    help="The simulation file (*.csc) to use, it must have been configured to use the Sensevid's running script in Cooja first", default=sim_files[0])
parser.add_argument('-t', '--timeout', type=float, nargs='?',
                    help="The delay before the sink times out. When sink times out, it'll start the next trace until reaching the last one, then it'll end the sink's connection. It must be at least 3 times inter-packet delay if specified", default=10.)
# parser.add_argument('-p', '--period', type=str, nargs='?',
#                     help='If provided determines the inter-packet delay, else st-packet timestamps are considered')
parser.add_argument('-tr', '--traces', type=int, nargs='?',
                    help="The number of traces to send during the simulation, each trace will be sent after a delay starting when sink times out, it means we wait for the previous trace to be recepted to send a new one", default=1)
parser.add_argument('-d', '--delay', type=float, nargs='?',
                    help="The delay to wait after the sink's timeout before sending the next trace, this parameter is ignored for the last trace", default=5.)
# parser.add_argument('-v', '--video_file', type=str, nargs='?',
#                     help="The reference video file to use with Sensevid to get the simulation's results. When using None, Sensevid isn't called", default=vid_files[0])
parser.add_argument('-ccr', '--channel_check_rate', type=int, nargs='?',
                    help="The channel check rate (CCR) to use, it must be a power of 2 in range [2..128]", default=128)
parser.add_argument('-mac', '--media_access_control', type=str, nargs='?',
                    help="The MAC protocol to use between: contikimac, cxmac, nullrdc", default="nullrdc")
parser.add_argument('-g', '--grid', type=str, nargs='?',
                    help="The grid type to generate between: grid, randgrid, random. It is generated using the specified '-csc [SIM_FILE]' as a template", default=None)
parser.add_argument('-n', '--nodes', type=int, nargs='?',
                    help="The number of nodes of the generated grid", default=9)
parser.add_argument('-x', '--dim_x', type=float, nargs='?',
                    help="The X dimension of the generated grid", default=90)
parser.add_argument('-y', '--dim_y', type=float, nargs='?',
                    help="The Y dimension of the generated grid", default=90)
parser.add_argument('-dg', '--dodag', action='store_true',
                    help="If set, a live RPL DODAG will be displayed (RPL only)")
parser.add_argument('-l', '--logs', action='store_true',
                    help="If set, logs from Cooja (./COOJA.testlog) will be displayed by the client. Note that errors and warnings will always be displayed")
parser.add_argument('-s', '--safe', action='store_true',
                    help="If set, source will start sending on a 'READY TO SEND' msg, else after all nodes have been started. It needs to be set when somehow source is blocked (e.g.: timer, long setting up)")
parser.add_argument('-c', '--cleanup', action='store_true', help="If set, exit the client right after the directories' cleanup. This cleanup erases all previous files and directories generated by this client. These are: ./rt_*, ./rt-packet_*, ./COOJA.log, ./COOJA.testlog, ./COOJA_*.testlog, ./sensevid-generated.csc, ./sim_dir/rt-frame_*, ./sim_dir/decodedFrames_* and Make related")


# Class to color stdout text
class clr:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'


# Verify args
args = parser.parse_args()
if len(sim_files) > 1 and args.grid is None:
    print(clr.WARNING + clr.BOLD +
          "Warning: multiple simulations files (*.csc) detected, it has been set to " + args.sim_file + clr.ENDC)
# if len(st_files) > 1:
#     print(clr.WARNING + clr.BOLD +
#           "Warning: multiple 'st-packet' files detected, it has been set to " + args.st_file + clr.ENDC)
# if len(vid_files) > 1:
#     print(clr.WARNING + clr.BOLD +
#           "Warning: multiple video files (*.avi) detected, it has been set to " + args.st_file + clr.ENDC)
try:
    # if not args.st_file:
    #     raise Exception(
    #         "Can't find any 'st-packet' file, please provide '-st [ST_FILE]'" + clr.ENDC)
    # elif not
    if not args.sim_file:
        raise Exception(
            "Can't find any simulation file (*.csc), please provide '-csc [SIM_FILE]'" + clr.ENDC)
    elif args.grid is not None:
        args.grid = args.grid.lower()
        args.dim_x = float(args.dim_x)
        args.dim_y = float(args.dim_y)
        if args.grid not in ["grid", "randgrid", "random"]:
            raise Exception(
                "Invalid grid, please select a valid one between: grid, randgrid, random" + clr.ENDC)
        elif args.nodes <= 2:
            raise Exception("You must pick at least 2 nodes")
        elif args.grid == "grid" or args.grid == "randgrid":
            if args.nodes**.5 % 1 != 0:  # Perfect square
                raise Exception(
                    "You must enter a perfect square number of nodes for grid and randgrid" + clr.ENDC)
    elif not os.path.exists("Makefile"):
        raise Exception("Can't find Makefile" + clr.ENDC)
    elif args.media_access_control.lower() not in ["contikimac", "cxmac", "nullrdc"]:
        raise Exception(
            "Invalid MAC protocol, please select a valid one between: contikimac, cxmac, nullrdc" + clr.ENDC)
    elif not ((args.channel_check_rate & (args.channel_check_rate - 1)) == 0) or args.channel_check_rate <= 0 or args.channel_check_rate > 128:
        raise Exception(
            "Invalid channel check rate (CCR), it must be a power of 2 in range [2..128]" + clr.ENDC)
except Exception:
    print(clr.FAIL + clr.BOLD)
    traceback.print_exc()
    os._exit(1)
# if args.period is not None:
#     ipd = float(args.period)  # inter-packet delay.
#     if args.timeout < 3 * ipd:
#         print(clr.WARNING + clr.BOLD + "Warning: the current timeout (-t --timeout) is too low for the specified inter-packet delay ({}s), timeout has been adjusted from {}s to {}s".format(ipd, args.timeout, 3 * ipd) + clr.ENDC)
#         args.timeout = 3 * ipd


# Cleanup
print(clr.OKGREEN + clr.BOLD + "Cleaning up directories" + clr.ENDC)
# sim_dir = (os.path.splitext(os.path.basename(args.video_file))
#            [0] + "-sim") if args.video_file else None
# for file in glob('rt_*'):
#     os.remove(file)
# for file in glob('rt-packet_*'):
# os.remove(file)
if os.path.exists("COOJA.log"):
    os.remove("COOJA.log")
if os.path.exists("COOJA.testlog"):
    # We remove the old .testlog to avoid misreading the new one
    os.remove("COOJA.testlog")
for file in glob('COOJA_*.testlog'):
    os.remove(file)
if os.path.exists("sensevid-generated.csc"):
    os.remove("sensevid-generated.csc")
# if sim_dir and os.path.isdir(sim_dir):
#     for file in glob(os.path.join(sim_dir, 'rt-frame_*')):
#         os.remove(file)
#     folders = [folder for folder in os.listdir(
#         sim_dir) if os.path.isdir(os.path.join(sim_dir, folder))]
#     for folder in folders:
#         if folder.startswith("decodedFrames_"):
#             rmtree(os.path.join(sim_dir, folder))


# Modify the Makefile
if not args.cleanup:
    with open("Makefile", "r") as makefile:
        rdc = "{}_driver".format(args.media_access_control.lower())
        ccr = str(args.channel_check_rate)
        lrdc = "NETSTACK_CONF_RDC={}".format(rdc)
        lccr = "NETSTACK_RDC_CHANNEL_CHECK_RATE={}".format(ccr)
        nline = "DEFINES = {},{}\n".format(lrdc, lccr)
        oline = makefile.readlines()
        cline = -1
        found = False
        for idx in range(len(oline)):
            if oline[idx].strip().startswith("DEFINES"):
                search = re.search(
                    'NETSTACK_CONF_RDC=\s*([^\s,]+)\s*,?', oline[idx].strip())
                if search:
                    oline[idx] = oline[idx].replace(search.group(1), rdc)
                else:
                    oline[idx] = oline[idx][:-1] + \
                        ("," if not oline[idx].strip().endswith(
                            ',') else "") + lrdc + "\n"
                search = re.search(
                    'NETSTACK_RDC_CHANNEL_CHECK_RATE=\s*([^\s,]+)\s*,?', oline[idx].strip())
                if search:
                    oline[idx] = oline[idx].replace(search.group(1), ccr)
                else:
                    oline[idx] = oline[idx][:-1] + \
                        ("," if not oline[idx].strip().endswith(
                            ',') else "") + lccr + "\n"
                found = True
            elif oline[idx].strip().startswith("CONTIKI") and cline < 0:
                cline = idx
        if not found:
            oline.insert(cline + 1, nline)

    with open("Makefile", "w") as makefile:
        makefile.writelines(oline)


# Generate the grid simulation file (sensevid-generated.csc)
if not args.cleanup and args.grid is not None:
    print(clr.OKGREEN + clr.BOLD +
          "Generating a {} with {} nodes".format(args.grid, args.nodes) + clr.ENDC)
    tree = ET.parse(args.sim_file)
    root = tree.getroot()
    sim = root.findall('simulation')[0]
    dim = int(sqrt(args.nodes))
    oneX = args.dim_x / (dim - 1)
    oneY = args.dim_y / (dim - 1)
    identifiers = [elt.text for elt in root.findall(
        'simulation/motetype/identifier')]
    for elt in sim.findall('mote'):
        sim.remove(elt)
    for node in range(1, args.nodes + 1):
        mote = ET.SubElement(sim, 'mote')
        ET.SubElement(mote, 'breakpoints')
        ic = ET.SubElement(mote, 'interface_config')
        ic.text = "org.contikios.cooja.interfaces.Position"
        x = ET.SubElement(ic, 'x')
        if args.grid == "grid":
            x.text = str(round(oneX * ((node - 2) % dim) if node >
                         2 else (0 if node == 1 else args.dim_x), 1))
        elif args.grid == "randgrid":
            if random() < 0.5:
                x.text = str(round(args.dim_x / 20 - random() * args.dim_x / 20 + (oneX * (
                    (node - 2) % dim) if node > 2 else (0 if node == 1 else args.dim_x)), 1))
            else:
                x.text = str(round(args.dim_x / 20 + random() * args.dim_x / 20 + (oneX * (
                    (node - 2) % dim) if node > 2 else (0 if node == 1 else args.dim_x)), 1))
        elif args.grid == "random":
            x.text = str(round(random() * float(args.dim_x), 1))
        y = ET.SubElement(ic, 'y')
        if args.grid == "grid":
            y.text = str(round(oneY * ((node - 1) // dim) if node >
                         2 else (0 if node == 1 else args.dim_y), 1))
        elif args.grid == "randgrid":
            if random() < 0.5:
                y.text = str(round(args.dim_y / 20 - random() * args.dim_y / 20 + (oneY * (
                    (node - 2) // dim) if node > 2 else (0 if node == 1 else args.dim_y)), 1))
            else:
                y.text = str(round(args.dim_y / 20 + random() * args.dim_y / 20 + (oneY * (
                    (node - 2) // dim) if node > 2 else (0 if node == 1 else args.dim_y)), 1))
        elif args.grid == "random":
            y.text = str(round(random() * args.dim_y, 1))
        ET.SubElement(ic, 'z').text = "0.0"
        ic = ET.SubElement(mote, 'interface_config')
        ic.text = "org.contikios.cooja.mspmote.interfaces.MspClock"
        dev = ET.SubElement(ic, 'deviation')
        dev.text = "1.0"
        ic = ET.SubElement(mote, 'interface_config')
        ic.text = "org.contikios.cooja.mspmote.interfaces.MspMoteID"
        ET.SubElement(ic, 'id').text = str(node)
        ET.SubElement(
            mote, 'motetype_identifier').text = identifiers[node - 1 if node - 1 < len(identifiers) else -1]
    timeline = root.findall('plugin')
    if len(timeline) > 0:
        for idx, text in enumerate([elt.text for elt in timeline]):
            if 'org.contikios.cooja.plugins.TimeLine' in text:
                pc = timeline[idx].find('plugin_config')
                for elt in pc.findall('mote'):
                    pc.remove(elt)
                for node in range(args.nodes):
                    mote = ET.Element('mote')
                    mote.text = str(node)
                    pc.insert(node, mote)
                break

    def pretty_print(data): return '\n'.join([line for line in parseString(data).toprettyxml(
        indent=' '*2, encoding='UTF-8').decode().split('\n') if line.strip()])
    with open("sensevid-generated.csc", "w") as file:
        file.write(pretty_print(ET.tostring(root, 'utf-8')))
    args.sim_file = "sensevid-generated.csc"


# # Open the sim's file to get all Make cmds
# root = ET.parse(args.sim_file).getroot()
# cmds = list(set([cmd.text for cmd in root.findall(
#     'simulation/motetype/commands')])) if not args.cleanup else []
# # cmds = ["make TARGET=sky clean", "make TARGET=z1 clean",
# #         "make TARGET=wismote clean", "make TARGET=cooja clean"] + cmds
# cmds = ["make TARGET=cooja clean"] + cmds
# for cmd in cmds:
#     _, err = subprocess.Popen(shlex.split(
#         cmd), stdout=subprocess.PIPE, stderr=subprocess.PIPE).communicate()
#     err = err.decode()
#     if err:
#         try:
#             if "error" in err.lower() or "fatal" in err.lower() and not "HEAD" in err:
#                 raise Exception("Error while executing `" +
#                                 cmd + "`:\n" + err + clr.ENDC)
#             else:
#                 print(clr.WARNING + clr.BOLD + "Warning while executing `" +
#                       cmd + "`:\n" + err + clr.ENDC)
#         except Exception:
#             print(clr.FAIL + clr.BOLD)
#             traceback.print_exc()
#             os._exit(1)
#     print(clr.OKGREEN + clr.BOLD +
#           "make: Successfully executed `{}`".format(cmd) + clr.ENDC)

# if args.cleanup:
#     os._exit(0)

# # Make the sim's file and get the next cmd
# startCmd, err = subprocess.Popen(shlex.split("make TARGET=cooja --nogui=Simulation " +
#                                  args.sim_file), stdout=subprocess.PIPE, stderr=subprocess.PIPE).communicate()
# err = err.decode()
# startCmd = startCmd.decode()

# if err or not startCmd.startswith('java '):
#     try:
#         if "error" in err.lower() or "fatal" in err.lower() and not "HEAD" in err:
#             raise Exception("Error while executing make:\n" + startCmd +
#                             ("\n" if startCmd and err else "") + err + clr.ENDC)
#         else:
#             print(clr.WARNING + clr.BOLD + "Warning while executing make:\n" +
#                   startCmd + ("\n" if startCmd and err else "") + err + clr.ENDC)
#     except Exception:
#         print(clr.FAIL + clr.BOLD)
#         traceback.print_exc()
#         os._exit(1)
# print(clr.OKGREEN + clr.BOLD +
#       "make: Successfully compiled {}".format(args.sim_file) + clr.ENDC)
# startCmd = startCmd.replace("quickstart", "-nogui")


# def sim(simProc):
#     # Thread to read and print errors if any occured while creating/running the sim
#     print("\n" + clr.OKGREEN + clr.BOLD + "Starting simulation" + clr.ENDC)
#     error = False

#     def errorOnSim():
#         # Raise when an error occured in Cooja
#         try:
#             raise Exception(
#                 "An error occured while creating or running the simulation" + clr.ENDC)
#         except Exception:
#             print(clr.FAIL + clr.BOLD)
#             if simProc.poll() is None:
#                 print("Ending simulation (Exception)")
#                 os.killpg(os.getpgid(simProc.pid), signal.SIGTERM)
#             traceback.print_exc()
#             os._exit(1)

#     # Reading output of Cooja
#     while True:
#         output = simProc.stdout.readline()
#         if output == '' and simProc.poll() is not None:
#             if error:
#                 errorOnSim()
#             break
#         if output:
#             line = output.strip().decode()
#             # We filter the output of Cooja to get Error/Warning
#             if not line.startswith("INFO") and not line.startswith("Message:") and line != "":
#                 if 'FATAL' in line or 'ERROR' in line:
#                     if not error and simProc.poll() is None:
#                         print(
#                             clr.FAIL + clr.BOLD + "An error has occured, simulation will be stopped soon")
#                         t = threading.Timer(2., errorOnSim)
#                         t.start()
#                     error = True
#                     print(clr.FAIL + clr.BOLD + line + clr.ENDC)
#                 elif 'WARNING' in line:
#                     print(clr.WARNING + clr.BOLD + line + clr.ENDC)
#                 elif args.logs:
#                     print(clr.OKBLUE + line + clr.ENDC)


# # Start the sim in a new thread to avoid blocking the program
# print("StartCmd : ", shlex.split(startCmd))
# simP = subprocess.Popen(shlex.split(
#     startCmd), stdout=subprocess.PIPE, stderr=subprocess.PIPE, preexec_fn=os.setpgrp)
# Sim = threading.Thread(target=sim, args=(simP,))
# Sim.start()


# def simReady(simProc):
# time.sleep(2.5)  # We wait a little to be sure that simulation is ready
# print(clr.OKGREEN + clr.BOLD + "Simulation is ready" + clr.ENDC)

# st_file = open(args.st_file, "r")

# # Start the source
# src = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
# src.connect((args.host, args.source_port))
# print("\n" + clr.OKGREEN + clr.BOLD +
#       "Source connection on {}:{}".format(args.host, args.source_port) + clr.ENDC)

# global srcStop
# srcStop = 0

# def srcSend(i):
#     # Send packets (source)
#     global srcStop
#     time.sleep(.5)
#     oldstime = 0
#     st_file.seek(0)
#     print(clr.OKGREEN + clr.BOLD +
#           "Source is now sending trace {}".format(i) + clr.ENDC)
#     for line in st_file:
#         if i < srcStop:
#             print(clr.FAIL + clr.BOLD +
#                   "Source stopped sending trace {}, you may increase the '-t [TIMEOUT]' value".format(i) + clr.ENDC)
#             break
#         if not line.lstrip().startswith('#'):
#             line_words = line.split()
#             payload_size = line_words[2]
#             seqno = line_words[1]

#             if args.period is None:
#                 time_to_send = float(line_words[0])
#                 ipd = time_to_send - oldstime
#                 oldstime = time_to_send
#             else:
#                 ipd = float(args.period)

#             src.sendall((payload_size + " " + seqno + "\n").encode())

#             print(clr.OKGREEN + "{} {} sleeping for {}".format(seqno,
#                   payload_size, ipd) + clr.ENDC)
#             time.sleep(ipd)
#     if i >= args.traces - 1:
#         time.sleep(.5)
#         st_file.close()
#         # print clr.OKGREEN + clr.BOLD + "Source connection closed" + clr.ENDC
#         # src.close()

# def nc(host, port, timeout):
#     # Used by the sink
#     # Works like the nc cmd, with some modifications (timeout)
#     global srcStop
#     sink = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
#     sink.connect((host, port))
#     print(clr.OKGREEN + clr.BOLD +
#           "Sink is now listening on {}:{}".format(host, port) + clr.ENDC)

#     def cantRecept(i):
#         # Raise when it doesn't recept any packet after a little time
#         global srcStop
#         srcStop = i + 1
#         if i < args.traces - 1:
#             print(clr.FAIL + clr.BOLD +
#                   "Sink hasn't recepted any packet yet, going to next trace" + clr.ENDC)
#         else:
#             try:
#                 raise Exception(
#                     "Sink hasn't recepted any packet yet, you may change the MAC protocol in use" + clr.ENDC)
#             except Exception:
#                 print(clr.FAIL + clr.BOLD)
#                 if simProc.poll() is None:
#                     print("Ending simulation (Exception)")
#                     os.killpg(os.getpgid(simProc.pid), signal.SIGTERM)
#                 traceback.print_exc()
#                 os._exit(1)

#     # Recept and write data (sink)
#     precLine = 0
#     for i in range(args.traces):
#         print("\n" + clr.OKGREEN + clr.BOLD +
#               "Now writing rt_{}".format(i) + clr.ENDC)
#         rt_file = open("rt_{}".format(i), "w")
#         threading.Thread(target=srcSend, args=(i,)).start()
#         sink.settimeout(3.5 * args.timeout)
#         t = threading.Timer(3.5 * args.timeout, cantRecept, [i])
#         t.start()
#         while 1:
#             try:
#                 data = sink.recv(1024)
#             except socket.timeout:
#                 print(clr.OKGREEN + clr.BOLD +
#                       "Sink has timed out" + clr.ENDC)
#                 srcStop = i + 1
#                 break
#             else:
#                 t.cancel()
#                 if not data:
#                     try:
#                         raise Exception("Sink has recepted an EOT, you may increase the timeout value in {}\'s script".format(
#                             args.sim_file) + clr.ENDC)
#                     except Exception:
#                         print(clr.FAIL + clr.BOLD)
#                         if simProc.poll() is None:
#                             print("Ending simulation (Exception)")
#                             os.killpg(os.getpgid(simProc.pid),
#                                       signal.SIGTERM)
#                         traceback.print_exc()
#                         os._exit(1)
#                     break
#                 rt_file.write(data.decode())
#                 sink.settimeout(timeout)

#         # Split logs
#         sink.settimeout(None)
#         print(clr.OKGREEN + clr.BOLD +
#               "Splitting logs into COOJA_{}.testlog".format(i) + clr.ENDC)
#         with open("COOJA.testlog", "r") as logs, open("COOJA_{}.testlog".format(i), "w") as nlogs:
#             line = 0
#             while 1:
#                 lline = logs.readline()
#                 if not lline:
#                     break
#                 line += 1
#                 if line > precLine:
#                     nlogs.write(lline)
#             precLine = line
#         if i < args.traces - 1:
#             print(clr.OKGREEN + clr.BOLD +
#                   "Now waiting {} seconds before next trace".format(args.delay) + clr.ENDC)
#             time.sleep(args.delay)
#     print(clr.OKGREEN + clr.BOLD + "Sink connection closed" + clr.ENDC)
#     sink.close()
#     rt_file.close()

# # Start the sink in a new thread to avoid blocking the program
# sink = threading.Thread(target=nc, args=(
#     args.host, args.sink_port, args.timeout,))
# sink.start()

# sink.join()  # We wait for the Sink to stop, meaning that all packets have been recepted
# print(clr.OKGREEN + clr.BOLD + "Source connection closed" + clr.ENDC)
# src.close()
# if simProc.poll() is None:
#     print(clr.OKGREEN + clr.BOLD + "Ending simulation" + clr.ENDC)
#     os.killpg(os.getpgid(simProc.pid), signal.SIGTERM)
# else:
#     try:
#         raise Exception("Simulation haven't ended up correctly" + clr.ENDC)
#     except Exception:
#         print(clr.FAIL + clr.BOLD)
#         traceback.print_exc()
#         os._exit(1)

# # Rebuilding rt-packet cmd
# print("")
# for i in range(args.traces):
#     print(clr.OKGREEN + clr.BOLD +
#           "Rebuilding rt-packet_{}".format(i) + clr.ENDC)
#     with open("rt-packet_{}".format(i), "w") as out:
#         _, err = subprocess.Popen(shlex.split("awk 'FNR==NR{seq[$1]=$1;next}{ if ($2==seq[$2]) print $0}' rt_" + str(
#             i) + " " + args.st_file), stdout=out, stderr=subprocess.PIPE).communicate()
#     if err:
#         try:
#             raise Exception(err + clr.ENDC)
#         except Exception:
#             print(clr.FAIL + clr.BOLD)
#             traceback.print_exc()
#             os._exit(1)
#     time.sleep(.1)
#     try:
#         if os.stat("rt-packet_{}".format(i)).st_size == 0:
#             raise Exception(
#                 "Error while rebuilding rt-packet_{}".format(i) + clr.ENDC)
#     except Exception:
#         print(clr.FAIL + clr.BOLD)
#         traceback.print_exc()
#         os._exit(1)
#     print(clr.OKGREEN + clr.BOLD +
#           "Successfully rebuilt rt-packet_{}".format(i) + clr.ENDC)

# # If we have a video, we decode received frames using Sensevid to reconstruct it
# if args.video_file:
#     print("")
#     for i in range(args.traces):
#         print(clr.OKGREEN + clr.BOLD +
#               "Decoding received frames {}".format(i) + clr.ENDC)
#         output, err = subprocess.Popen(shlex.split("./sensevid {} -r rt-packet_{}".format(
#             args.video_file, i)), stdout=subprocess.PIPE, stderr=subprocess.PIPE).communicate()
#         if output or err:
#             try:
#                 raise Exception("Please make sure that sensevid is in this folder\n" +
#                                 output + ("\n" if output and err else "") + err + clr.ENDC)
#             except Exception:
#                 print(clr.FAIL + clr.BOLD)
#                 traceback.print_exc()
#                 os._exit(1)
#         print(clr.OKGREEN + clr.BOLD +
#               "Successfully decoded received frames {}".format(i) + clr.ENDC)
#         while 1:
#             time.sleep(.1)
#             if os.path.exists(os.path.join(sim_dir, "rt-frame")):
#                 os.rename(os.path.join(sim_dir, "rt-frame"),
#                           os.path.join(sim_dir, "rt-frame_{}".format(i)))
#                 break
#         while 1:
#             time.sleep(.1)
#             if os.path.isdir(os.path.join(sim_dir, "decodedFrames")):
#                 os.rename(os.path.join(sim_dir, "decodedFrames"), os.path.join(
#                     sim_dir, "decodedFrames_{}".format(i)))
#                 break
# os._exit(0)


# def logReader(simProc):
#     # Read COOJA.testlog while simulation is running
#     def cantRead():
#         # If we can't read the log file
#         try:
#             raise Exception(
#                 "Can't read COOJA.testlog, simulation may have not started correctly" + clr.ENDC)
#         except Exception:
#             print(clr.FAIL + clr.BOLD)
#             if simProc.poll() is None:
#                 print("Ending simulation (Exception)")
#                 os.killpg(os.getpgid(simProc.pid), signal.SIGTERM)
#             traceback.print_exc()
#             os._exit(1)

#     # Binding
#     t = threading.Timer(30., cantRead)
#     t.start()
#     while 1:
#         if os.path.exists("COOJA.testlog"):
#             t.cancel()
#             file = open("COOJA.testlog", 'r')
#             print(clr.OKGREEN + clr.BOLD +
#                   "Successfully binded with Cooja" + clr.ENDC)
#             break
#         time.sleep(.1)

#     def neverReady():
#         # If we can't detect when to start sending
#         try:
#             if not args.safe:
#                 raise Exception(
#                     "Can't autodetect when the simulation is ready, please send a 'READY TO SEND' message and use the '--safeready' option" + clr.ENDC)
#             else:
#                 raise Exception(
#                     "Can't detect the 'READY TO SEND' message, you may not have sent it" + clr.ENDC)
#         except Exception:
#             print(clr.FAIL + clr.BOLD)
#             if simProc.poll() is None:
#                 print("Ending simulation (Exception)")
#                 os.killpg(os.getpgid(simProc.pid), signal.SIGTERM)
#             traceback.print_exc()
#             os._exit(1)

#     # Reading the log
#     num = 0
#     nbMotes = 9999
#     started = False
#     t = threading.Timer(45., neverReady)
#     t.start()
#     if args.dodag:
#         G = nx.DiGraph(directed=True)
#         pos = {}
#         plt.ion()
#     while 1:
#         where = file.tell()
#         line = file.readline()
#         if not line:
#             time.sleep(.1)
#             file.seek(where)
#         else:
#             if args.logs:
#                 print(clr.OKBLUE + "Cooja | " + line[:-1] + clr.ENDC)
#             # DODAG construction
#             if args.dodag and (" Position (" in line.strip() or "PREF " in line.strip() or "RM " in line.strip()):
#                 mote = line[:-1].split(" ")[1][:-1]  # Parent mote
#                 if " Position (" in line.strip():
#                     p = line[:-1].split(" ")[-1][0:-1].split(";")
#                     # p = line[:-1].split(" ")[-1][1:-1].split(";")
#                     pos[mote] = float(p[0]), -float(p[1])
#                     G.add_nodes_from(pos.keys())
#                     print(
#                         clr.OKBLUE + "Mote {} has been placed at ({};{})".format(mote, p[0], p[1]) + clr.ENDC)
#                 elif "PREF " in line.strip():
#                     motePref = line[:-1].split(" ")[-1]
#                     if motePref != "NULL":
#                         motePref = str(int(motePref.split(":")[-2], 16))
#                         G.add_edge(mote, motePref)
#                         print(
#                             clr.OKBLUE + "Mote {} is now the prefered of parent {}".format(motePref, mote) + clr.ENDC)
#                     else:
#                         for edge in G.out_edges(mote):
#                             G.remove_edge(mote, edge)
#                         print(
#                             clr.OKBLUE + "Parent mote {} has no more children".format(mote) + clr.ENDC)
#                 elif "RM " in line.strip():
#                     moteDel = str(
#                         int(line[:-1].split(" ")[-1].split(":")[-2], 16))
#                     G.remove_edge(mote, moteDel)
#                     print(
#                         clr.OKBLUE + "Mote {} is no more the prefered of parent {}".format(moteDel, mote) + clr.ENDC)
#                 plt.clf()
#                 nx.draw(G, pos, with_labels=True, font_color='white', node_color='deepskyblue',
#                         arrowstyle='-|>', edge_color='cyan', node_size=500)
#                 # plt.show()
#                 plt.pause(0.0001)
#             # Ending reader
#             if line.startswith("Test ended"):
#                 break
#             # # Detecting when to start sending
#             # if not started:
#             #     if "Number of motes: " in line:
#             #         nbMotes = int(re.search(r'\d+', line).group())
#             #     if not args.safe:
#             #         if "Starting " in line:
#             #             num += 1
#             #         if num >= nbMotes and not started:
#             #             t.cancel()
#             #             started = True
#             #             threading.Thread(
#             #                 target=simReady, args=(simProc,)).start()
#             #     elif "READY TO SEND" in line:
#             #         t.cancel()
#             #         started = True
#             #         threading.Thread(target=simReady, args=(simProc,)).start()


# """
# process = Process(target=logReader, args=(simP,))
# process.start()
# process.join()
# """
# # Start the logReader in a new thread to avoid blocking the program
# LogReader = threading.Thread(target=logReader, args=(simP,))
# LogReader.start()

