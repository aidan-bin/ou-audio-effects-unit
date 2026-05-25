EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 3 5
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L Connector_Generic:Conn_02x05_Odd_Even J?
U 1 1 61AAE6F2
P 5850 3150
AR Path="/61AAE6F2" Ref="J?"  Part="1" 
AR Path="/61AA9AB7/61AAE6F2" Ref="J2"  Part="1" 
F 0 "J2" H 5900 3567 50  0000 C CNN
F 1 "SWD" H 5900 3476 50  0000 C CNN
F 2 "Connector_PinHeader_1.27mm:PinHeader_2x05_P1.27mm_Vertical_SMD" H 5850 3150 50  0001 C CNN
F 3 "~" H 5850 3150 50  0001 C CNN
	1    5850 3150
	1    0    0    -1  
$EndComp
Text GLabel 6150 2950 2    50   Input ~ 0
SWDIO
Text GLabel 6150 3050 2    50   Input ~ 0
SWDCLK
Text GLabel 6150 3150 2    50   Input ~ 0
SWO
$Comp
L power:GND #PWR?
U 1 1 61AAE6FB
P 5600 3350
AR Path="/61AAE6FB" Ref="#PWR?"  Part="1" 
AR Path="/61AA9AB7/61AAE6FB" Ref="#PWR044"  Part="1" 
F 0 "#PWR044" H 5600 3100 50  0001 C CNN
F 1 "GND" H 5605 3177 50  0000 C CNN
F 2 "" H 5600 3350 50  0001 C CNN
F 3 "" H 5600 3350 50  0001 C CNN
	1    5600 3350
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR?
U 1 1 61AAE701
P 5650 2950
AR Path="/61AAE701" Ref="#PWR?"  Part="1" 
AR Path="/61AA9AB7/61AAE701" Ref="#PWR042"  Part="1" 
F 0 "#PWR042" H 5650 2800 50  0001 C CNN
F 1 "+3.3V" V 5650 3200 50  0000 C CNN
F 2 "" H 5650 2950 50  0001 C CNN
F 3 "" H 5650 2950 50  0001 C CNN
	1    5650 2950
	0    -1   -1   0   
$EndComp
Text GLabel 6150 3350 2    50   Input ~ 0
NRST
NoConn ~ 5650 3250
NoConn ~ 6150 3250
Wire Wire Line
	5600 3350 5600 3150
Wire Wire Line
	5600 3150 5650 3150
Wire Wire Line
	5650 3050 5600 3050
Wire Wire Line
	5600 3050 5600 3150
Connection ~ 5600 3150
Wire Wire Line
	5650 3350 5600 3350
Connection ~ 5600 3350
$Comp
L Connector:USB_B_Micro J?
U 1 1 61AB4338
P 3600 4350
AR Path="/61AB4338" Ref="J?"  Part="1" 
AR Path="/61AA9AB7/61AB4338" Ref="J4"  Part="1" 
F 0 "J4" H 3600 4850 50  0000 C CNN
F 1 "USB_B_Micro" H 3600 4750 50  0000 C CNN
F 2 "STM32AudioEffects:USB_Micro-B_Molex-105017-0001" H 3750 4300 50  0001 C CNN
F 3 "~" H 3750 4300 50  0001 C CNN
	1    3600 4350
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR?
U 1 1 61AB433E
P 3600 4750
AR Path="/61AB433E" Ref="#PWR?"  Part="1" 
AR Path="/61AA9AB7/61AB433E" Ref="#PWR049"  Part="1" 
F 0 "#PWR049" H 3600 4500 50  0001 C CNN
F 1 "GND" H 3605 4577 50  0000 C CNN
F 2 "" H 3600 4750 50  0001 C CNN
F 3 "" H 3600 4750 50  0001 C CNN
	1    3600 4750
	1    0    0    -1  
$EndComp
NoConn ~ 3500 4750
$Comp
L Power_Protection:USBLC6-2SC6 U?
U 1 1 61AB4345
P 3600 3100
AR Path="/61AB4345" Ref="U?"  Part="1" 
AR Path="/61AA9AB7/61AB4345" Ref="U5"  Part="1" 
F 0 "U5" H 4000 2800 50  0000 C CNN
F 1 "USBLC6-2SC6" H 4000 2700 50  0000 C CNN
F 2 "Package_TO_SOT_SMD:SOT-23-6" H 3600 2600 50  0001 C CNN
F 3 "https://www.st.com/resource/en/datasheet/usblc6-2.pdf" H 3800 3450 50  0001 C CNN
	1    3600 3100
	1    0    0    -1  
