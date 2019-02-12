# Victor Cook
# Plot results for PODC paper

library(tidyverse)
setwd('/Users/victor/Downloads/resultsQ/')

cbbPalette <- c("#E69F00", "#56B4E9", "#009E73", "#F0E442", "#0072B2", "#D55E00", "#CC79A7")

sQ <- read.table('Qstack.dat', sep = "",header = TRUE)
# type	mix	threads	ms	ops	
sQ <- sQ %>% mutate(type=as.factor(type), mix=as.factor(mix), threads=as.numeric(threads), ms=as.numeric(ms), ops=as.numeric(ops))
op_per_ms <- sQ$ops / (sQ$ms * 1000)
sQ <- add_column(sQ, op_per_ms)
summary(sQ)

cbbPalette <- c("#E69F00", "#56B4E9", "#009E73", "#F0E442", "#0072B2", "#D55E00", "#CC79A7")

p3 <- ggplot(data=sQ, mapping=aes(x=threads, y=op_per_ms, color=type, linetype=arch, shape=mix)) +
  scale_color_manual(values=cbbPalette) +
  # theme(legend.position="top") +
  scale_linetype_manual(values=c( "dotted", "solid", "longdash", "twodash")) +
  scale_x_continuous(expand = c(0, 0), limits = c(0, 33)) + 
  scale_y_continuous(expand = c(0, 0), limits = c(0, 31)) +
  xlab("Threads") +
  ylab("Method calls / microsecond") +
  theme(axis.title=element_text(size=14), 
        axis.text=element_text(size=12), 
        legend.text=element_text(size=11),
        legend.spacing.y = unit(0.3, 'cm')) +
  geom_smooth(level=0.95) +
  geom_point(alpha=0.70) 
p3
ggsave(paste("Qstack",".png", sep = ""), p3, "png")

