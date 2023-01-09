# Building A Custom OpenTelemetry Collector
## Introduction / Synopsis
This is a quick guide to building a custom OpenTelemetry Collector and serves to document my own experiences at the time of writing (v0.68.0) for a niche use-case. These notes is not representative of best-practice and it is subject to change and may be completely irrelevant in the future.

**Note of caution**: When building a custom OpenTelemetry Collector in favour of a vendor-supplied distribution, you may be forgoing or invalidating support offered by a vendor. Carefully consider this when implementing in production environments.

Personal benefits for compiling a custom collector for my own use-case(s) were:
* Custom architecture (MIPS, ARMV6L, etc) for low power (Raspberry Pi) or convenient devices (OpenWRT router) where a binary may not be present
* Incorporate Splunk exporters (`signalfx` & `splunkhec`)
* Ability to define a custom feature manifest in single contiguous binary…
* … leading to a smaller binary footprint, resulting in:
	* Maximise performance, no unnecessary components running
	* Reduced vulnerability / attack potential (less things to exploit, even if unused)
	* Reduced storage requirements (e.g. 20MB vs 180MB)
	* Reduced memory requirements
* Incorporating into package-management processes

Please note, this guide does not cover the specifics for building with respect to embedded / exotic systems as this may require extensive additional steps which are out-of-scope for this guide. These steps may include: 
* building a development sandbox / staging environment
* building a cross-compilation toolchain
* utilising packaging facilities 
* time, patience and coffee

## Prerequisites	
If you are thinking about building a custom OpenTelemetry collector, you need the following:
* Have CLI competency of your preferred host platforms shell.
* Provide a host platform for building the collector (Linux, MacOS, Windows).
	* If this is a local install, you may want to temporarily disable anything that may interfere with the retrieval of go packages via HTTPS, such as VPN, ZScaler, etc.
