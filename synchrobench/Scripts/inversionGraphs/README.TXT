To generate full set of pdf documents (histograms, raw data, entropies) in every directory: 
/1000i_100
/10000i_10
/100000i_1

Step1: Create 1k,2k,4k,8k,16k,24k,32k subfolders
Step2: Put inversions data files to appropriate subfolders
Step3: Run every of Rscript's (kFifo_entropy_1.R, kFifo_histograms_2.R, kFifo_raw_4.R) as follows
       Rscript kFifo_raw_4.R . 
       NOTE: don't forget, the last argument is a location where input inversions data should be placed
