EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 4 5
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
Text GLabel 6450 3000 2    50   Input ~ 0
OPAMP3_VIN+
Text GLabel 5050 4850 0    50   Input ~ 0
DAC1_OUT1
Text GLabel 5650 6000 2    50   Input ~ 0
OPAMP2_VIN-
Text GLabel 5650 5900 2    50   Input ~ 0
OPAMP2_VIN+
Text GLabel 7250 3050 2    50   Input ~ 0
OPAMP3_VOUT
Text GLabel 6450 3100 2    50   Input ~ 0
OPAMP3_VIN-
$Comp
L Connector:AudioJack2 J6
U 1 1 61ACF631
P 4750 1550
F 0 "J6" H 4782 1875 50  0000 C CNN
F 1 "6.35mm Jack" H 4782 1784 50  0000 C CNN
F 2 "STM32AudioEffects:Jack_6.35mm_Neutrik_NMJ4HFD2_Horizontal" H 4750 1550 50  0001 C CNN
F 3 "~" H 4750 1550 50  0001 C CNN
	1    4750 1550
	1    0    0    -1  
$EndComp
Text GLabel 5050 1550 2    50   Input ~ 0
LINE_IN
Text GLabel 5900 1550 0    50   Input ~ 0
LINE_OUT
$Comp
L power:GND #PWR052
U 1 1 61AD0392
P 5450 1800
F 0 "#PWR052" H 5450 1550 50  0001 C CNN
F 1 "GND" H 5455 1627 50  0000 C CNN
F 2 "" H 5450 1800 50  0001 C CNN
F 3 "" H 5450 1800 50  0001 C CNN
	1    5450 1800
	1    0    0    -1  
$EndComp
Text GLabel 4950 3000 0    50   Input ~ 0
LINE_IN
$Comp
L Device:R_Small_US R14
U 1 1 61AD3C82
P 5000 1700
F 0 "R14" H 5068 1746 50  0000 L CNN
F 1 "1M" H 5068 1655 50  0000 L CNN
F 2 "Resistor_SMD:R_0402_1005Metric" H 5000 1700 50  0001 C CNN
F 3 "~" H 5000 1700 50  0001 C CNN
	1    5000 1700
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C19
U 1 1 61AD446C
P 5200 3000
F 0 "C19" V 4971 3000 50  0000 C CNN
F 1 "10u" V 5062 3000 50  0000 C CNN
F 2 "Capacitor_SMD:C_0805_2012Metric" H 5200 3000 50  0001 C CNN
F 3 "~" H 5200 3000 50  0001 C CNN
	1    5200 3000
	0    1    1    0   
$EndComp
$Comp
L Device:R_Small_US R16
U 1 1 61AD4C26
P 5200 2550
F 0 "R16" V 4995 2550 50  0000 C CNN
F 1 "220k" V 5086 2550 50  0000 C CNN
F 2 "Resistor_SMD:R_0402_1005Metric" H 5200 2550 50  0001 C CNN
F 3 "~" H 5200 2550 50  0001 C CNN
	1    5200 2550
	0    1    1    0   
$EndComp
$Comp
L Device:R_Small_US R17
U 1 1 61AD6034
P 5500 2550
F 0 "R17" V 5295 2550 50  0000 C CNN
F 1 "220k" V 5386 2550 50  0000 C CNN
F 2 "Resistor_SMD:R_0402_1005Metric" H 5500 2550 50  0001 C CNN
F 3 "~" H 5500 2550 50  0001 C CNN
	1    5500 2550
	0    1    1    0   
$EndComp
$Comp
L power:GND #PWR055
U 1 1 61AD6449
P 5050 2600
F 0 "#PWR055" H 5050 2350 50  0001 C CNN
F 1 "GND" H 5055 2427 50  0000 C CNN
F 2 "" H 5050 2600 50  0001 C CNN
F 3 "" H 5050 2600 50  0001 C CNN
	1    5050 2600
	1    0    0    -1  
$EndComp
Wire Wire Line
	5050 2600 5050 2550
Wire Wire Line
	5050 2550 5100 2550
$Comp
L power:+3.3V #PWR?
U 1 1 61AD7CA9
P 5700 2500
AR Path="/61AD7CA9" Ref="#PWR?"  Part="1" 
AR Path="/61AB9E49/61AD7CA9" Ref="#PWR054"  Part="1" 
F 0 "#PWR054" H 5700 2350 50  0001 C CNN
F 1 "+3.3V" H 5715 2673 50  0000 C CNN
F 2 "" H 5700 2500 50  0001 C CNN
F 3 "" H 5700 2500 50  0001 C CNN
	1    5700 2500
	1    0    0    -1  
$EndComp
Wire Wire Line
	5600 2550 5700 2550
Wire Wire Line
	5700 2550 5700 2500
