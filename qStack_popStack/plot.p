# Plot program for CPP Con 2021
set title "Live Performance Results" font ", 14"
set xlabel "Number of Threads" font ", 14"
set ylabel "Throughput (ops/microsecond)" font ", 14"
set tics font ", 11"
#set xrange [0:65]

set print "-"

set style line 1 lt 1 lw 1.5 pt 1 ps 1.25 lc "#FFF0E442" 
set style line 2 lt 1 lw 1.5 pt 9 ps 1.25 lc "#FF56B4E9"
set style line 3 lt 1 lw 1.5 pt 7 ps 1.25 lc "#FFE69F00"
set style line 4 lt 1 lw 1.5 pt 5 ps 1.25 lc "#FF009E73"
set style line 5 lt 1 lw 1.5 pt 5 ps 1.25 lc "#FFAA00AA"

#set term png
#set output "results.png"

while (1) {
	pause 1
	plot "QStack_No_Branch75.dat" using 3:6 smooth unique title 'Qstack No Branch' with linespoints ls 1, \
	"QStack75.dat" using 3:6 smooth unique title 'Qstack' with linespoints ls 2, \
	"EBS75.dat" using 3:6 smooth unique title 'EBS' with linespoints ls 3, \
	"Treiber75.dat" using 3:6 smooth unique title 'Treiber' with linespoints ls 4, \
	"QStack_Depth_Push75.dat" using 3:6 smooth unique title 'QStack_Depth_Push' with linespoints ls 5, \
}
