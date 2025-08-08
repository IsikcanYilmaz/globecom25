#!/usr/bin/env python3

import serial 
import argparse
import time
import sys, os
import json
import pdb
import traceback
from common import *
from pprint import pprint

DEFAULT_RESULTS_DIR = "./results/"
SERIAL_TIMEOUT_S = 10
EXPERIMENT_TIMEOUT_S = 60*10
MULTITHREADED = True

devices = {'sender':None, 'receiver':None, 'routers':[]}
ifaceId = None # We assume this is the same number for all devices
args = None
comm = None

threads = []
futures = []

"""
Example usage: ./automator.py /dev/ttyACM0 /dev/ttyACM1 --rpl --experiment --results_dir ../results/
"""

def parseIfconfig(dev, rawStr):
    global ifaceId
    iface = None
    hwaddr = None
    linkLocalAddr = None
    globalAddr = None
    for i in rawStr.replace("  ", " ").split("\n"):
        i = i.lstrip()
        if ("Iface" in i):
            iface = i.split(" ")[1]
            if (not ifaceId):
                ifaceId = iface
                print(f"INTERFACE ID {ifaceId}")
        if ("HWaddr" in i):
            hwaddr = i.split(" ")[2]
            dev["hwaddr"] = hwaddr
        if ("fe80" in i):
            linkLocalAddr = i.split(" ")[2]
            dev["linkLocalAddr"] = linkLocalAddr
        if ("global" in i):
            globalAddr = i.split(" ")[2]
            dev["globalAddr"] = globalAddr
    success = True
    if ("globalAddr" not in dev.keys()):
        print(f"{bcolors.FAIL}{dev['name']} globalAddr not picked up!{bcolors.ENDC}")
        # success = False
    if ("linkLocalAddr" not in dev.keys()):
        print(f"{bcolors.FAIL}{dev['name']} linkLocalAddr not picked up!{bcolors.ENDC}")
        success = False
    return success

def getL2Stats(dev):
    global comm
    outStrRaw = comm.sendSerialCommand(dev, f"ifconfig {ifaceId} stats l2")

def getIpv6Stats(dev):
    global comm
    outStrRaw = comm.sendSerialCommand(dev, f"ifconfig {ifaceId} stats ipv6")

def resetNetstats(dev):
    global comm
    outStrRaw = comm.sendSerialCommand(dev, f"ifconfig {ifaceId} stats all reset")
            
def getAddresses(dev, exitOnFail=True):
    global comm
    success = False
    count = 0
    print(f"Getting addresses for {dev['name']}")
    while (not success and count < 4):
        outStrRaw = comm.sendSerialCommand(dev, "ifconfig", cooldownS=2)
        success = parseIfconfig(dev, outStrRaw)
        if (success):
            break
        print(f"{bcolors.FAIL}Problem running getAddresses on {dev['name']}... Trying again {count} {bcolors.ENDC}")
        time.sleep(1)
        count += 1
    if (not success):
        print(f"{bcolors.FAIL}GET ADDRESSES FAILED FOR {dev['name']}{bcolors.ENDC}")
        if (exitOnFail):
            sys.exit(1)

def setGlobalAddress(dev):
    global comm
    outStrRaw = comm.sendSerialCommand(dev, f"ifconfig {ifaceId} add 2001::{dev['id']}")

def unsetGlobalAddress(dev):
    global comm
    outStrRaw = comm.sendSerialCommand(dev, f"ifconfig {ifaceId} del {dev['globalAddr']}")

def unsetRpl(dev):
    global comm
    outStrRaw = comm.sendSerialCommand(dev, f"rpl rm {ifaceId}")

def setRplRoot(dev):
    global comm
    outStrRaw = comm.sendSerialCommand(dev, f"rpl root {ifaceId} 2001::{dev['id']}")

@background
def sendCmdBackground(dev, cmd):
    global comm, args
    cooldownS = (2 if args.fitiot else 0.5)
    comm.sendSerialCommand(dev, cmd, cooldownS=cooldownS)

