# CellBalancer_Attiny
Simple cell balancer using Attiny 85 with internal 1MHz osccilator, with serial communication.
Designed to use with LiFePo4 (working range 2.8V - 5V), balancing current max. 4A.
[https://github.com/DanoPTT/CellBalancer_Attiny/blob/master/Pictures/Module%20test.jpg|alt=octocat]
Serial communication uses 4800 baud, but with INVERTED levels, so false 0V, true is Vcc.
To communicate with standard UART simple level converter with optoisolation is used

