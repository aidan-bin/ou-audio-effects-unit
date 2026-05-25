EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 5
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
L power:+3.3V #PWR07
U 1 1 6114A97B
P 6000 2150
F 0 "#PWR07" H 6000 2000 50  0001 C CNN
F 1 "+3.3V" H 6015 2323 50  0000 C CNN
F 2 "" H 6000 2150 50  0001 C CNN
F 3 "" H 6000 2150 50  0001 C CNN
	1    6000 2150
	1    0    0    -1  
$EndComp
Wire Wire Line
	6000 2150 6000 2200
Wire Wire Line
	6000 2200 6100 2200
Wire Wire Line
	6400 2200 6400 2250
Connection ~ 6000 2200
Wire Wire Line
	6000 2200 6000 2250
Wire Wire Line
	6300 2250 6300 2200
Connection ~ 6300 2200
Wire Wire Line
	6300 2200 6400 2200
Wire Wire Line
	6200 2250 6200 2200
Connection ~ 6200 2200
Wire Wire Line
	6200 2200 6300 2200
Wire Wire Line
	6100 2250 6100 2200
Connection ~ 6100 2200
Wire Wire Line
	6100 2200 6200 2200
$Comp
L power:+3.3VA #PWR08
U 1 1 6114C171
P 6500 2150
F 0 "#PWR08" H 6500 2000 50  0001 C CNN
F 1 "+3.3VA" H 6515 2323 50  0000 C CNN
F 2 "" H 6500 2150 50  0001 C CNN
F 3 "" H 6500 2150 50  0001 C CNN
	1    6500 2150
	1    0    0    -1  
$EndComp
Wire Wire Line
	6500 2150 6500 2250
$Comp
L power:+3.3V #PWR01
U 1 1 6114CB48
P 6900 1400
F 0 "#PWR01" H 6900 1250 50  0001 C CNN
F 1 "+3.3V" H 6915 1573 50  0000 C CNN
F 2 "" H 6900 1400 50  0001 C CNN
F 3 "" H 6900 1400 50  0001 C CNN
	1    6900 1400
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR05
U 1 1 61150F78
P 7300 1700
F 0 "#PWR05" H 7300 1450 50  0001 C CNN
F 1 "GND" H 7305 1527 50  0000 C CNN
F 2 "" H 7300 1700 50  0001 C CNN
F 3 "" H 7300 1700 50  0001 C CNN
	1    7300 1700
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR06
U 1 1 611546EB
P 7600 1700
F 0 "#PWR06" H 7600 1450 50  0001 C CNN
F 1 "GND" H 7605 1527 50  0000 C CNN
F 2 "" H 7600 1700 50  0001 C CNN
F 3 "" H 7600 1700 50  0001 C CNN
	1    7600 1700
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3VA #PWR02
U 1 1 61154CC1
P 7600 1400
F 0 "#PWR02" H 7600 1250 50  0001 C CNN
F 1 "+3.3VA" H 7615 1573 50  0000 C CNN
F 2 "" H 7600 1400 50  0001 C CNN
F 3 "" H 7600 1400 50  0001 C CNN
	1    7600 1400
	1    0    0    -1  
$EndComp
Text Notes 6800 1100 0    50   ~ 0
ADC Ref Filter
$Comp
L power:+3.3V #PWR03
U 1 1 61161B5A
P 4450 1450
F 0 "#PWR03" H 4450 1300 50  0001 C CNN
F 1 "+3.3V" H 4465 1623 50  0000 C CNN
F 2 "" H 4450 1450 50  0001 C CNN
F 3 "" H 4450 1450 50  0001 C CNN
	1    4450 1450
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR04
U 1 1 611629BF
P 4450 1650
F 0 "#PWR04" H 4450 1400 50  0001 C CNN
F 1 "GND" H 4455 1477 50  0000 C CNN
F 2 "" H 4450 1650 50  0001 C CNN
F 3 "" H 4450 1650 50  0001 C CNN
	1    4450 1650
	1    0    0    -1  
$EndComp
Text Notes 4350 1150 0    50   ~ 0
Decoupling Capacitors
Wire Wire Line
	6000 5850 6000 5900
Wire Wire Line
	6000 5900 6100 5900
Wire Wire Line
	6400 5900 6400 5850
Wire Wire Line
	6100 5850 6100 5900
Connection ~ 6100 5900
Wire Wire Line
	6100 5900 6200 5900
Wire Wire Line
	6200 5850 6200 5900