@background
def setManualRoutesSingleDevice(idx, dev):
    cooldownS = (2 if args.fitiot else 0.5)
    nextHop = ""
    prevHop = ""
    if idx == len(devices["routers"])-1: # Last router in the line. Next hop is the rx
        nextHop = devices["receiver"]["linkLocalAddr"]
        prevHop = devices["routers"][idx-1]["linkLocalAddr"] if len(devices["routers"])>1 else devices["sender"]["linkLocalAddr"]
    elif idx == 0: # First router in the line
        nextHop = devices["routers"][idx+1]["linkLocalAddr"] if len(devices["routers"])>1 else devices["receiver"]["linkLocalAddr"]
        prevHop = devices["sender"]["linkLocalAddr"]
    else: # Router after 0th and before nth
        nextHop = devices["routers"][idx+1]["linkLocalAddr"]
        prevHop = devices["routers"][idx-1]["linkLocalAddr"]

    print(f"Setting R{idx} nextHop:{nextHop} prevHop:{prevHop}")
    # tx->rx
    outStrRaw = comm.sendSerialCommand(dev, f"nib route add {ifaceId} {devices['receiver']['globalAddr']} {nextHop}", cooldownS=cooldownS)
    # rx->tx
    outStrRaw = comm.sendSerialCommand(dev, f"nib route add {ifaceId} {devices['sender']['globalAddr']} {prevHop}", cooldownS=cooldownS)

# NOTE AND TODO: This only sets the nib entries for the source and the destination basically. if you want any of the other nodes to be reachable you'll haveto consider the logic for it
async def setManualRoutes(devices):
    global comm, args
    if (len(devices["routers"]) == 0):
        return
    print("Setting routes manually...")

    cooldownS = (1.5 if args.fitiot else 0.5)

    # From the sender to the receiver
    print(f"Setting Sender->Receiver {devices['routers'][0]['linkLocalAddr']}")
    outStrRaw = comm.sendSerialCommand(devices["sender"], f"nib route add {ifaceId} {devices['receiver']['globalAddr']} {devices['routers'][0]['linkLocalAddr']}", cooldownS=0.5) # Sender routes thru first router towards receiver

    # From the receiver to the sender
    print(f"Setting Receiver->Sender {devices['routers'][-1]['linkLocalAddr']}")
    outStrRaw = comm.sendSerialCommand(devices["receiver"], f"nib route add {ifaceId} {devices['sender']['globalAddr']} {devices['routers'][-1]['linkLocalAddr']}", cooldownS=0.5) # Receiver routes thru last router towards sender

    futures = []
    for idx, dev in enumerate(devices["routers"]):
        future = setManualRoutesSingleDevice(idx, dev)
        futures.append(future)
    time.sleep(1)
    await asyncio.gather(*futures)

def setIperfTarget(dev, targetGlobalAddr):
    global comm
    outStrRaw = comm.sendSerialCommand(dev, f"iperf target {targetGlobalAddr}")

def pingTest(srcDev, dstDev):
    global comm
    dstIp = dstDev["globalAddr"]
    print(f"{srcDev['globalAddr']} Pinging {dstIp}")
    outStrRaw = comm.sendSerialCommand(srcDev, f"ping {dstIp}", cooldownS=5, captureOutput=True)
    if ("100% packet loss" in outStrRaw):
        print(bcolors.FAIL + "PING TEST FAILED!!!!" + bcolors.ENDC)
    else:
        print(bcolors.OKGREEN + "Ping test passed." + bcolors.ENDC)

def setTxPower(dev, txpower):
    global comm
    outStrRaw = comm.sendSerialCommand(dev, f"setpwr {txpower}")

def setRetrans(dev, retrans):
    global comm, args
    outStrRaw = ""
    if (args.fitiot):
        outStrRaw = comm.sendSerialCommand(dev, f"ifconfig {ifaceId} set csma_retries {retrans}")
        outStrRaw += comm.sendSerialCommand(dev, f"ifconfig {ifaceId} set retrans {retrans}")
    else:
        outStrRaw = comm.sendSerialCommand(dev, f"setretrans {retrans}")

def setAllDevicesRetrans(retrans):
    global args, devices
    if (args.retrans != None):
        print(f"Setting L2 retransmissions to {args.retrans}")
        setRetrans(devices["sender"], args.retrans)
        setRetrans(devices["receiver"], args.retrans)
        for dev in devices["routers"]:
            setRetrans(dev, args.retrans)