$EndComp
$Comp
L power:VBUS #PWR?
U 1 1 61AB434B
P 3900 4150
AR Path="/61AB434B" Ref="#PWR?"  Part="1" 
AR Path="/61AA9AB7/61AB434B" Ref="#PWR046"  Part="1" 
F 0 "#PWR046" H 3900 4000 50  0001 C CNN
F 1 "VBUS" V 3915 4278 50  0000 L CNN
F 2 "" H 3900 4150 50  0001 C CNN
F 3 "" H 3900 4150 50  0001 C CNN
	1    3900 4150
	0    1    1    0   
$EndComp
NoConn ~ 3900 4550
Text GLabel 3900 4350 2    50   Input ~ 0
USB_CONN_D+
Text GLabel 3900 4450 2    50   Input ~ 0
USB_CONN_D-
$Comp
L power:VBUS #PWR?
U 1 1 61AB4354
P 3600 2700
AR Path="/61AB4354" Ref="#PWR?"  Part="1" 
AR Path="/61AA9AB7/61AB4354" Ref="#PWR041"  Part="1" 
F 0 "#PWR041" H 3600 2550 50  0001 C CNN
F 1 "VBUS" H 3615 2873 50  0000 C CNN
F 2 "" H 3600 2700 50  0001 C CNN
F 3 "" H 3600 2700 50  0001 C CNN
	1    3600 2700
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR?
U 1 1 61AB435A
P 3600 3500
AR Path="/61AB435A" Ref="#PWR?"  Part="1" 
AR Path="/61AA9AB7/61AB435A" Ref="#PWR045"  Part="1" 
F 0 "#PWR045" H 3600 3250 50  0001 C CNN
F 1 "GND" H 3605 3327 50  0000 C CNN
F 2 "" H 3600 3500 50  0001 C CNN
F 3 "" H 3600 3500 50  0001 C CNN
	1    3600 3500
	1    0    0    -1  
$EndComp
Text GLabel 3200 3200 0    50   Input ~ 0
USB_CONN_D-
Text GLabel 4000 3200 2    50   Input ~ 0
USB_CONN_D+
Text GLabel 4000 3000 2    50   Input ~ 0
USB_D+
Text GLabel 3200 3000 0    50   Input ~ 0
USB_D-
Text Notes 3500 2400 0    50   ~ 0
USB
Text Notes 5750 2650 0    50   ~ 0
Debug
Text GLabel 5800 4450 0    50   Input ~ 0
PC1
Text GLabel 5800 4550 0    50   Input ~ 0
PC2
Text GLabel 5800 4650 0    50   Input ~ 0
PC3
Text GLabel 5800 4750 0    50   Input ~ 0
PC4
$Comp
L power:GND #PWR?
U 1 1 61BE6A55
P 5800 4350
AR Path="/61BE6A55" Ref="#PWR?"  Part="1" 
AR Path="/61AA9AB7/61BE6A55" Ref="#PWR048"  Part="1" 
F 0 "#PWR048" H 5800 4100 50  0001 C CNN
F 1 "GND" V 5805 4222 50  0000 R CNN
F 2 "" H 5800 4350 50  0001 C CNN
F 3 "" H 5800 4350 50  0001 C CNN
	1    5800 4350
	0    1    1    0   
$EndComp
$Comp
L power:+3.3V #PWR?
U 1 1 61BE6EA4
P 5800 4250
AR Path="/61BE6EA4" Ref="#PWR?"  Part="1" 
AR Path="/61AA9AB7/61BE6EA4" Ref="#PWR047"  Part="1" 
F 0 "#PWR047" H 5800 4100 50  0001 C CNN
F 1 "+3.3V" V 5815 4378 50  0000 L CNN
F 2 "" H 5800 4250 50  0001 C CNN
F 3 "" H 5800 4250 50  0001 C CNN
	1    5800 4250
	0    -1   -1   0   
$EndComp
$Comp
L Connector_Generic:Conn_01x06 J5
U 1 1 61C25278
P 6000 4450
F 0 "J5" H 6080 4442 50  0000 L CNN
F 1 "Conn_01x06" H 6080 4351 50  0000 L CNN
F 2 "Connector_PinHeader_1.27mm:PinHeader_1x06_P1.27mm_Vertical" H 6000 4450 50  0001 C CNN
F 3 "~" H 6000 4450 50  0001 C CNN
	1    6000 4450
	1    0    0    -1  