Connection ~ 6200 5900
Wire Wire Line
	6200 5900 6300 5900
Wire Wire Line
	6300 5850 6300 5900
Connection ~ 6300 5900
Wire Wire Line
	6300 5900 6400 5900
$Comp
L power:GND #PWR021
U 1 1 61147B66
P 6000 5950
F 0 "#PWR021" H 6000 5700 50  0001 C CNN
F 1 "GND" H 6005 5777 50  0000 C CNN
F 2 "" H 6000 5950 50  0001 C CNN
F 3 "" H 6000 5950 50  0001 C CNN
	1    6000 5950
	1    0    0    -1  
$EndComp
Wire Wire Line
	6000 5950 6000 5900
Connection ~ 6000 5900
$Comp
L Switch:SW_SPDT SW2
U 1 1 6114FED3
P 4050 2850
F 0 "SW2" H 4050 2525 50  0000 C CNN
F 1 "SW_SPDT" H 4050 2616 50  0000 C CNN
F 2 "Button_Switch_SMD:SW_SPDT_PCM12" H 4050 2850 50  0001 C CNN
F 3 "~" H 4050 2850 50  0001 C CNN
	1    4050 2850
	-1   0    0    1   
$EndComp
$Comp
L power:+3.3V #PWR010
U 1 1 61172E90
P 3750 2650
F 0 "#PWR010" H 3750 2500 50  0001 C CNN
F 1 "+3.3V" H 3765 2823 50  0000 C CNN
F 2 "" H 3750 2650 50  0001 C CNN
F 3 "" H 3750 2650 50  0001 C CNN
	1    3750 2650
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR012
U 1 1 61173C8C
P 3750 3050
F 0 "#PWR012" H 3750 2800 50  0001 C CNN
F 1 "GND" H 3755 2877 50  0000 C CNN
F 2 "" H 3750 3050 50  0001 C CNN
F 3 "" H 3750 3050 50  0001 C CNN
	1    3750 3050
	1    0    0    -1  
$EndComp
Wire Wire Line
	3750 2650 3750 2750
Wire Wire Line
	3750 2750 3850 2750
Wire Wire Line
	3750 3050 3750 2950
Wire Wire Line
	3750 2950 3850 2950
$Comp
L Switch:SW_Push SW1
U 1 1 6115AE4B
P 3000 2800
F 0 "SW1" H 3000 3085 50  0000 C CNN
F 1 "SW_Push" H 3000 2994 50  0000 C CNN
F 2 "Button_Switch_SMD:SW_SPST_PTS645" H 3000 3000 50  0001 C CNN
F 3 "~" H 3000 3000 50  0001 C CNN
	1    3000 2800
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR011
U 1 1 61167E0D
P 2650 2800
F 0 "#PWR011" H 2650 2550 50  0001 C CNN
F 1 "GND" V 2655 2672 50  0000 R CNN
F 2 "" H 2650 2800 50  0001 C CNN
F 3 "" H 2650 2800 50  0001 C CNN
	1    2650 2800
	0    1    1    0   
$EndComp
Text GLabel 5500 3650 0    50   Input ~ 0
HSE_IN
Text GLabel 5500 3750 0    50   Input ~ 0
HSE_OUT
$Comp
L power:GND #PWR013
U 1 1 6118A25F
P 1500 3100
F 0 "#PWR013" H 1500 2850 50  0001 C CNN
F 1 "GND" H 1505 2927 50  0000 C CNN
F 2 "" H 1500 3100 50  0001 C CNN
F 3 "" H 1500 3100 50  0001 C CNN
	1    1500 3100
	1    0    0    -1  
$EndComp
Text GLabel 1250 2750 0    50   Input ~ 0
HSE_IN
Text GLabel 1950 2750 2    50   Input ~ 0
HSE_OUT
$Comp
L Device:C_Small C9
U 1 1 6118E473
P 1300 2900
F 0 "C9" H 1208 2946 50  0000 R CNN
F 1 "33p" H 1208 2855 50  0000 R CNN
F 2 "Capacitor_SMD:C_0402_1005Metric" H 1300 2900 50  0001 C CNN
F 3 "~" H 1300 2900 50  0001 C CNN
	1    1300 2900
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C10
U 1 1 6118EC84
P 1700 2900
F 0 "C10" H 1792 2946 50  0000 L CNN
F 1 "33p" H 1792 2855 50  0000 L CNN
F 2 "Capacitor_SMD:C_0402_1005Metric" H 1700 2900 50  0001 C CNN
F 3 "~" H 1700 2900 50  0001 C CNN
	1    1700 2900
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small_US R1
U 1 1 6118F6C9
P 1850 2750
F 0 "R1" V 1645 2750 50  0000 C CNN
F 1 "220" V 1736 2750 50  0000 C CNN
F 2 "Resistor_SMD:R_0402_1005Metric" H 1850 2750 50  0001 C CNN
F 3 "~" H 1850 2750 50  0001 C CNN
	1    1850 2750
	0    1    1    0   
