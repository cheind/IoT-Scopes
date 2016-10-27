import numpy as np
import matplotlib.pyplot as plt
from scipy import signal

ard = np.loadtxt("arduino.csv")[:,0]
hw = np.loadtxt("hardware.csv")[:,0] * 1e6

diffs = np.abs(ard - hw)
plt.plot(hw, diffs)
plt.show()

detrend_diffs = signal.detrend(diffs)
plt.plot(hw, detrend_diffs)
plt.show()



