EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 2 5
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
Text Label 7450 5000 0    50   ~ 0
1.25V
Text GLabel 5550 5600 0    50   Input ~ 0
BUCK_IN
$Comp
L power:GND #PWR?
U 1 1 61A8CEF3
P 5550 5400
AR Path="/61A8CEF3" Ref="#PWR?"  Part="1" 
AR Path="/61A3BF96/61A8CEF3" Ref="#PWR039"  Part="1" 
F 0 "#PWR039" H 5550 5150 50  0001 C CNN
F 1 "GND" V 5555 5272 50  0000 R CNN
F 2 "" H 5550 5400 50  0001 C CNN
F 3 "" H 5550 5400 50  0001 C CNN
	1    5550 5400
	0    1    1    0   
$EndComp
Connection ~ 7450 5000
Wire Wire Line
	7450 5500 7450 5000
$Comp
L Device:C_Small C?
U 1 1 61A8CF02
P 6600 5700
AR Path="/61A8CF02" Ref="C?"  Part="1" 
AR Path="/61A3BF96/61A8CF02" Ref="C17"  Part="1" 
F 0 "C17" H 6692 5746 50  0000 L CNN
F 1 "10u" H 6692 5655 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric" H 6600 5700 50  0001 C CNN
F 3 "~" H 6600 5700 50  0001 C CNN
	1    6600 5700
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR?
U 1 1 61A8CF08
P 6600 5800
AR Path="/61A8CF08" Ref="#PWR?"  Part="1" 
AR Path="/61A3BF96/61A8CF08" Ref="#PWR040"  Part="1" 
F 0 "#PWR040" H 6600 5550 50  0001 C CNN
F 1 "GND" V 6605 5672 50  0000 R CNN
F 2 "" H 6600 5800 50  0001 C CNN
F 3 "" H 6600 5800 50  0001 C CNN
	1    6600 5800
	1    0    0    -1  
$EndComp
$Comp
L Device:D_Schottky_Small D?
U 1 1 61A8CF0E
P 7050 5500
AR Path="/61A8CF0E" Ref="D?"  Part="1" 
AR Path="/61A3BF96/61A8CF0E" Ref="D6"  Part="1" 
F 0 "D6" H 7050 5293 50  0000 C CNN
F 1 "B5819W" H 7050 5384 50  0000 C CNN
F 2 "Diode_SMD:D_SOD-123" V 7050 5500 50  0001 C CNN
F 3 "~" V 7050 5500 50  0001 C CNN
	1    7050 5500
	-1   0    0    1   
$EndComp
Wire Wire Line
	6600 5600 6600 5500
Connection ~ 6000 4450
Wire Wire Line
	5800 4450 6000 4450
Wire Wire Line
	7450 4450 7450 5000
Wire Wire Line
	5800 4650 5800 5000
Wire Wire Line
	5800 4450 5800 4550
Wire Wire Line
	6500 4150 6500 4450
Text GLabel 4200 4150 0    50   Input ~ 0
BUCK_IN
Wire Wire Line
	4900 4150 5000 4150
Wire Wire Line
	4900 4150 4750 4150
Connection ~ 4900 4150
Wire Wire Line
	4900 3800 4900 4150
Wire Wire Line
	6150 3800 4900 3800
Wire Wire Line
	6150 4150 6150 3800
Wire Wire Line
	6100 4150 6150 4150
Wire Wire Line
	4200 4150 4400 4150
$Comp
L power:+3.3V #PWR?
U 1 1 61A8CF55
P 6500 4150
AR Path="/61A8CF55" Ref="#PWR?"  Part="1" 
AR Path="/61A3BF96/61A8CF55" Ref="#PWR031"  Part="1" 
F 0 "#PWR031" H 6500 4000 50  0001 C CNN
F 1 "+3.3V" H 6515 4323 50  0000 C CNN
F 2 "" H 6500 4150 50  0001 C CNN
F 3 "" H 6500 4150 50  0001 C CNN
	1    6500 4150
	1    0    0    -1  