$EndComp
Wire Wire Line
	1250 2750 1300 2750
Wire Wire Line
	1600 2750 1700 2750
Wire Wire Line
	1300 2800 1300 2750
Connection ~ 1300 2750
Wire Wire Line
	1300 2750 1400 2750
Wire Wire Line
	1700 2800 1700 2750
Connection ~ 1700 2750
Wire Wire Line
	1700 2750 1750 2750
Wire Wire Line
	1300 3000 1300 3050
Wire Wire Line
	1300 3050 1500 3050
Wire Wire Line
	1700 3050 1700 3000
Wire Wire Line
	1500 3050 1500 3100
Connection ~ 1500 3050
Wire Wire Line
	1500 3050 1700 3050
$Comp
L power:GND #PWR09
U 1 1 611A7839
P 1500 2550
F 0 "#PWR09" H 1500 2300 50  0001 C CNN
F 1 "GND" H 1505 2377 50  0000 C CNN
F 2 "" H 1500 2550 50  0001 C CNN
F 3 "" H 1500 2550 50  0001 C CNN
	1    1500 2550
	-1   0    0    1   
$EndComp
Wire Wire Line
	1500 2850 1500 3050
Wire Wire Line
	1500 2550 1500 2650
Text GLabel 5500 2450 0    50   Input ~ 0
NRST
Text GLabel 3350 2800 2    50   Input ~ 0
NRST
Text GLabel 5500 2650 0    50   Input ~ 0
BOOT0
Text GLabel 4450 2850 2    50   Input ~ 0
BOOT0
Text GLabel 6900 3750 2    50   Input ~ 0
SWDIO
Text GLabel 6900 3850 2    50   Input ~ 0
SWDCLK
Text GLabel 6900 4450 2    50   Input ~ 0
SWO
$Comp
L Device:C_Small C11
U 1 1 6155990E
P 3000 2950
F 0 "C11" V 3137 2950 50  0000 C CNN
F 1 "100n" V 3228 2950 50  0000 C CNN
F 2 "Capacitor_SMD:C_0402_1005Metric" H 3000 2950 50  0001 C CNN
F 3 "~" H 3000 2950 50  0001 C CNN
	1    3000 2950
	0    1    1    0   
$EndComp
$Comp
L Device:CP1_Small C1
U 1 1 61564363
P 4450 1550
F 0 "C1" H 4541 1596 50  0000 L CNN
F 1 "4.7u" H 4541 1505 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric" H 4450 1550 50  0001 C CNN
F 3 "~" H 4450 1550 50  0001 C CNN
	1    4450 1550
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C2
U 1 1 6156A8F1
P 4900 1550
F 0 "C2" H 4808 1504 50  0000 R CNN
F 1 "100n" H 4808 1595 50  0000 R CNN
F 2 "Capacitor_SMD:C_0402_1005Metric" H 4900 1550 50  0001 C CNN
F 3 "~" H 4900 1550 50  0001 C CNN
	1    4900 1550
	-1   0    0    1   
$EndComp
$Comp
L Device:C_Small C3
U 1 1 6156AAD6
P 5250 1550
F 0 "C3" H 5158 1504 50  0000 R CNN
F 1 "100n" H 5158 1595 50  0000 R CNN
F 2 "Capacitor_SMD:C_0402_1005Metric" H 5250 1550 50  0001 C CNN
F 3 "~" H 5250 1550 50  0001 C CNN
	1    5250 1550
	-1   0    0    1   
$EndComp
$Comp
L Device:C_Small C4
U 1 1 6156ADA3
P 5600 1550
F 0 "C4" H 5508 1504 50  0000 R CNN
F 1 "100n" H 5508 1595 50  0000 R CNN
F 2 "Capacitor_SMD:C_0402_1005Metric" H 5600 1550 50  0001 C CNN
F 3 "~" H 5600 1550 50  0001 C CNN
	1    5600 1550
	-1   0    0    1   
