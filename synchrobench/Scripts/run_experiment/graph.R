# Plot synchrobench entropy results
rm(list = ls()) # to cleanup environment
args = commandArgs(trailingOnly=TRUE)

library(reshape)
library(scales)
library(ggforce)
library(zoo)
library(ggplot2)
library(tidyverse)
library(plyr)

help <- function() {
    cat(
"Usage: Rscript graph.R <output>.pdf [*_inversions.dat]+.
Creates a single graph with all the specified inversion files.
Inversion files should be in the format AMD_{items}i_{ops_thousands}ko_{threads}t_inversions.dat\n"
	)
}

if (length(args) <= 1) {
	help();
	stop("Invalid arguments\n", call=FALSE);
}

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

load_file_to_hist_plot <- function(plot, filename, color, index, start_index=1) {
	cat ("slot is:\n")
	cat (typeof(plot))
	cat("loading file\n", filename)
	# Read table	
	table <- read.table(filename, sep = "",header = TRUE)
	table <- table %>% mutate(arch=as.factor(arch), algo=as.factor(algo), method=as.factor(method), proc=as.numeric(proc), 
						object=as.factor(object), item=as.numeric(item), invoke=as.numeric(invoke), finish=as.numeric(finish),
						start_order=as.numeric(start_order), method2=as.factor(method2), invoke2=as.numeric(invoke2), finish2=as.numeric(finish2),
						finish_order=as.numeric(finish_order), inversion=as.numeric(inversion))
	cat('read table.')
	zero_time <- min(table$invoke)
	zero_time
	table$invoke <- table$invoke - zero_time
	table$finish <- table$finish - zero_time
	table["call_interval"] <- table$finish - table$invoke

	table$invoke2 <- table$invoke2 - zero_time
	table$finish2 <- table$finish2 - zero_time
	table["call_interval2"] <- table$finish2 - table$invoke2
	cat('starting summary')

	cat('counting inversions now')

	# Count inversions

	end_index <- length(table$start_order)
	table <- table[start_index:end_index,]
	table$finish_order
	table["invTable"] <- 0
	table$invTable2 <- table$inversion 
	sum2 <- sum(table$invTable2)
	zero2 <- sum(table$invTable2 == 0)
	
	cat('calculating entropyShannon now')
	# tabulate the discrete probabilities
	inv <- table(table$invTable2)/(end_index - start_index + 1)
	Tinv <- as.data.frame(inv)
	entropy <- entropyShannon(inv)/(end_index - start_index + 1)
	Tinv$Var1=as.numeric(Tinv$Var1)[Tinv$Var1]
	# gg plot will wrongly assume all the data is the same unless we do this.
	Tinv$key = index

	# shift vectors
	for (i in min(Tinv$Var1):max(Tinv$Var1)) {
		Tinv$Var1[i] <- Tinv$Var1[i] - 1
	}
	
	cat('generating histogram now')
	# histograms P(x)
	plot <- plot + 
	geom_errorbarh(data = Tinv, mapping = aes(x = Var1, y = Freq, xmin = Var1-0.5, xmax = Var1+0.5), col = color, size=1, height=0) +
	geom_line(data = Tinv, aes(x=Var1, y=Freq), color = color, size=0.5)

	return (list(sum2, entropy, plot))
}

# random distinct colours to fill graphs. Colours after the 7th one are from
# http://medialab.github.io/iwanthue/
color_template <- c("#808080", "#FF00FF", "#00FFFF", "#800000", "#0000FF", "#00FF00", "#FF0000", "#873d63",
					"#75d651", "#ab3ad2", "#d0d13d", "#6f5dd3", "#61883a", "#cd53b0", "#63d29e", "#4a2b79", 
					"#c9d488", "#3a2b3f", "#d64f29", "#86c3c6", "#d24c63", "#40573f", "#ce9ecd", "#c88d3b", 
					"#6481b5", "#774029", "#c59c88") 


get_params_of_file <- function(filename) {
	base <- basename(filename);
	return (str_split(str_split(base, "[.]")[[1]][1], "_")[[1]])
}