$EndComp
$Comp
L Device:CP1_Small C?
U 1 1 61A8CF5B
P 4750 4350
AR Path="/61A8CF5B" Ref="C?"  Part="1" 
AR Path="/61A3BF96/61A8CF5B" Ref="C13"  Part="1" 
F 0 "C13" H 4659 4396 50  0000 R CNN
F 1 "4.7u" H 4659 4305 50  0000 R CNN
F 2 "Capacitor_SMD:C_0805_2012Metric" H 4750 4350 50  0001 C CNN
F 3 "~" H 4750 4350 50  0001 C CNN
	1    4750 4350
	1    0    0    -1  
$EndComp
Connection ~ 4750 4150
Wire Wire Line
	4750 4150 4750 4250
Wire Wire Line
	4600 4150 4750 4150
$Comp
L Device:Ferrite_Bead_Small FB?
U 1 1 61A8CF64
P 4500 4150
AR Path="/61A8CF64" Ref="FB?"  Part="1" 
AR Path="/61A3BF96/61A8CF64" Ref="FB1"  Part="1" 
F 0 "FB1" V 4263 4150 50  0000 C CNN
F 1 "600@600MHz" V 4354 4150 50  0000 C CNN
F 2 "Inductor_SMD:L_0805_2012Metric" V 4430 4150 50  0001 C CNN
F 3 "~" H 4500 4150 50  0001 C CNN
	1    4500 4150
	0    1    1    0   
$EndComp
Text Notes 3750 1850 0    50   ~ 0
Power Input + Power LED
Wire Wire Line
	7150 4450 7450 4450
Wire Wire Line
	7150 4450 7150 4550
Text Notes 3800 3700 0    50   ~ 0
5-9V to 3.3V Switching Converter + ON/OFF Switch (with bypass)
Wire Wire Line
	6500 4450 6350 4450
Wire Wire Line
	6150 4450 6000 4450
Connection ~ 7150 4450
Wire Wire Line
	7100 4450 7150 4450
Connection ~ 6500 4450
Wire Wire Line
	6500 4550 6500 4450
Wire Wire Line
	6000 4450 6000 4550
Connection ~ 5850 4150
Wire Wire Line
	5900 4150 5850 4150
Wire Wire Line
	4900 4550 4900 4650
$Comp
L power:GND #PWR?
U 1 1 61A8CF79
P 4900 4850
AR Path="/61A8CF79" Ref="#PWR?"  Part="1" 
AR Path="/61A3BF96/61A8CF79" Ref="#PWR037"  Part="1" 
F 0 "#PWR037" H 4900 4600 50  0001 C CNN
F 1 "GND" H 4905 4677 50  0000 C CNN
F 2 "" H 4900 4850 50  0001 C CNN
F 3 "" H 4900 4850 50  0001 C CNN
	1    4900 4850
	1    0    0    -1  
$EndComp
Wire Wire Line
	4900 4550 5000 4550
$Comp
L Device:C_Small C?
U 1 1 61A8CF80
P 4900 4750
AR Path="/61A8CF80" Ref="C?"  Part="1" 
AR Path="/61A3BF96/61A8CF80" Ref="C16"  Part="1" 
F 0 "C16" H 4808 4704 50  0000 R CNN
F 1 "330p" H 4808 4795 50  0000 R CNN
F 2 "Capacitor_SMD:C_0402_1005Metric" H 4900 4750 50  0001 C CNN
F 3 "~" H 4900 4750 50  0001 C CNN
	1    4900 4750
	1    0    0    1   