$EndComp
$Comp
L Device:C_Small C5
U 1 1 6156B194
P 5950 1550
F 0 "C5" H 5858 1504 50  0000 R CNN
F 1 "100n" H 5858 1595 50  0000 R CNN
F 2 "Capacitor_SMD:C_0402_1005Metric" H 5950 1550 50  0001 C CNN
F 3 "~" H 5950 1550 50  0001 C CNN
	1    5950 1550
	-1   0    0    1   
$EndComp
$Comp
L Device:C_Small C6
U 1 1 6156B5E9
P 6300 1550
F 0 "C6" H 6208 1504 50  0000 R CNN
F 1 "100n" H 6208 1595 50  0000 R CNN
F 2 "Capacitor_SMD:C_0402_1005Metric" H 6300 1550 50  0001 C CNN
F 3 "~" H 6300 1550 50  0001 C CNN
	1    6300 1550
	-1   0    0    1   
$EndComp
Wire Wire Line
	4450 1450 4900 1450
Connection ~ 4450 1450
Connection ~ 4900 1450
Wire Wire Line
	4900 1450 5250 1450
Connection ~ 5250 1450
Wire Wire Line
	5250 1450 5600 1450
Connection ~ 5600 1450
Wire Wire Line
	5600 1450 5950 1450
Connection ~ 5950 1450
Wire Wire Line
	5950 1450 6300 1450
Wire Wire Line
	6300 1650 5950 1650
Connection ~ 4450 1650
Connection ~ 4900 1650
Wire Wire Line
	4900 1650 4450 1650
Connection ~ 5250 1650
Wire Wire Line
	5250 1650 4900 1650
Connection ~ 5600 1650
Wire Wire Line
	5600 1650 5250 1650
Connection ~ 5950 1650
Wire Wire Line
	5950 1650 5600 1650
$Comp
L Device:CP1_Small C7
U 1 1 6157B344
P 7300 1600
F 0 "C7" H 7391 1646 50  0000 L CNN
F 1 "1u" H 7391 1555 50  0000 L CNN
F 2 "Capacitor_SMD:C_0402_1005Metric" H 7300 1600 50  0001 C CNN
F 3 "~" H 7300 1600 50  0001 C CNN
	1    7300 1600
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C8
U 1 1 6157B64E
P 7600 1600
F 0 "C8" H 7508 1554 50  0000 R CNN
F 1 "10n" H 7508 1645 50  0000 R CNN
F 2 "Capacitor_SMD:C_0402_1005Metric" H 7600 1600 50  0001 C CNN
F 3 "~" H 7600 1600 50  0001 C CNN
	1    7600 1600
	-1   0    0    1   
$EndComp
Wire Wire Line
	7600 1500 7350 1500
Wire Wire Line
	7600 1400 7600 1500
Connection ~ 7600 1500
$Comp
L Device:L_Small L1
U 1 1 6158AB14
P 7100 1500
F 0 "L1" V 7285 1500 50  0000 C CNN
F 1 "39n" V 7194 1500 50  0000 C CNN
F 2 "Inductor_SMD:L_0402_1005Metric" H 7100 1500 50  0001 C CNN
F 3 "~" H 7100 1500 50  0001 C CNN
	1    7100 1500
	0    -1   -1   0   
$EndComp
Wire Wire Line
	7200 1500 7300 1500
Connection ~ 7300 1500
Wire Wire Line
	7000 1500 6900 1500
Wire Wire Line
	6900 1500 6900 1400
Wire Wire Line
	3100 2950 3200 2950
Wire Wire Line
	3200 2950 3200 2800
Wire Wire Line
	3350 2800 3200 2800
Connection ~ 3200 2800
Wire Wire Line
	2800 2800 2800 2950
Wire Wire Line
	2800 2950 2900 2950
Wire Wire Line
	2650 2800 2800 2800
Connection ~ 2800 2800
$Comp
L Device:R_Small_US R2
U 1 1 615A6119
P 4350 2850
F 0 "R2" V 4145 2850 50  0000 C CNN
F 1 "10k" V 4236 2850 50  0000 C CNN
F 2 "Resistor_SMD:R_0402_1005Metric" H 4350 2850 50  0001 C CNN
F 3 "~" H 4350 2850 50  0001 C CNN
	1    4350 2850
	0    1    1    0   
$EndComp
$Comp
L Device:Crystal_GND24_Small X1
U 1 1 61187ED9
P 1500 2750
F 0 "X1" H 1150 2950 50  0000 L CNN
F 1 "12MHz" H 1150 2850 50  0000 L CNN
F 2 "Crystal:Crystal_SMD_3225-4Pin_3.2x2.5mm" H 1500 2750 50  0001 C CNN
F 3 "~" H 1500 2750 50  0001 C CNN
	1    1500 2750
	1    0    0    -1  
