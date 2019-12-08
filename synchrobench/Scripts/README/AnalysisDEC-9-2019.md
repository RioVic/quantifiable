# Overview

Tests were done across several variables:

1. Number of physical cores (1, 2, 4, 6, 12) - this was simulated with best attempt by limiting the number of processors in virtualbox.
2. Number of running threads (2, 4, 6, 15, 100, 1000)
3. Number of unique items (1, 2, 5, 20)
4. Number of operations (500k, 3m)

Each test is a run of random operations.

It is expected that number of operations shouldn't affect results at all, however they do. The 500k cases have systematically fewer inversions than 3m cases. However the results are repeatable, which could mean that this was due to predicatable behavior as the threads "warmed up" and started executing one by one. So it is OK to just interpret the 3m results.

# Experiment setup

The experiment runs in a Virtualbox VM with ubuntu, with a core i7 8750H (6 cores, 12 threads w/hyperthreading)

Number of cores was simulated by limiting the "number of processors" setting in virtualbox. Note that the operating system still schedules the VM threads across all bare metal cores. 

# Observations

## 6 processor results:

The following comment was made by me (max) and Victor after observing the 6 thread results for 1 item:

Max Wu: "The hypothesis is that more threads = more frequent and larger inversions = more entropy. From the graph, we see that from 2 to 6 threads, the number of (1) inversions actually decrease but this is compensated with increase in (>1) inversions. From 15 to 1000 threads, the number of (1) actually increase but it's easy to forget that this is expected, and occurrences of all inversion sizes (except 0 of course) increase."

Victor: [7:31 a.m., 2019-12-01] Victor Cook: I like what you said about 2-6 threads
[7:32 a.m., 2019-12-01] Victor Cook: Up to the number of cores we see one behavior
[7:32 a.m., 2019-12-01] Victor Cook: Then it changes.

So I tested that hypothesis with fewer and more processors.

## How do # virtualbox processors affect entropy

[7:48 a.m., 2019-12-01] Victor Cook: I have a 32 core 64 hyper server in my office.
[7:49 a.m., 2019-12-01] Victor Cook: Just run your tests and help us setup the experiment.   That is why I want you to spend time on your description and use cases.
[7:55 a.m., 2019-12-01] Max wu: Good to know. And yeah I expected you guys to run tests yourselves for the paper, I'm only writing the scripts and finding some insights. I wonder if there's some kind of simulator for the more core case. But I feel that might not be useful anyway because it will depend on how Intel/amd decide to make 1000 cores in the future.
[7:56 a.m., 2019-12-01] Max wu: Like chiplets vs monaloth, how cache is shared, etc.

By comparing the 1 item results across 1, 2, 4, 6 and 12 processors, we see that the general trend is that more cores = more inversions and more entropy. For the single core case, the inversions are close to zero no matter the number of threads. This is probably because inversions can only happen during scheduling switches in a single core system, which only affect a microscopic fraction of instructions run.

### Unusual result

A reasonable hypothesis is that the experiment with 2 threads would run the same whether there are 2 cores in the system or 4 or 64. However, it seems that the inversions and entropies of 2 threads, 4 threads and 6 threads actually increased when going from 6 to 12 cores. 2 and 4 core only increased marginally, while 6 threads increased by a lot. 
*This is a surprising result: Investing on a machine with more cores will actually lead to worse performance with lazy-lists when limited to a few threads, a discovery made thanks to entropy*.

## How do # items affect entropy

A reasonable hypothesis is that fewer items lead to more entropy, as the threshold required to cause a surprise is much smaller with fewer items. This is mostly true. However...

### Interesting result

It appears that when the min(#processors, #threads) <= # items, the entropy is rather high. But as soon as the # threads > # items, the entropy drops dramatically, although still present.

I believe this is caused by behavior in lazy lists which I have not spent time investigating, and that we can find different quirks like this with other data structures. Perhaps the way lazy lists allocates resources in contention works great when the number of resources is fewer than threads.

This perhaps means that, more items can actually cause more entropy too in cases with few available threads, by causing chaos with shared resources.

You have to pull up multiple graphs to see this result.

## How do #threads affect entropy

One would expect that more threads lead to more entropy as contention increases. This is true, just until the number of threads reach the number of unique items. Afterwards, the contention drops off a cliff, but then continue to rise gradually.
