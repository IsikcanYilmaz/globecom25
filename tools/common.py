#!/usr/bin/env python3

import serial
import time
import subprocess
import sys, os, asyncio
from pprint import pprint

SERIAL_TIMEOUT_S = 10
SERIAL_COMMAND_BUFFER_SIZE = 50
NC_RETRIES = 3

class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[32m'
    WARNING = '\033[93m'
    FAIL = '\033[31m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def background(f):
    def wrapped(*args, **kwargs):
        return asyncio.get_event_loop().run_in_executor(None, f, *args, **kwargs)
    return wrapped

def interact(): # debug
    import code
    code.InteractiveConsole(locals=globals()).interact()

def sendSerialCommand_local(dev, cmd, cooldownS=0.5, captureOutput=True):
    s = dev["ser"]
    s.reset_input_buffer()
    s.reset_output_buffer()
    while(len(cmd) > 0):
        s.write(cmd[0:SERIAL_COMMAND_BUFFER_SIZE].encode())
        cmd = cmd[SERIAL_COMMAND_BUFFER_SIZE:]
        time.sleep(0.05)
    s.write("\n".encode())
    time.sleep(cooldownS)
    if (captureOutput):
        resp = s.read(s.in_waiting).decode()
        return resp

def sendSerialCommand_fitiot(dev, cmd, cooldownS=1, captureOutput=True):
    procCmd = f"echo \'{cmd}\' | nc -q {cooldownS} {dev['name']} 20000"
    out = ""
    trial = 0
    while (trial <= NC_RETRIES):
        proc = subprocess.Popen(procCmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        time.sleep(cooldownS)
        try:
            out = proc.communicate()[0].decode()
        except Exception as e:
            print("Error occured:", e)
        proc.terminate()
        proc.wait()
        if ("refused" in out):
            trial += 1
            print(f"Connection refused. Trying again {trial}/{NC_RETRIES}")
            time.sleep(1)
        else:
            break
    if (trial > NC_RETRIES):
        print("sendSerialCommand_fitiot failed! NC Connection refused")
    if (captureOutput):
        return out

def flushDevice_local(dev):
    s = dev["ser"]
    s.write("\n\n\n".encode())
    s.read(s.in_waiting)
    s.reset_input_buffer()
    s.reset_output_buffer()
    s.read(s.in_waiting)

def flushDevice_fitiot(dev):
    sendSerialCommand_fitiot(dev, "\n\n", captureOutput=False)

def resetDevice_local(dev):
    s = dev["ser"]
    s.write("\n\nreboot\n".encode())
    s.reset_input_buffer()
    s.reset_output_buffer()
    time.sleep(1)

def resetDevice_fitiot(dev):
    sendSerialCommand_fitiot(dev, "reboot", cooldownS=4, captureOutput=False)

# Hacky. Text from the board isnt cleanly separated. sometimes it comes with extra lines that arent json. this separates them
def parseDirtyJson(text):
    for i in text.split("\n"):
        if "{" in i:
            return i

class Device: # TODO CURRENTLY UNUSED
    def __init__(self):
        pass

class DeviceCommunicator:
    def __init__(self, fitiot=False, serialAggStr=None):
        self.fitiot = fitiot
        self.serialAgg = False

        if (fitiot):
            self.hostname = os.uname().nodename
        else:
            self.hostname = "local"

        if (fitiot and serialAggStr):
            self.serialAgg = True
            pass
    
    def sendSerialCommand(self, dev, cmd, cooldownS=2, captureOutput=True, printOut=True):
        out = ""
        nameStr = f"{bcolors.OKCYAN}{dev['name']}{bcolors.ENDC}"
        if (printOut):
            print(f"{nameStr} > {cmd}")
        if (self.fitiot):
            out = sendSerialCommand_fitiot(dev, cmd, cooldownS, captureOutput)
        else:
            out = sendSerialCommand_local(dev, cmd, cooldownS, captureOutput)
        if (printOut):
            print(f"{nameStr} < {out}")
        if captureOutput:
            return out
        return

    def flushDevice(self, dev):
        if (self.fitiot):
            return flushDevice_fitiot(dev)
        else:
            return flushDevice_local(dev)

    def resetDevice(self, dev):
        if (self.fitiot):
            return resetDevice_fitiot(dev)
        else:
            return resetDevice_local(dev)

