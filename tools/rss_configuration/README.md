# DYNANIC RSS Configuration Tool

This Python script configures Receive Side Scaling (RSS) for the DYNANIC FPGA firmware. It allows you to define hash functions, hash keys, packet input configurations, and manage the Redirection Table (RETA) across Ethernet channels.

This script is based on a CESNET's tool from ndk-app-nic here:
https://gitlab.liberouter.org/ndk/ndk-app-nic/-/blob/devel/app/sw/ndk_nic_ctl/ndk_nic_ctl/nic_rss.py?ref_type=heads

An example of a configuration file is in `rss_conf.yaml`.

## Requirements

* Python 3
* `nfb` package
* `PyYAML`

## Usage

Run the script via the command line to apply configurations, modify the RETA table, or dump the current state.

```bash
# Apply a YAML configuration file (Recommended)
./rss_ctl.py --rss_conf rss_conf.yaml

# Dump the current RSS configuration (Default)
./rss_ctl.py -d /dev/nfb0 -i 0 --dump

# Reset RETA table for a specific interface (e.g., interface 0 to value 1)
./rss_ctl.py --reta_rst 0 1

# Edit a specific RETA item (interface 0, index 5, queue 2)
./rss_ctl.py --reta_edit 0 5 2

```

**Global Options:**

* `-d, --device`: Path to the NFB device (default: `/dev/nfb0`).
* `-i, --index`: Index of the RSS component (relative to the APP core, default: `0`).

## YAML Configuration File

The script uses a YAML file to apply bulk configurations. Key parameters include:

* **`ifc`**: Ethernet channel to configure (`all` or a specific integer).
* **`input_conf`**: Packet types and fields used for hashing (e.g., `ipv4`, `ipv6`, `sorted_input`, `nonfrag_ipv4_tcp`).
* **`hash_type`**: Algorithm used (`toeplitz` or `simple_xor`).
* **`hash_key`**: 40-byte hex value, `default`, or `symmetric`.
* **`reta`**: Defines the automatic distribution range (`queue_min` to `queue_max`).
