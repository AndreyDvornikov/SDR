import numpy as np
import matplotlib.pyplot as plt

# Открываем файл для чтения
name = "txdata.pcm"

data = []
imag = []
real = []
count = []
counter = 0
with open(name, "rb") as f:
    index = 0
    while (byte := f.read(2)):
        if(index %2 == 0):
            real.append(int.from_bytes(byte, byteorder='little', signed=True))
            counter += 1
            count.append(counter)
        else:
            imag.append(int.from_bytes(byte, byteorder='little', signed=True))
        index += 1
        
# Инициализируем список для хранения данных
namet = "rxdata.pcm"
d = []
i = []
r = []
co = []
countr = 0
with open(namet, "rb") as f:
    ind = 0
    while (byte := f.read(2)):
        if (ind % 2 == 0):
            r.append(int.from_bytes(byte, byteorder='little', signed=True))
            countr += 1
            co.append(countr)
        else:
            i.append(int.from_bytes(byte, byteorder='little', signed=True))
        ind += 1

fig, axs = plt.subplots(2, 1, layout='constrained')
axs[0].plot(count,(imag),color='red', label = 'Q')  # Используем scatter для диаграммы созвездия
axs[0].plot(count,(real), color='blue', label = 'I')  # Используем scatter для диаграммы созвездия
axs[0].set_title("RX")
axs[0].legend()
n = min(len(co), len(r), len(i))
axs[1].plot(co[:n],(r[:n]), color = 'blue', label = 'I')
axs[1].plot(co[:n],(i[:n]), color = 'red', label = 'Q')
axs[1].set_title("TX")
axs[1].legend()
plt.show()