def parseDeviceJsons(j, caching=False):
    global args
    # Expects {"rx":{}, "tx":{}, "relays":{}, "config":{}}
    if (caching):
        timeDiffSecs = j["rx"]["timeDiff"] / 1000000
    else:
        timeDiffSecs = j["tx"]["timeDiff"] / 1000000
    numLostPackets = j["tx"]["numSentPkts"] - (j["rx"]["numReceivedPkts"] - j["rx"]["numDuplicates"])
    lossPercent = numLostPackets * 100 / j["tx"]["numSentPkts"]
    sendRate = j["tx"]["numSentPkts"] * j["config"]["payloadSizeBytes"] / timeDiffSecs
    receiveRate = (j["rx"]["numReceivedPkts"] - j["rx"]["numDuplicates"]) * j["config"]["payloadSizeBytes"] / timeDiffSecs
    cacheHits = sum([i["results"]["cacheHits"] for i in j["relays"]])
    return {"timeDiffSecs":timeDiffSecs, "numLostPackets":numLostPackets, "lossPercent":lossPercent, "sendRate":sendRate, "receiveRate":receiveRate, "sumCacheHits":cacheHits}

def averageRoundsJsons(j):
    avgNumLostPkts = sum([j[i]["results"]["numLostPackets"] for i in range(0, len(j))])/len(j)
    avgLossPercent = sum([j[i]["results"]["lossPercent"] for i in range(0, len(j))])/len(j)
    avgSendRate = sum([j[i]["results"]["sendRate"] for i in range(0, len(j))])/len(j)
    avgReceiveRate = sum([j[i]["results"]["receiveRate"] for i in range(0, len(j))])/len(j)
    avgTimeDiffSecs = sum([j[i]["results"]["timeDiffSecs"] for i in range(0, len(j))])/len(j)
    avgSumCacheHits = sum([j[i]["results"]["sumCacheHits"] for i in range(0, len(j))])/len(j)
    return {"avgLostPackets":avgNumLostPkts, "avgLossPercent":avgLossPercent, "avgSendRate":avgSendRate, "avgReceiveRate":avgReceiveRate, "avgTimeDiffSecs":avgTimeDiffSecs, "avgSumCacheHits":avgSumCacheHits}

async def resetAllDevicesNetstats():
    global devices, ifaceId
    resetNetstats(devices["sender"])
    resetNetstats(devices["receiver"])
    futures = [] 
    for dev in devices["routers"]:
        # resetNetstats(dev)
        future = sendCmdBackground(dev, f"ifconfig {ifaceId} stats all reset")
        futures.append(future)
    time.sleep(1)
    await asyncio.gather(*futures)

async def restartAllDevices():
    global devices, comm
    print("Restarting all devices")
    comm.sendSerialCommand(devices["sender"], "iperf restart")
    comm.sendSerialCommand(devices["receiver"], "iperf restart")
    futures = []
    for dev in devices["routers"]:
        # comm.sendSerialCommand(dev, "iperf restart")
        future = sendCmdBackground(dev, "iperf restart")
        futures.append(future)
    time.sleep(1)
    await asyncio.gather(*futures)

def flushAllDevices():
    global devices, comm
    print("Flushing all")
    comm.flushDevice(devices["sender"])
    comm.flushDevice(devices["receiver"])
    for dev in devices["routers"]:
        comm.flushDevice(dev)