$EndComp
$Comp
L MCU_ST_STM32F3:STM32F303RETx U1
U 1 1 6112D496
P 6200 4050
F 0 "U1" H 7050 2150 50  0000 C CNN
F 1 "STM32F303RETx" H 7050 2250 50  0000 C CNN
F 2 "Package_QFP:LQFP-64_10x10mm_P0.5mm" H 5600 2350 50  0001 R CNN
F 3 "http://www.st.com/st-web-ui/static/active/en/resource/technical/document/datasheet/DM00118585.pdf" H 6200 4050 50  0001 C CNN
	1    6200 4050
	1    0    0    -1  
$EndComp
Text Notes 900  2250 0    50   ~ 0
Crystal, Reset Button, and Boot Select Switch
Text GLabel 7750 3650 2    50   Input ~ 0
USB_D+
Text GLabel 7750 3550 2    50   Input ~ 0
USB_D-
Text GLabel 6900 5450 2    50   Input ~ 0
OPAMP3_VIN+
Text GLabel 6900 2750 2    50   Input ~ 0
POT_C
Text GLabel 6900 2850 2    50   Input ~ 0
DAC1_OUT1
Text GLabel 6900 2950 2    50   Input ~ 0
OPAMP2_VIN-
Text GLabel 6900 3050 2    50   Input ~ 0
OPAMP2_VOUT
Text GLabel 6900 4150 2    50   Input ~ 0
OPAMP2_VIN+
Text GLabel 6900 4250 2    50   Input ~ 0
OPAMP3_VOUT
Text GLabel 6900 4350 2    50   Input ~ 0
OPAMP3_VIN-
Text GLabel 6900 2550 2    50   Input ~ 0
POT_B
Text GLabel 6900 2450 2    50   Input ~ 0
POT_A
Text GLabel 5500 4150 0    50   Input ~ 0
POT_D
$Sheet
S 9000 850  1900 1100
U 61A3BF96
F0 "power" 50
F1 "power.sch" 50
$EndSheet
$Sheet
S 9000 5050 1900 1150
U 61AA9AB7
F0 "connectors" 50
F1 "connectors.sch" 50
$EndSheet
$Sheet
S 9000 3650 1900 1100
U 61AB9E49
F0 "audio i/o" 50
F1 "audio_io.sch" 50
$EndSheet
Text GLabel 6900 5150 2    50   Input ~ 0
OPAMP4_VIN-
Text GLabel 6900 5350 2    50   Input ~ 0
OPAMP4_VOUT
Text GLabel 6900 5250 2    50   Input ~ 0
OPAMP4_VIN+
$Sheet
S 9000 2250 1900 1100
U 61B8C98C
F0 "user i/o" 50
F1 "user_io.sch" 50
$EndSheet
$Comp
L STM32AudioEffects:I2C_OLED_Display U?
U 1 1 61BA7091
P 2750 4450
AR Path="/61B8C98C/61BA7091" Ref="U?"  Part="1" 
AR Path="/61BA7091" Ref="U2"  Part="1" 
F 0 "U2" H 2704 4775 50  0000 C CNN
F 1 "I2C_OLED_Display" H 2704 4684 50  0000 C CNN
F 2 "STM32AudioEffects:I2C_OLED_DISPLAY" H 2750 4650 50  0001 C CNN
F 3 "" H 2750 4650 50  0001 C CNN
	1    2750 4450
	1    0    0    -1  
$EndComp
Text GLabel 2500 4350 0    50   Input ~ 0
I2C1_SDA
Text GLabel 2500 4450 0    50   Input ~ 0
I2C1_SCL
$Comp
L power:+3.3V #PWR017
U 1 1 61BA7099
P 2000 4500
AR Path="/61BA7099" Ref="#PWR017"  Part="1" 
AR Path="/61AB9E49/61BA7099" Ref="#PWR?"  Part="1" 
AR Path="/61B8C98C/61BA7099" Ref="#PWR?"  Part="1" 
F 0 "#PWR017" H 2000 4350 50  0001 C CNN
F 1 "+3.3V" H 1942 4537 50  0000 R CNN
F 2 "" H 2000 4500 50  0001 C CNN
F 3 "" H 2000 4500 50  0001 C CNN
	1    2000 4500
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR?
U 1 1 61BA709F
P 2000 4750
AR Path="/61AB9E49/61BA709F" Ref="#PWR?"  Part="1" 
AR Path="/61B8C98C/61BA709F" Ref="#PWR?"  Part="1" 
AR Path="/61BA709F" Ref="#PWR019"  Part="1" 
F 0 "#PWR019" H 2000 4500 50  0001 C CNN
F 1 "GND" H 2005 4577 50  0000 C CNN
F 2 "" H 2000 4750 50  0001 C CNN
F 3 "" H 2000 4750 50  0001 C CNN
	1    2000 4750
	1    0    0    -1  
