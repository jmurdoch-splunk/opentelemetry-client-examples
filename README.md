OpenTelemetry Client Examples
=============================

In this repo there are a few examples of metrics that can be sent to an OpenTelemetry OTLP HTTP endpoint. 

This uses JSON interpretations of the proto definition - not protobuf, so are not representative of good practice or performance.

Examples are:
- Arduino code for a RP2040 host that logs to /v1/metrics
- Logs example in JSON for /v1/metrics
- Metric example in JSON for /v1/logs

All examples are function on OpenTelemetry Collector 0.6.4.