Wire Wire Line
	5300 2550 5350 2550
Wire Wire Line
	5350 2550 5350 3000
Wire Wire Line
	5350 3000 5300 3000
Connection ~ 5350 2550
Wire Wire Line
	5350 2550 5400 2550
$Comp
L power:GND #PWR051
U 1 1 61AD8590
P 5000 1800
F 0 "#PWR051" H 5000 1550 50  0001 C CNN
F 1 "GND" H 5005 1627 50  0000 C CNN
F 2 "" H 5000 1800 50  0001 C CNN
F 3 "" H 5000 1800 50  0001 C CNN
	1    5000 1800
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small_US R18
U 1 1 61AD87CD
P 5500 3000
F 0 "R18" V 5295 3000 50  0000 C CNN
F 1 "1.2k" V 5386 3000 50  0000 C CNN
F 2 "Resistor_SMD:R_0402_1005Metric" H 5500 3000 50  0001 C CNN
F 3 "~" H 5500 3000 50  0001 C CNN
	1    5500 3000
	0    1    1    0   
$EndComp
Wire Wire Line
	5350 3000 5400 3000
Connection ~ 5350 3000
$Comp
L Device:R_Small_US R19
U 1 1 61AD8C52
P 5800 3000
F 0 "R19" V 5595 3000 50  0000 C CNN
F 1 "24k" V 5686 3000 50  0000 C CNN
F 2 "Resistor_SMD:R_0402_1005Metric" H 5800 3000 50  0001 C CNN
F 3 "~" H 5800 3000 50  0001 C CNN
	1    5800 3000
	0    1    1    0   
$EndComp
Wire Wire Line
	5600 3000 5650 3000
Wire Wire Line
	5650 3000 5650 2700
Connection ~ 5650 3000
Wire Wire Line
	5650 3000 5700 3000
$Comp
L Device:C_Small C20
U 1 1 61AD98DE
P 6000 3200
F 0 "C20" H 5908 3154 50  0000 R CNN
F 1 "1n" H 5908 3245 50  0000 R CNN
F 2 "Capacitor_SMD:C_0402_1005Metric" H 6000 3200 50  0001 C CNN
F 3 "~" H 6000 3200 50  0001 C CNN
	1    6000 3200
	-1   0    0    1   
$EndComp
Wire Wire Line
	5900 3000 6000 3000
Wire Wire Line
	6000 3000 6000 3100
$Comp
L power:GND #PWR056
U 1 1 61ADA109
P 6000 3300
F 0 "#PWR056" H 6000 3050 50  0001 C CNN
F 1 "GND" H 6005 3127 50  0000 C CNN
F 2 "" H 6000 3300 50  0001 C CNN
F 3 "" H 6000 3300 50  0001 C CNN
	1    6000 3300
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C18
U 1 1 61ADA567
P 6350 2700
F 0 "C18" V 6579 2700 50  0000 C CNN
F 1 "1n" V 6488 2700 50  0000 C CNN
F 2 "Capacitor_SMD:C_0402_1005Metric" H 6350 2700 50  0001 C CNN
F 3 "~" H 6350 2700 50  0001 C CNN
	1    6350 2700
	0    -1   -1   0   
$EndComp
$Comp
L power:GND #PWR057
U 1 1 61ADAC6A
P 6300 3950
F 0 "#PWR057" H 6300 3700 50  0001 C CNN
F 1 "GND" H 6305 3777 50  0000 C CNN
F 2 "" H 6300 3950 50  0001 C CNN
F 3 "" H 6300 3950 50  0001 C CNN
	1    6300 3950
	1    0    0    -1  
$EndComp
$Comp
L Device:CP1_Small C21
U 1 1 61ADB04D
P 6300 3850
F 0 "C21" H 6391 3896 50  0000 L CNN
F 1 "10u" H 6391 3805 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric" H 6300 3850 50  0001 C CNN
F 3 "~" H 6300 3850 50  0001 C CNN
	1    6300 3850
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small_US R21
U 1 1 61ADB7AB
P 6300 3650
F 0 "R21" H 6232 3604 50  0000 R CNN
F 1 "2.4k" H 6232 3695 50  0000 R CNN
F 2 "Resistor_SMD:R_0402_1005Metric" H 6300 3650 50  0001 C CNN
F 3 "~" H 6300 3650 50  0001 C CNN
	1    6300 3650
	-1   0    0    1   
$EndComp
$Comp
L Device:R_Small_US R20
U 1 1 61ADBB07
P 6500 3450
F 0 "R20" V 6705 3450 50  0000 C CNN
F 1 "27k" V 6614 3450 50  0000 C CNN
F 2 "Resistor_SMD:R_0402_1005Metric" H 6500 3450 50  0001 C CNN
F 3 "~" H 6500 3450 50  0001 C CNN
	1    6500 3450
	0    -1   -1   0   