$EndComp
Text Notes 1900 4050 0    50   ~ 0
User Display
Text GLabel 7750 4850 2    50   Input ~ 0
I2C1_SDA
Text GLabel 8450 3950 2    50   Input ~ 0
I2C1_SCL
Text GLabel 6900 3350 2    50   Input ~ 0
BTN_B
Text GLabel 6900 3250 2    50   Input ~ 0
BTN_C
Text GLabel 5500 4850 0    50   Input ~ 0
SWITCH_A
Text GLabel 5500 4950 0    50   Input ~ 0
SWITCH_B
Text GLabel 5500 5050 0    50   Input ~ 0
SWITCH_C
Text GLabel 6900 3450 2    50   Input ~ 0
BTN_A
Text GLabel 5500 4250 0    50   Input ~ 0
PC1
Text GLabel 5500 4350 0    50   Input ~ 0
PC2
Text GLabel 5500 4450 0    50   Input ~ 0
PC3
Text GLabel 5500 4550 0    50   Input ~ 0
PC4
$Comp
L Device:R_Small_US R3
U 1 1 61BE9A8F
P 7600 3350
AR Path="/61BE9A8F" Ref="R3"  Part="1" 
AR Path="/61AA9AB7/61BE9A8F" Ref="R?"  Part="1" 
F 0 "R3" H 7668 3396 50  0000 L CNN
F 1 "1.5k" H 7668 3305 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" H 7600 3350 50  0001 C CNN
F 3 "~" H 7600 3350 50  0001 C CNN
	1    7600 3350
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR014
U 1 1 61BE9A97
P 7600 3250
AR Path="/61BE9A97" Ref="#PWR014"  Part="1" 
AR Path="/61AA9AB7/61BE9A97" Ref="#PWR?"  Part="1" 
F 0 "#PWR014" H 7600 3100 50  0001 C CNN
F 1 "+3.3V" H 7615 3423 50  0000 C CNN
F 2 "" H 7600 3250 50  0001 C CNN
F 3 "" H 7600 3250 50  0001 C CNN
	1    7600 3250
	1    0    0    -1  
$EndComp
Wire Wire Line
	7750 3550 6900 3550
Wire Wire Line
	7750 3650 7600 3650
Wire Wire Line
	7600 3450 7600 3650
Connection ~ 7600 3650
Wire Wire Line
	7600 3650 6900 3650
$Comp
L Device:LED_Small D1
U 1 1 61BFDAC5
P 4800 4150
AR Path="/61BFDAC5" Ref="D1"  Part="1" 
AR Path="/61AA9AB7/61BFDAC5" Ref="D?"  Part="1" 
F 0 "D1" V 4846 4080 50  0000 R CNN
F 1 "BLUE" V 4755 4080 50  0000 R CNN
F 2 "LED_SMD:LED_0603_1608Metric" V 4800 4150 50  0001 C CNN
F 3 "~" V 4800 4150 50  0001 C CNN
	1    4800 4150
	0    -1   -1   0   
$EndComp
$Comp
L power:GND #PWR016
U 1 1 61BFDACB
P 4800 4450
AR Path="/61BFDACB" Ref="#PWR016"  Part="1" 
AR Path="/61AA9AB7/61BFDACB" Ref="#PWR?"  Part="1" 
F 0 "#PWR016" H 4800 4200 50  0001 C CNN
F 1 "GND" H 4805 4277 50  0000 C CNN
F 2 "" H 4800 4450 50  0001 C CNN
F 3 "" H 4800 4450 50  0001 C CNN
	1    4800 4450
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small_US R6
U 1 1 61BFDAD3
P 4800 4350
AR Path="/61BFDAD3" Ref="R6"  Part="1" 
AR Path="/61AA9AB7/61BFDAD3" Ref="R?"  Part="1" 
F 0 "R6" H 4868 4396 50  0000 L CNN
F 1 "33" H 4868 4305 50  0000 L CNN
F 2 "Resistor_SMD:R_0402_1005Metric" H 4800 4350 50  0001 C CNN
F 3 "~" H 4800 4350 50  0001 C CNN
	1    4800 4350
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small_US R5
U 1 1 61BFDAD9
P 5000 3950
AR Path="/61BFDAD9" Ref="R5"  Part="1" 
AR Path="/61AA9AB7/61BFDAD9" Ref="R?"  Part="1" 
F 0 "R5" V 4795 3950 50  0000 C CNN
F 1 "0" V 4886 3950 50  0000 C CNN
F 2 "Resistor_SMD:R_0402_1005Metric" H 5000 3950 50  0001 C CNN
F 3 "~" H 5000 3950 50  0001 C CNN
	1    5000 3950
	0    1    1    0   
