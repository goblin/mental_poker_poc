plot "stats.dat" using 1:2 title "running time in secs", "stats.dat" using 1:3 title "total data transferred", "stats.dat" using 1:(column(3) / (column(1))) title "data transferred per peer"
