OpenTelemetry Client Examples
=============================

In this repo there are a few examples of how to implement OpenTelemetry from a client software perspective:

Examples are:
- JSON examples for a log and a metric, derived from the latest proto definition
- C example (IoT implementation on Arduino/RP2040/ESP32) using the aformentioned JSON
- Building a Custom OpenTelemetry Collector 
  - Only recommended for advanced use-cases or development
  - If in doubt, pick up an official release here:
    - [OpenTelemetry Upstream](https://github.com/open-telemetry/opentelemetry-collector/releases)
    - [Splunk-supported Distribution](https://github.com/signalfx/splunk-otel-collector/releases)

All examples function with OpenTelemetry Collector 0.6.8
