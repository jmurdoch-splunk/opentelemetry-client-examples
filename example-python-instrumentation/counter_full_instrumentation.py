import time
import logging
from random import randrange

# logging.basicConfig(filename='counter.log',
#         filemode='a',
#         format='%(asctime)s - %(message)s',
#         level=logging.INFO)

####################################
# OTEL: libraries and dependencies #
####################################

# For callbacks
from typing import Iterable

# Metrics
from opentelemetry.metrics import (
    CallbackOptions,
    Observation,
    get_meter_provider,
    set_meter_provider,
    get_meter
)
from opentelemetry.sdk.metrics import MeterProvider
from opentelemetry.sdk.metrics.export import (
    ConsoleMetricExporter,
    PeriodicExportingMetricReader,
)

# Traces
from opentelemetry.trace import (
        set_tracer_provider,
        get_tracer_provider,
        get_tracer,
        get_current_span,
)
from opentelemetry.sdk.trace import TracerProvider
from opentelemetry.sdk.trace.export import (
    ConsoleSpanExporter,
    BatchSpanProcessor,
)

# Logs
from opentelemetry._logs import (
        set_logger_provider,
        get_logger_provider,
        get_logger
)
from opentelemetry.sdk._logs import LoggerProvider, LoggingHandler
from opentelemetry.sdk._logs.export import (
        ConsoleLogExporter,
        BatchLogRecordProcessor
)

# Exporters
from opentelemetry.exporter.otlp.proto.grpc.metric_exporter import OTLPMetricExporter
from opentelemetry.exporter.otlp.proto.grpc.trace_exporter import OTLPSpanExporter
from opentelemetry.exporter.otlp.proto.grpc._log_exporter import OTLPLogExporter

# Resource
from opentelemetry.sdk.resources import Resource

#########################################################
# OTEL: Resource definition for both metrics and traces #
#########################################################
my_resource = Resource.create({
    "service.name": "counter_manual_instrumentation.py",
    "deployment.environment": "jmurdoch-env",
    "service.version": "1.0.0",
})

###############################
# OTEL: Metric Provider Setup #
###############################
my_metric_readers = [
    PeriodicExportingMetricReader(
        OTLPMetricExporter(
            endpoint="localhost:4317",
            insecure=True,
            timeout=20
        )
    )
]

my_meter_provider = MeterProvider(
        resource=my_resource, 
        metric_readers=my_metric_readers)
set_meter_provider(my_meter_provider)

meter = get_meter_provider().get_meter("my.metrics")

###############################
# OTEL: Traces Provider Setup #
###############################
my_span_processor = BatchSpanProcessor(
    OTLPSpanExporter(
        endpoint="localhost:4317",
        insecure=True,
        timeout=20
    )
)

my_tracer_provider = TracerProvider(resource=my_resource)
set_tracer_provider(my_tracer_provider)

# It is not possible to instantiate a span processor with TracerProvider
my_tracer_provider.add_span_processor(my_span_processor)

tracer = get_tracer_provider().get_tracer("my.traces")

############################
# OTEL: Log Provider Setup #
############################
my_log_record_processor = BatchLogRecordProcessor(
    OTLPLogExporter(
        endpoint="localhost:4317",
        insecure=True,
        timeout=20
    )
)
my_logger_provider = LoggerProvider(resource=my_resource)
set_logger_provider(my_logger_provider)

my_logger_provider.add_log_record_processor(my_log_record_processor)

# use the local logging module
my_logging_handler = LoggingHandler(
                        level=logging.NOTSET, 
                        logger_provider=my_logger_provider)
logging.getLogger().addHandler(my_logging_handler)
my_logging = logging.getLogger("counter")
my_logging.setLevel(logging.DEBUG)

########################################################
# OTEL: Create an in-line counter to be called in code #
########################################################
test_counter = meter.create_counter(
    name="test.obs_counter",
    description="Keeps track of the counter",
    unit="1"
)

#####################################################
# OTEL: Create a gauge that calls out to a function #
#####################################################
def obs_gauge_func(options: CallbackOptions) -> Iterable[Observation]:
    yield Observation(delta_value, {})

test_obs_gauge = meter.create_observable_gauge(
    name="test.obs_gauge",
    callbacks=[obs_gauge_func]
)

#######################################################
# OTEL: the parent span that launches the child spans #
#######################################################
with tracer.start_as_current_span("parent") as parent:

    ################
    # Main Program #
    ################
    span_list = [ "first-child", "second-child", "third-child" ]

    for span_name in span_list:
        #############################################
        # OTEL: start a new child span on each loop #
        #############################################
        with tracer.start_span(span_name) as child:
            # the value we want to change by per cycle
            delta_value = 0
    
            # the value to increment in a monotonically non-decreasing function
            counter_value = 0

            my_logging.info('Info: Starting ' + span_name)
            while counter_value < 50:
                # create a new random value
                delta_value = randrange(10)

                # increment the counter with the random value
                counter_value += delta_value

                # print the values
                print("Span:", span_name, 
                      "\tDelta:", delta_value,
                      "\tCounter:", counter_value)

                #####################################
                # OTEL: Manual Increment of counter #
                #####################################
                test_counter.add(delta_value,
                    { "test.type": "Counter" }
                )
                ###############################################################
                # OTEL: Force push all metrics via OTLP, ignoring the timeout #
                ###############################################################
                my_meter_provider.force_flush()

                # wait a second
                time.sleep(1)
            my_logging.info('Info: Completed ' + span_name)
            ##################################################
            # Splunk: set error to "true" creates a red flag #
            # - will be created by OTel itself if it fails   #
            ##################################################
            # https://opentelemetry.io/docs/specs/semconv/general/trace/
            if span_name == "second-child":
                child.set_attribute("error", "true")
                # Set the keyword in the error
                child.set_attribute("http.status_code", "401")
                # 
                child.add_event("API failure detected")
                my_logging.error('Error: API failure detected in ' + span_name)