def experiment(mode=1, delayus=50000, payloadsizebytes=32, transfersizebytes=4096, rounds=1, resultsDir="./"):
    global devices, comm, args
    txDev = devices["sender"]
    rxDev = devices["receiver"]
    routers = devices["routers"]

    outFilenamePrefix = f"m{mode}_delay{delayus}_pl{payloadsizebytes}_tx{transfersizebytes}_routers{len(devices['routers'])}"
    overallJson = []

    averagesFilename = f"{resultsDir}/{outFilenamePrefix}_averages.json"
    experimentFilename = f"{resultsDir}/{outFilenamePrefix}.json"

    for round in range(0, rounds):
        roundFilename = f"{resultsDir}/{outFilenamePrefix}_round{round}.json"

        # The experiment may have been run before and bombed. 
        # Check if the experiment was run before. if so, simply load up that round json
        # if ()

        txOut = ""
        rxOut = ""

        print("----------------")
        print(f"EXPERIMENT mode:{mode} delayus:{delayus} payloadsizebytes:{payloadsizebytes} transfersizebytes:{transfersizebytes} round:{round}")

        comm.flushDevice(rxDev)
        comm.flushDevice(txDev)

        resetAllDevicesNetstats()

        rxOut += comm.sendSerialCommand(rxDev, f"iperf config mode {mode} delayus {delayus} payloadsizebytes {payloadsizebytes} transfersizebytes {transfersizebytes}", cooldownS=3)
        txOut += comm.sendSerialCommand(txDev, f"iperf config mode {mode} delayus {delayus} payloadsizebytes {payloadsizebytes} transfersizebytes {transfersizebytes}", cooldownS=3)

        rxOut += comm.sendSerialCommand(rxDev, "iperf receiver")
        txOut += comm.sendSerialCommand(txDev, "iperf sender start")

        print("RX:", rxOut)
        print("TX:", txOut)
        
        now = time.time()

        if (args.fitiot):
            expectedTime = (delayus / 1000000) * (transfersizebytes / payloadsizebytes)
            time.sleep(expectedTime + 60) # TODO better output handling
        else:
            txSer = txDev["ser"]
            rxSer = rxDev["ser"]
            while("done" not in txOut):
                if (txSer.in_waiting > 0):
                    raw = txSer.readline().decode()
                    # print(f">{raw}")
                    txOut += raw
                if (time.time() - now > EXPERIMENT_TIMEOUT_S):
                    print("EXPERIMENT TIMEOUT")
                    return
                time.sleep(0.1)
            rxOut += rxSer.read(rxSer.in_waiting).decode()

        # TODO Hacky parsing below. Could do better formatting on the fw side
        parsingSuccess = False
        for i in range(0, 3):
            rxJsonRaw = comm.sendSerialCommand(rxDev, "iperf results all", captureOutput=True, cooldownS=1).replace("[IPERF][I] ", "").split("\n")[1:-1]
            # print("<", rxJsonRaw)
            txJsonRaw = comm.sendSerialCommand(txDev, "iperf results all", captureOutput=True, cooldownS=1).replace("[IPERF][I] ", "").split("\n")[1:-1]
            # print("<", txJsonRaw)

            rxJsonRaw = " ".join(rxJsonRaw)
            txJsonRaw = " ".join(txJsonRaw)

            try:
                rxJson = json.loads(rxJsonRaw)
                txJson = json.loads(txJsonRaw)
                parsingSuccess = True
                break # If success, break out of the loop. otherwise, try 3 times
            except Exception as e:
                print(traceback.format_exc())
                print("trying again")
                comm.flushDevice(rxDev)
                comm.flushDevice(txDev)
                time.sleep(1)

        if not parsingSuccess:
            print("Couldnt parse json results!")
            return

        rxOut += rxJsonRaw
        txOut += txJsonRaw

        resetAllDevicesNetstats()

        rxOut += comm.sendSerialCommand(rxDev, "iperf stop")
        txOut += comm.sendSerialCommand(txDev, "iperf stop")

        deviceJson = {"rx":rxJson["results"], "tx":txJson["results"], "config":txJson["config"]}
        roundOverallJson = {"deviceoutput":deviceJson, "results":parseDeviceJsons(deviceJson)}

        with open(roundFilename, "w") as f:
            json.dump(roundOverallJson, f, indent=4)

        pprint(roundOverallJson)
        overallJson.append(roundOverallJson)

    with open(experimentFilename, "w") as f:
        json.dump(overallJson, f, indent=4)

    overallAveragesJson = averageRoundsJsons(overallJson)
    with open(averagesFilename, "w") as f:
        json.dump(overallAveragesJson, f, indent=4)

    print(f"Results written to {resultsDir}")
    print("----------------")

def bulkExperiments(resultsDir):
    # Create directory if needed
    if (not os.path.isdir(resultsDir)):
        print(f"{resultsDir} not found! Creating")
        try:
            os.mkdir(resultsDir)
        except Exception as e:
            print(e)
            print(f"Exception while creating results dir {resultsDir}. Will use . as resultsDir")
            resultsDir = "./"

    delayUsArr = [5000, 10000, 15000, 20000, 25000, 30000]
    payloadSizeArr = [32, 16, 8]
    transferSizeArr = [4096]
    rounds = 20
    mode = 1

    # Write down the config
    with open(f"{resultsDir}/config.txt", "w") as f:
        f.write(" ".join(sys.argv))
        f.write(f"\ndelayUsArr:{delayUsArr}, payloadSizeArr:{payloadSizeArr}, transferSizeArr:{transferSizeArr}, rounds:{rounds}")

    # Sweep
    experimentCount = 0
    numExperiments = len(delayUsArr) * len(payloadSizeArr) * len(transferSizeArr)
    for delayUs in delayUsArr:
        for payloadSize in payloadSizeArr:
            for transferSize in transferSizeArr:
                print(f"Running experiment {experimentCount}/{numExperiments}")
                experiment(1, delayUs, payloadSize, transferSize, rounds, resultsDir)
                experimentCount += 1
                time.sleep(2)

