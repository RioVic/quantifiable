# Victor Cook
# Plot Entropy results for CGO 2020
rm(list = ls()) # to cleanup environment
args = commandArgs(trailingOnly=TRUE)

library(reshape)
library(scales)
library(ggforce)
library(zoo)
library(ggplot2)
library(tidyverse)
library(plyr)


if (length(args)==0) {
    stop("Wrong agrument number!!! You should specify raw data input directory (with 16k, 32k and 1k subdirectories)\n", call=FALSE)
}

wd <- setwd(args[1])
cat("Current working dir: ", wd, "\n")

# inversion table function
# input sequence A must be a permutation of integers from 1 to len
# to use with any numbers, sort them and index them
inversionTable <- function(A, len){
  # returns the inversion table B for the input sequence A
  B <- A
  # for every index j
  for (j in 1:len) {
    B[j] <- 0
    # count the items greater than j to the left of j in A
    for (n in 1:len) {
      # cat(j,B[j],n,A[n], '\n')
      if (A[n] > j)
        B[j] <- B[j] + 1 
      if (A[n] == j) 
        break
    }
  }
  return(B)
}

# inversion test cases
seq1 <- c(5,9,1,8,2,6,4,7,3)
seq1 <- c(9,8,7,6,5,4,3,2,1)
seq1 <- c(1,2,3,5,4,6,7,8,9)
seq1 <- c(2,1,4,3,6,5,8,7,9)
invTab1 <- inversionTable(seq1, 9)
invTab1
inv1 <- sum(invTab1)
inv1

# List input raw data files by mask
# 1k / 16k / 32k
list_files <- list.files("./1k/", recursive=TRUE, pattern="AMD_.*inversions.dat$", full.names = FALSE)

