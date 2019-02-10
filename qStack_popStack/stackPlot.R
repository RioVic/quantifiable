# Victor Cook
# Plot results for PODC paper

library(tidyverse)
setwd('C:/Users/zpain/Desktop/Qstack')

sr <- read_csv('results.csv')
sr <- sr %>% mutate(type=as.factor(type), mix=as.factor(mix), threads=as.numeric(threads), clock_ms=as.numeric(ms), ops=as.numeric(ops))
op_per_ms <- sr$ops / sr$clock_ms
sr <- add_column(sr, op_per_ms)
summary(sr)


p1 <- ggplot(data=sr, mapping=aes(x=threads, y=op_per_ms, color=type, shape=mix)) +
  scale_x_continuous(expand = c(0, 0), limits = c(1, 8)) + 
  scale_y_continuous(expand = c(0, 0)) +
  geom_smooth() +
  geom_point(alpha=0.75)
p1

ggsave(paste("stack50",".png", sep = ""), p1, "png")