async def cachingExperiment(delayus=10000, payloadsizebytes=32, transfersizebytes=4096, rounds=1, cache=1, numcacheblocks=16, resultsDir="./"):
    global devices, comm, args
    txDev = devices["sender"]
    rxDev = devices["receiver"]
    routers = devices["routers"]
    
    outFilenamePrefix = f"cache{cache}_numcache{numcacheblocks}_delay{delayus}_pl{payloadsizebytes}_tx{transfersizebytes}_routers{len(devices['routers'])}"
    overallJson = []

    averagesFilename = f"{resultsDir}/{outFilenamePrefix}_averages.json"
    experimentFilename = f"{resultsDir}/{outFilenamePrefix}.json"

    for round in range(0, rounds):
        roundFilename = f"{resultsDir}/{outFilenamePrefix}_round{round}.json"

        # The experiment may have been run before and bombed. 
        # Check if the experiment was run before. if so, simply load up that round json
        roundCompletedBefore = False
        if (os.path.basename(roundFilename) in os.listdir(resultsDir)):
            try:
                f = open(roundFilename)
                j = json.load(f)
                f.close()
                overallJson.append(j)
                print(f"{bcolors.OKGREEN}{roundFilename} Round file already there. Moving on{bcolors.ENDC}")
                continue
            except Exception as e:
                print("Error reading old round file!", e)
                
        txOut = ""
        rxOut = ""

        print("----------------")
        print(f"EXPERIMENT delayus:{delayus} payloadsizebytes:{payloadsizebytes} transfersizebytes:{transfersizebytes} round:{round}")

        comm.flushDevice(rxDev)
        comm.flushDevice(txDev)

        await resetAllDevicesNetstats()

        rxOut += comm.sendSerialCommand(rxDev, f"iperf config mode 2 delayus {delayus} plsize {payloadsizebytes} xfer {transfersizebytes} cache {cache}", cooldownS=3)
        txOut += comm.sendSerialCommand(txDev, f"iperf config mode 2 delayus {delayus} plsize {payloadsizebytes} xfer {transfersizebytes} cache {cache}", cooldownS=3)

        for r in devices["routers"]:
            comm.flushDevice(r)

        futures = []
        for r in devices["routers"]:
            # comm.sendSerialCommand(r, f"iperf config mode 2 delayus {delayus} plsize {payloadsizebytes} xfer {transfersizebytes} cache {cache}")
            future = sendCmdBackground(r, f"iperf config mode 2 delayus {delayus} plsize {payloadsizebytes} xfer {transfersizebytes} cache {cache}")
            futures.append(future)
        time.sleep(1)
        await asyncio.gather(*futures)

        futures = []
        for r in devices["routers"]:
            # comm.sendSerialCommand(r, "iperf relayer")
            future = sendCmdBackground(r, f"iperf config numcacheblocks {numcacheblocks}")
            futures.append(future)
        time.sleep(1)
        await asyncio.gather(*futures)

        futures = []
        for r in devices["routers"]:
            # comm.sendSerialCommand(r, "iperf relayer")
            future = sendCmdBackground(r, "iperf relayer")
            futures.append(future)
        time.sleep(1)
        await asyncio.gather(*futures)

        rxOut += comm.sendSerialCommand(rxDev, "iperf receiver", cooldownS=1)
        txOut += comm.sendSerialCommand(txDev, "iperf sender start")

        print("RX:", rxOut)
        print("TX:", txOut)
        
        now = time.time()

        if (args.fitiot):
            expectedTime = (delayus / 1000000) * (transfersizebytes / payloadsizebytes)
            time.sleep(expectedTime + (30 if cache else 10)) # TODO better output handling
        else:
            txSer = txDev["ser"]
            rxSer = rxDev["ser"]
            while("complete" not in rxOut):
                if (txSer.in_waiting > 0 or rxSer.in_waiting > 0):
                    raw = rxSer.read(rxSer.in_waiting).decode()
                    print(f"rx> {raw}")
                    rxOut += raw
                    raw = txSer.read(txSer.in_waiting).decode()
                    print(f"tx> {raw}")
                    txOut += raw
                if (time.time() - now > EXPERIMENT_TIMEOUT_S):
                    print("EXPERIMENT TIMEOUT")
                    return
                print(f"{rxSer.in_waiting} {time.time() - now}")
                time.sleep(0.1)
            txOut += txSer.read(txSer.in_waiting).decode()
            rxOut += rxSer.read(rxSer.in_waiting).decode()

        flushAllDevices()

        # TODO Hacky parsing below. Could do better formatting on the fw side
        parsingSuccess = False
        for i in range(0, 3):
            rxJsonRaw = comm.sendSerialCommand(rxDev, "iperf results all", captureOutput=True, cooldownS=1).replace("[IPERF][I] ", "").split("\n")[1:-1]
            txJsonRaw = comm.sendSerialCommand(txDev, "iperf results all", captureOutput=True, cooldownS=1).replace("[IPERF][I] ", "").split("\n")[1:-1]

            rxJsonRaw = " ".join(rxJsonRaw)
            txJsonRaw = " ".join(txJsonRaw)

            try:
                rxJson = json.loads(rxJsonRaw)
                txJson = json.loads(txJsonRaw)
                parsingSuccess = True
                break # If success, break out of the loop. otherwise, try 3 times
            except Exception as e:
                print(traceback.format_exc())
                print("trying again")
                comm.flushDevice(rxDev)
                comm.flushDevice(txDev)
                time.sleep(1)

        if not parsingSuccess:
            print(f"{bcolors.FAIL}Couldnt parse json results!{bcolors.ENDC}")
            return

        rxOut += rxJsonRaw
        txOut += txJsonRaw

       # get router results
        routerJson = []
        for dev in devices["routers"]:
            rJsonRaw = comm.sendSerialCommand(dev, "iperf results all", captureOutput=True, cooldownS=1).replace("[IPERF][I] ", "").split("\n")[1:-1]
            # print("<", rJsonRaw)
            rJsonRaw = " ".join(rJsonRaw)
            rJson = json.loads(rJsonRaw)
            routerJson.append(rJson)

        await resetAllDevicesNetstats()
        time.sleep(1)
        await restartAllDevices()
        time.sleep(1)

        deviceJson = {"rx":rxJson["results"], "tx":txJson["results"], "config":txJson["config"]} 

        deviceJson["relays"] = routerJson

        try:
            roundOverallJson = {"deviceoutput":deviceJson, "results":parseDeviceJsons(deviceJson, True)}
        except Exception as e:
            print("Error occurred while processing device json!", e)
            print(traceback.format_exc()) 
            sys.exit(2)

        with open(roundFilename, "w") as f:
            json.dump(roundOverallJson, f, indent=4)

        pprint(roundOverallJson)
        overallJson.append(roundOverallJson)

    with open(experimentFilename, "w") as f:
        json.dump(overallJson, f, indent=4)

    overallAveragesJson = averageRoundsJsons(overallJson)
    with open(averagesFilename, "w") as f:
        json.dump(overallAveragesJson, f, indent=4)

    print(f"Results written to {resultsDir}")
    print("----------------")

