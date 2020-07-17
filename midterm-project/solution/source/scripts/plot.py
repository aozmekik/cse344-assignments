import matplotlib.pyplot as plt
from matplotlib.pyplot import figure

from datetime import datetime
import os


def get_x(file):
    with open(file, 'r') as file:
        data = file.read().replace(':', ' ')
    return [int(s) for s in data.split() if s.isdigit()]

def get_y1(file):
    with open(file, 'r') as file:
        data = file.read()
    return [int(s) for s in data.split() if s.isdigit()]

def get_y(file):
    with open(file, 'r') as file:
        data = file.read().replace('=', ' ')
    return [int(s) for s in data.split() if s.isdigit()]

figure(num=None, figsize=(10, 10), dpi=120, facecolor='w', edgecolor='k')


os.system('./info.sh')


x1 = get_x("../source/x1.txt")
y1 = get_y1("../source/y1.txt")
x2 = x1
y2 = get_y("../source/y2.txt")
x3 = get_x("../source/x3.txt")
y3 = get_y("../source/y3.txt")


plt.subplot(3, 1, 1)
plt.plot(x1, y1, 'ro')
plt.ylabel('# of students at counter')

plt.subplot(3, 1, 2)
plt.plot(x2, y2, 'bo')
plt.ylabel('# of plates at counter')

plt.subplot(3, 1, 3)
plt.plot(x3, y3, 'go')
plt.ylabel('# of plates at kitchen')
plt.xlabel('time')


# plt.axis([0, 800, 0, 10])
now = datetime.now()

current_time = now.strftime("%H:%M:%S")

plt.savefig('plots/graph{0}.png'.format(current_time))