for (i in 1: length(list_files)) {

    n1k_file <- list_files[i]
    n16k_file <- list_files[i]
    n32k_file <- list_files[i]
    n2k_file <- list_files[i]
    n4k_file <- list_files[i]
    n8k_file <- list_files[i]
    n24k_file <- list_files[i]

    n16k_file <- str_replace(n16k_file, "1k", "16k")
    n32k_file <- str_replace(n32k_file, "1k", "32k")
    n2k_file <- str_replace(n2k_file, "1k", "2k")
    n4k_file <- str_replace(n4k_file, "1k", "4k")
    n8k_file <- str_replace(n8k_file, "1k", "8k")
    n24k_file <- str_replace(n24k_file, "1k", "24k")

    output_file <- str_replace(n1k_file, "_inversions.dat", ".pdf")


    #ssp <- unlist(strsplit(output_file, "_"))
    #sthds <- strtoi(substr(ssp[4], 0, nchar(ssp[4])-1), base=10)
    #skbts <- strtoi(substr(ssp[3], 0, nchar(ssp[3])-1), base=10)
    # AMD_KFifoQueue_16k_10t_100i_1000_inversions.dat
    #output_file <- paste(ssp[1],sprintf("%02dt",sthds),ssp[5],ssp[6],sep="_")

    raw_output_file <- str_replace(output_file, "AMD", "raw")
    hist_output_file <- str_replace(output_file, "AMD", "hist")

    n1k_file_path = paste("./1k/", n1k_file, sep = "")
    n16k_file_path = paste("./16k/", n16k_file, sep = "")
    n32k_file_path = paste("./32k/", n32k_file, sep="")
    n2k_file_path = paste("./2k/", n2k_file, sep="")
    n4k_file_path = paste("./4k/", n4k_file, sep="")
    n8k_file_path = paste("./8k/", n8k_file, sep="")
    n24k_file_path = paste("./24k/", n24k_file, sep="")

    if (file.exists(n1k_file_path)
	&& file.exists(n16k_file_path)
	&& file.exists(n32k_file_path)
        && file.exists(n2k_file_path)
        && file.exists(n4k_file_path)
        && file.exists(n8k_file_path)
        && file.exists(n24k_file_path)) {

    cat("Processing ", n1k_file_path, "+", n16k_file_path, "+ ... to ", raw_output_file, " and ", hist_output_file, "\n\n")

# Columns of input data

# arch algo	   method	proc	object	item	invoke	          finish	start_order	method2	item2	invoke2	          finish2	finish_order	inversion
# AMD	 QStack Pop	  0	    x	      99999	1399704548252440	0	      0	          Pop	    99999	1399703187767120	0	      0	            0
# Quantifiable
tQ <- read.table(n1k_file_path, sep = "",header = TRUE)
tQ <- tQ %>% mutate(arch=as.factor(arch), algo=as.factor(algo), method=as.factor(method), proc=as.numeric(proc), 
                    object=as.factor(object), item=as.numeric(item), invoke=as.numeric(invoke), finish=as.numeric(finish),
                    start_order=as.numeric(start_order), method2=as.factor(method2), invoke2=as.numeric(invoke2), finish2=as.numeric(finish2),
                    finish_order=as.numeric(finish_order), inversion=as.numeric(inversion))
zero_time <- min(tQ$invoke)
zero_time
tQ$invoke <- tQ$invoke - zero_time
tQ$finish <- tQ$finish - zero_time
tQ["call_interval"] <- tQ$finish - tQ$invoke

tQ$invoke2 <- tQ$invoke2 - zero_time
tQ$finish2 <- tQ$finish2 - zero_time
tQ["call_interval2"] <- tQ$finish2 - tQ$invoke2
summary(tQ)

# arch algo	   method	proc	object	item	invoke	          finish	start_order	method2	item2	invoke2	          finish2	finish_order	inversion
# AMD	 EBS Pop	  0	    x	      99999	1399704548252440	0	      0	          Pop	    99999	1399703187767120	0	      0	            0
# EBS
tE <- read.table(n16k_file_path, sep = "",header = TRUE)
tE <- tE %>% mutate(arch=as.factor(arch), algo=as.factor(algo), method=as.factor(method), proc=as.numeric(proc), 
                    object=as.factor(object), item=as.numeric(item), invoke=as.numeric(invoke), finish=as.numeric(finish),
                    start_order=as.numeric(start_order), method2=as.factor(method2), invoke2=as.numeric(invoke2), finish2=as.numeric(finish2),
                    finish_order=as.numeric(finish_order), inversion=as.numeric(inversion))
zero_time <- min(tE$invoke)
zero_time
tE$invoke <- tE$invoke - zero_time
tE$finish <- tE$finish - zero_time
tE["call_interval"] <- tE$finish - tE$invoke

tE$invoke2 <- tE$invoke2 - zero_time
tE$finish2 <- tE$finish2 - zero_time
tE["call_interval2"] <- tE$finish2 - tE$invoke2
summary(tE)

# Treiber
tT <- read.table(n32k_file_path, sep = "",header = TRUE)
tT <- tT %>% mutate(arch=as.factor(arch), algo=as.factor(algo), method=as.factor(method), proc=as.numeric(proc), 
                    object=as.factor(object), item=as.numeric(item), invoke=as.numeric(invoke), finish=as.numeric(finish),
                    start_order=as.numeric(start_order), method2=as.factor(method2), invoke2=as.numeric(invoke2), finish2=as.numeric(finish2),
                    finish_order=as.numeric(finish_order), inversion=as.numeric(inversion))
zero_time <- min(tT$invoke)
zero_time
tT$invoke <- tT$invoke - zero_time
tT$finish <- tT$finish - zero_time
tT["call_interval"] <- tT$finish - tT$invoke

tT$invoke2 <- tT$invoke2 - zero_time
tT$finish2 <- tT$finish2 - zero_time
tT["call_interval2"] <- tT$finish2 - tT$invoke2
summary(tT)


# 2k 
t2T <- read.table(n2k_file_path, sep = "",header = TRUE)
t2T <- t2T %>% mutate(arch=as.factor(arch), algo=as.factor(algo), method=as.factor(method), proc=as.numeric(proc),
                    object=as.factor(object), item=as.numeric(item), invoke=as.numeric(invoke), finish=as.numeric(finish),
                    start_order=as.numeric(start_order), method2=as.factor(method2), invoke2=as.numeric(invoke2), finish2=as.numeric(finish2),
                    finish_order=as.numeric(finish_order), inversion=as.numeric(inversion))
zero_time <- min(t2T$invoke)
t2T$invoke <- t2T$invoke - zero_time
t2T$finish <- t2T$finish - zero_time
t2T["call_interval"] <- t2T$finish - t2T$invoke
t2T$invoke2 <- t2T$invoke2 - zero_time
t2T$finish2 <- t2T$finish2 - zero_time
t2T["call_interval2"] <- t2T$finish2 - t2T$invoke2
summary(t2T)

# 4k 
t4T <- read.table(n4k_file_path, sep = "",header = TRUE)
t4T <- t4T %>% mutate(arch=as.factor(arch), algo=as.factor(algo), method=as.factor(method), proc=as.numeric(proc),
                    object=as.factor(object), item=as.numeric(item), invoke=as.numeric(invoke), finish=as.numeric(finish),
                    start_order=as.numeric(start_order), method2=as.factor(method2), invoke2=as.numeric(invoke2), finish2=as.numeric(finish2),
                    finish_order=as.numeric(finish_order), inversion=as.numeric(inversion))
zero_time <- min(t4T$invoke)
t4T$invoke <- t4T$invoke - zero_time
t4T$finish <- t4T$finish - zero_time
t4T["call_interval"] <- t4T$finish - t4T$invoke
t4T$invoke2 <- t4T$invoke2 - zero_time
t4T$finish2 <- t4T$finish2 - zero_time
t4T["call_interval2"] <- t4T$finish2 - t4T$invoke2
summary(t4T)



# 8k
t8T <- read.table(n8k_file_path, sep = "",header = TRUE)
t8T <- t8T %>% mutate(arch=as.factor(arch), algo=as.factor(algo), method=as.factor(method), proc=as.numeric(proc),
                    object=as.factor(object), item=as.numeric(item), invoke=as.numeric(invoke), finish=as.numeric(finish),
                    start_order=as.numeric(start_order), method2=as.factor(method2), invoke2=as.numeric(invoke2), finish2=as.numeric(finish2),
                    finish_order=as.numeric(finish_order), inversion=as.numeric(inversion))
zero_time <- min(t8T$invoke)
t8T$invoke <- t8T$invoke - zero_time
t8T$finish <- t8T$finish - zero_time
t8T["call_interval"] <- t8T$finish - t8T$invoke
t8T$invoke2 <- t8T$invoke2 - zero_time
t8T$finish2 <- t8T$finish2 - zero_time
t8T["call_interval2"] <- t8T$finish2 - t8T$invoke2
summary(t8T)



# 24k
t24T <- read.table(n24k_file_path, sep = "",header = TRUE)
t24T <- t24T %>% mutate(arch=as.factor(arch), algo=as.factor(algo), method=as.factor(method), proc=as.numeric(proc),
                    object=as.factor(object), item=as.numeric(item), invoke=as.numeric(invoke), finish=as.numeric(finish),
                    start_order=as.numeric(start_order), method2=as.factor(method2), invoke2=as.numeric(invoke2), finish2=as.numeric(finish2),
                    finish_order=as.numeric(finish_order), inversion=as.numeric(inversion))
zero_time <- min(t24T$invoke)
t24T$invoke <- t24T$invoke - zero_time
t24T$finish <- t24T$finish - zero_time
t24T["call_interval"] <- t24T$finish - t24T$invoke
t24T$invoke2 <- t24T$invoke2 - zero_time
t24T$finish2 <- t24T$finish2 - zero_time
t24T["call_interval2"] <- t24T$finish2 - t24T$invoke2
summary(t24T)



i1 <- 100
i2 <- length(tQ$start_order)

# count the inversions QStack
tQ <- tQ[i1:i2,]
tQ$finish_order
tQ["invTable"] <- 0
tQ$invTable2 <- tQ$inversion
tQsum2 <- sum(tQ$invTable2)
tQzero2 <- sum(tQ$invTable2 == 0)
tQone2 <- sum(tQ$invTable2 == 1)

# count the inversions EBS
i2 <- length(tE$start_order)
tE <- tE[i1:i2,]
tE$finish_order
tE["invTable"] <- 0
tE$invTable2 <- tE$inversion
tEsum2 <- sum(tE$invTable2)
tEzero2 <- sum(tE$invTable2 == 0)

# count the inversions Treiber
i2 <- length(tT$start_order)
tT <- tT[i1:i2,]
tT$finish_order
tT["invTable"] <- 0
tT$invTable2 <- tT$inversion 
tTsum2 <- sum(tT$invTable2)
tTzero2 <- sum(tT$invTable2 == 0)

# count the inversions 2k
i2 <- length(t2T$start_order)
t2T <- t2T[i1:i2,]
t2T$finish_order
t2T["invTable"] <- 0
t2T$invTable2 <- t2T$inversion
t2Tsum2 <- sum(t2T$invTable2)
t2Tzero2 <- sum(t2T$invTable2 == 0)

# count the inversions 4k
i2 <- length(t4T$start_order)
t4T <- t4T[i1:i2,]
t4T$finish_order
t4T["invTable"] <- 0
t4T$invTable2 <- t4T$inversion
t4Tsum2 <- sum(t4T$invTable2)
t4Tzero2 <- sum(t4T$invTable2 == 0)

# count the inversions 8k
i2 <- length(t8T$start_order)
t8T <- t8T[i1:i2,]
t8T$finish_order
t8T["invTable"] <- 0
t8T$invTable2 <- t8T$inversion
t8Tsum2 <- sum(t8T$invTable2)
t8Tzero2 <- sum(t8T$invTable2 == 0)

# count the inversions 24k
i2 <- length(t24T$start_order)
t24T <- t24T[i1:i2,]
t24T$finish_order
t24T["invTable"] <- 0
t24T$invTable2 <- t24T$inversion
t24Tsum2 <- sum(t24T$invTable2)
t24Tzero2 <- sum(t24T$invTable2 == 0)


#                EBS Orange  T Green    Fuscia     Q blue     Dk Blue    Dk Orange  Fuscia  
#cbbPalette <- c("#E69F00",  "#009E73", "#CC79A7", "#56B4E9", "#0072B2", "#D55E00", "#CC79A7")

max_i <- i2
#
# Parameters to scale raw and hist plots up to N-threads
# For threads = 1,2,3,4 the following settings are recommended
#     max_p <- 70 scale_p_breaks <- 16 scale_p_minor_breaks <- 4
# For number of threads at about 16 the following settings are recommended
#     max_p <- 200 scale_p_breaks <- 32 scale_p_minor_breaks <- 8
#

#max_p <- 320
#scale_p_breaks <- 80
#scale_p_minor_breaks <- 20

max_p <- 192
scale_p_breaks <- 32
scale_p_minor_breaks <- 8

# raw data plots X(item)
#raw_i2 <- ggplot() +
#  labs( x="Item index", y="Inversion distance of events X(item)") +
#   geom_point(aes(x=tQ$start_order, y=tQ$invTable2), col = "#56B4E9", alpha=0.3, size=1, show.legend=TRUE) +
#   geom_point(aes(x=tE$start_order, y=tE$invTable2), col = "#E69F00", alpha=0.3, size=1, show.legend=TRUE) +
#  geom_point(aes(x=tT$start_order, y=tT$invTable2), col = "#009E73", alpha=0.3, size=1, show.legend=TRUE) +
#  scale_x_continuous(expand=c(0,0), limits = c(0,max_i), breaks = seq(0,i2,max_i/5),
#                     minor_breaks = seq(0,max_i,max_i/10)) +
#  scale_y_continuous(expand=c(0,0), limits=c(-1,max_p), breaks = seq(0,max_p,scale_p_breaks), 
#                     minor_breaks = seq(0,max_p,scale_p_minor_breaks)) +
#  theme_bw() +
#  theme(axis.title=element_text(size=8), 
#        axis.text=element_text(size=6),
#        panel.grid.minor = element_line(size = 0.5),
#        aspect.ratio=0.6,
#	plot.margin=unit(c(0,10,0,10),"points"))
#ggsave(raw_output_file)

# calculate entropy H(X)
entropyShannon <- function(P){
  # returns the Shannon entropy H(X) given the distribution P(x)
  H <- 0
  len <- length(P)
  # for every index i
  for (i in 1:len) {
    h = P[i] * log2(P[i])
    H <- H + h
    # cat(i,"  ",P[i],"\n")
  }
  return(-H)
}

# tabulate the discrete probabilities
# tQ$invTable2
tQinv <- table(tQ$invTable2)/i2
tEinv <- table(tE$invTable2)/i2
tTinv <- table(tT$invTable2)/i2
t2Tinv <- table(t2T$invTable2)/i2
t4Tinv <- table(t4T$invTable2)/i2
t8Tinv <- table(t8T$invTable2)/i2
t24Tinv <- table(t24T$invTable2)/i2

TtQinv <- as.data.frame(tQinv)
TtEinv <- as.data.frame(tEinv)
TtTinv <- as.data.frame(tTinv)
T2tTinv <- as.data.frame(t2Tinv)
T4tTinv <- as.data.frame(t4Tinv)
T8tTinv <- as.data.frame(t8Tinv)
T24tTinv <- as.data.frame(t24Tinv)

tQentropy <- entropyShannon(tQinv)/i2
tEentropy <- entropyShannon(tEinv)/i2
tTentropy <- entropyShannon(tTinv)/i2
t2Tentropy <- entropyShannon(t2Tinv)/i2
t4Tentropy <- entropyShannon(t4Tinv)/i2
t8Tentropy <- entropyShannon(t8Tinv)/i2
t24Tentropy <- entropyShannon(t24Tinv)/i2

max_p <- 32
scale_p_breaks <- 4 
scale_p_minor_breaks <- 1 

#max_p <- 320
#scale_p_breaks <- 80
#scale_p_minor_breaks <- 20

#max_y <- max_i / 4

max_y = 0.25 

TtQinv$Var1=as.numeric(TtQinv$Var1)[TtQinv$Var1]
TtEinv$Var1=as.numeric(TtEinv$Var1)[TtEinv$Var1]
TtTinv$Var1=as.numeric(TtTinv$Var1)[TtTinv$Var1]
T2tTinv$Var1=as.numeric(T2tTinv$Var1)[T2tTinv$Var1]
T4tTinv$Var1=as.numeric(T4tTinv$Var1)[T4tTinv$Var1]
T8tTinv$Var1=as.numeric(T8tTinv$Var1)[T8tTinv$Var1]
T24tTinv$Var1=as.numeric(T24tTinv$Var1)[T24tTinv$Var1]

# shift vactors
for (i in min(TtQinv$Var1):max(TtQinv$Var1)) {
    TtQinv$Var1[i] <- TtQinv$Var1[i] - 1
}
for (i in min(TtEinv$Var1):max(TtEinv$Var1)) {
    TtEinv$Var1[i] <- TtEinv$Var1[i] - 1
}
for (i in min(TtTinv$Var1):max(TtTinv$Var1)) {
    TtTinv$Var1[i] <- TtTinv$Var1[i] - 1
}
for (i in min(T2tTinv$Var1):max(T2tTinv$Var1)) {
    T2tTinv$Var1[i] <- T2tTinv$Var1[i] - 1
}
for (i in min(T4tTinv$Var1):max(T4tTinv$Var1)) {
    T4tTinv$Var1[i] <- T4tTinv$Var1[i] - 1
}
for (i in min(T8tTinv$Var1):max(T8tTinv$Var1)) {
    T8tTinv$Var1[i] <- T8tTinv$Var1[i] - 1
}
for (i in min(T24tTinv$Var1):max(T24tTinv$Var1)) {
    T24tTinv$Var1[i] <- T24tTinv$Var1[i] - 1
}

# histograms P(x)
hist_i2 <- ggplot() +
geom_errorbarh(data = TtQinv, mapping = aes(x = Var1, y = Freq, xmin = Var1-0.5, xmax = Var1+0.5), col = "#808080", size=1, height=0) +
geom_line(data = TtQinv, aes(x=Var1, y=Freq), color = "#808080", size=0.5) +
geom_errorbarh(data = T2tTinv, mapping = aes(x = Var1, y = Freq, xmin = Var1-0.5, xmax = Var1+0.5), col = "#FF00FF", size=1, height=0) +
geom_line(data = T2tTinv, aes(x=Var1, y=Freq), color = "#FF00FF", size=0.5) +
geom_errorbarh(data = T4tTinv, mapping = aes(x = Var1, y = Freq, xmin = Var1-0.5, xmax = Var1+0.5), col = "#00FFFF", size=1, height=0) +
geom_line(data = T4tTinv, aes(x=Var1, y=Freq), color = "#00FFFF", size=0.5) +
geom_errorbarh(data = T8tTinv, mapping = aes(x = Var1, y = Freq, xmin = Var1-0.5, xmax = Var1+0.5), col = "#800000", size=1, height=0) +
geom_line(data = T8tTinv, aes(x=Var1, y=Freq), color = "#800000", size=0.5) +
geom_errorbarh(data = TtEinv, mapping = aes(x = Var1, y = Freq, xmin = Var1-0.5, xmax = Var1+0.5), col = "#0000FF", size=1, height=0) +
geom_line(data = TtEinv, aes(x=Var1, y=Freq), color = "#0000FF", size=0.5) +
geom_errorbarh(data = T24tTinv, mapping = aes(x = Var1, y = Freq, xmin = Var1-0.5, xmax = Var1+0.5), col = "#00FF00", size=1, height=0) +
geom_line(data = T24tTinv, aes(x=Var1, y=Freq), color = "#00FF00", size=0.5) +
geom_errorbarh(data = TtTinv, mapping = aes(x = Var1, y = Freq, xmin = Var1-0.5, xmax = Var1+0.5), col = "#FF0000", size=1, height=0) +
geom_line(data = TtTinv, aes(x=Var1, y=Freq), color = "#FF0000", size=0.5) +

  labs(x="Inversion distance of events X(item)", y="Probability P(x) of inversion event") +
  scale_x_continuous(expand=c(0,0), limits = c(-1,max_p), breaks = seq(0,max_p,scale_p_breaks),
                     minor_breaks = seq(0,max_p,scale_p_minor_breaks)) +
  scale_y_continuous(expand=c(0,0), breaks = seq(0,max_y,max_y/5), labels = seq(0,max_y,max_y/5)) +
  coord_cartesian(ylim=c(0,max_y)) +
  annotate("rect", xmin=c(max_p*0.75,max_p*0.75,max_p*0.75,max_p*0.75,max_p*0.75,max_p*0.75,max_p*0.75),
	           xmax=c(max_p*0.75+max_p/40,max_p*0.75+max_p/40,max_p*0.75+max_p/40,max_p*0.75+max_p/40,max_p*0.75+max_p/40,max_p*0.75+max_p/40,max_p*0.75+max_p/40),
                   ymin=c(max_y/75 + max_y*0.6, max_y/75 + max_y*0.6+2*max_y/40, max_y/75 + max_y*0.6+4*max_y/40, max_y/75 + max_y*0.6+6*max_y/40, max_y/75 + max_y*0.6+8*max_y/40, max_y/75 + max_y*0.6+10*max_y/40, max_y/75 + max_y*0.6+12*max_y/40),
		   ymax=c(max_y/90 + max_y*0.6, max_y/90 + max_y*0.6+2*max_y/40, max_y/90 + max_y*0.6+4*max_y/40, max_y/90 + max_y*0.6+6*max_y/40, max_y/90 + max_y*0.6+8*max_y/40, max_y/90 + max_y*0.6+10*max_y/40, max_y/90 + max_y*0.6+12*max_y/40),
		   #ymax=c(max_y*0.6+max_y/40, max_y*0.6+3*max_y/40, max_y*0.6+5*max_y/40, max_y*0.6+7*max_y/40, max_y*0.6+9*max_y/40, max_y*0.6+11*max_y/40, max_y*0.6+13*max_y/40),
           color=c("#808080", "#FF00FF", "#00FFFF", "#800000", "#0000FF", "#00FF00", "#FF0000"),
           fill=c(alpha("#808080", 0.1), alpha("#FF00FF", 0.1), alpha("#00FFFF", 0.1), alpha("#800000",0.1), alpha("#0000FF",0.1), alpha("#00FF00",0.1),alpha("#FF0000", 0.1))) +
  annotate("text", size=4, x=c(max_p*0.8, max_p*0.8, max_p*0.8, max_p*0.8, max_p*0.8, max_p*0.8, max_p*0.8),
                   y=c(
                       max_y*0.6+(0.5*max_y/40)
                       , max_y*0.6+2*max_y/40+(0.5*max_y/40)
		       , max_y*0.6+4*max_y/40+(0.5*max_y/40)
                       , max_y*0.6+6*max_y/40+(0.5*max_y/40)
                       , max_y*0.6+8*max_y/40+(0.5*max_y/40)
                       , max_y*0.6+10*max_y/40+(0.5*max_y/40)
                       , max_y*0.6+12*max_y/40+(0.5*max_y/40)
                       ),
           label= c(
                     "1k"
                     , "2k"
                     , "4k"
                     , "8k"
                     , "16k"
                     , "24k"
                     , "32k"
                     ), hjust = 0) +
  annotate("text", size=4, x=c(max_p*0.8-max_p/15, max_p*0.8-max_p/15, max_p*0.8-max_p/15, max_p*0.8-max_p/15, max_p*0.8-max_p/15, max_p*0.8-max_p/15, max_p*0.8-max_p/15),
           y=c(max_y*0.6+(0.5*max_y/40), max_y*0.6+2*max_y/40+(0.5*max_y/40), max_y*0.6+4*max_y/40+(0.5*max_y/40), max_y*0.6+6*max_y/40+(0.5*max_y/40), max_y*0.6+8*max_y/40+(0.5*max_y/40), max_y*0.6+10*max_y/40+(0.5*max_y/40), max_y*0.6+12*max_y/40+(0.5*max_y/40)),
           label= c(
                      sprintf("%3.2e      %.2e",tQsum2, tQentropy)		   
                    , sprintf("%3.2e      %.2e",t2Tsum2, t2Tentropy) 
                    , sprintf("%3.2e      %.2e",t4Tsum2, t4Tentropy)
                    , sprintf("%3.2e      %.2e",t8Tsum2, t8Tentropy)
                    , sprintf("%3.2e      %.2e",tEsum2,tEentropy)
                    , sprintf("%3.2e      %.2e",t24Tsum2, t24Tentropy)
                    , sprintf("%3.2e      %.2e",tTsum2, tTentropy)
                    ), 
           hjust = 1) +
  annotate("text", size=4, x=max_p*0.8-max_p/20, y=max_y*0.6+(15*max_y/40), label=sprintf("Inversions      Entropy  "), hjust = 1) +
  theme_bw() +
  theme(axis.title=element_text(size=14), 
        axis.text=element_text(size=12),
        panel.grid.minor = element_line(size = 0.5),
        aspect.ratio=0.6,
        plot.margin=unit(c(0,10,0,10),"points"))
#hist_i2
ggsave(hist_output_file)

}
}