$EndComp
$Comp
L power:GND #PWR?
U 1 1 61A8CF86
P 5400 4850
AR Path="/61A8CF86" Ref="#PWR?"  Part="1" 
AR Path="/61A3BF96/61A8CF86" Ref="#PWR038"  Part="1" 
F 0 "#PWR038" H 5400 4600 50  0001 C CNN
F 1 "GND" H 5405 4677 50  0000 C CNN
F 2 "" H 5400 4850 50  0001 C CNN
F 3 "" H 5400 4850 50  0001 C CNN
	1    5400 4850
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR?
U 1 1 61A8CF8C
P 7150 4750
AR Path="/61A8CF8C" Ref="#PWR?"  Part="1" 
AR Path="/61A3BF96/61A8CF8C" Ref="#PWR036"  Part="1" 
F 0 "#PWR036" H 7150 4500 50  0001 C CNN
F 1 "GND" H 7155 4577 50  0000 C CNN
F 2 "" H 7150 4750 50  0001 C CNN
F 3 "" H 7150 4750 50  0001 C CNN
	1    7150 4750
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR?
U 1 1 61A8CF92
P 6500 4750
AR Path="/61A8CF92" Ref="#PWR?"  Part="1" 
AR Path="/61A3BF96/61A8CF92" Ref="#PWR034"  Part="1" 
F 0 "#PWR034" H 6500 4500 50  0001 C CNN
F 1 "GND" H 6505 4577 50  0000 C CNN
F 2 "" H 6500 4750 50  0001 C CNN
F 3 "" H 6500 4750 50  0001 C CNN
	1    6500 4750
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR?
U 1 1 61A8CF98
P 6000 4750
AR Path="/61A8CF98" Ref="#PWR?"  Part="1" 
AR Path="/61A3BF96/61A8CF98" Ref="#PWR033"  Part="1" 
F 0 "#PWR033" H 6000 4500 50  0001 C CNN
F 1 "GND" H 6005 4577 50  0000 C CNN
F 2 "" H 6000 4750 50  0001 C CNN
F 3 "" H 6000 4750 50  0001 C CNN
	1    6000 4750
	1    0    0    -1  
$EndComp
$Comp
L Device:CP1_Small C?
U 1 1 61A8CF9E
P 6500 4650
AR Path="/61A8CF9E" Ref="C?"  Part="1" 
AR Path="/61A3BF96/61A8CF9E" Ref="C14"  Part="1" 
F 0 "C14" H 6591 4696 50  0000 L CNN
F 1 "22u" H 6591 4605 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric" H 6500 4650 50  0001 C CNN
F 3 "~" H 6500 4650 50  0001 C CNN
	1    6500 4650
	1    0    0    -1  
$EndComp
$Comp
L Device:D_Schottky_Small D?
U 1 1 61A8CFA4
P 6000 4650
AR Path="/61A8CFA4" Ref="D?"  Part="1" 
AR Path="/61A3BF96/61A8CFA4" Ref="D5"  Part="1" 
F 0 "D5" V 5954 4720 50  0000 L CNN
F 1 "B5819W" V 6045 4720 50  0000 L CNN
F 2 "Diode_SMD:D_SOD-123" V 6000 4650 50  0001 C CNN
F 3 "~" V 6000 4650 50  0001 C CNN
	1    6000 4650
	0    1    1    0   
$EndComp
$Comp
L Device:R_Small_US R?
U 1 1 61A8CFAA
P 7150 4650
AR Path="/61A8CFAA" Ref="R?"  Part="1" 
AR Path="/61A3BF96/61A8CFAA" Ref="R13"  Part="1" 
F 0 "R13" H 7218 4696 50  0000 L CNN
F 1 "11k" H 7218 4605 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" H 7150 4650 50  0001 C CNN
F 3 "~" H 7150 4650 50  0001 C CNN
	1    7150 4650
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small_US R?
U 1 1 61A8CFB0
P 7000 4450
AR Path="/61A8CFB0" Ref="R?"  Part="1" 
AR Path="/61A3BF96/61A8CFB0" Ref="R12"  Part="1" 
F 0 "R12" V 6795 4450 50  0000 C CNN
F 1 "18k" V 6886 4450 50  0000 C CNN
F 2 "Resistor_SMD:R_0603_1608Metric" H 7000 4450 50  0001 C CNN
F 3 "~" H 7000 4450 50  0001 C CNN
	1    7000 4450
	0    1    1    0   
$EndComp
$Comp
L Device:L_Core_Iron_Small L?
U 1 1 61A8CFB6
P 6250 4450
AR Path="/61A8CFB6" Ref="L?"  Part="1" 
AR Path="/61A3BF96/61A8CFB6" Ref="L2"  Part="1" 
F 0 "L2" V 6455 4450 50  0000 C CNN
F 1 "220u" V 6364 4450 50  0000 C CNN
F 2 "Inductor_SMD:L_1210_3225Metric" H 6250 4450 50  0001 C CNN
F 3 "~" H 6250 4450 50  0001 C CNN
	1    6250 4450
	0    -1   -1   0   
