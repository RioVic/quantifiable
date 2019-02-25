# Victor Cook
# Plot Stack results for PODC paper

library(tidyverse)
setwd('/Users/victor/Downloads/resultsQ/StackAMD')

cbbPalette <- c("#E69F00", "#56B4E9", "#009E73", "#F0E442", "#0072B2", "#D55E00", "#CC79A7")

sQ <- read.table('StackAMD.dat', sep = "",header = TRUE)
# type	mix	threads	ms	ops
sQ <- sQ %>% mutate(architecture=as.factor(arch), algorithm=as.factor(type), 
                    mix=as.factor(mix), threads=as.numeric(threads), 
                    ms=as.numeric(ms), ops=as.numeric(ops))
op_per_ms <- sQ$ops / (sQ$ms * 1000)
sQ <- add_column(sQ, op_per_ms)
xOffset <- ifelse(sQ$mix != "25-75", sQ$threads, sQ$threads-0.2)
sQ <- add_column(sQ, xOffset)
sQ$xOffset
sQ$xOffset <- ifelse(sQ$mix != "75-25", sQ$xOffset, sQ$threads+0.2)
sQ$xOffset
summary(sQ)
sQ

cbbPalette <- c("#E69F00", "#56B4E9", "#009E73", "#F0E442") # , "#009E73", "#0072B2", "#D55E00", "#CC79A7")

p3 <- ggplot(data=sQ, aes(x=xOffset, y=op_per_ms, color=algorithm, 
                                  linetype=algorithm, shape=mix)) +  #shape=mix
  geom_smooth(level=0.95, span = 0.33, size=0.5) +
  # stat_smooth(method = "loess", formula = y ~ x, size = 1) +
  geom_point(alpha=0.75, size=1) + # shape=3,
  scale_color_manual(values=cbbPalette) +
  # theme(legend.position="top") +
  scale_linetype_manual(values=c( "dashed", "solid", "dotted", "twodash")) +
  scale_shape_manual(values=c(1, 3, 4)) + 
  scale_x_continuous(expand = c(0, 0), limits = c(0.5, 64.5), 
                     minor_breaks = seq(0.5,64.5,1.0)) + 
  scale_y_continuous(expand = c(0, 0), limits = c(0, 87),
                     minor_breaks = seq(5,87,5)) +
  xlab("Threads") +
  ylab(expression("Method calls / "~mu~"-sec")) + theme_bw() +
  theme(axis.title=element_text(size=18), 
        axis.text=element_text(size=14),
        panel.grid.major.x = element_blank(),
        panel.grid.minor = element_line(size = 0.5),
        legend.text=element_text(size=18),
        #legend.spacing.x = unit(0.1, 'cm'),
        legend.position="top",
        legend.title = element_blank(),
     #   legend.key = element_rect(fill = 'white', size = 2.5),
      #  legend.key.fill = 'white',
        legend.key = element_rect(fill = "white"),
        legend.key.width = unit(1, "cm")) +
   guides(color=guide_legend(override.aes=list(fill=NA))) +
  guides(shape = guide_legend(override.aes = list(size = 3)))
  guides(fill=guide_legend(nrow=2,byrow=TRUE))
p3
ggsave(paste("StackAMDH",".png", sep = ""), p3, "png")
