# lpm_perf.py: Tool to generate graphs of LPM performance based on data from table_filler example.
# Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
# SPDX-License-Identifier: BSD-3-Clause
# Author: Pavlina Patova <patova@dyna-nic.com>, April 2026
#
# Run the 'table_filler' application to obtain the data required by this script.
# If necessary, add new keys or alter existing ones to ensure compatibility with the LPM table.
# Use the following arguments:
# --failure-exit == To exit on first failure, so that we can get the number of rules inserted before the failure.
# --next-stats 1000 == Print stats every 1000 ms (1 second). This script does not read the time value from files; rather, it expects data to be printed at one-second intervals.
# Select the appropriate key type (1, 2, or 3) and group (0 or 1).
# Example command: ./table_filler --key 1 --group 0 --rules 1000000 --next-stats 1000 --failure-exit 2>random_prefix32.txt
# This command will provide data on random IPv4 addresses.
# Use the -i parameter to obtain data for incremental prefixes.
# In order to obtain data on exclusive prefixes, you will need to modify the code that generates them.
# For example change ipc increment:
#   ```
#   ipv4.hdr.dst_addr = ipc;
#   ipc += 256;
#   ```
#
# Unauthorized copying of this file, via any medium is strictly prohibited.
# Proprietary and confidential, additional license terms may apply.

import matplotlib.pyplot as plt


# Insert file names with data aquired from table_filler output into lists.
all_files = [
    'exclusive_prefix24.txt',
    'increment_prefix24.txt',
    'increment_prefix32.txt',
    'random_prefix24.txt',
    'random_prefix32.txt',

    'exclusive_prefix24_old.txt',
    'increment_prefix32_old.txt',
    'random_prefix32_old.txt',
]

save_file_name = 'lpm_performance.png'


def process_file(file_name):
    data_list = []
    try:
        with open(file_name, 'r', encoding='utf-8') as f:
            for line in f:
                # Split the line by comma and process each part
                columns = line.split(',')
                for col in columns:
                    # Remove text before the colon (including the colon)
                    vals = col.split(':')

                    # Do not process lines that start with 'failed' or 'time[ms]'
                    if vals[0].strip() == 'failed' or vals[0].strip() == 'time[ms]':
                        continue

                    # Convert to number and store
                    rules = vals[-1].strip()
                    if rules:
                        try:
                            data_list.append(float(rules))
                        except ValueError:
                            print(f"Warning: Value '{rules}' in the file {file_name} can not be converted to a number.")
        return data_list
    except FileNotFoundError:
        print(f"Error: File {file_name} not found.")
        return []


def main():
    plt.figure(figsize=(10, 6))

    for file in all_files:
        clean_data = process_file(file)
        if clean_data:
            plt.plot(clean_data, marker='o', label=f'Data from file: {file}')

    plt.title('LPM performance')
    plt.xlabel('Time [s]')
    plt.ylabel('Number of rules')
    plt.legend()
    plt.grid(True, linestyle='--', alpha=0.7)

    plt.tight_layout()
    plt.show()

    plt.savefig(save_file_name, bbox_inches='tight')


if __name__ == "__main__":
    main()
