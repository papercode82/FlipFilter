# About the Datasets

This repository provides small demo traces for quick testing. The full datasets are not redistributed here and can be obtained from their official sources.

## CAIDA

Description: This dataset contains IP-level traffic traces collected by CAIDA in 2019. Each packet is identified by its source and destination IP addresses.

Sample File: A small demo trace is included in this repository. The demo file contains source and destination IP addresses:

```text
src_ip dst_ip
```

Full Dataset: Available upon request from the official CAIDA website:

https://catalog.caida.org/dataset/passive_2019_pcap

## StackOverflow

Description: The StackOverflow demo trace is derived from Stack Exchange data. Each record in the demo file contains a content ID and a user ID.

Sample File: A small demo trace is included in this repository. The demo file is formatted as:

```text
content_id user_id
```

Source Data: The original data dump is available from Internet Archive:

https://archive.org/details/stackexchange

Raw download directory:

https://archive.org/download/stackexchange