$EndComp
Wire Wire Line
	4800 4050 4800 3950
Wire Wire Line
	4800 3950 4900 3950
Wire Wire Line
	5100 3950 5500 3950
Text Label 4800 3950 2    50   ~ 0
LED_STATUS
Wire Wire Line
	2500 4550 2000 4550
Wire Wire Line
	2000 4550 2000 4500
Wire Wire Line
	2500 4650 2000 4650
Wire Wire Line
	2000 4650 2000 4750
$Comp
L Device:R_Small_US R4
U 1 1 61C0E93F
P 8250 3750
AR Path="/61C0E93F" Ref="R4"  Part="1" 
AR Path="/61AA9AB7/61C0E93F" Ref="R?"  Part="1" 
F 0 "R4" H 8318 3796 50  0000 L CNN
F 1 "4.7k" H 8318 3705 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" H 8250 3750 50  0001 C CNN
F 3 "~" H 8250 3750 50  0001 C CNN
	1    8250 3750
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR015
U 1 1 61C0E949
P 8250 3650
AR Path="/61C0E949" Ref="#PWR015"  Part="1" 
AR Path="/61AA9AB7/61C0E949" Ref="#PWR?"  Part="1" 
F 0 "#PWR015" H 8250 3500 50  0001 C CNN
F 1 "+3.3V" H 8265 3823 50  0000 C CNN
F 2 "" H 8250 3650 50  0001 C CNN
F 3 "" H 8250 3650 50  0001 C CNN
	1    8250 3650
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small_US R7
U 1 1 61C12199
P 7600 4650
AR Path="/61C12199" Ref="R7"  Part="1" 
AR Path="/61AA9AB7/61C12199" Ref="R?"  Part="1" 
F 0 "R7" H 7668 4696 50  0000 L CNN
F 1 "4.7k" H 7668 4605 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" H 7600 4650 50  0001 C CNN
F 3 "~" H 7600 4650 50  0001 C CNN
	1    7600 4650
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR018
U 1 1 61C121A3
P 7600 4550
AR Path="/61C121A3" Ref="#PWR018"  Part="1" 
AR Path="/61AA9AB7/61C121A3" Ref="#PWR?"  Part="1" 
F 0 "#PWR018" H 7600 4400 50  0001 C CNN
F 1 "+3.3V" H 7615 4723 50  0000 C CNN
F 2 "" H 7600 4550 50  0001 C CNN
F 3 "" H 7600 4550 50  0001 C CNN
	1    7600 4550
	1    0    0    -1  
$EndComp
Wire Wire Line
	8250 3850 8250 3950
Wire Wire Line
	8250 3950 8450 3950
Wire Wire Line
	8250 3950 6900 3950
Connection ~ 8250 3950
Wire Wire Line
	7750 4850 7600 4850
Wire Wire Line
	7600 4850 7600 4750
Wire Wire Line
	7600 4850 6900 4850
Connection ~ 7600 4850
NoConn ~ 5500 4750
NoConn ~ 5500 4650
NoConn ~ 5500 5150
NoConn ~ 5500 5250
NoConn ~ 5500 5350
NoConn ~ 5500 5550
NoConn ~ 5500 5650
NoConn ~ 6900 3150
NoConn ~ 6900 4550
NoConn ~ 6900 4650
NoConn ~ 6900 4750
NoConn ~ 6900 4950
NoConn ~ 6900 5050
NoConn ~ 6900 5550
NoConn ~ 6900 5650
Text Notes 800  1100 0    157  ~ 31
STM32F3 Audio Effects Unit
$Comp
L power:PWR_FLAG #FLG01
U 1 1 61C844F1
P 7350 1500
F 0 "#FLG01" H 7350 1575 50  0001 C CNN
F 1 "PWR_FLAG" H 7350 1673 50  0000 C CNN
F 2 "" H 7350 1500 50  0001 C CNN
F 3 "~" H 7350 1500 50  0001 C CNN
	1    7350 1500
	1    0    0    -1  