$EndComp
$Comp
L power:GND #PWR?
U 1 1 61A8CFBC
P 4750 4450
AR Path="/61A8CFBC" Ref="#PWR?"  Part="1" 
AR Path="/61A3BF96/61A8CFBC" Ref="#PWR032"  Part="1" 
F 0 "#PWR032" H 4750 4200 50  0001 C CNN
F 1 "GND" H 4755 4277 50  0000 C CNN
F 2 "" H 4750 4450 50  0001 C CNN
F 3 "" H 4750 4450 50  0001 C CNN
	1    4750 4450
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small_US R?
U 1 1 61A8CFC2
P 6000 4150
AR Path="/61A8CFC2" Ref="R?"  Part="1" 
AR Path="/61A3BF96/61A8CFC2" Ref="R11"  Part="1" 
F 0 "R11" V 5795 4150 50  0000 C CNN
F 1 "2" V 5886 4150 50  0000 C CNN
F 2 "Resistor_SMD:R_1206_3216Metric" H 6000 4150 50  0001 C CNN
F 3 "~" H 6000 4150 50  0001 C CNN
	1    6000 4150
	0    1    1    0   
$EndComp
Wire Wire Line
	5850 4250 5850 4350
Connection ~ 5850 4250
Wire Wire Line
	5800 4250 5850 4250
Wire Wire Line
	5850 4150 5800 4150
Wire Wire Line
	5850 4350 5800 4350
Wire Wire Line
	5850 4150 5850 4250
$Comp
L Regulator_Switching:MC34063AD U?
U 1 1 61A8CFD4
P 5400 4350
AR Path="/61A8CFD4" Ref="U?"  Part="1" 
AR Path="/61A3BF96/61A8CFD4" Ref="U4"  Part="1" 
F 0 "U4" H 5400 4817 50  0000 C CNN
F 1 "MC34063AD" H 5400 4726 50  0000 C CNN
F 2 "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm" H 5450 3900 50  0001 L CNN
F 3 "http://www.onsemi.com/pub_link/Collateral/MC34063A-D.PDF" H 5900 4250 50  0001 C CNN
	1    5400 4350
	1    0    0    -1  
$EndComp
$Comp
L Device:LED_Small D?
U 1 1 61A8CFDA
P 6750 3300
AR Path="/61A8CFDA" Ref="D?"  Part="1" 
AR Path="/61A3BF96/61A8CFDA" Ref="D4"  Part="1" 
F 0 "D4" H 6750 3093 50  0000 C CNN
F 1 "GREEN" H 6750 3184 50  0000 C CNN
F 2 "LED_SMD:LED_0603_1608Metric" V 6750 3300 50  0001 C CNN
F 3 "~" V 6750 3300 50  0001 C CNN
	1    6750 3300
	-1   0    0    1   
$EndComp
$Comp
L Device:R_Small_US R?
U 1 1 61A8CFE0
P 6950 3300
AR Path="/61A8CFE0" Ref="R?"  Part="1" 
AR Path="/61A3BF96/61A8CFE0" Ref="R10"  Part="1" 
F 0 "R10" V 7155 3300 50  0000 C CNN
F 1 "33" V 7064 3300 50  0000 C CNN
F 2 "Resistor_SMD:R_0402_1005Metric" H 6950 3300 50  0001 C CNN
F 3 "~" H 6950 3300 50  0001 C CNN
	1    6950 3300
	0    -1   -1   0   
$EndComp
$Comp
L power:GND #PWR?
U 1 1 61A8CFE6
P 7050 3300
AR Path="/61A8CFE6" Ref="#PWR?"  Part="1" 
AR Path="/61A3BF96/61A8CFE6" Ref="#PWR030"  Part="1" 
F 0 "#PWR030" H 7050 3050 50  0001 C CNN
F 1 "GND" V 7055 3172 50  0000 R CNN
F 2 "" H 7050 3300 50  0001 C CNN
F 3 "" H 7050 3300 50  0001 C CNN
	1    7050 3300
	0    -1   -1   0   