def bulkCachingExperiments(resultsDir):
    pass

def tester(dev):
    getL2Stats(dev)
    getIpv6Stats(dev)
    resetNetstats(dev)

async def setRoles():
    outStrRaw = comm.sendSerialCommand(devices["sender"], f"iperf sender")
    outStrRaw = comm.sendSerialCommand(devices["receiver"], f"iperf receiver")
    futures = []
    for dev in devices["routers"]:
        # outStrRaw = comm.sendSerialCommand(dev, "iperf relayer")
        future = sendCmdBackground(dev, "iperf relayer")
        futures.append(future)
    time.sleep(1)
    await asyncio.gather(*futures)

def main():
    global args, comm
    parser = argparse.ArgumentParser()
    parser.add_argument("sender")
    parser.add_argument("receiver")
    parser.add_argument("-r", "--router", nargs="*")
    parser.add_argument("--rpl", action="store_true", default=False)
    parser.add_argument("--experiment_test", action="store_true", default=False)
    parser.add_argument("--experiment", action="store_true", default=False)
    parser.add_argument("--caching_experiment", action="store_true", default=False)
    parser.add_argument("--fitiot", action="store_true", default=False)
    parser.add_argument("--results_dir", type=str)
    parser.add_argument("--txpower", type=int)
    parser.add_argument("--retrans", type=int)
    parser.add_argument("--test", action="store_true", default=False)
    parser.add_argument("--set_roles", action="store_true", default=False)
    args = parser.parse_args()

    # print(args)
    # return

    print(f"SENDER {args.sender}, RECEIVER {args.receiver}, ROUTER(s) {args.router}")
    print(f"RPL {args.rpl}, FITIOT {args.fitiot}, EXPERIMENT {args.experiment}")

    comm = DeviceCommunicator(args.fitiot)

    if (args.fitiot): # TODO make this dictionary a class and have this distinction logic be done in its constructor
        devices["sender"] = {"name":args.sender, "id":1}
        devices["receiver"] = {"name":args.receiver}
    else:
        devices["sender"] = {"name":args.sender, "id":1, "ser":serial.Serial(args.sender, timeout=SERIAL_TIMEOUT_S)}
        devices["receiver"] = {"name":args.receiver, "ser":serial.Serial(args.receiver, timeout=SERIAL_TIMEOUT_S)}

    comm.flushDevice(devices["sender"])
    comm.flushDevice(devices["receiver"])

    getAddresses(devices["sender"])
    getAddresses(devices["receiver"])

    if (args.txpower != None):
        print(f"Setting txpowers to {args.txpower}")
        setTxPower(devices["sender"], args.txpower)
        setTxPower(devices["receiver"], args.txpower)
        for dev in devices["routers"]:
            setTxPower(dev, args.txpower)

    unsetRpl(devices["receiver"])
    unsetRpl(devices["sender"])

    if ("globalAddr" in devices["sender"]): # TODO still needs some work: if one of the devices reboots, its easier to reboot every device and have them go thru this script all over again
        unsetGlobalAddress(devices["sender"])

    if ("globalAddr" in devices["receiver"]):
        unsetGlobalAddress(devices["receiver"])

    if (args.router):
        for j, i in enumerate(args.router):
            if (args.fitiot):
                r = {"name":i, "id":j+2}
            else:
                r = {"name":i, "id":j+2, "ser":serial.Serial(i, timeout=SERIAL_TIMEOUT_S)}
            comm.flushDevice(r)
            getAddresses(r)
            unsetRpl(r)
            if ("globalAddr" in r):
                unsetGlobalAddress(r)
            setGlobalAddress(r)
            getAddresses(r)
            devices["routers"].append(r)
    devices["receiver"]["id"] = len(devices["routers"])+2

    flushAllDevices()

    setGlobalAddress(devices["sender"])
    getAddresses(devices["sender"])
    setGlobalAddress(devices["receiver"])
    getAddresses(devices["receiver"]) # may not be needed? 

    setAllDevicesRetrans(args.retrans)

    if (args.rpl):
        setRplRoot(devices["sender"])
    else:
        asyncio.run(setManualRoutes(devices))

    if (len(devices["routers"]) > 0):
        setIperfTarget(devices["sender"], devices["receiver"]["globalAddr"])
        for dev in devices["routers"]:
            setIperfTarget(dev, devices["receiver"]["globalAddr"])

    if (args.set_roles):
        asyncio.run(setRoles())

    # pdb.set_trace()

    pingTest(devices["sender"], devices["receiver"])

    pprint(devices)

    if (args.test):
        tester(devices["sender"])
        tester(devices["receiver"])
        return

    if (args.experiment_test):
        #asyncio.run(cachingExperiment(delayus= 50000, cache=1, rounds=500))
        #asyncio.run(cachingExperiment(delayus= 50000, cache=1, numcacheblocks=8, rounds=500))
        asyncio.run(cachingExperiment(delayus= 50000, cache=1, numcacheblocks=4, rounds=500))
        asyncio.run(cachingExperiment(delayus= 50000, cache=1, numcacheblocks=16, rounds=500))
        asyncio.run(cachingExperiment(delayus= 50000, cache=1, numcacheblocks=1, rounds=500))

    if (args.results_dir):
        args.results_dir = os.path.abspath(args.results_dir)

    elif (args.caching_experiment):
        bulkCachingExperiments((args.results_dir if args.results_dir else DEFAULT_RESULTS_DIR))
    elif (args.experiment):
        bulkExperiments(resultsDir=(args.results_dir if args.results_dir else DEFAULT_RESULTS_DIR))
    print("DONE")
    

if __name__ == "__main__":
    main()