* Installed the Go programming language software (AKA “GoLang”), either to a non-superuser location from [Go’s pre-compiled distributions](https://go.dev/dl/) or by installing globally via an OS package manager (e.g. Ubuntu): 
`$ sudo apt install golang`

## Build the OpenTelemetry Collector builder
Fetch and compile the OpenTelemetry Collector builder using the installed go distribution. The go executable may be in a different location:
```
$ export GO_BIN_PATH=~/go/bin 
$ export PATH=${GO_BIN_PATH}:${PATH}
$ GO111MODULE=on go install go.opentelemetry.io/collector/cmd/builder@latest
```
Verify the builder binary is present (directory may vary if you used the pre-compiled binaries):
```
$ ls -l go/bin/builder 
-rwxrwxr-x 1 ubuntu ubuntu 8896510 Jan  5 09:58 go/bin/builder
```

## Create an OpenTelemetry Collector build manifest
The build manifest describes how the OpenTelemetry Collector is compiled and what features are to be included. A sample build manifest is here: [https://github.com/open-telemetry/opentelemetry-collector/blob/main/cmd/otelcorecol/builder-config.yaml](https://github.com/open-telemetry/opentelemetry-collector/blob/main/cmd/otelcorecol/builder-config.yaml) 

The following configuration is one which includes the Splunk SignalFx endpoint / Splunk HEC endpoint exporters:
```
$ cat >> builder-config.yaml <<EOF
dist:
  name: otelcol
  description: Local OpenTelemetry Collector binary, testing only.
  version: 0.68.0-dev
  otelcol_version: 0.68.0

receivers:
  - gomod: go.opentelemetry.io/collector/receiver/otlpreceiver v0.68.0
exporters:
  - gomod: go.opentelemetry.io/collector/exporter/loggingexporter v0.68.0
  - gomod: go.opentelemetry.io/collector/exporter/otlpexporter v0.68.0
  - gomod: go.opentelemetry.io/collector/exporter/otlphttpexporter v0.68.0
  - gomod: github.com/open-telemetry/opentelemetry-collector-contrib/exporter/signalfxexporter v0.68.0
  - gomod: github.com/open-telemetry/opentelemetry-collector-contrib/exporter/splunkhecexporter v0.68.0
extensions:
  - gomod: go.opentelemetry.io/collector/extension/ballastextension v0.68.0
  - gomod: go.opentelemetry.io/collector/extension/zpagesextension v0.68.0
processors:
  - gomod: go.opentelemetry.io/collector/processor/batchprocessor v0.68.0
  - gomod: go.opentelemetry.io/collector/processor/memorylimiterprocessor v0.68.0
EOF
```

## KNOWN ISSUE: Mitigate an erroneous OAuth 2.0 dependency ahead of compilation
If you were to initiate a build at the time of writing (v0.68.0 on Jan 9th, 2023), the OAuth 2.0 Go module causes some dependency issues by using multiple copies of compute/metadata, as shown here:
```
go.opentelemetry.io/collector/cmd/otelcorecol imports
	go.opentelemetry.io/collector/exporter/otlpexporter imports
	go.opentelemetry.io/collector/config/configgrpc imports
	go.opentelemetry.io/contrib/instrumentation/google.golang.org/grpc/otelgrpc tested by
	go.opentelemetry.io/contrib/instrumentation/google.golang.org/grpc/otelgrpc.test imports
	google.golang.org/grpc/interop imports
	golang.org/x/oauth2/google imports
	cloud.google.com/go/compute/metadata: ambiguous import: found package cloud.google.com/go/compute/metadata in multiple modules:
	cloud.google.com/go/compute v1.6.1 (/home/ubuntu/go/pkg/mod/cloud.google.com/go/compute@v1.6.1/metadata)
	cloud.google.com/go/compute/metadata v0.2.0 (/home/ubuntu/go/pkg/mod/cloud.google.com/go/compute/metadata@v0.2.0)
```

This can be mitigated in a few ways (e.g. using replace statements in go.mod), but the simplest is to preemptively install the offending module and remove the troublesome directory (metadata) and wipe any cache related to it before proceeding to the full build:
```
$ go install cloud.google.com/go/compute@v1.6.1
$ rm -rf go/pkg/mod/cloud.google.com/go/compute@v1.6.1/metadata 
$ rm -rf ~/go/pkg/mod/cache 
$ rm -rf ~/.cache
```

## Perform the OpenTelemetry Collector build
Now we can proceed with the build:
`$ ~/go/bin/builder --config builder-config.yaml`

Output should look as follows:
```
2023-01-05T10:29:26.963Z	INFO	internal/command.go:125	OpenTelemetry Collector Builder	{“version”: “dev”, “date”: “unknown”}
2023-01-05T10:29:26.968Z	INFO	internal/command.go:158	Using config file	{“path”: “builder-config.yaml”}
2023-01-05T10:29:26.969Z	INFO	builder/config.go:107	Using go	{“go-executable”: “/usr/bin/go”}
2023-01-05T10:29:26.971Z	INFO	builder/main.go:76	Sources created	{“path”: “/tmp/otelcol-distribution4289703501”}
2023-01-05T10:32:01.816Z	INFO	builder/main.go:118	Getting go modules
2023-01-05T10:32:04.804Z	INFO	builder/main.go:87	Compiling
2023-01-05T10:32:39.614Z	INFO	builder/main.go:99	Compiled	{“binary”: “/tmp/otelcol-distribution4289703501/otelcorecol”}
```

The finished binary should then be manually copied out to any appropriate location:
```
$ ls -l /tmp/otelcol-distribution4289703501/otelcol 
-rwxrwxr-x 1 ubuntu ubuntu 20185088 Jan  5 10:32 /tmp/otelcol-distribution4289703501/otelcol
$ sudo cp /tmp/otelcol-distribution4289703501/otelcol /usr/bin
```

## Running with a basic configuration
OpenTelemetry Collector does not provide a one-size-fits-all configuration. Here is a heavily-stripped down config.yaml that accepts metrics via OTLP-over-HTTP (port 4318) as input and Splunk SignalFx as output:
```
$ cat >> config.yaml <<EOF
extensions:
  memory_ballast:
    size_mib: 512
  zpages:
    endpoint: 0.0.0.0:55679

receivers:
  otlp:
    protocols:
      http:
        endpoint: 0.0.0.0:4318

processors:
  batch:
  memory_limiter:
    limit_mib: 1536
    spike_limit_mib: 512
    check_interval: 5s

exporters:
  signalfx:
    access_token: "<PUT_YOUR_TOKEN_HERE>"
    api_url: "https://api.us1.signalfx.com"
    ingest_url: "https://ingest.us1.signalfx.com"
    sync_host_metadata: true
    correlation:
  logging:
    loglevel: debug

service:
  pipelines:
    metrics:
      receivers: [otlp]
      processors: [memory_limiter, batch]
      exporters: [signalfx]
  extensions: [memory_ballast, zpages]
EOF 
```

The OpenTelemetry Collector then can be run at the command line with:
`$ ./otelcol --config config.yaml`

Typical next steps would be to package up your binary and write some init.d / systemd init scripts for it. 

## Troubleshooting
### Initial testing / Nothing immediately working
Run the otelcol binary at the command-line and observe the output whilst it is being utilised. Bad configuration, inactivity and ancillary errors should be visible in the standard output. Logs may also be available in your platforms logging solution (e.g. syslog, Windows Events).

A simple test configuration will be a receiver with `otlp`  using  `http`  to a  `logging` exporter, using some of the JSON provided in this repo to test successful throughput.

### Receiver / Exporter checking
Incorporate the zpages extension into both OpenTelemetry Collector binaries and the configuration if not already done. It allows diagnostic introspection & instrumentation using HTTP without using any other tooling.  

## Further Resources
[OpenTelemetry Collector - Source](https://github.com/open-telemetry/opentelemetry-collector)
[OpenTelemetry Collector Builder - Source](https://github.com/open-telemetry/opentelemetry-collector/tree/main/cmd/builder)
[OpenTelemetry Collector Contrib - Additional Components](https://github.com/open-telemetry/opentelemetry-collector-contrib)
[OpenTelemetry Collector - Splunk Distribution](https://github.com/signalfx/splunk-otel-collector)
[Go Programming Language - Binaries](https://go.dev/dl/)