$EndComp
$Comp
L power:+3.3V #PWR?
U 1 1 61A8CFEC
P 6450 3300
AR Path="/61A8CFEC" Ref="#PWR?"  Part="1" 
AR Path="/61A3BF96/61A8CFEC" Ref="#PWR029"  Part="1" 
F 0 "#PWR029" H 6450 3150 50  0001 C CNN
F 1 "+3.3V" V 6465 3428 50  0000 L CNN
F 2 "" H 6450 3300 50  0001 C CNN
F 3 "" H 6450 3300 50  0001 C CNN
	1    6450 3300
	0    -1   -1   0   
$EndComp
$Comp
L Device:R_Small_US R?
U 1 1 61A8CFF2
P 6550 3300
AR Path="/61A8CFF2" Ref="R?"  Part="1" 
AR Path="/61A3BF96/61A8CFF2" Ref="R9"  Part="1" 
F 0 "R9" V 6755 3300 50  0000 C CNN
F 1 "0" V 6664 3300 50  0000 C CNN
F 2 "Resistor_SMD:R_0402_1005Metric" H 6550 3300 50  0001 C CNN
F 3 "~" H 6550 3300 50  0001 C CNN
	1    6550 3300
	0    -1   -1   0   
$EndComp
$Comp
L power:PWR_FLAG #FLG03
U 1 1 61C855E7
P 6500 4150
F 0 "#FLG03" H 6500 4225 50  0001 C CNN
F 1 "PWR_FLAG" V 6500 4278 50  0000 L CNN
F 2 "" H 6500 4150 50  0001 C CNN
F 3 "~" H 6500 4150 50  0001 C CNN
	1    6500 4150
	0    1    1    0   
$EndComp
Connection ~ 6500 4150
$Comp
L Device:CP1_Small C?
U 1 1 61CA068F
P 6850 4650
AR Path="/61CA068F" Ref="C?"  Part="1" 
AR Path="/61A3BF96/61CA068F" Ref="C15"  Part="1" 
F 0 "C15" H 6941 4696 50  0000 L CNN
F 1 "10u" H 6941 4605 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric" H 6850 4650 50  0001 C CNN
F 3 "~" H 6850 4650 50  0001 C CNN
	1    6850 4650
	1    0    0    -1  
$EndComp
Wire Wire Line
	6850 4450 6850 4550
Wire Wire Line
	6500 4450 6850 4450
$Comp
L power:GND #PWR?
U 1 1 61CA18E8
P 6850 4750
AR Path="/61CA18E8" Ref="#PWR?"  Part="1" 
AR Path="/61A3BF96/61CA18E8" Ref="#PWR035"  Part="1" 
F 0 "#PWR035" H 6850 4500 50  0001 C CNN
F 1 "GND" H 6855 4577 50  0000 C CNN
F 2 "" H 6850 4750 50  0001 C CNN
F 3 "" H 6850 4750 50  0001 C CNN
	1    6850 4750
	1    0    0    -1  
$EndComp
Wire Wire Line
	6850 4450 6900 4450
Connection ~ 6850 4450
Wire Wire Line
	5800 5000 7450 5000
Wire Wire Line
	7150 5500 7450 5500
Wire Wire Line
	6950 5500 6600 5500
Connection ~ 6600 5500
$Comp
L Connector:Barrel_Jack J1
U 1 1 61CB972B
P 4500 2750
F 0 "J1" H 4557 2433 50  0000 C CNN
F 1 "Barrel_Jack" H 4557 2524 50  0000 C CNN
F 2 "Connector_BarrelJack:BarrelJack_CUI_PJ-063AH_Horizontal" H 4550 2710 50  0001 C CNN
F 3 "~" H 4550 2710 50  0001 C CNN
	1    4500 2750
	1    0    0    1   