$EndComp
Connection ~ 7350 1500
Wire Wire Line
	7350 1500 7300 1500
$Comp
L Memory_NVRAM:FM24C64B U3
U 1 1 61ECB21F
P 2450 6250
F 0 "U3" H 2700 5900 50  0000 C CNN
F 1 "M24C64" H 2700 5800 50  0000 C CNN
F 2 "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm" H 2450 6250 50  0001 C CNN
F 3 "" H 2250 6600 50  0001 C CNN
	1    2450 6250
	1    0    0    -1  
$EndComp
Text Notes 2100 5550 0    39   ~ 0
Non-Volatile Memory
Text GLabel 5500 5450 0    39   Input ~ 0
~NVM_WE
Text GLabel 2050 6450 0    39   Input ~ 0
~NVM_WE
Text GLabel 2850 6150 2    50   Input ~ 0
I2C1_SDA
Text GLabel 2850 6350 2    50   Input ~ 0
I2C1_SCL
$Comp
L power:GND #PWR?
U 1 1 61ECE6D7
P 2450 6650
AR Path="/61AB9E49/61ECE6D7" Ref="#PWR?"  Part="1" 
AR Path="/61B8C98C/61ECE6D7" Ref="#PWR?"  Part="1" 
AR Path="/61ECE6D7" Ref="#PWR024"  Part="1" 
F 0 "#PWR024" H 2450 6400 50  0001 C CNN
F 1 "GND" H 2455 6477 50  0000 C CNN
F 2 "" H 2450 6650 50  0001 C CNN
F 3 "" H 2450 6650 50  0001 C CNN
	1    2450 6650
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR020
U 1 1 61ECE9A0
P 2450 5850
AR Path="/61ECE9A0" Ref="#PWR020"  Part="1" 
AR Path="/61AB9E49/61ECE9A0" Ref="#PWR?"  Part="1" 
AR Path="/61B8C98C/61ECE9A0" Ref="#PWR?"  Part="1" 
F 0 "#PWR020" H 2450 5700 50  0001 C CNN
F 1 "+3.3V" H 2465 6023 50  0000 C CNN
F 2 "" H 2450 5850 50  0001 C CNN
F 3 "" H 2450 5850 50  0001 C CNN
	1    2450 5850
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C12
U 1 1 61ED20F0
P 3500 6300
F 0 "C12" H 3408 6254 50  0000 R CNN
F 1 "100n" H 3408 6345 50  0000 R CNN
F 2 "Capacitor_SMD:C_0402_1005Metric" H 3500 6300 50  0001 C CNN
F 3 "~" H 3500 6300 50  0001 C CNN
	1    3500 6300
	-1   0    0    1   
$EndComp
$Comp
L power:+3.3V #PWR022
U 1 1 61ED3263
P 3500 6200
AR Path="/61ED3263" Ref="#PWR022"  Part="1" 
AR Path="/61AB9E49/61ED3263" Ref="#PWR?"  Part="1" 
AR Path="/61B8C98C/61ED3263" Ref="#PWR?"  Part="1" 
F 0 "#PWR022" H 3500 6050 50  0001 C CNN
F 1 "+3.3V" H 3515 6373 50  0000 C CNN
F 2 "" H 3500 6200 50  0001 C CNN
F 3 "" H 3500 6200 50  0001 C CNN
	1    3500 6200
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR?
U 1 1 61ED3653
P 3500 6400
AR Path="/61AB9E49/61ED3653" Ref="#PWR?"  Part="1" 
AR Path="/61B8C98C/61ED3653" Ref="#PWR?"  Part="1" 
AR Path="/61ED3653" Ref="#PWR023"  Part="1" 
F 0 "#PWR023" H 3500 6150 50  0001 C CNN
F 1 "GND" H 3505 6227 50  0000 C CNN
F 2 "" H 3500 6400 50  0001 C CNN
F 3 "" H 3500 6400 50  0001 C CNN
	1    3500 6400
	1    0    0    -1  
$EndComp
NoConn ~ 2050 6050
NoConn ~ 2050 6150
NoConn ~ 2050 6250
NoConn ~ 6900 2650
$EndSCHEMATC