$EndComp
Wire Wire Line
	6300 3100 6450 3100
Wire Wire Line
	5650 2700 6250 2700
Wire Wire Line
	6450 3000 6000 3000
Connection ~ 6000 3000
Wire Wire Line
	6450 2700 7100 2700
Wire Wire Line
	7100 2700 7100 3050
Wire Wire Line
	7100 3050 7250 3050
Wire Wire Line
	4950 1550 5000 1550
Wire Wire Line
	5000 1550 5000 1600
Connection ~ 5000 1550
Wire Wire Line
	5000 1550 5050 1550
$Comp
L Device:R_Small_US R15
U 1 1 61AEF228
P 5950 1700
F 0 "R15" H 5883 1746 50  0000 R CNN
F 1 "1M" H 5883 1655 50  0000 R CNN
F 2 "Resistor_SMD:R_0402_1005Metric" H 5950 1700 50  0001 C CNN
F 3 "~" H 5950 1700 50  0001 C CNN
	1    5950 1700
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR053
U 1 1 61AEF443
P 5950 1800
F 0 "#PWR053" H 5950 1550 50  0001 C CNN
F 1 "GND" H 5955 1627 50  0000 C CNN
F 2 "" H 5950 1800 50  0001 C CNN
F 3 "" H 5950 1800 50  0001 C CNN
	1    5950 1800
	1    0    0    -1  
$EndComp
Wire Wire Line
	5900 1550 5950 1550
Wire Wire Line
	5950 1600 5950 1550
Connection ~ 5950 1550
Wire Wire Line
	5950 1550 6000 1550
$Comp
L Connector:AudioJack2 J7
U 1 1 61ACFCB4
P 6200 1550
F 0 "J7" H 6250 1900 50  0000 R CNN
F 1 "6.35mm Jack" H 6450 1800 50  0000 R CNN
F 2 "STM32AudioEffects:Jack_6.35mm_Neutrik_NMJ4HFD2_Horizontal" H 6200 1550 50  0001 C CNN
F 3 "~" H 6200 1550 50  0001 C CNN
	1    6200 1550
	-1   0    0    -1  
$EndComp
Wire Wire Line
	4950 3000 5100 3000
$Comp
L Device:C_Small C23
U 1 1 61AFB92F
P 5300 4850
F 0 "C23" V 5071 4850 50  0000 C CNN
F 1 "10u" V 5162 4850 50  0000 C CNN
F 2 "Capacitor_SMD:C_0805_2012Metric" H 5300 4850 50  0001 C CNN
F 3 "~" H 5300 4850 50  0001 C CNN
	1    5300 4850
	0    1    1    0   
$EndComp
Wire Wire Line
	5050 4850 5200 4850
Wire Wire Line
	6450 5950 6350 5950
Wire Wire Line
	6350 5950 6350 6150
Wire Wire Line
	6350 6150 5550 6150
Wire Wire Line
	5550 6150 5550 6000
Wire Wire Line
	5550 6000 5650 6000
Text GLabel 6550 4950 2    50   Input ~ 0
OPAMP4_VIN-
Text GLabel 7350 4900 2    50   Input ~ 0
OPAMP4_VOUT
$Comp
L Device:R_Small_US R22
U 1 1 61B1A8AD
P 5600 4850
F 0 "R22" V 5395 4850 50  0000 C CNN
F 1 "3.3k" V 5486 4850 50  0000 C CNN
F 2 "Resistor_SMD:R_0402_1005Metric" H 5600 4850 50  0001 C CNN
F 3 "~" H 5600 4850 50  0001 C CNN
	1    5600 4850
	0    1    1    0   
$EndComp
$Comp
L Device:R_Small_US R23
U 1 1 61B1A8B8
P 5900 4850
F 0 "R23" V 5695 4850 50  0000 C CNN
F 1 "4.3k" V 5786 4850 50  0000 C CNN
F 2 "Resistor_SMD:R_0603_1608Metric" H 5900 4850 50  0001 C CNN
F 3 "~" H 5900 4850 50  0001 C CNN
	1    5900 4850
	0    1    1    0   
$EndComp
Wire Wire Line
	5700 4850 5750 4850
Wire Wire Line
	5750 4850 5750 4550
Connection ~ 5750 4850
Wire Wire Line
	5750 4850 5800 4850
$Comp
L Device:C_Small C24
U 1 1 61B1A8C6
P 6100 5050
F 0 "C24" H 6008 5004 50  0000 R CNN
F 1 "1n" H 6008 5095 50  0000 R CNN
F 2 "Capacitor_SMD:C_0402_1005Metric" H 6100 5050 50  0001 C CNN
F 3 "~" H 6100 5050 50  0001 C CNN
	1    6100 5050
	-1   0    0    1   
