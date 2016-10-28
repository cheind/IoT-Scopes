import numpy as np
import matplotlib.pyplot as plt
from scipy import signal

ard = np.loadtxt("arduino.csv")[:,0] / 1e6
hw = np.loadtxt("hardware.csv")[:,0]

diffs = (hw - ard) * 1e6
plt.plot(hw, diffs)
plt.title("Timing differences over time")
plt.xlabel("Time $s$")
plt.ylabel("Timing differences $us$ (hardware - arduino)")
plt.show()


detrend_diffs = signal.detrend(diffs)
plt.plot(hw, detrend_diffs)
plt.title("Detrended timing differences over time")
plt.xlabel("Time $s$")
plt.ylabel("Timing differences $us$ (hardware - arduino)")
plt.show()

print("Mean {}, Std {}".format(np.mean(detrend_diffs), np.std(detrend_diffs)))