$EndComp
$Comp
L Device:Battery BT?
U 1 1 61CBB0C5
P 3800 2600
AR Path="/61CBB0C5" Ref="BT?"  Part="1" 
AR Path="/61A3BF96/61CBB0C5" Ref="BT1"  Part="1" 
F 0 "BT1" H 3908 2646 50  0000 L CNN
F 1 "9V" H 3908 2555 50  0000 L CNN
F 2 "TestPoint:TestPoint_2Pads_Pitch5.08mm_Drill1.3mm" V 3800 2660 50  0001 C CNN
F 3 "~" V 3800 2660 50  0001 C CNN
	1    3800 2600
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR?
U 1 1 61CBBA23
P 4800 2850
AR Path="/61CBBA23" Ref="#PWR?"  Part="1" 
AR Path="/61A3BF96/61CBBA23" Ref="#PWR027"  Part="1" 
F 0 "#PWR027" H 4800 2600 50  0001 C CNN
F 1 "GND" H 4805 2677 50  0000 C CNN
F 2 "" H 4800 2850 50  0001 C CNN
F 3 "" H 4800 2850 50  0001 C CNN
	1    4800 2850
	1    0    0    -1  
$EndComp
$Comp
L Device:D_Schottky_Small D?
U 1 1 61CBBF80
P 5100 2650
AR Path="/61CBBF80" Ref="D?"  Part="1" 
AR Path="/61A3BF96/61CBBF80" Ref="D3"  Part="1" 
F 0 "D3" H 5100 2443 50  0000 C CNN
F 1 "B5819W" H 5100 2534 50  0000 C CNN
F 2 "Diode_SMD:D_SOD-123" V 5100 2650 50  0001 C CNN
F 3 "~" V 5100 2650 50  0001 C CNN
	1    5100 2650
	-1   0    0    1   
$EndComp
$Comp
L Device:Polyfuse_Small F?
U 1 1 61CBC5D4
P 6550 2450
AR Path="/61CBC5D4" Ref="F?"  Part="1" 
AR Path="/61A3BF96/61CBC5D4" Ref="F1"  Part="1" 
F 0 "F1" V 6663 2450 50  0000 C CNN
F 1 "250mA" V 6754 2450 50  0000 C CNN
F 2 "Fuse:Fuse_1206_3216Metric" H 6600 2250 50  0001 L CNN
F 3 "~" H 6550 2450 50  0001 C CNN
	1    6550 2450
	0    1    1    0   
$EndComp
Text GLabel 6900 2450 2    50   Input ~ 0
BUCK_IN
$Comp
L Transistor_FET:AO3401A Q?
U 1 1 61CCE9A3
P 5500 2350
AR Path="/61CCE9A3" Ref="Q?"  Part="1" 
AR Path="/61A3BF96/61CCE9A3" Ref="Q1"  Part="1" 
F 0 "Q1" V 5842 2350 50  0000 C CNN
F 1 "AO3401A" V 5751 2350 50  0000 C CNN
F 2 "Package_TO_SOT_SMD:SOT-23" H 5700 2275 50  0001 L CIN
F 3 "http://www.aosmd.com/pdfs/datasheet/AO3401A.pdf" H 5500 2350 50  0001 L CNN
	1    5500 2350
	0    -1   -1   0   
$EndComp
$Comp
L Device:R_Small_US R?
U 1 1 61CD3EB6
P 5500 2750
AR Path="/61CD3EB6" Ref="R?"  Part="1" 
AR Path="/61A3BF96/61CD3EB6" Ref="R8"  Part="1" 
F 0 "R8" H 5568 2796 50  0000 L CNN
F 1 "10k" H 5568 2705 50  0000 L CNN
F 2 "Resistor_SMD:R_0402_1005Metric" H 5500 2750 50  0001 C CNN
F 3 "~" H 5500 2750 50  0001 C CNN
	1    5500 2750
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR?
U 1 1 61CD54AE
P 5500 2850
AR Path="/61CD54AE" Ref="#PWR?"  Part="1" 
AR Path="/61A3BF96/61CD54AE" Ref="#PWR028"  Part="1" 
F 0 "#PWR028" H 5500 2600 50  0001 C CNN
F 1 "GND" H 5505 2677 50  0000 C CNN
F 2 "" H 5500 2850 50  0001 C CNN
F 3 "" H 5500 2850 50  0001 C CNN
	1    5500 2850
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR?
U 1 1 61CE5C87
P 3800 2800
AR Path="/61CE5C87" Ref="#PWR?"  Part="1" 
AR Path="/61A3BF96/61CE5C87" Ref="#PWR026"  Part="1" 
F 0 "#PWR026" H 3800 2550 50  0001 C CNN
F 1 "GND" H 3805 2627 50  0000 C CNN
F 2 "" H 3800 2800 50  0001 C CNN
F 3 "" H 3800 2800 50  0001 C CNN
	1    3800 2800
	1    0    0    -1  