$EndComp
Wire Wire Line
	6000 4850 6100 4850
Wire Wire Line
	6100 4850 6100 4950
$Comp
L power:GND #PWR058
U 1 1 61B1A8D2
P 6100 5150
F 0 "#PWR058" H 6100 4900 50  0001 C CNN
F 1 "GND" H 6105 4977 50  0000 C CNN
F 2 "" H 6100 5150 50  0001 C CNN
F 3 "" H 6100 5150 50  0001 C CNN
	1    6100 5150
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C22
U 1 1 61B1A8DC
P 6450 4550
F 0 "C22" V 6679 4550 50  0000 C CNN
F 1 "2.2n" V 6588 4550 50  0000 C CNN
F 2 "Capacitor_SMD:C_0805_2012Metric" H 6450 4550 50  0001 C CNN
F 3 "~" H 6450 4550 50  0001 C CNN
	1    6450 4550
	0    -1   -1   0   
$EndComp
Wire Wire Line
	6400 4950 6550 4950
Wire Wire Line
	5750 4550 6350 4550
Wire Wire Line
	6550 4850 6100 4850
Connection ~ 6100 4850
Wire Wire Line
	6550 4550 7200 4550
Wire Wire Line
	7200 4550 7200 4900
Wire Wire Line
	7200 4900 7350 4900
Wire Wire Line
	5400 4850 5500 4850
Wire Wire Line
	7200 4900 7200 5100
Wire Wire Line
	7200 5100 6400 5100
Wire Wire Line
	6400 5100 6400 4950
Connection ~ 7200 4900
Text GLabel 5150 5650 0    50   Input ~ 0
OPAMP4_VOUT
$Comp
L Device:R_POT_US RV2
U 1 1 61B2A186
P 5400 5900
F 0 "RV2" H 5332 5946 50  0000 R CNN
F 1 "10k" H 5332 5855 50  0000 R CNN
F 2 "STM32AudioEffects:Potentiometer_Alps_RK09K_Single_Vertical" H 5400 5900 50  0001 C CNN
F 3 "~" H 5400 5900 50  0001 C CNN
	1    5400 5900
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR059
U 1 1 61B2CEC4
P 5400 6050
F 0 "#PWR059" H 5400 5800 50  0001 C CNN
F 1 "GND" H 5405 5877 50  0000 C CNN
F 2 "" H 5400 6050 50  0001 C CNN
F 3 "" H 5400 6050 50  0001 C CNN
	1    5400 6050
	1    0    0    -1  
$EndComp
Wire Wire Line
	5150 5650 5400 5650
Wire Wire Line
	5400 5650 5400 5750
Text Notes 4550 5500 0    50   ~ 0
Output Volume Control
Text Notes 4550 4300 0    50   ~ 0
DAC Lowpass Filter
Text Notes 4550 2200 0    50   ~ 0
Input Anti-Aliasing Filter
Text Notes 4500 1100 0    50   ~ 0
Input/Output 6.35mm Jacks
$Comp
L Device:R_POT_US RV1
U 1 1 61B40921
P 6850 3450
F 0 "RV1" V 6737 3450 50  0000 C CNN
F 1 "10k" V 6646 3450 50  0000 C CNN
F 2 "STM32AudioEffects:Potentiometer_Alps_RK09K_Single_Vertical" H 6850 3450 50  0001 C CNN
F 3 "~" H 6850 3450 50  0001 C CNN
	1    6850 3450
	0    -1   -1   0   
$EndComp
Connection ~ 7100 3050
Wire Wire Line
	5550 5900 5650 5900
Text Label 5350 3000 3    50   ~ 0
IN_OPAMP_BIAS
Text GLabel 6550 4850 2    50   Input ~ 0
OPAMP4_VIN+
Wire Wire Line
	6000 1450 5450 1450
Wire Wire Line
	5450 1800 5450 1450
Connection ~ 5450 1450
Wire Wire Line
	5450 1450 4950 1450
Wire Wire Line
	7100 3050 7100 3300
Wire Wire Line
	6300 3100 6300 3450
Wire Wire Line
	7100 3450 7000 3450
Wire Wire Line
	6700 3450 6600 3450
Wire Wire Line
	6400 3450 6300 3450
Connection ~ 6300 3450
Wire Wire Line
	6300 3450 6300 3550
Wire Wire Line
	6850 3300 7100 3300
Connection ~ 7100 3300
Wire Wire Line
	7100 3300 7100 3450
Text GLabel 6450 5950 2    50   Input ~ 0
OPAMP2_VOUT
Text Notes 6500 6200 0    39   ~ 0
Note: See power.sch for ON/OFF + bypass DPDT switch
$EndSCHEMATC
