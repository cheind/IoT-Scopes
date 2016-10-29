"""
    This file is part of DigitalScope - a software for capturing digital input events.

    Copyright(C) 2016 Christoph Heindl

    All rights reserved.
    This software may be modified and distributed under the terms
    of the BSD license.See the LICENSE file for details.

    This Python script connects to your Arduino via serial connection and
    records signals sent by DigitalScope. This script is compatible with any
    example program found in examples\ directory. 
"""

import traceback
import sys
import time
import argparse
import signal
import uuid
import os.path
import os
from queue import Queue, Empty

from io import StringIO
from serial import Serial
from serial.threaded import LineReader, ReaderThread

import numpy as np
import matplotlib.pyplot as plt

def parseArgs():
    parser = argparse.ArgumentParser(prog="receive")
    parser.add_argument("-p", "--port", help="Serial port Arduino is connected to", required=True)
    parser.add_argument("-b", "--baudrate", type=int, help="Baudrate of communication", default="9600")
    parser.add_argument("--show-signal", help="Display signal as interactive diagram", action="store_true") 
    parser.add_argument("--out-dir", help="Directory to store captured CSV files to", default="./")

    args = parser.parse_args()
    return args

def showSignal(data):
    plt.title("DigitalScope - Capture")
    plt.xlabel("Time $us$")
    plt.ylabel("State")
    plt.step(data[:,0], data[:,1], where='post')
    plt.ylim(-0.5, 1.5)
    plt.show()
    plt.close()

args = parseArgs()
q = Queue()
done = False

class SerialIO(LineReader):

    def connection_made(self, transport):        
        super(SerialIO, self).connection_made(transport)
        print("Connection established")

    def handle_line(self, line):
        args = line.split(" ", 1)
        if len(args) != 2:
            return
        choice = {
            "LOG": self.onLog,
            "DATA": self.onData,
        }.get(args[0], self.onUnknown)
        choice(args[1])

    def connection_lost(self, exc):
        if exc:
            print("Failed {}".format(exc))
        print("Connection closed")
        done = True

    def onLog(self, msg):
        print("ARDUINO {}".format(msg))
    
    def onUnknown(self, data):
        print("Unknown {}".format(data))

    def onData(self, data):
        arr = np.loadtxt(StringIO(data), dtype=int)
        arr = arr.reshape((-1,2))            
        filename = str(uuid.uuid4())[:8] + ".csv"
        np.savetxt(os.path.join(args.out_dir, filename), arr, fmt="%d", header="Time(us) State(HIGH/LOW)")
        print("ARDUINO Data captured, saved to {}".format(filename))
        if args.show_signal:
           q.put(arr)

def signal_handler(signal, frame):
    print("Cleaning up...")
    global done
    done = True

def main():
    signal.signal(signal.SIGINT, signal_handler)
    print("Press Ctrl+C to quit")

    if not os.path.exists(args.out_dir):
        os.makedirs(args.out_dir)
   
    ser = Serial(port=args.port, baudrate=args.baudrate, timeout=1)
    with ReaderThread(ser, SerialIO) as protocol:
        while not done:
            try:                
                arr = q.get(False)
                if args.show_signal:
                    showSignal(arr)
            except Empty:
                pass
            time.sleep(0.1)        

if __name__ == "__main__":
    main()