$EndComp
Text Notes 5450 4100 0    50   ~ 0
Spare GPIO Header
Text GLabel 7700 3050 0    50   Input ~ 0
LINE_IN
Text GLabel 7700 3250 0    50   Input ~ 0
LINE_OUT
$Comp
L power:GND #PWR?
U 1 1 61C5DE2A
P 7700 3150
AR Path="/61AB9E49/61C5DE2A" Ref="#PWR?"  Part="1" 
AR Path="/61AA9AB7/61C5DE2A" Ref="#PWR043"  Part="1" 
F 0 "#PWR043" H 7700 2900 50  0001 C CNN
F 1 "GND" V 7705 3022 50  0000 R CNN
F 2 "" H 7700 3150 50  0001 C CNN
F 3 "" H 7700 3150 50  0001 C CNN
	1    7700 3150
	0    1    1    0   
$EndComp
$Comp
L Connector_Generic:Conn_01x03 J?
U 1 1 61C5DE30
P 7900 3150
AR Path="/61AB9E49/61C5DE30" Ref="J?"  Part="1" 
AR Path="/61AA9AB7/61C5DE30" Ref="J3"  Part="1" 
F 0 "J3" H 7980 3192 50  0000 L CNN
F 1 "Conn_01x03" H 7980 3101 50  0000 L CNN
F 2 "Connector_PinHeader_1.27mm:PinHeader_1x03_P1.27mm_Vertical" H 7900 3150 50  0001 C CNN
F 3 "~" H 7900 3150 50  0001 C CNN
	1    7900 3150
	1    0    0    -1  
$EndComp
Text Notes 7400 2900 0    50   ~ 0
Audio I/O Header
$Comp
L Mechanical:MountingHole_Pad H1
U 1 1 61D3DBEE
P 7600 4050
F 0 "H1" V 7554 4200 50  0000 L CNN
F 1 "MountingHole_Pad" V 7645 4200 50  0000 L CNN
F 2 "MountingHole:MountingHole_4.3mm_M4_Pad_Via" H 7600 4050 50  0001 C CNN
F 3 "~" H 7600 4050 50  0001 C CNN
	1    7600 4050
	0    1    1    0   
$EndComp
$Comp
L Mechanical:MountingHole_Pad H2
U 1 1 61D3DFAD
P 7600 4250
F 0 "H2" V 7554 4400 50  0000 L CNN
F 1 "MountingHole_Pad" V 7645 4400 50  0000 L CNN
F 2 "MountingHole:MountingHole_4.3mm_M4_Pad_Via" H 7600 4250 50  0001 C CNN
F 3 "~" H 7600 4250 50  0001 C CNN
	1    7600 4250
	0    1    1    0   
$EndComp
$Comp
L Mechanical:MountingHole_Pad H3
U 1 1 61D3E538
P 7600 4450
F 0 "H3" V 7554 4600 50  0000 L CNN
F 1 "MountingHole_Pad" V 7645 4600 50  0000 L CNN
F 2 "MountingHole:MountingHole_4.3mm_M4_Pad_Via" H 7600 4450 50  0001 C CNN
F 3 "~" H 7600 4450 50  0001 C CNN
	1    7600 4450
	0    1    1    0   
$EndComp
$Comp
L Mechanical:MountingHole_Pad H4
U 1 1 61D3E72B
P 7600 4650
F 0 "H4" V 7554 4800 50  0000 L CNN
F 1 "MountingHole_Pad" V 7645 4800 50  0000 L CNN
F 2 "MountingHole:MountingHole_4.3mm_M4_Pad_Via" H 7600 4650 50  0001 C CNN
F 3 "~" H 7600 4650 50  0001 C CNN
	1    7600 4650
	0    1    1    0   
$EndComp
$Comp
L power:GND #PWR?
U 1 1 61D3EC09
P 7400 4750
AR Path="/61D3EC09" Ref="#PWR?"  Part="1" 
AR Path="/61AA9AB7/61D3EC09" Ref="#PWR050"  Part="1" 
F 0 "#PWR050" H 7400 4500 50  0001 C CNN
F 1 "GND" H 7405 4577 50  0000 C CNN
F 2 "" H 7400 4750 50  0001 C CNN
F 3 "" H 7400 4750 50  0001 C CNN
	1    7400 4750
	1    0    0    -1  
$EndComp
Wire Wire Line
	7400 4750 7400 4650
Wire Wire Line
	7400 4050 7500 4050
Wire Wire Line
	7500 4250 7400 4250
Connection ~ 7400 4250
Wire Wire Line
	7400 4250 7400 4050
Wire Wire Line
	7500 4450 7400 4450
Connection ~ 7400 4450
Wire Wire Line
	7400 4450 7400 4250
Wire Wire Line
	7500 4650 7400 4650
Connection ~ 7400 4650
Wire Wire Line
	7400 4650 7400 4450
$EndSCHEMATC
