# How to use measure.py script
The tool measures the throughput using Read and Generate applications and produces
a CSV output file for analysis.

Usage:
* Clone `https://github.com/CESNET/ndk-fpga` repository and install `ofm`: https://github.com/CESNET/ndk-fpga/tree/devel/python/ofm
* Install python3.11-nfb package
* Start the measurement script in the desired mode, example: `sudo python measure.py --mode rx -t 10 -l32 --queues-per-port 16 --ports 2 -d /dev/nfb1`
* Adjust parameters in the measurement.py script if necessary (e.g., burst size or core allocation)
* Values produced by this script are in following csv format: `Length,DMA Speed,L2 Speed,L1 Speed`.
* Analyze the results in the generated CSV file to evaluate performance. You can use attached `charts.py` to create throughtput graph.