generate_legend_labels <- function(filenames) {
	first_params <- get_params_of_file(filenames[1])
	n_params <- length(first_params)
	params_same <- rep(TRUE, n_params)

	for (filename in filenames) {
		params <- get_params_of_file(filename)
		for (i in 1:n_params) {
			if (!identical(params[i], first_params[i])) params_same[i] <- FALSE
		}
	}

	labels <- c()
	for (filename in filenames) {
		params <- get_params_of_file(filename)
		label <- ""
		for (i in 1:n_params) {
			if (!params_same[i]) {
				label <- paste(label, " ", params[i])
			}
		}
		labels <- c(labels, substr(label, 2, nchar(label)))
	}
	return (labels);
}


generate_colors <- function(n_rows) {
	return (color_template[1:n_rows])
}

generate_fills <- function(n_rows) {
	ret <- c()
	for (i in 1:n_rows) {
		ret <- c(ret, alpha(color_template[i], 0.1))
	}
	return (ret)
}

finish_hist_plot <- function(plot, max_p, max_y, filenames, inv_counts, entropies) {
	n_rows <- length(filenames)

	stat_labels <- c();
	for (i in 1:n_rows) {
		stat_labels = c(stat_labels, sprintf("%3.2e      %.2e", inv_counts[i], entropies[i]))		   
	}

	plot + 
	# Inversion and Entropy result labels
	annotate("text", size=4, 
			         x=rep(max_p*0.8-max_p/15, n_rows),
		   			 y=seq(max_y*0.9, length.out=n_rows, by=-2*max_y/40),
		   	         label= stat_labels, 
		             hjust = 1
	) 
}

init_hist_plot <- function(plot, max_p, max_y, scale_p_breaks, scale_p_minor_breaks, filenames) {

	n_rows <- length(filenames)

	new_plot <- plot +
	labs(x="Inversion distance of events X(item)", y="Probability P(x) of inversion event") +
	scale_x_continuous(expand=c(0,0), limits = c(-1,max_p), breaks = seq(0,max_p,scale_p_breaks),
					 minor_breaks = seq(0,max_p,scale_p_minor_breaks)) +
	scale_y_continuous(expand=c(0,0), breaks = seq(0,max_y,max_y/5), labels = seq(0,max_y,max_y/5)) +
	coord_cartesian(ylim=c(0,max_y)) +

	# Legend colour key
	annotate("rect", xmin=rep(max_p*0.75, n_rows),
			         xmax=rep(max_p*0.75+max_p/40, n_rows),
		   			 ymin=seq(max_y*0.9 , length.out=n_rows, by=-2*max_y/40),
		   			 ymax=seq(max_y*0.9 - max_y/300, length.out=n_rows, by=-2*max_y/40),
				     color=generate_colors(n_rows),
					 fill=generate_fills(n_rows)
	) +

	# Legend labels
	annotate("text", size=4, 
			 		 x=rep(max_p*0.8, n_rows),
		   			 y=seq(max_y*0.9, length.out=n_rows, by=-2*max_y/40),
					 label=generate_legend_labels(filenames),
					 hjust = 0
	) +

	# Inversion and entroy table title
	annotate("text", size=4, x=max_p*0.8-max_p/20, y=max_y*0.6+(15*max_y/40), label=sprintf("Inversions      Entropy  "), hjust = 1) +

	# Plot theme
	theme_bw() +
	theme(axis.title=element_text(size=14), 
		axis.text=element_text(size=12),
		panel.grid.minor = element_line(size = 0.5),
		aspect.ratio=0.6,
		plot.margin=unit(c(0,10,0,10),"points")
	)

	return (new_plot)
}

save_hist_plot <- function(output_filename) {
	ggsave(output_filename)
}


output_filename <- args[1]
filenames <- args[2:length(args)]

max_p <- 16
scale_p_breaks <- 4
max_y <- 0.10

plot <- ggplot()
plot <- init_hist_plot(plot, max_p, max_y, scale_p_breaks, 1, filenames)
inversions <- c()
entropies <- c()
for (i in 1:length(filenames)) {
	f <- filenames[i]
	inv_ent_plot <- load_file_to_hist_plot(plot, f, color_template[i], i) 
	inversions <- c(inversions, inv_ent_plot[1])
	entropies <- c(entropies, inv_ent_plot[2])
	plot <- inv_ent_plot[3][[1]]
}
finish_hist_plot(plot, max_p, max_y, filenames, inversions, entropies)

save_hist_plot(output_filename)