$EndComp
Wire Wire Line
	5800 2250 5700 2250
$Comp
L Device:D_Schottky_Small D?
U 1 1 61CF73A7
P 6250 2250
AR Path="/61CF73A7" Ref="D?"  Part="1" 
AR Path="/61A3BF96/61CF73A7" Ref="D2"  Part="1" 
F 0 "D2" V 6296 2180 50  0000 R CNN
F 1 "B5819W" V 6205 2180 50  0000 R CNN
F 2 "Diode_SMD:D_SOD-123" V 6250 2250 50  0001 C CNN
F 3 "~" V 6250 2250 50  0001 C CNN
	1    6250 2250
	0    -1   -1   0   
$EndComp
Wire Wire Line
	5800 2450 6250 2450
Wire Wire Line
	6250 2450 6250 2350
$Comp
L power:VBUS #PWR?
U 1 1 61CF865D
P 6250 2000
AR Path="/61CF865D" Ref="#PWR?"  Part="1" 
AR Path="/61A3BF96/61CF865D" Ref="#PWR025"  Part="1" 
F 0 "#PWR025" H 6250 1850 50  0001 C CNN
F 1 "VBUS" H 6265 2173 50  0000 C CNN
F 2 "" H 6250 2000 50  0001 C CNN
F 3 "" H 6250 2000 50  0001 C CNN
	1    6250 2000
	1    0    0    -1  
$EndComp
Wire Wire Line
	6250 2000 6250 2150
Wire Wire Line
	6900 2450 6800 2450
$Comp
L power:PWR_FLAG #FLG02
U 1 1 61CFBFC5
P 6800 2450
F 0 "#FLG02" H 6800 2525 50  0001 C CNN
F 1 "PWR_FLAG" V 6800 2578 50  0000 L CNN
F 2 "" H 6800 2450 50  0001 C CNN
F 3 "~" H 6800 2450 50  0001 C CNN
	1    6800 2450
	1    0    0    -1  
$EndComp
Wire Wire Line
	6450 2450 6250 2450
Connection ~ 6250 2450
Wire Wire Line
	6650 2450 6800 2450
Connection ~ 6800 2450
Wire Wire Line
	4800 2650 5000 2650
Wire Wire Line
	3800 2250 5300 2250
Wire Wire Line
	3800 2250 3800 2400
Wire Wire Line
	5200 2650 5500 2650
Connection ~ 5500 2650
Wire Wire Line
	5500 2650 5800 2650
Wire Wire Line
	5500 2550 5500 2650
Wire Wire Line
	5800 2250 5800 2450
Connection ~ 5800 2450
Wire Wire Line
	5800 2450 5800 2650
Text Notes 4300 3200 0    39   ~ 0
Note: Tip/Center (pin 1) negative
Text GLabel 5550 6000 0    50   Input ~ 0
LINE_IN
Text GLabel 5950 5900 2    50   Input ~ 0
LINE_OUT
Text GLabel 5550 5800 0    50   Input ~ 0
OPAMP2_VOUT
Wire Wire Line
	5950 5500 6600 5500
$Comp
L STM32AudioEffects:SW_PUSH_OFF_ON_DPDT SW3
U 1 1 61DCCF53
P 5750 5700
F 0 "SW3" H 5750 6185 50  0000 C CNN
F 1 "SW_PUSH_OFF_ON_DPDT" H 5750 6094 50  0000 C CNN
F 2 "STM32AudioEffects:SW_DPDT_FOOT_17.2x12.2mm_Vertical" H 5750 5500 50  0001 C CNN
F 3 "~" H 5750 5500 50  0001 C CNN
	1    5750 5700
	-1   0    0    -1  
$EndComp
$EndSCHEMATC
